// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ModifyAttributeGameplayEffect.generated.h"

class UFighterAttributeSet;

/**
 * 行动/咒力/领域能量增减 GE（Instant，Additive）：
 * 数值经 SetByCaller(Data.Amount) 传入（负=消耗，正=恢复）。
 */
UCLASS()
class UModifyCursedEnergyGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UModifyCursedEnergyGameplayEffect();
};

UCLASS()
class UModifyDomainEnergyGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UModifyDomainEnergyGameplayEffect();
};
