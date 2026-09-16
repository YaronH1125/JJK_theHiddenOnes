// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DodgeAbility.generated.h"

class AFighterCharacter;
class UAbilityTask_ApplyRootMotionConstantForce;

/**
 * 闪避 GA（M3.3/M3.4）：方向/后撤位移 + 无敌窗口 + 恢复期。
 * 取消变招时由共享入口先行完成全部检查（状态/窗口/资源），本能力
 * 激活期间负责：扣费（资源 GE）、终止旧攻击 GA、无敌与恢复标签、位移。
 */
UCLASS()
class UDodgeAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDodgeAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleInvulnEnd();

	UFUNCTION()
	void HandleRecoveryEnd();

	UPROPERTY()
	TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> MoveTask;
	FTimerHandle InvulnTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	TWeakObjectPtr<AFighterCharacter> CachedFighter;
};
