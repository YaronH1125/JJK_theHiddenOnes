// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/FighterDefinition.h"

#include "Training/DodgeAbility.h"
#include "Training/MeleeComboAbility.h"
#include "Training/StanceSwitchAbility.h"

UFighterDefinition::UFighterDefinition()
{
	// 默认普攻：M3 三段连击 GA；DA 资产可覆盖
	MeleeAttackAbility = UMeleeComboAbility::StaticClass();
	DodgeAbility = UDodgeAbility::StaticClass();
	StanceSwitchAbility = UStanceSwitchAbility::StaticClass();
}
