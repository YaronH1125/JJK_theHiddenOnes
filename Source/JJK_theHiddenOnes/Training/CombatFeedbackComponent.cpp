#include "Training/CombatFeedbackComponent.h"
#include "Training/CombatFeedbackProfile.h"
#include "Training/CombatHitComponent.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Training/TrainingGameMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CStop(TEXT("JJK.Feedback.HitStop"), 0, TEXT("Local hit stop. Enable only after clock regression."));
static TAutoConsoleVariable<int32> CReaction(TEXT("JJK.Feedback.Reaction"), 1, TEXT("Reaction animation channel"));
static TAutoConsoleVariable<int32> CAudio(TEXT("JJK.Feedback.Audio"), 1, TEXT("Audio consumer channel"));
static TAutoConsoleVariable<int32> CFX(TEXT("JJK.Feedback.RangedFX"), 1, TEXT("Ranged impact channel"));
static TAutoConsoleVariable<int32> CCamera(TEXT("JJK.Feedback.Camera"), 1, TEXT("Camera consumer channel"));
static TAutoConsoleVariable<int32> CHUD(TEXT("JJK.Feedback.HUD"), 1, TEXT("HUD consumer channel"));
static TAutoConsoleVariable<int32> CLog(TEXT("JJK.Feedback.Log"), 1, TEXT("Real settlement/session log"));

UCombatFeedbackComponent::UCombatFeedbackComponent()
{
 PrimaryComponentTick.bCanEverTick = true;
 PrimaryComponentTick.TickGroup = TG_PrePhysics;
}
void UCombatFeedbackComponent::BeginPlay()
{
 Super::BeginPlay();
 OnContact.AddUniqueDynamic(this, &UCombatFeedbackComponent::HandleContactStop);
 auto* F = CastChecked<AFighterCharacter>(GetOwner());
 F->AddTickPrerequisiteComponent(this);
 F->GetMesh()->AddTickPrerequisiteComponent(this);
 F->GetCharacterMovement()->AddTickPrerequisiteComponent(this);
 // Delegate is registered once by each fighter, but only the first valid dispatcher flushes.
 FlushHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &UCombatFeedbackComponent::FlushWorldCallback);
}
void UCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 Cleanup(ECombatFeedbackEnd::Destroyed);
 FWorldDelegates::OnWorldPostActorTick.Remove(FlushHandle);
 OnContact.RemoveDynamic(this, &UCombatFeedbackComponent::HandleContactStop);
 Super::EndPlay(Reason);
}
void UCombatFeedbackComponent::FlushWorldCallback(UWorld* World, ELevelTick TickType, float Delta)
{
 if (World != GetWorld()) return;
 for (TActorIterator<AFighterCharacter> It(World); It; ++It)
 {
  if (It->GetCombatFeedback() != this) return;
  FlushWorld(World, TickType, Delta);
  return;
 }
}
void UCombatFeedbackComponent::FlushWorld(UWorld* World, ELevelTick, float)
{
 for (TActorIterator<AFighterCharacter> It(World); It; ++It) It->ProcessCombatEvents();
 for (TActorIterator<AFighterCharacter> It(World); It; ++It) It->GetCombatFeedback()->ApplyPendingStop();
}
int32 UCombatFeedbackComponent::Round(UWorld* World)
{
 const auto* GM = World ? World->GetAuthGameMode<ATrainingGameMode>() : nullptr;
 return GM ? GM->GetFeedbackRoundId() : 1;
}
bool UCombatFeedbackComponent::IsCurrent(int32 R, int32 G) const { return R == Round(GetWorld()) && G == Generation; }
const UCombatFeedbackProfile* UCombatFeedbackComponent::GetProfile() const
{
 const auto* F = Cast<AFighterCharacter>(GetOwner());
 return F && F->GetDefinition() && F->GetDefinition()->FeedbackProfile
  ? F->GetDefinition()->FeedbackProfile.Get() : GetDefault<UCombatFeedbackProfile>();
}
bool UCombatFeedbackComponent::UsesExternalRangedImpact() const { return GetProfile()->bExternalRangedImpact; }
ECombatFeedbackTier UCombatFeedbackComponent::GetAttackTier(const UAttackDefinition* Attack) const
{
 const auto* Fighter = Cast<AFighterCharacter>(GetOwner());
 const auto* Definition = Fighter ? Fighter->GetDefinition() : nullptr;
 if (!Attack || !Definition) return ECombatFeedbackTier::Light;
 if (Attack == Definition->HeavyPunchDefinition || Attack == Definition->HeavyKickDefinition) return ECombatFeedbackTier::Heavy;
 if ((Definition->ComboSegments.Num() > 1 && Attack == Definition->ComboSegments.Last())
  || (Definition->KickSegments.Num() > 1 && Attack == Definition->KickSegments.Last())) return ECombatFeedbackTier::Finisher;
 if (Definition->KickSegments.Contains(Attack)
  || (Definition->ComboSegments.Num() > 2 && Attack == Definition->ComboSegments[2])) return ECombatFeedbackTier::Medium;
 return ECombatFeedbackTier::Light;
}
bool UCombatFeedbackComponent::IsChannelEnabled(ECombatFeedbackChannel C) const
{
 switch (C) {
 case ECombatFeedbackChannel::HitStop: return CStop.GetValueOnGameThread() != 0;
 case ECombatFeedbackChannel::Reaction: return CReaction.GetValueOnGameThread() != 0;
 case ECombatFeedbackChannel::Audio: return CAudio.GetValueOnGameThread() != 0;
 case ECombatFeedbackChannel::RangedFX: return CFX.GetValueOnGameThread() != 0;
 case ECombatFeedbackChannel::Camera: return CCamera.GetValueOnGameThread() != 0;
 default: return CHUD.GetValueOnGameThread() != 0;
 }
}
FTransform UCombatFeedbackComponent::MuzzleTransform() const
{
 const auto* F = CastChecked<AFighterCharacter>(GetOwner());
 if (F->GetMesh()->DoesSocketExist(F->GetMuzzleSocketName())) return F->GetMesh()->GetSocketTransform(F->GetMuzzleSocketName());
 return FTransform(F->GetActorRotation(), F->GetActorLocation() + FVector(0,0,60));
}
int64 UCombatFeedbackComponent::BeginAction(FName Move, ECombatFeedbackTier Tier)
{
 const int64 Id = ++NextSession;
 Sessions.Add(Id, {Move, Tier});
 Action(Id, ECombatActionStage::Start);
 return Id;
}
void UCombatFeedbackComponent::Action(int64 Id, ECombatActionStage Stage, float Q, ECombatFeedbackEnd Reason, int64 Attack, const FTransform* MuzzleOverride)
{
 FSession* S = Sessions.Find(Id);
 if (!S) return;
 if (Stage == ECombatActionStage::Full) { if (S->bFull || Q < 1.f - KINDA_SMALL_NUMBER) return; S->bFull = true; ++FullCount; }
 if (Stage == ECombatActionStage::Fire) { if (!Attack || S->Fired.Contains(Attack)) return; S->Fired.Add(Attack); ++FireCount; }
 FCombatActionFeedback E;
 E.Source = Cast<AFighterCharacter>(GetOwner()); E.RoundId = Round(GetWorld()); E.Generation = Generation;
 E.SessionId = Id; E.AttackInstanceId = Attack; E.MoveId = S->Move; E.Tier = S->Tier;
 E.Stage = Stage; E.PaidQ = Q; E.Muzzle = MuzzleOverride ? *MuzzleOverride : MuzzleTransform(); E.EndReason = Reason;
 if (Stage == ECombatActionStage::End) Sessions.Remove(Id);
 LastAction = E; ++ActionCount;
 if (CLog.GetValueOnGameThread() && Stage != ECombatActionStage::Update)
  UE_LOG(LogTemp, Log, TEXT("[FeedbackAction] source=%s round=%d gen=%d session=%lld move=%s stage=%d q=%.4f reason=%d attack=%lld"),
   *GetNameSafe(GetOwner()), E.RoundId, E.Generation, Id, *E.MoveId.ToString(), int32(Stage), Q, int32(Reason), Attack);
 OnAction.Broadcast(E);
}
void UCombatFeedbackComponent::PublishContact(FCombatContactFeedback E)
{
 if (!E.Source.IsValid() || E.Source.Get() != GetOwner() || !IsCurrent(E.RoundId, E.SourceGeneration)) return;
 if (E.Target.IsValid() && !E.Target->GetCombatFeedback()->IsCurrent(E.RoundId, E.TargetGeneration)) return;
 const FString Key = FString::Printf(TEXT("%d:%lld:%d:%u"), E.RoundId, E.AttackInstanceId, E.SegmentId, E.Target.IsValid() ? E.Target->GetUniqueID() : 0);
 if (ContactKeys.Contains(Key)) return;
 ContactKeys.Add(Key); LastContact = E; ++ContactCount;
 if (CLog.GetValueOnGameThread()) UE_LOG(LogTemp, Log, TEXT("[FeedbackContact] source=%s target=%s round=%d attack=%lld segment=%d move=%s result=%d tier=%d damage=%.3f armor=%d lethal=%d fallback=%d location=%s normal=%s"),
  *GetNameSafe(E.Source.Get()), *GetNameSafe(E.Target.Get()), E.RoundId, E.AttackInstanceId, E.SegmentId, *E.MoveId.ToString(), int32(E.Result), int32(E.Tier), E.ActualDamage, E.bArmored, E.bLethal, E.bLocationFallback, *E.Location.ToString(), *E.Normal.ToString());
 OnContact.Broadcast(E);
}
void UCombatFeedbackComponent::HandleContactStop(const FCombatContactFeedback& E)
{
 const float Stop = GetProfile()->StopFor(E);
 if (!E.bRanged && !E.bLethal) RequestStop(Stop, true);
 if (E.Target.IsValid() && !E.bArmored && !E.bLethal) E.Target->GetCombatFeedback()->RequestStop(Stop);
}
void UCombatFeedbackComponent::Lifecycle(int64 Attack, ECombatFeedbackEnd Reason, FVector Location)
{
 FCombatLifecycleFeedback E; E.RoundId = Round(GetWorld()); E.Generation = Generation;
 E.Source = Cast<AFighterCharacter>(GetOwner()); E.AttackInstanceId = Attack; E.Reason = Reason; E.Location = Location;
 if (CLog.GetValueOnGameThread()) UE_LOG(LogTemp, Log, TEXT("[FeedbackLife] source=%s round=%d gen=%d attack=%lld reason=%d"), *GetNameSafe(GetOwner()), E.RoundId, Generation, Attack, int32(Reason));
 LastLifecycle = E; ++LifecycleCount;
 OnLifecycle.Broadcast(E);
}
void UCombatFeedbackComponent::Cleanup(ECombatFeedbackEnd Reason)
{
 ClearStop(TEXT("cleanup"));
 for (const auto& Effect : OwnedEffects) if (Effect.IsValid()) Effect->DestroyComponent();
 OwnedEffects.Reset();
 TArray<int64> Ids; Sessions.GetKeys(Ids);
 for (int64 Id : Ids) {
  const FSession* Session = Sessions.Find(Id);
  // Input release does not end the world-owned domain. Its scheduler must keep
  // publishing real Fire/End events after menu/focus cleanup invalidates callbacks.
  if (Reason == ECombatFeedbackEnd::Cancel && Session && Session->Tier == ECombatFeedbackTier::DomainOrb) continue;
  Action(Id, ECombatActionStage::End, 0, Reason);
 }
 Lifecycle(0, Reason, GetOwner()->GetActorLocation());
 ++Generation; ContactKeys.Reset();
}
double UCombatFeedbackComponent::GetActionTime() const
{
 return (bStopped ? StopStart : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.)) - PausedTime;
}
void UCombatFeedbackComponent::RequestStop(float Seconds, bool bOutgoingConfirmation)
{
 if (Seconds > 0.f && IsChannelEnabled(ECombatFeedbackChannel::HitStop))
 {
  PendingStop = FMath::Max(PendingStop, Seconds);
  bPendingOutgoingStop |= bOutgoingConfirmation;
 }
}
void UCombatFeedbackComponent::ApplyPendingStop()
{
 const float Requested = PendingStop; PendingStop = 0.f;
 const bool bOutgoingConfirmation = bPendingOutgoingStop; bPendingOutgoingStop = false;
 // Resume at the same end-of-frame boundary as entry: animation, timer and local
 // timestamps all skip the same ticks. Do not begin another stop at the cap.
 if (bStopped) {
  const double Now = GetWorld()->GetTimeSeconds();
  if (Requested > 0.f) StopEnd = FMath::Min(StopStart + FMath::Clamp(GetProfile()->ContinuousStopLimit, 0.f, .100f), FMath::Max(StopEnd, Now + Requested));
  if (!IsChannelEnabled(ECombatFeedbackChannel::HitStop) || Now >= StopEnd) { ClearStop(TEXT("deadline")); return; }
 }

 auto* F = CastChecked<AFighterCharacter>(GetOwner());
 if (Requested <= 0.f || !IsChannelEnabled(ECombatFeedbackChannel::HitStop) || F->IsDead()
   || (F->HasSuperArmor() && !bOutgoingConfirmation) || F->IsThrowPaired() || F->HasCombatTag(TAG_State_KnockedDown)) return;
 const double Now = GetWorld()->GetTimeSeconds();
 const double Cap = FMath::Clamp(GetProfile()->ContinuousStopLimit, 0.f, .100f);
 if (!bStopped)
 {
  bStopped = true; StopStart = Now; StopEnd = Now;
  bSavedPauseAnims = F->GetMesh()->bPauseAnims;
  bSavedMovementTick = F->GetCharacterMovement()->IsComponentTickEnabled();
  // Evaluate the reaction that was established by the batch before holding its pose.
  F->GetMesh()->TickAnimation(0.f, false);
  F->GetMesh()->RefreshBoneTransforms();
  F->GetMesh()->bPauseAnims = true;
  F->GetCharacterMovement()->SetComponentTickEnabled(false);
  F->PauseActionTimers(true);
 }
 // Refresh even on reentry: a new reaction may have replaced the held montage.
 const bool WasPaused = F->GetMesh()->bPauseAnims;
 F->GetMesh()->bPauseAnims = false;
 F->GetMesh()->TickAnimation(0.f, false);
 F->GetMesh()->RefreshBoneTransforms();
 F->GetMesh()->bPauseAnims = WasPaused;
 StopEnd = FMath::Min(StopStart + Cap, FMath::Max(StopEnd, Now + Requested));
 UE_LOG(LogTemp, Log, TEXT("[HitStop] %s start=%.6f end=%.6f request=%.6f"), *F->GetName(), StopStart, StopEnd, Requested);
}
void UCombatFeedbackComponent::ClearStop(const TCHAR* Reason)
{
 PendingStop = 0.f;
 bPendingOutgoingStop = false;
 if (!bStopped) return;
 auto* F = CastChecked<AFighterCharacter>(GetOwner());
 const double Now = GetWorld()->GetTimeSeconds();
 LastStopDuration = Now - StopStart; PausedTime += LastStopDuration; bStopped = false;
 F->GetMesh()->bPauseAnims = bSavedPauseAnims;
 F->GetCharacterMovement()->SetComponentTickEnabled(bSavedMovementTick);
 F->ConsumeMovementInputVector();
 F->PauseActionTimers(false);
 F->GetCombatHit()->ResetSweepHistory();
 UE_LOG(LogTemp, Log, TEXT("[HitStop] %s resume=%s measured=%.6f requested=%.6f tick=%.6f"), *F->GetName(), Reason, LastStopDuration, StopEnd - StopStart, GetWorld()->GetDeltaSeconds());
}
void UCombatFeedbackComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Delta, TickType, Tick);
 // Deadline processing belongs to the world post-actor safe point.
}

void UCombatFeedbackComponent::TrackTransientEffect(UActorComponent* Effect)
{
 OwnedEffects.RemoveAll([](const auto& P) { return !P.IsValid(); });
 if (IsValid(Effect)) OwnedEffects.AddUnique(Effect);
}
