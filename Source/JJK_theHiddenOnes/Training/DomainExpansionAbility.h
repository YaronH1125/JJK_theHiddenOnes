// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Training/CombatTypes.h"
#include "DomainExpansionAbility.generated.h"

class AFighterCharacter;

/**
 * 领域展开 GA（M6.6）：
 * R → 结印 1.0s（可被打断，不能主动取消/切形态）→
 * 捕获目标（存活/同擂台/≤12m/无阻隔）→ 成功登记 DomainSessionId →
 * 失败不退已提交领域能量。领域中对手继续普通战斗，自动炮由 GameMode 调度。
 */
UCLASS()
class UDomainExpansionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDomainExpansionAbility();

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
	void HandleCastComplete();

	UFUNCTION()
	void HandleFighterInterrupted();

	void FailDomain(const TCHAR* Reason);

	FTimerHandle CastTimerHandle;
	TWeakObjectPtr<AFighterCharacter> CachedFighter;
};
