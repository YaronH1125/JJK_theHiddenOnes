// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "JJK_theHiddenOnesCharacter.h"
#include "Training/TrainingTypes.h"
#include "Training/TrainingSettings.h"
#include "Training/CombatTypes.h"
#include "GameplayEffectTypes.h"
#include "FighterCharacter.generated.h"

class UFighterAbilitySystemComponent;
class UFighterAttributeSet;
class UCombatHitComponent;
class UCombatInputComponent;
class UFighterDefinition;
class UGameplayAbility;
class UAttackDefinition;
class UInputMappingContext;
class UTargetingComponent;
struct FGameplayAbilitySpecHandle;
struct FOnAttributeChangeData;
struct FCombatEvent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FJJKCombatBoundary);

/**
 * 玩家与对手共享的战斗角色基座：
 * - Character 持有 ASC 与 AttributeSet（OwnerActor = AvatarActor = 本角色）。
 * - 初始数值与外观标识来自 FighterDefinition；属性初始化具备防重复保护，
 *   重新 Possess 只更新 ActorInfo 控制信息，不重复初始化数值或授予能力。
 * - 集中初始化/重置入口仅用于初始化与训练重置，不作为正常战斗扣血方式。
 */
UCLASS(Blueprintable)
class AFighterCharacter : public AJJK_theHiddenOnesCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFighterCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Fighter|GAS")
	UFighterAbilitySystemComponent* GetFighterAbilitySystemComponent() const { return AbilitySystem; }

	UFUNCTION(BlueprintPure, Category = "Fighter|GAS")
	UFighterAttributeSet* GetFighterAttributeSet() const { return AttributeSet; }

	/** 角色定义（只读配置） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fighter")
	TObjectPtr<UFighterDefinition> Definition;

	UFUNCTION(BlueprintPure, Category = "Fighter")
	UFighterDefinition* GetDefinition() const { return Definition; }

	/** 训练场身份（GameMode 分配） */
	UFUNCTION(BlueprintPure, Category = "Fighter")
	EFighterRole GetRole() const { return FighterRole; }

	void SetRole(EFighterRole InRole) { FighterRole = InRole; }

	/** 目标选择组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fighter", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTargetingComponent> Targeting;

	UFUNCTION(BlueprintPure, Category = "Fighter")
	UTargetingComponent* GetTargeting() const { return Targeting; }

	/** 共享动作请求入口（轻/重输入会话） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fighter", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatInputComponent> CombatInput;

	UFUNCTION(BlueprintPure, Category = "Fighter")
	UCombatInputComponent* GetCombatInput() const { return CombatInput; }

	/** 命中检测与结算组件（窗口采样、去重、批次结算） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fighter", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatHitComponent> CombatHit;

	UFUNCTION(BlueprintPure, Category = "Fighter")
	UCombatHitComponent* GetCombatHit() const { return CombatHit; }

	UFUNCTION(BlueprintPure, Category = "Fighter")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Fighter")
	TSubclassOf<UGameplayAbility> GetMeleeAttackAbilityClass() const;

	UFUNCTION(BlueprintPure, Category = "Fighter")
	EFighterStance GetStance() const { return Stance; }

	UFUNCTION(BlueprintPure, Category = "Fighter")
	bool IsAttacking() const;

	UFUNCTION(BlueprintPure, Category = "Fighter")
	bool IsGuardIntent() const;

	/** 当前是否处于可生效的防御姿态（意图 + 无阻止状态；面向判断在结算处按来向计算） */
	bool IsGuarding() const;

	/** 共享动作请求通用校验（死亡/未初始化/硬直/倒地/形态） */
	EActionRequestResult ValidateAttackRequest() const;

	/** 请求一段攻击序列（输入入口；连击中则该请求来自缓存消费路径） */
	bool RequestAttackSequence(ECachedAction Action);

	/** 闪避请求（含取消变招判定）；返回是否成功激活 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool RequestDodge(FVector Direction);

	/** 切形态请求 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool RequestStanceSwitch();

	/** 切形态完成回调（StanceSwitchAbility 调用）：切换 Stance 并记录时间戳 */
	void NotifyStanceSwitched();

	/** 消耗行动资源（GE 扣除）；不足返回 false */
 bool SpendActionResource(float Amount);
 void ApplyCombatDamage(AFighterCharacter* Target, float RawDamage, float ResolvedDamage, ETrainingContact Kind);


	/** 恢复行动资源（GE 增加） */
	void RestoreActionResource(float Amount);

	/** 命中事件入队：由 CombatHitComponent 在自身 Tick 内、扫掠之后统一处理（M2.5 换血保证） */
	void QueueCombatEvent(const FCombatEvent& Event);

	/** 事件处理与查询（由 CombatHitComponent 调度） */
	void ProcessCombatEvents();
	bool HasPendingCombatEvents() const { return PendingCombatEvents.Num() > 0; }

	/** 投技配对：由攻击方驱动，双方进入配对锁定 */
	bool BeginThrowPair(AFighterCharacter* Partner, float Duration, float Damage, float PairDistance);
	void EndThrowPair(bool bRestore);
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsThrowPaired() const { return HasCombatTag(TAG_State_ThrowPaired); }

	/** 霸体：受击不中断（伤害照常） */
	bool HasSuperArmor() const;

	/** 供 GA/结算使用的内部查询 */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool HasCombatTag(const FGameplayTag& Tag) const;

	/** 来向是否在防御者正面弧内（防御结算用） */
	bool IsAttackFromFront(AActor* Attacker, float HalfAngleDeg) const;
	float GetGuardFrontArcHalfAngle() const;

	/** 条件投技可转检查（距离/地面/配对占用/状态） */
	bool CanThrowTarget(AFighterCharacter* Target, bool bAttackCanThrow);
	UAttackDefinition* ResolveAttackDefinition(ECachedAction Action) const;
	UAttackDefinition* ResolveCurrentAttackDefinition() const;
	UAnimMontage* GetHitReactMontage() const;
	bool ModifyActionResource(float SignedAmount);
	void TickResourceRegen();

	/** 闪避请求的暂存（RequestDodge 设置，DodgeAbility 激活时读取） */
	struct FDodgeRequest
	{
		bool bPending = false;
		bool bCancel = false;
		FVector Direction = FVector::ZeroVector;
	};
	FDodgeRequest ConsumePendingDodge();
	void ClearPendingDodge() { PendingDodge.bPending = false; }
	/** 最近移动输入方向（世界空间；闪避无方向时后撤用） */
	FVector LastMoveInputDirection = FVector::ZeroVector;
	FVector GetLastDodgeDirection() const { return LastMoveInputDirection; }

	/** GA 消费待执行攻击序列 */
	bool ConsumePendingSequence(TArray<TObjectPtr<UAttackDefinition>>& OutSequence, int32& OutSegmentIndex);
	void SetPendingSequenceSegment(int32 Index) { PendingSegmentIndex = Index; }
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void JJKDebugForceHitReact();
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void JJKDebugKill();
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanAct() const;
	void RefreshMovementControl();
 UPROPERTY(BlueprintAssignable, Category = "Combat")
 FJJKCombatBoundary OnRecovered;
 /** 延迟受击/死亡队列已处理，训练统计可读取实际保护/恢复状态。 */
 UPROPERTY(BlueprintAssignable, Category = "Combat")
 FJJKCombatBoundary OnCombatEventsProcessed;
 UPROPERTY(BlueprintAssignable, Category = "Combat")
 FJJKCombatBoundary OnComboEnded;
 void RestoreCursedEnergyOnHit();
 FActiveGameplayEffectHandle ApplyCombatState(FGameplayTag Tag, float Duration);
 void ClearReactionEffects();
 FActiveGameplayEffectHandle StunEffect, KnockdownEffect, DeathEffect, ThrowEffect, GuardEffect, GetUpEffect;

	virtual void Jump() override;

	/**
	 * 按定义初始化属性与外观。
	 * 幂等：数值初始化只生效一次（StatsInitCount 不增长）；授予能力去重。
	 * 已初始化后如需强制刷新数值，请使用 ResetToInitialState。
	 */
	UFUNCTION(BlueprintCallable, Category = "Fighter")
	void InitializeFromDefinition();

	/** 训练重置：恢复定义初始数值、回出生点、清速度与目标；不影响已授予能力 */
	UFUNCTION(BlueprintCallable, Category = "Fighter")
	void ResetToInitialState();

	UFUNCTION(BlueprintPure, Category = "Fighter")
	bool IsStatsInitialized() const { return StatsInitCount > 0; }

	UFUNCTION(BlueprintPure, Category = "Fighter|GAS")
	int32 GetStatsInitCount() const { return StatsInitCount; }

	/** 生成时的初始变换，供训练重置使用 */
	UFUNCTION(BlueprintPure, Category = "Fighter")
	FTransform GetInitialTransform() const { return InitialTransform; }

	/** 由 GameMode 在生成时记录 */
	void RecordInitialTransform(const FTransform& InTransform);

	/** 训练场身份颜色覆盖（P1/P2 区分用）；未设置时用定义的 MarkerColor */
	void SetMarkerColor(const FLinearColor& Color);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DoMove(float Right, float Forward) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void PawnClientRestart() override;

	/** 更新 ASC ActorInfo；重新 Possess 时允许重复调用 */
	void InitAbilityActorInfo();

	/** 按定义设置属性初值；防重复由调用方守卫 */
	void ApplyDefinitionStats();

	/** 授予定义中的能力；按能力类去重，重置不重复授予 */
	void GrantAbilities();

	/** 按定义应用 P1/P2 颜色标识（覆盖材质） */
	void ApplyMarkerVisual();

	/** 初始化玩家的默认输入映射上下文（对手无控制器，自动跳过） */
	void AddDefaultMappingContext() const;

	/** ASC 与属性集 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fighter|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFighterAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fighter|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFighterAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "Fighter|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Fighter|Input")
	int32 MappingPriority = 0;

private:
	UPROPERTY()
	EFighterRole FighterRole = EFighterRole::Unassigned;

	TOptional<FLinearColor> MarkerColorOverride;

	FTransform InitialTransform;

	/** 数值初始化次数；>0 表示已初始化，调试幂等性用 */
	int32 StatsInitCount = 0;

	/** 已授予能力类，防止重复授予 */
	TSet<TSubclassOf<UGameplayAbility>> GrantedAbilityClasses;

	// ---------- M2/M3 战斗状态 ----------
	/** 命中事件队列（受击/死亡/倒地/防御硬直），CombatHitComponent 在扫掠之后统一处理 */
	TArray<FCombatEvent> PendingCombatEvents;

	bool bDead = false;
	FTimerHandle HitStunTimerHandle;
	FTimerHandle KnockdownTimerHandle;
	FTimerHandle ResourceRegenTimerHandle;
	FTimerHandle ThrowPairTimerHandle;
	FTimerHandle ThrowWatchTimerHandle;

	void ApplyHitReactNow(const FCombatEvent& Event);
	void ApplyGuardStunNow(const FCombatEvent& Event);
	void ApplyKnockdownNow(const FCombatEvent& Event);
	void Die(AActor* InInstigator);
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void RemoveHitStun();
	void EndKnockdown();
	void BeginGetUp();
	FTimerHandle GetUpTimerHandle;

	/** 最近一次受击来源与位置（调试显示） */
	FVector LastHitLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> LastHitInstigator;

	// ---------- M3 状态 ----------
	UPROPERTY()
	EFighterStance Stance = EFighterStance::Melee;

	/** 防御意图（按住 F）；能否生效由结算按状态与朝向判定 */

	/** 行动资源上次消耗时刻 */
	double LastResourceSpendTime = -1000.0;

	/** 投技配对对象 */
	TWeakObjectPtr<AFighterCharacter> ThrowPartner;

	/** 待执行攻击序列（输入入口设置，GA 激活时消费） */
	UPROPERTY()
	TArray<TObjectPtr<UAttackDefinition>> PendingSequence;
	int32 PendingSegmentIndex = 0;

	FDodgeRequest PendingDodge;
	bool bMovementLocked = false;
	bool bSavedOrientToMovement = true;
	float SavedMaxWalkSpeed = 500.f;
	bool FindThrowPosition(AFighterCharacter* Partner, float PairDistance, FVector& OutPosition) const;
	void TickThrowPair();
	TWeakObjectPtr<UAnimMontage> ThrowMontage;
	bool bThrowDriver = false;
	uint8 PreThrowMovementMode = 1;

	bool bPendingStanceSwitch = false;
	double LastStanceSwitchTime = -1000.0;

	static constexpr float ResourceRegenTimerInterval = 0.25f;

};
