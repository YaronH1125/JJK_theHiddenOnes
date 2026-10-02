#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/CombatFeedbackTypes.h"
#include "CombatFeedbackComponent.generated.h"

class UCombatFeedbackProfile;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FContactFeedbackDelegate, const FCombatContactFeedback&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActionFeedbackDelegate, const FCombatActionFeedback&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLifecycleFeedbackDelegate, const FCombatLifecycleFeedback&, Event);

/** One dispatcher per fighter. Contacts broadcast on SOURCE only; victim consumers subscribe to both fighters. */
UCLASS(ClassGroup=(JJK), meta=(BlueprintSpawnableComponent))
class UCombatFeedbackComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCombatFeedbackComponent();
 UPROPERTY(BlueprintAssignable) FContactFeedbackDelegate OnContact;
 UPROPERTY(BlueprintAssignable) FActionFeedbackDelegate OnAction;
 UPROPERTY(BlueprintAssignable) FLifecycleFeedbackDelegate OnLifecycle;
 UPROPERTY(BlueprintReadOnly) FCombatContactFeedback LastContact;
 UPROPERTY(BlueprintReadOnly) FCombatActionFeedback LastAction;
 UPROPERTY(BlueprintReadOnly) FCombatLifecycleFeedback LastLifecycle;
 UPROPERTY(BlueprintReadOnly) int32 LifecycleCount = 0;
 UPROPERTY(BlueprintReadOnly) int32 ContactCount = 0;
 UPROPERTY(BlueprintReadOnly) int32 ActionCount = 0;
 UPROPERTY(BlueprintReadOnly) int32 FullCount = 0;
 UPROPERTY(BlueprintReadOnly) int32 FireCount = 0;
 UFUNCTION(BlueprintPure) int32 GetGeneration() const { return Generation; }
 UFUNCTION(BlueprintPure) double GetActionTime() const;
 UFUNCTION(BlueprintPure) bool IsStopped() const { return bStopped; }
 UFUNCTION(BlueprintPure) float GetLastStopDuration() const { return LastStopDuration; }
 UFUNCTION(BlueprintPure) bool IsCurrent(int32 EventRound, int32 EventGeneration) const;
 UFUNCTION(BlueprintPure) bool IsChannelEnabled(ECombatFeedbackChannel Channel) const;
 UFUNCTION(BlueprintPure) const UCombatFeedbackProfile* GetProfile() const;
 ECombatFeedbackTier GetAttackTier(const UAttackDefinition* Attack) const;
 int64 AllocateAttackId() { return ++NextAttack; }
 int64 BeginAction(FName Move, ECombatFeedbackTier Tier);
 void Action(int64 Session, ECombatActionStage Stage, float PaidQ = 0.f,
  ECombatFeedbackEnd Reason = ECombatFeedbackEnd::None, int64 Attack = 0, const FTransform* MuzzleOverride = nullptr);
 void PublishContact(FCombatContactFeedback Event);
 void Lifecycle(int64 Attack, ECombatFeedbackEnd Reason, FVector Location);
 void Cleanup(ECombatFeedbackEnd Reason);
 /** Outgoing confirmations may pause the attacker's armor; armored incoming hits never request this. */
 void RequestStop(float Seconds, bool bOutgoingConfirmation = false);
 UFUNCTION(BlueprintCallable, Category="Feedback|Debug", meta=(DevelopmentOnly)) void DebugRequestStop(float Seconds) { RequestStop(Seconds); }
 void TrackTransientEffect(UActorComponent* Effect);
 void ClearStop(const TCHAR* Reason);
 void ApplyPendingStop();
 /** Called once per world after all sweeps, before any character is interrupted. */
 static void FlushWorld(UWorld* World, ELevelTick TickType, float Delta);
 static int32 Round(UWorld* World);

 bool UsesExternalRangedImpact() const;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick) override;
private:
 void FlushWorldCallback(UWorld* World, ELevelTick TickType, float Delta);
 UFUNCTION() void HandleContactStop(const FCombatContactFeedback& Event);
 FTransform MuzzleTransform() const;
 int32 Generation = 1;
 int64 NextSession = 0;
 int64 NextAttack = 0;
 struct FSession { FName Move; ECombatFeedbackTier Tier; bool bFull = false; TSet<int64> Fired; };
 TMap<int64, FSession> Sessions;
 // Bounded by one match. Reset releases keys and invalidates all delayed consumers.
 TSet<FString> ContactKeys;
 bool bStopped = false;
 bool bSavedPauseAnims = false;
 bool bSavedMovementTick = false;
 double StopStart = 0., StopEnd = 0., PausedTime = 0.;
 float PendingStop = 0.f, LastStopDuration = 0.f;
 bool bPendingOutgoingStop = false;
 TArray<TWeakObjectPtr<UActorComponent>> OwnedEffects;
 FDelegateHandle FlushHandle;
};
