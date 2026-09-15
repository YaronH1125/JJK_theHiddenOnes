// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TrainingTypes.generated.h"

/** 训练场身份；由 GameMode 在生成时分配 */
UENUM(BlueprintType)
enum class EFighterRole : uint8
{
	Unassigned,
	Player,
	Opponent
};
