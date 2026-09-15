// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/DamageGameplayEffect.h"

#include "Training/CombatTypes.h"
#include "Training/FighterAttributeSet.h"

UDamageGameplayEffect::UDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UFighterAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat Caller;
	Caller.DataTag = TAG_Data_Damage;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Caller);

	Modifiers.Add(Modifier);
}
