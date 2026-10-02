#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/CombatFeedbackTypes.h"
#include "Training/Feedback/Audio/CombatAudioProfile.h"
#include "CombatAudioConsumer.generated.h"

class AFighterCharacter;
class UAudioComponent;
class UCombatFeedbackComponent;
class USoundConcurrency;

/** One source-owned subscriber; impact events never subscribe a second time on the victim. */
UCLASS(ClassGroup=(JJK), Blueprintable, meta=(BlueprintSpawnableComponent))
class UCombatAudioConsumer : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatAudioConsumer();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback|Audio") TObjectPtr<UCombatAudioProfile> AudioProfile;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") int32 PlayedCueCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") int32 MissingCueCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") int32 DuplicateCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") int32 DroppedCueCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") int32 StolenVoiceCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") int32 ActiveLoopCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") int32 ActiveOneShotCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") int32 LoopStartCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") int64 ActiveChargeSession = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") float LoopPaidQ = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") float LoopGain = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") float LoopPitch = 1.f;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") ECombatAudioCueKind LastCue = ECombatAudioCueKind::PunchSwing;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") FVector LastCueLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Audio") FName LastSound;
	UFUNCTION(BlueprintPure, Category="Feedback|Audio") int32 GetCuePlayCount(ECombatAudioCueKind Kind) const;
	UFUNCTION(BlueprintPure, Category="Feedback|Audio") bool IsChargeLoopPlaying() const;
	/** Contract tests only: consumes a snapshot locally, never publishes or settles gameplay. */
	UFUNCTION(BlueprintCallable, Category="Feedback|Audio|Debug", meta=(DevelopmentOnly))
	void DebugConsumeAction(const FCombatActionFeedback& Event);
	UFUNCTION(BlueprintCallable, Category="Feedback|Audio|Debug", meta=(DevelopmentOnly))
	void DebugConsumeContact(const FCombatContactFeedback& Event);
	/** Idempotent owner-only teardown; A may call this from additional UI lifecycles. */
	UFUNCTION(BlueprintCallable, Category="Feedback|Audio") void StopOwnedAudio();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick) override;

private:
	UFUNCTION() void HandleAction(const FCombatActionFeedback& Event);
	UFUNCTION() void HandleContact(const FCombatContactFeedback& Event);
	UFUNCTION() void HandleLifecycle(const FCombatLifecycleFeedback& Event);
	void HandleDeactivate();
	bool Enabled() const;
	UAudioComponent* PlayCue(ECombatAudioCueKind Kind, FVector Location, int64 Session = 0, int64 Attack = 0, bool bLoop = false, bool bPlayerVictim = false);
	bool AdmitVoice(USoundConcurrency* Concurrency, float Priority);
	void StopCharge();
	void StopSessionTails(int64 Session);
	void UpdateLoop(float Q);
	void DuckSecondary();
	void TickSwing();
	struct FVoice
	{
		TWeakObjectPtr<UAudioComponent> Component;
		TWeakObjectPtr<USoundConcurrency> Concurrency;
		int64 Session = 0, Attack = 0;
		ECombatAudioCueKind Kind = ECombatAudioCueKind::PunchSwing;
		double EndTime = 0., StartedTime = 0.;
		float Gain = 0.f, Priority = 0.f;
	};
	TArray<FVoice> Voices;
	TWeakObjectPtr<AFighterCharacter> Fighter;
	TWeakObjectPtr<UCombatFeedbackComponent> Feedback;
	TWeakObjectPtr<UAudioComponent> ChargeLoop;
	TMap<ECombatAudioCueKind, int32> Counts, PreviousVariant;
	TSet<FString> ContactKeys, ActionKeys;
	TSet<int64> FullSessions;
	FCombatActionFeedback PendingSwing;
	bool bSwingPending = false;
	int64 HighestStartSession = 0;
	int32 SeenGeneration = 0;
	double DuckUntil = 0.;
	FDelegateHandle DeactivateHandle;
};
