// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Training/CombatTypes.h"
#include "ChargedBlastAbility.generated.h"

class AFighterCharacter;

/** 蓄力炮阶段：蓄力 → 锁向前摇 → 发射/收招 → 恢复 */
UENUM(BlueprintType)
enum class EBlastPhase : uint8
{
	None,
	Charging,
	Windup,
	Recovery
};

/**
 * 蓄力炮基类（M6.3/M6.4）：按实际游戏时间推进蓄力、增量成本与伤害；
 * 基础成本合法激活时提交，增量随强度增长支付且受当前咒力约束（qPaid）；
 * 移动炮松开即进锁向前摇；超级炮低于门槛收招（已扣不退、启动一次冷却）、
 * 满蓄保持不限时；冷却只在发射或已付费中断时启动一次。
 * 死亡/重置/失焦/命中硬直清理会话；已命中不回滚，已扣成本不退。
 */
UCLASS()
class UChargedBlastAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UChargedBlastAbilityBase();

	/** 外部松开信号（输入组件按形态路由） */
	void NotifyExternalRelease();

	/** 外部中止（切形态等）：已扣不退，超级炮按中断进冷却（08 A03） */
	void CancelFromOutside();
	/** M7 表现占位：投射体参数 */
	virtual float GetProjectileSpeed() const { return 5000.f; }
	virtual float GetProjectileRadius() const { return 12.f; }

	/** 阶段与强度查询（调试显示与验收） */
	EBlastPhase GetPhase() const { return Phase; }
	float GetPaidQ() const { return PaidQ; }
	bool IsCharging() const { return Phase == EBlastPhase::Charging; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual float GetMinCost() const { return 0.f; }
	virtual float GetMaxCost() const { return 0.f; }
	virtual float GetMinDamage() const { return 0.f; }
	virtual float GetMaxDamage() const { return 0.f; }
	virtual float GetChargeCapTime() const { return 0.f; }
	virtual float GetLockWindup() const { return 0.f; }
	virtual float GetRange() const { return 0.f; }
	virtual float GetRecoveryTime() const { return 0.f; }
	virtual bool HasMinChargeGate() const { return false; }
	virtual float GetMinChargeTime() const { return 0.f; }
	/** 蓄力进度：超级炮按 clamp((t-门槛)/(cap-门槛))（08 A03），普通炮按 t/cap */
	virtual float ComputeChargeQ(float ElapsedSeconds) const;
	/** A02：与近战共享攻防口径的结算参数 */
	virtual float GetGuardStunDuration() const { return 0.6f; }
	virtual float GetHitStunDuration() const { return 0.4f; }
	virtual float GetInterruptLevel() const { return 1.f; }
	virtual float GetKnockbackStrength() const { return 500.f; }
	/** 解析当前瞄准点（锁定目标 > 相机 > 朝向）；松开时捕获固定 */
	FVector ResolveAimPoint() const;
	/** 按当前蓄力时长结算一次成本/强度（帧指针挂时由松开补齐最后一段） */
	void UpdateChargeProgress();
	virtual float GetCooldown() const { return 0.f; }
	virtual float GetMoveSpeedScale() const { return 0.f; }
	virtual bool LocksMovementWhileCharging() const { return false; }
	virtual void FireBlast(float q, float Damage) { }
	virtual bool IsStationaryBlast() const { return false; }
	/** 蓄力期状态标签：基类打 BlastCharging；移动炮叠加 RangedBlastCharging */
	virtual void ApplyChargeStateTags(class UFighterAbilitySystemComponent* ASC);
	virtual void ClearChargeStateTags(class UFighterAbilitySystemComponent* ASC);

private:
	void ClearTimers();
	void StartChargeTick();
	void HandleChargeTick();
	void HandleExternalRelease();
	void HandleWindupDone();
	void HandleRecoveryDone();
	void StartWindup();
	void FireOnce();
	void AbortBlast(const TCHAR* Reason, bool bPaidInterrupt);
	void ApplySuperBlastCooldown();
	void HandleCooldownDone();
	void ApplyMoveSpeedScale(float Scale);
	void RestoreMoveSpeed();

protected:
	AFighterCharacter* GetFighter() const;
	bool PassesActivationChecks(FString& OutReason) const;

protected:
	TWeakObjectPtr<AFighterCharacter> CachedFighter;

	EBlastPhase Phase = EBlastPhase::None;
	double Now() const;
	double ChargeStartTime = 0.0;
	float PaidCost = 0.f;
	float PaidQ = 0.f;
	bool bReleaseSignaled = false;
	bool bCooldownTagApplied = false;
	/** A02 方向解耦：松开时捕获瞄准点，前摇期不继续瞬时追随目标 */
	FVector LockedAimPoint = FVector::ZeroVector;
	bool bAimPointCaptured = false;
	/** 领域能量实例键：每发递增（08：每攻击实例封顶） */
	uint64 ShotCounter = 0;
	/** 超级炮成功发射后同样进入完整冷却（08：发射或中断后完整冷却） */
	bool bSuperBlastFired = false;
	float StoredBaseMoveSpeed = 500.f;
	bool bMoveSpeedModified = false;
	FTimerHandle ChargeTickHandle;
	FTimerHandle PhaseTimerHandle;
	FTimerHandle CooldownTimerHandle;
};
