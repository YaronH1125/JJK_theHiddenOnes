// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "MeleeComboAbility.generated.h"

class AFighterCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAttackDefinition;

/**
 * M2 单段拳击 GA：管理激活、Montage、命中窗口阶段与结束清理。
 * - 窗口默认由动画通知驱动（AttackWindowOpen/Close），事件由 CombatHitComponent 以实例校验；
 *   Definition 关闭通知来源时使用配置时间窗口（显式记录的回退）。
 * - 所有退出路径（完成、被打断、取消、死亡、重置）都经 EndAbility 关窗清状态；
 *   通知丢失或 Montage 播放失败也能结束能力。
 */
UCLASS()
class UMeleeComboAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UMeleeComboAbility();

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
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	AFighterCharacter* GetFighter() const;

	void FinishMontageTask();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	TWeakObjectPtr<AFighterCharacter> CachedFighter;
	TWeakObjectPtr<const UAttackDefinition> ActiveDefinition;

	uint64 AttackInstanceId = 0;
	bool bInstanceActive = false;
	bool bAttackTagApplied = false;
	FTimerHandle WindowOpenTimerHandle;
	FTimerHandle WindowCloseTimerHandle;
};
