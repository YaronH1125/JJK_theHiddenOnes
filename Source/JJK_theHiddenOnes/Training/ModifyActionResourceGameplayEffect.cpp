// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/ModifyActionResourceGameplayEffect.h"

#include "Training/CombatTypes.h"
#include "Training/FighterAttributeSet.h"

UModifyActionResourceGameplayEffect::UModifyActionResourceGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UFighterAttributeSet::GetActionResourceAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat Caller;
	Caller.DataTag = TAG_Data_Amount;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Caller);

	Modifiers.Add(Modifier);
}
