#include "Training/Feedback/Animation/CombatPoseConsumer.h"

#include "Training/CombatFeedbackComponent.h"
#include "Training/CombatFeedbackProfile.h"
#include "Training/CombatInputComponent.h"
#include "Training/CombatTypes.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

UCombatPoseConsumer::UCombatPoseConsumer()
{
 PrimaryComponentTick.bCanEverTick = true;
 PrimaryComponentTick.TickGroup = TG_PostPhysics;
 // Hard references on the configured native consumer CDO establish preload and Cook dependencies.
 static ConstructorHelpers::FObjectFinder<UAnimMontage> Start(TEXT("/Game/Characters/Ishigori/Repaired/Feedback/IG_AM_FB_GuardStart.IG_AM_FB_GuardStart"));
 static ConstructorHelpers::FObjectFinder<UAnimMontage> Loop(TEXT("/Game/Characters/Ishigori/Repaired/Feedback/IG_AM_FB_GuardLoop.IG_AM_FB_GuardLoop"));
 static ConstructorHelpers::FObjectFinder<UAnimMontage> End(TEXT("/Game/Characters/Ishigori/Repaired/Feedback/IG_AM_FB_GuardEnd.IG_AM_FB_GuardEnd"));
 static ConstructorHelpers::FObjectFinder<UAnimMontage> Recoil(TEXT("/Game/Characters/Ishigori/Repaired/Feedback/IG_AM_FB_SuperRecoil.IG_AM_FB_SuperRecoil"));
 GuardStart = Start.Object; GuardLoop = Loop.Object; GuardEnd = End.Object; SuperRecoil = Recoil.Object;
}

void UCombatPoseConsumer::BeginPlay()
{
 Super::BeginPlay();
 Fighter = Cast<AFighterCharacter>(GetOwner());
 Feedback = Fighter.IsValid() ? Fighter->GetCombatFeedback() : nullptr;
 if (!Feedback.IsValid()) return;
 SeenGeneration = Feedback->GetGeneration();
 Feedback->OnAction.AddUniqueDynamic(this, &UCombatPoseConsumer::HandleAction);
 Feedback->OnLifecycle.AddUniqueDynamic(this, &UCombatPoseConsumer::HandleLifecycle);
}

void UCombatPoseConsumer::EndPlay(const EEndPlayReason::Type Reason)
{
 if (Feedback.IsValid())
 {
  Feedback->OnAction.RemoveDynamic(this, &UCombatPoseConsumer::HandleAction);
  Feedback->OnLifecycle.RemoveDynamic(this, &UCombatPoseConsumer::HandleLifecycle);
 }
 StopOwnedPose();
 Super::EndPlay(Reason);
}

UAnimInstance* UCombatPoseConsumer::Anim() const
{
 return Fighter.IsValid() && Fighter->GetMesh() ? Fighter->GetMesh()->GetAnimInstance() : nullptr;
}

bool UCombatPoseConsumer::Enabled() const
{
 return Fighter.IsValid() && Feedback.IsValid() && Feedback->IsChannelEnabled(ECombatFeedbackChannel::Reaction);
}

bool UCombatPoseConsumer::HighPriorityState() const
{
 return !Fighter.IsValid() || Fighter->IsDead() || Fighter->IsThrowPaired()
  || Fighter->HasCombatTag(TAG_State_HitStun) || Fighter->HasCombatTag(TAG_State_KnockedDown)
  || Fighter->HasCombatTag(TAG_State_GettingUp) || Fighter->HasCombatTag(TAG_State_DodgeInvulnerable)
  || Fighter->HasCombatTag(TAG_State_DodgeRecovery) || Fighter->HasCombatTag(TAG_State_StanceSwitching)
  || Fighter->HasCombatTag(TAG_State_DomainCasting);
}

bool UCombatPoseConsumer::Play(UAnimMontage* Montage, FName State)
{
 UAnimInstance* Instance = Anim();
 if (!Instance || !Montage) return false;
 if (Instance->Montage_Play(Montage) <= 0.f) return false;
 OwnedMontage = Montage; PoseState = State;
 return true;
}

void UCombatPoseConsumer::StopOwnedPose()
{
 if (UAnimInstance* Instance = Anim())
  if (OwnedMontage.IsValid() && Instance->Montage_IsActive(OwnedMontage.Get()))
   Instance->Montage_Stop(0.f, OwnedMontage.Get());
 OwnedMontage.Reset(); PoseState = NAME_None; bGuardEngaged = false;
}

void UCombatPoseConsumer::StopReactionMontages()
{
 // Hit/guard impacts are started by FighterCharacter after settlement. Turning
 // Reaction off removes only that library's poses, never an attack or dodge.
 const UFighterDefinition* Definition = Fighter.IsValid() ? Fighter->GetDefinition() : nullptr;
 const UCombatReactionLibrary* Library = Definition ? Definition->ReactionLibrary.Get() : nullptr;
 UAnimInstance* Instance = Anim();
 if (!Library || !Instance) return;
 for (UAnimMontage* Montage : {Library->Light.Get(), Library->Heavy.Get(), Library->Guard.Get(), Library->Left.Get(), Library->Right.Get(), Library->Back.Get()})
  if (Montage && Instance->Montage_IsActive(Montage)) Instance->Montage_Stop(0.f, Montage);
}

void UCombatPoseConsumer::HandleAction(const FCombatActionFeedback& Event)
{
 if (!Feedback.IsValid() || Event.Source.Get() != Fighter.Get() || !Feedback->IsCurrent(Event.RoundId, Event.Generation)) return;
 SeenGeneration = Event.Generation;
 if (Event.Stage == ECombatActionStage::Start)
 {
  StopOwnedPose();
  SuperSession = Event.Tier == ECombatFeedbackTier::SuperBlast ? Event.SessionId : 0;
 }
 if (Event.Tier != ECombatFeedbackTier::SuperBlast || Event.SessionId != SuperSession) return;
 if (Event.Stage == ECombatActionStage::Fire && Event.AttackInstanceId > 0 && Event.AttackInstanceId != LastRecoilAttack)
 {
  LastRecoilAttack = Event.AttackInstanceId;
  // Successful fire happens while the blast ability still owns its recovery lock.
  if (Enabled() && !HighPriorityState() && !Fighter->HasCombatTag(TAG_State_GuardStun)
   && Play(SuperRecoil, TEXT("SuperRecoil"))) ++RecoilCount;
 }
 if (Event.Stage == ECombatActionStage::End)
 {
  SuperSession = 0;
  if (Event.EndReason != ECombatFeedbackEnd::Completed) StopOwnedPose();
 }
}

void UCombatPoseConsumer::HandleLifecycle(const FCombatLifecycleFeedback& Event)
{
 if (!Feedback.IsValid() || Event.Source.Get() != Fighter.Get() || !Feedback->IsCurrent(Event.RoundId, Event.Generation)) return;
 if (Event.Reason == ECombatFeedbackEnd::Completed || Event.Reason == ECombatFeedbackEnd::Expire) return;
 if (Event.AttackInstanceId == 0 || Event.AttackInstanceId == LastRecoilAttack)
 {
  StopOwnedPose(); SuperSession = 0;
 }
}

void UCombatPoseConsumer::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Delta, Type, Tick);
 if (!Enabled()) { StopOwnedPose(); StopReactionMontages(); return; }
 if (SeenGeneration != Feedback->GetGeneration())
 {
  StopOwnedPose(); SuperSession = 0; SeenGeneration = Feedback->GetGeneration();
 }
 if (HighPriorityState() || !Fighter->GetCombatInput()->AreRequestsEnabled()) { StopOwnedPose(); return; }
 UAnimInstance* Instance = Anim();
 if (!Instance) return;
 if (PoseState == TEXT("SuperRecoil"))
 {
  if (Instance->Montage_IsActive(SuperRecoil)) return;
  OwnedMontage.Reset(); PoseState = NAME_None;
 }
 if (Fighter->HasCombatTag(TAG_State_GuardStun))
 {
  // The real impact owns the slot until both montage and gameplay stun recover.
  if (OwnedMontage.IsValid()) StopOwnedPose();
  bGuardEngaged = true; PoseState = TEXT("GuardImpact"); return;
 }
 if (Fighter->IsAttacking() || Fighter->HasCombatTag(TAG_State_BlastCharging)) { StopOwnedPose(); return; }
 UAnimMontage* Current = Instance->GetCurrentActiveMontage();
 if (Current && Current != OwnedMontage.Get()) return;
 if (Fighter->IsGuarding())
 {
  if (!bGuardEngaged)
  {
   if (Play(GuardStart, TEXT("GuardStart"))) { bGuardEngaged = true; ++GuardStartCount; }
  }
  else if (OwnedMontage.Get() != GuardLoop &&
   (OwnedMontage.Get() != GuardStart || !Instance->Montage_IsActive(GuardStart) || Instance->Montage_GetPosition(GuardStart) >= .20f))
  {
   if (Play(GuardLoop, TEXT("GuardLoop")))
   {
    Instance->Montage_SetNextSection(TEXT("Default"), TEXT("Default"), GuardLoop); ++GuardLoopCount;
   }
  }
 }
 else if (bGuardEngaged && Fighter->CanAct())
 {
  bGuardEngaged = false;
  if (Play(GuardEnd, TEXT("GuardEnd"))) ++GuardEndCount;
 }
 else if (OwnedMontage.IsValid() && !Instance->Montage_IsActive(OwnedMontage.Get()))
 {
  OwnedMontage.Reset(); PoseState = NAME_None;
 }
}
