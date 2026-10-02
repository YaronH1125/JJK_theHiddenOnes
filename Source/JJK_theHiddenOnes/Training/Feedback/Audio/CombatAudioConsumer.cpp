#include "Training/Feedback/Audio/CombatAudioConsumer.h"

#include "Training/AttackDefinition.h"
#include "Training/CombatFeedbackComponent.h"
#include "Training/CombatHitComponent.h"
#include "Training/FighterCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<float> DAudioGain(TEXT("JJK.Feedback.AudioGain"), 1.f, TEXT("D combat audio gain, clamped 0..2"));
static TAutoConsoleVariable<int32> DAudioLog(TEXT("JJK.Feedback.AudioLog"), 0, TEXT("D diagnostic playback logging; off in normal play"));

UCombatAudioConsumer::UCombatAudioConsumer()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	static ConstructorHelpers::FObjectFinder<UCombatAudioProfile> Profile(TEXT("/Game/CombatFeedback/Profiles/Audio/DA_CombatAudio.DA_CombatAudio"));
	if (Profile.Succeeded()) AudioProfile = Profile.Object;
}

void UCombatAudioConsumer::BeginPlay()
{
	Super::BeginPlay();
	Fighter = Cast<AFighterCharacter>(GetOwner());
	Feedback = Fighter.IsValid() ? Fighter->GetCombatFeedback() : nullptr;
	if (!Feedback.IsValid()) return;
	SeenGeneration = Feedback->GetGeneration();
	Feedback->OnAction.AddUniqueDynamic(this, &UCombatAudioConsumer::HandleAction);
	Feedback->OnContact.AddUniqueDynamic(this, &UCombatAudioConsumer::HandleContact);
	Feedback->OnLifecycle.AddUniqueDynamic(this, &UCombatAudioConsumer::HandleLifecycle);
	DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UCombatAudioConsumer::HandleDeactivate);
}

void UCombatAudioConsumer::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Feedback.IsValid())
	{
		Feedback->OnAction.RemoveDynamic(this, &UCombatAudioConsumer::HandleAction);
		Feedback->OnContact.RemoveDynamic(this, &UCombatAudioConsumer::HandleContact);
		Feedback->OnLifecycle.RemoveDynamic(this, &UCombatAudioConsumer::HandleLifecycle);
	}
	FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
	StopOwnedAudio();
	Super::EndPlay(Reason);
}

bool UCombatAudioConsumer::Enabled() const
{
	return AudioProfile && Feedback.IsValid() && Feedback->IsChannelEnabled(ECombatFeedbackChannel::Audio)
		&& DAudioGain.GetValueOnGameThread() > 0.f;
}

int32 UCombatAudioConsumer::GetCuePlayCount(ECombatAudioCueKind Kind) const { return Counts.FindRef(Kind); }
bool UCombatAudioConsumer::IsChargeLoopPlaying() const { return ChargeLoop.IsValid() && ChargeLoop->IsPlaying(); }
void UCombatAudioConsumer::DebugConsumeAction(const FCombatActionFeedback& Event)
{
#if !UE_BUILD_SHIPPING
	HandleAction(Event);
#endif
}
void UCombatAudioConsumer::DebugConsumeContact(const FCombatContactFeedback& Event)
{
#if !UE_BUILD_SHIPPING
	HandleContact(Event);
#endif
}

bool UCombatAudioConsumer::AdmitVoice(USoundConcurrency* Concurrency, float Priority)
{
	if (!Concurrency) return true;
	// Engine concurrency resolves on the audio thread. Bound owned components on
	// the game thread as well, so a same-frame burst cannot grow an async backlog.
	const int32 Limit = FMath::Max(1, Concurrency->Concurrency.GetMaxCount());
	const double Now = GetWorld()->GetTimeSeconds();
	while (true)
	{
		int32 Active = 0, VictimIndex = INDEX_NONE;
		UCombatAudioConsumer* VictimOwner = nullptr;
		float LowestPriority = TNumericLimits<float>::Max();
		double OldestTime = TNumericLimits<double>::Max();
		for (TActorIterator<AFighterCharacter> It(GetWorld()); It; ++It)
		{
			if (Concurrency->Concurrency.bLimitToOwner && *It != Fighter.Get()) continue;
			UCombatAudioConsumer* Other = It->FindComponentByClass<UCombatAudioConsumer>();
			if (!Other) continue;
			for (int32 I = Other->Voices.Num() - 1; I >= 0; --I)
			{
				FVoice& Voice = Other->Voices[I];
				if (Voice.Concurrency.Get() != Concurrency) continue;
				UAudioComponent* Audio = Voice.Component.Get();
				if (!Audio || !Audio->IsPlaying() || Now >= Voice.EndTime)
				{
					if (Audio) { Audio->Stop(); Audio->DestroyComponent(); }
					Other->Voices.RemoveAtSwap(I);
					continue;
				}
			}
			// Select only after pruning, since RemoveAtSwap changes array indices.
			for (int32 I = 0; I < Other->Voices.Num(); ++I)
			{
				const FVoice& Voice = Other->Voices[I];
				if (Voice.Concurrency.Get() != Concurrency) continue;
				++Active;
				if (Voice.Priority < LowestPriority || (Voice.Priority == LowestPriority && Voice.StartedTime < OldestTime))
				{
					LowestPriority = Voice.Priority; OldestTime = Voice.StartedTime;
					VictimOwner = Other; VictimIndex = I;
				}
			}
			Other->ActiveOneShotCount = Other->Voices.Num();
		}
		if (Active < Limit) return true;
		if (!VictimOwner || Priority < LowestPriority) { ++DroppedCueCount; return false; }
		if (UAudioComponent* Audio = VictimOwner->Voices[VictimIndex].Component.Get()) { Audio->Stop(); Audio->DestroyComponent(); }
		VictimOwner->Voices.RemoveAtSwap(VictimIndex);
		VictimOwner->ActiveOneShotCount = VictimOwner->Voices.Num();
		++VictimOwner->StolenVoiceCount;
	}
}

UAudioComponent* UCombatAudioConsumer::PlayCue(ECombatAudioCueKind Kind, FVector Location, int64 Session, int64 Attack, bool bLoop, bool bPlayerVictim)
{
	if (!Enabled() || !GetWorld() || !Fighter.IsValid()) return nullptr;
	const FCombatAudioCue* Cue = AudioProfile->Find(Kind);
	if (!Cue || Cue->Variants.IsEmpty()) { ++MissingCueCount; return nullptr; }
	int32 Index = FMath::RandRange(0, Cue->Variants.Num() - 1);
	if (Cue->Variants.Num() > 1 && PreviousVariant.Contains(Kind) && Index == PreviousVariant[Kind])
		Index = (Index + FMath::RandRange(1, Cue->Variants.Num() - 1)) % Cue->Variants.Num();
	USoundBase* Sound = Cue->Variants[Index];
	if (!Sound) { ++MissingCueCount; return nullptr; }
	const float Priority = bPlayerVictim ? FMath::Max(5.f, Cue->Priority) : Cue->Priority;
	if (!bLoop && !AdmitVoice(Cue->Concurrency, Priority)) return nullptr;
	PreviousVariant.Add(Kind, Index);
	UAudioComponent* Audio = NewObject<UAudioComponent>(Fighter.Get());
	// Registering a default AudioComponent otherwise auto-plays before its final
	// location is set, then the explicit Play would restart the same cue.
	Audio->bAutoActivate = false;
	Audio->bAutoDestroy = false;
	Audio->bStopWhenOwnerDestroyed = true;
	Audio->bOverridePriority = true;
	Audio->Priority = Priority;
	Audio->SetSound(Sound);
	Audio->AttenuationSettings = Cue->Attenuation;
	if (Cue->Concurrency) Audio->ConcurrencySet.Add(Cue->Concurrency);
	const float Gain = FMath::Clamp(Cue->Gain * AudioProfile->MasterGain * FMath::Clamp(DAudioGain.GetValueOnGameThread(), 0.f, 2.f), 0.f, 1.f);
	Audio->SetVolumeMultiplier(Gain);
	Audio->SetPitchMultiplier(1.f + FMath::FRandRange(-Cue->PitchVariation, Cue->PitchVariation));
	if (bLoop) Audio->SetupAttachment(Fighter->GetMesh(), Fighter->GetMuzzleSocketName());
	Audio->RegisterComponentWithWorld(GetWorld());
	if (!bLoop) Audio->SetWorldLocation(Location);
	Audio->Play();
	LastCue = Kind; LastCueLocation = Location; LastSound = Sound->GetFName();
	++PlayedCueCount; ++Counts.FindOrAdd(Kind);
	if (!bLoop)
	{
		FVoice& Voice = Voices.AddDefaulted_GetRef();
		Voice.Component = Audio; Voice.Concurrency = Cue->Concurrency;
		Voice.Session = Session; Voice.Attack = Attack; Voice.Kind = Kind;
		Voice.StartedTime = GetWorld()->GetTimeSeconds();
		Voice.EndTime = Voice.StartedTime + FMath::Max(.05f, Cue->MaxLife);
		Voice.Gain = Gain; Voice.Priority = Priority;
	}
	ActiveOneShotCount = Voices.Num();
#if !UE_BUILD_SHIPPING
	if (DAudioLog.GetValueOnGameThread()) UE_LOG(LogTemp, Log, TEXT("[CombatAudio] owner=%s cue=%d session=%lld attack=%lld sound=%s"),
		*GetNameSafe(GetOwner()), int32(Kind), Session, Attack, *LastSound.ToString());
#endif
	return Audio;
}

void UCombatAudioConsumer::UpdateLoop(float Q)
{
	LoopPaidQ = FMath::Clamp(Q, 0.f, 1.f);
	if (!AudioProfile || !ChargeLoop.IsValid()) return;
	const FCombatAudioCue* Cue = AudioProfile->Find(ECombatAudioCueKind::ChargeLoop);
	if (!Cue) return;
	LoopGain = Cue->Gain * AudioProfile->MasterGain * FMath::Clamp(DAudioGain.GetValueOnGameThread(), 0.f, 2.f)
		* FMath::Lerp(AudioProfile->ChargeMinGain, AudioProfile->ChargeMaxGain, LoopPaidQ);
	if (GetWorld()->GetTimeSeconds() < DuckUntil) LoopGain *= AudioProfile->SuperDuckGain;
	LoopGain = FMath::Clamp(LoopGain, 0.f, 1.f);
	LoopPitch = FMath::Lerp(AudioProfile->ChargeMinPitch, AudioProfile->ChargeMaxPitch, LoopPaidQ);
	ChargeLoop->SetVolumeMultiplier(LoopGain);
	ChargeLoop->SetPitchMultiplier(LoopPitch);
}

void UCombatAudioConsumer::StopCharge()
{
	if (UAudioComponent* Audio = ChargeLoop.Get()) { Audio->Stop(); Audio->DestroyComponent(); }
	ChargeLoop.Reset(); ActiveChargeSession = 0; ActiveLoopCount = 0; LoopPaidQ = 0.f; LoopGain = 0.f;
}

void UCombatAudioConsumer::StopSessionTails(int64 Session)
{
	for (int32 I = Voices.Num() - 1; I >= 0; --I)
	{
		if (Voices[I].Session != Session || (Voices[I].Kind != ECombatAudioCueKind::ChargeStart && Voices[I].Kind != ECombatAudioCueKind::ChargeFull)) continue;
		if (UAudioComponent* Audio = Voices[I].Component.Get()) { Audio->Stop(); Audio->DestroyComponent(); }
		Voices.RemoveAtSwap(I);
	}
	ActiveOneShotCount = Voices.Num();
}

void UCombatAudioConsumer::HandleAction(const FCombatActionFeedback& Event)
{
	if (!Feedback.IsValid() || Event.Source.Get() != Fighter.Get() || !Feedback->IsCurrent(Event.RoundId, Event.Generation)) return;
	if (SeenGeneration != Event.Generation)
	{
		StopOwnedAudio(); SeenGeneration = Event.Generation; HighestStartSession = 0;
	}
	const bool bManual = Event.Tier == ECombatFeedbackTier::MobileBlast || Event.Tier == ECombatFeedbackTier::SuperBlast;
	if (Event.Stage == ECombatActionStage::Start)
	{
		const FString Key = FString::Printf(TEXT("S:%lld"), Event.SessionId);
		if (ActionKeys.Contains(Key) || Event.SessionId < HighestStartSession) { ++DuplicateCount; return; }
		ActionKeys.Add(Key); HighestStartSession = Event.SessionId;
		if (!Enabled()) return;
		if (bManual)
		{
			if (ActiveChargeSession) StopSessionTails(ActiveChargeSession);
			StopCharge();
			ActiveChargeSession = Event.SessionId;
			PlayCue(ECombatAudioCueKind::ChargeStart, Event.Muzzle.GetLocation(), Event.SessionId);
			ChargeLoop = PlayCue(ECombatAudioCueKind::ChargeLoop, Event.Muzzle.GetLocation(), Event.SessionId, 0, true);
			ActiveLoopCount = ChargeLoop.IsValid() ? 1 : 0;
			if (ActiveLoopCount) ++LoopStartCount;
			UpdateLoop(Event.PaidQ);
		}
		else if (Event.Tier == ECombatFeedbackTier::DomainOrb)
			PlayCue(ECombatAudioCueKind::DomainStart, Event.Muzzle.GetLocation(), Event.SessionId);
		else { PendingSwing = Event; bSwingPending = true; }
	}
	else if (Event.Stage == ECombatActionStage::Update && bManual && ActiveChargeSession == Event.SessionId)
		UpdateLoop(Event.PaidQ);
	else if (Event.Stage == ECombatActionStage::Full && bManual && ActiveChargeSession == Event.SessionId && Event.PaidQ >= 1.f - KINDA_SMALL_NUMBER)
	{
		if (FullSessions.Contains(Event.SessionId)) { ++DuplicateCount; return; }
		FullSessions.Add(Event.SessionId); UpdateLoop(Event.PaidQ);
		PlayCue(ECombatAudioCueKind::ChargeFull, Event.Muzzle.GetLocation(), Event.SessionId);
	}
	else if (Event.Stage == ECombatActionStage::Fire && Event.AttackInstanceId)
	{
		const FString Key = FString::Printf(TEXT("F:%lld:%lld"), Event.SessionId, Event.AttackInstanceId);
		if (ActionKeys.Contains(Key)) { ++DuplicateCount; return; }
		ActionKeys.Add(Key);
		if (bManual && ActiveChargeSession == Event.SessionId) { StopCharge(); StopSessionTails(Event.SessionId); }
		if (bManual || Event.Tier == ECombatFeedbackTier::DomainOrb)
		{
			const ECombatAudioCueKind Kind = Event.Tier == ECombatFeedbackTier::MobileBlast ? ECombatAudioCueKind::MobileFire : ECombatAudioCueKind::SuperFire;
			if (Enabled() && Kind == ECombatAudioCueKind::SuperFire) DuckSecondary();
			PlayCue(Kind, Event.Muzzle.GetLocation(), Event.SessionId, Event.AttackInstanceId);
		}
	}
	else if (Event.Stage == ECombatActionStage::End)
	{
		const FString Key = FString::Printf(TEXT("E:%lld"), Event.SessionId);
		if (ActionKeys.Contains(Key)) { ++DuplicateCount; return; }
		ActionKeys.Add(Key);
		if (bSwingPending && PendingSwing.SessionId == Event.SessionId) bSwingPending = false;
		if (bManual && ActiveChargeSession == Event.SessionId)
		{
			StopCharge(); StopSessionTails(Event.SessionId);
			if (Event.EndReason == ECombatFeedbackEnd::Interrupted || Event.EndReason == ECombatFeedbackEnd::Rejected)
				PlayCue(ECombatAudioCueKind::ChargeCancel, Event.Muzzle.GetLocation(), Event.SessionId);
		}
		if (Event.Tier == ECombatFeedbackTier::DomainOrb && (Event.EndReason == ECombatFeedbackEnd::Completed || Event.EndReason == ECombatFeedbackEnd::Suppressed))
			PlayCue(ECombatAudioCueKind::DomainEnd, Event.Muzzle.GetLocation(), Event.SessionId);
	}
}

void UCombatAudioConsumer::HandleContact(const FCombatContactFeedback& Event)
{
	if (!Feedback.IsValid() || Event.Source.Get() != Fighter.Get() || !Feedback->IsCurrent(Event.RoundId, Event.SourceGeneration)) return;
	if (Event.Target.IsValid() && !Event.Target->GetCombatFeedback()->IsCurrent(Event.RoundId, Event.TargetGeneration)) return;
	const FString Key = FString::Printf(TEXT("%d:%lld:%d:%u"), Event.RoundId, Event.AttackInstanceId, Event.SegmentId, Event.Target.IsValid() ? Event.Target->GetUniqueID() : 0);
	if (ContactKeys.Contains(Key)) { ++DuplicateCount; return; }
	ContactKeys.Add(Key);
	ECombatAudioCueKind Kind;
	switch (Event.Result)
	{
	case ECombatFeedbackResult::Hit:
		if (Event.bRanged) Kind = ECombatAudioCueKind::RangedHit;
		else if (Event.Tier == ECombatFeedbackTier::Heavy || Event.Tier == ECombatFeedbackTier::Finisher) Kind = ECombatAudioCueKind::HeavyHit;
		else Kind = Event.MoveId.ToString().Contains(TEXT("Kick")) ? ECombatAudioCueKind::KickHit : ECombatAudioCueKind::PunchHit;
		break;
	case ECombatFeedbackResult::Guard: Kind = ECombatAudioCueKind::Guard; break;
	case ECombatFeedbackResult::Immune: Kind = ECombatAudioCueKind::Immune; break;
	case ECombatFeedbackResult::WorldImpact: Kind = ECombatAudioCueKind::WorldImpact; break;
	default: return;
	}
	PlayCue(Kind, Event.Location, 0, Event.AttackInstanceId, false, Event.Target.IsValid() && Event.Target->IsPlayerControlled());
}

void UCombatAudioConsumer::HandleLifecycle(const FCombatLifecycleFeedback& Event)
{
	if (!Feedback.IsValid() || Event.Source.Get() != Fighter.Get() || !Feedback->IsCurrent(Event.RoundId, Event.Generation)) return;
	if (!Event.AttackInstanceId) { StopOwnedAudio(); return; }
	const FString Key = FString::Printf(TEXT("L:%lld:%d"), Event.AttackInstanceId, int32(Event.Reason));
	if (ActionKeys.Contains(Key)) { ++DuplicateCount; return; }
	ActionKeys.Add(Key);
	if (Event.Reason == ECombatFeedbackEnd::Expire)
		PlayCue(ECombatAudioCueKind::Expire, Event.Location, 0, Event.AttackInstanceId);
	// Completed leaves short impact tails audible. Abnormal instance teardown removes its own voices.
	if (Event.Reason != ECombatFeedbackEnd::Completed && Event.Reason != ECombatFeedbackEnd::Expire)
		for (int32 I = Voices.Num() - 1; I >= 0; --I)
		{
			if (Voices[I].Attack != Event.AttackInstanceId) continue;
			if (UAudioComponent* Audio = Voices[I].Component.Get()) { Audio->Stop(); Audio->DestroyComponent(); }
			Voices.RemoveAtSwap(I);
		}
	ActiveOneShotCount = Voices.Num();
}

void UCombatAudioConsumer::TickSwing()
{
	if (!bSwingPending || !Fighter.IsValid() || !Feedback.IsValid()) return;
	if (!Feedback->IsCurrent(PendingSwing.RoundId, PendingSwing.Generation)) { bSwingPending = false; return; }
	const UCombatHitComponent* Hit = Fighter->GetCombatHit();
	const UAttackDefinition* Def = Hit->GetActiveDefinition();
	if (!Hit->HasActiveAttack() || !Def || Def->GetFName() != PendingSwing.MoveId) return;
	const UAnimInstance* Anim = Fighter->GetMesh()->GetAnimInstance();
	UAnimMontage* Montage = Def->Montage.Get();
	// B reviewed these actual windows. Reading animation progress handles held heavy poses
	// and local hit stop; no key polling, guessed wall-clock timers, or hit synthesis.
	if (!Anim || !Montage || !Anim->Montage_IsPlaying(Montage) ||
		Anim->Montage_GetPosition(Montage) < FMath::Max(0.f, Def->WindowStartTime - AudioProfile->SwingLeadSeconds)) return;
	bSwingPending = false;
	const ECombatAudioCueKind Kind = Def->TraceSocket.ToString().StartsWith(TEXT("foot")) ? ECombatAudioCueKind::KickSwing : ECombatAudioCueKind::PunchSwing;
	PlayCue(Kind, Fighter->GetMesh()->GetSocketLocation(Def->TraceSocket), PendingSwing.SessionId, Hit->GetActiveInstanceId());
}

void UCombatAudioConsumer::DuckSecondary()
{
	for (TActorIterator<AFighterCharacter> It(GetWorld()); It; ++It)
		if (UCombatAudioConsumer* Other = It->FindComponentByClass<UCombatAudioConsumer>())
			if (Other->AudioProfile) Other->DuckUntil = FMath::Max(Other->DuckUntil, GetWorld()->GetTimeSeconds() + Other->AudioProfile->SuperDuckSeconds);
}

void UCombatAudioConsumer::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(Delta, TickType, Tick);
	if (!Enabled()) { StopOwnedAudio(); return; }
	TickSwing();
	const double Now = GetWorld()->GetTimeSeconds();
	if (ChargeLoop.IsValid()) UpdateLoop(LoopPaidQ);
	for (int32 I = Voices.Num() - 1; I >= 0; --I)
	{
		UAudioComponent* Audio = Voices[I].Component.Get();
		if (!Audio || !Audio->IsPlaying() || Now >= Voices[I].EndTime)
		{
			if (Audio) { Audio->Stop(); Audio->DestroyComponent(); }
			Voices.RemoveAtSwap(I); continue;
		}
		const bool bSecondary = Voices[I].Kind == ECombatAudioCueKind::PunchSwing || Voices[I].Kind == ECombatAudioCueKind::KickSwing || Voices[I].Kind == ECombatAudioCueKind::Expire;
		Audio->SetVolumeMultiplier(Voices[I].Gain * (bSecondary && Now < DuckUntil ? AudioProfile->SuperDuckGain : 1.f));
	}
	ActiveOneShotCount = Voices.Num();
}

void UCombatAudioConsumer::HandleDeactivate() { StopOwnedAudio(); }

void UCombatAudioConsumer::StopOwnedAudio()
{
	StopCharge();
	for (FVoice& Voice : Voices) if (UAudioComponent* Audio = Voice.Component.Get()) { Audio->Stop(); Audio->DestroyComponent(); }
	Voices.Reset(); ActiveOneShotCount = 0; bSwingPending = false; DuckUntil = 0.;
	ContactKeys.Reset(); ActionKeys.Reset(); FullSessions.Reset();
}
