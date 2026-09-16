// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "MeleeComboAbility.generated.h"

class AFighterCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAttackDefinition;

/**
 * 近战攻击 GA（M3）：驱动一段攻击序列（A1→A2→A3 / 重拳 / 腿击 / 重踢）。
 * - 每段：分配攻击实例 → Montage → 时间窗口扫掠 → 恢复。
 * - 衔接窗口内轮询输入缓存（NextSegment/HeavyPunch/Kick/HeavyKick），按当前段允许集消费转段；
 *   未消费则 Montage 完成后结束。
 * - 所有退出路径经 EndAbility 关窗清状态；通知丢失/播放失败也能结束。
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

	UFUNCTION()
	void HandleWindowOpen();

	UFUNCTION()
	void HandleWindowClose();

	UFUNCTION()
	void HandleCachePoll();

	/** 进入序列中的下一段 */
	void AdvanceToSegment(int32 Index);

	AFighterCharacter* GetFighter() const;
	UAttackDefinition* CurrentDef() const;
	bool InstanceMatches() const;

	void FinishMontageTask();
	void ClearTimers();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	TWeakObjectPtr<AFighterCharacter> CachedFighter;

	/** 本序列的段配置 */
	UPROPERTY()
	TArray<TObjectPtr<UAttackDefinition>> SegmentSequence;

	int32 SegmentIndex = INDEX_NONE;
	uint64 AttackInstanceId = 0;
	bool bInstanceActive = false;
	bool bAttackTagApplied = false;
	bool bSuperArmorApplied = false;
	bool bComboWindowOpen = false;

	FTimerHandle WindowOpenTimerHandle;
	FTimerHandle WindowCloseTimerHandle;
	FTimerHandle ComboWindowOpenTimerHandle;
	FTimerHandle ComboWindowCloseTimerHandle;
	FTimerHandle ComboCachePollTimerHandle;
};