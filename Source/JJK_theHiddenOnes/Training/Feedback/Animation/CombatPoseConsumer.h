#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/CombatFeedbackTypes.h"
#include "CombatPoseConsumer.generated.h"

class AFighterCharacter;
class UAnimInstance;
class UAnimMontage;
class UCombatFeedbackComponent;

/** A owns the integration of B's guard and successful-fire presentation. No gameplay timers or tags. */
UCLASS(ClassGroup=(JJK), meta=(BlueprintSpawnableComponent))
class UCombatPoseConsumer : public UActorComponent
{
 GENERATED_BODY()
public:
 UCombatPoseConsumer();
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimMontage> GuardStart;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimMontage> GuardLoop;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimMontage> GuardEnd;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimMontage> SuperRecoil;
 UPROPERTY(BlueprintReadOnly) FName PoseState = NAME_None;
 UPROPERTY(BlueprintReadOnly) int32 GuardStartCount = 0;
 UPROPERTY(BlueprintReadOnly) int32 GuardLoopCount = 0;
 UPROPERTY(BlueprintReadOnly) int32 GuardEndCount = 0;
 UPROPERTY(BlueprintReadOnly) int32 RecoilCount = 0;
 UPROPERTY(BlueprintReadOnly) int64 LastRecoilAttack = 0;
 UFUNCTION(BlueprintCallable) void StopOwnedPose();
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Tick) override;
private:
 TWeakObjectPtr<AFighterCharacter> Fighter;
 TWeakObjectPtr<UCombatFeedbackComponent> Feedback;
 TWeakObjectPtr<UAnimMontage> OwnedMontage;
 int64 SuperSession = 0;
 int32 SeenGeneration = 0;
 bool bGuardEngaged = false;
 bool Enabled() const;
 bool HighPriorityState() const;
 UAnimInstance* Anim() const;
 bool Play(UAnimMontage* Montage, FName State);
 void StopReactionMontages();
 UFUNCTION() void HandleAction(const FCombatActionFeedback& Event);
 UFUNCTION() void HandleLifecycle(const FCombatLifecycleFeedback& Event);
};
