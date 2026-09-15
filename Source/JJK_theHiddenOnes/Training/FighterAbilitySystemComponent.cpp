// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/FighterAbilitySystemComponent.h"

UFighterAbilitySystemComponent::UFighterAbilitySystemComponent()
{
	// 单机训练场：Full 模式便于调试观察全部 GameplayEffect
	ReplicationMode = EGameplayEffectReplicationMode::Full;
}
