// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ModifyActionResourceGameplayEffect.generated.h"

/**
 * 行动资源增减 GE（Instant，Additive ActionResource）：
 * 消耗与恢复统一经 SetByCaller(Data.Amount) 传入（负值消耗、正值恢复）。
 */
UCLASS()
class UModifyActionResourceGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UModifyActionResourceGameplayEffect();
};
