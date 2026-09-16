// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "StanceSwitchAbility.generated.h"

class AFighterCharacter;

/**
 * 切形态 GA（M3）：E 切换近/远程形态。
 * 动作时长内打上 StanceSwitching 标签（期间动作请求被拒），
 * 结束时切换 Stance.Melee/Stance.Ranged；再次切换受间隔限制（共享入口检查）。
 * 远程形态 M3 不发炮：攻击请求在共享入口按形态拒绝。
 */
UCLASS()
class UStanceSwitchAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UStanceSwitchAbility();

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
	void HandleSwitchFinished();

	FTimerHandle SwitchTimerHandle;
	TWeakObjectPtr<AFighterCharacter> CachedFighter;
};
