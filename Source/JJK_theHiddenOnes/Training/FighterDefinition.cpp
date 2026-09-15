// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/FighterDefinition.h"

#include "Training/MeleeComboAbility.h"

UFighterDefinition::UFighterDefinition()
{
	// 默认普攻：M2 单段拳击 GA；DA 资产可覆盖
	MeleeAttackAbility = UMeleeComboAbility::StaticClass();
}
