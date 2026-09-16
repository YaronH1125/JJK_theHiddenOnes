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
	virtual float GetCooldown() const { return 0.f; }
	virtual float GetMoveSpeedScale() const { return 0.f; }
	virtual bool LocksMovementWhileCharging() const { return false; }
	virtual void FireBlast(float q, float Damage) { }
	virtual bool IsStationaryBlast() const { return false; }

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
	float StoredBaseMoveSpeed = 500.f;
	bool bMoveSpeedModified = false;
	FTimerHandle ChargeTickHandle;
	FTimerHandle PhaseTimerHandle;
	FTimerHandle CooldownTimerHandle;
};
