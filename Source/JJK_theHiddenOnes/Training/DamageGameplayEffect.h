// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DamageGameplayEffect.generated.h"

/**
 * 伤害 GE（Instant，Additive 生命）：伤害值经 SetByCaller(Data.Damage) 传入。
 * 纯数据 GE，构造期配置，无需蓝图资产；正常伤害与资源消耗一律走 GE（项目约束 C04）。
 */
UCLASS()
class UDamageGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDamageGameplayEffect();
};
