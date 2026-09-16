// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/CombatTypes.h"
#include "CombatHitComponent.generated.h"

class AFighterCharacter;
class UAnimSequenceBase;
class UAttackDefinition;

/**
 * 命中检测与结算入口（M2.3/M2.4/M2.5）：
 * - 有效窗口内按拳部 Socket 帧间扫掠采样；组件仅在窗口期 Tick。
 * - 去重键 = 攻击实例 + 命中段 + 目标，保持到本段结束；新攻击实例不共享去重。
 * - 统一结算顺序：验证 → 去重 → 死亡过滤 → 生成结果 → GE 伤害 → 受击/死亡事件（延迟到受击方自身下一 Tick 处理，
 *   保证同帧互中按"已有效接触换血"结算，被打断者未来窗口接触无效）。
 */
UCLASS(ClassGroup = (JJK), meta = (BlueprintSpawnableComponent))
class UCombatHitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatHitComponent();

	/** 攻击开始：分配实例并记录配置；窗口未开启，等待通知/定时打开 */
	uint64 BeginAttack(const UAttackDefinition* Definition);

	/** 动画通知驱动的窗口开关；按实例校验，拒绝旧动画遗留事件 */
	void HandleAnimWindowNotify(bool bOpen, const UAnimSequenceBase* Animation);

	/** 关闭当前窗口并保留实例（进入恢复阶段） */
	void CloseWindow();

	/** 攻击结束/中断/重置：关窗、清去重、停用实例 */
	void EndAttack();

	/** 有待处理战斗事件：确保组件 Tick 开启（事件在本组件 Tick 内、扫掠之后处理） */
	void NotifyEventsPending() { SetComponentTickEnabled(true); }

	/** 当前段配置（GA/闪避取消窗口使用） */
	const UAttackDefinition* GetActiveDefinition() const { return ActiveDefinition.Get(); }

	/** 是否处于当前段的闪避取消窗口内（段开始后 CancelWindowStart～End） */
	bool IsCancelWindowOpen() const;

	/** 当前段已进行时长（秒，自 BeginAttack） */
	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetSegmentElapsedTime() const;
	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetSegmentId() const { return SegmentId; }

	/** 当前阶段（调试显示用） */
	UFUNCTION(BlueprintPure, Category = "Combat")
	EAttackPhase GetPhase() const { return Phase; }

	void SetPhase(EAttackPhase InPhase) { Phase = InPhase; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int64 GetActiveInstanceId() const { return static_cast<int64>(ActiveInstanceId); }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsWindowOpen() const { return bWindowOpen; }

	/** 本次攻击累计有效命中数 */
	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetHitCount() const { return HitCountThisAttack; }

	/** 是否存在活动攻击实例 */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool HasActiveAttack() const { return bAttackActive; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 一帧内收集的候选接触 */
	struct FContactCandidate
	{
		TWeakObjectPtr<AFighterCharacter> Target;
		FVector HitLocation;
	};

	void ProcessSweep();
	void ApplyBatch(const TArray<FContactCandidate>& Contacts);

	AFighterCharacter* GetOwnerFighter() const;

	/** 窗口期开关（Tick 按需启停，避免常驻渲染帧采样） */
	void SetWindowTickEnabled(bool bEnabled);

	/** 当前攻击配置 */
	TWeakObjectPtr<const UAttackDefinition> ActiveDefinition;

	/** 攻击实例计数（角色生命周期内递增） */
	uint64 InstanceCounter = 0;
	uint64 ActiveInstanceId = 0;
	double SegmentBeginTime = 0.0;
	bool bAttackActive = false;
	bool bWindowOpen = false;

	EAttackPhase Phase = EAttackPhase::None;
	bool bCursedEnergyGranted = false;
	int32 SegmentId = 0;
	int32 HitCountThisAttack = 0;

	/** 本段去重集合 */
	TSet<FCombatHitDedupKey> DedupKeys;

	/** 上次采样 Socket 位置（帧间扫掠起点） */
	FVector LastSocketLocation = FVector::ZeroVector;
	bool bHasLastSocketLocation = false;
};
