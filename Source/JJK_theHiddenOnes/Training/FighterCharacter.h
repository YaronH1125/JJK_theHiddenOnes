// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "JJK_theHiddenOnesCharacter.h"
#include "Training/TrainingTypes.h"
#include "FighterCharacter.generated.h"

class UFighterAbilitySystemComponent;
class UFighterAttributeSet;
class UFighterDefinition;
class UGameplayAbility;
class UInputMappingContext;
class UTargetingComponent;
struct FGameplayAbilitySpecHandle;

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

	/** 训练场身份（GameMode 分配） */
	UFUNCTION(BlueprintPure, Category = "Fighter")
	EFighterRole GetRole() const { return FighterRole; }

	void SetRole(EFighterRole InRole) { FighterRole = InRole; }

	/** 目标选择组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fighter", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTargetingComponent> Targeting;

	UFUNCTION(BlueprintPure, Category = "Fighter")
	UTargetingComponent* GetTargeting() const { return Targeting; }

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
};
