// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "FighterAbilitySystemComponent.generated.h"

/**
 * 角色 ASC：M1 阶段仅确定复制模式并作为后续授予/输入映射的扩展点，
 * 不在基类塞入具体技能算法（02_架构设计.md 第 2 节）。
 */
UCLASS(ClassGroup = (JJK), meta = (BlueprintSpawnableComponent))
class UFighterAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
 UFighterAbilitySystemComponent();
 bool HasInfiniteResources() const;
 bool HasNoCooldown() const;
 bool CanPayTrainingCost() const;
 void PayTrainingCost();
 UFUNCTION(BlueprintPure, Category="Training") float GetTrainingCooldownRemaining() const;
 UFUNCTION(BlueprintPure, Category="Training|Debug") int32 GetGrantedAbilityCount() const { return GetActivatableAbilities().Num(); }
 UFUNCTION(BlueprintPure, Category="Training|Debug") int32 GetActiveEffectCount() const { return GetActiveEffects(FGameplayEffectQuery()).Num(); }

};
