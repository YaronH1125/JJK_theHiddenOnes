// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/ModifyAttributeGameplayEffect.h"

#include "Training/CombatTypes.h"
#include "Training/FighterAttributeSet.h"

namespace
{
	FGameplayModifierInfo MakeAmountModifier(const FGameplayAttribute& Attribute)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;

		FSetByCallerFloat Caller;
		Caller.DataTag = TAG_Data_Amount;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Caller);
		return Modifier;
	}
}

UModifyCursedEnergyGameplayEffect::UModifyCursedEnergyGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeAmountModifier(UFighterAttributeSet::GetCursedEnergyAttribute()));
}

UModifyDomainEnergyGameplayEffect::UModifyDomainEnergyGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeAmountModifier(UFighterAttributeSet::GetEnergyAttribute()));
}
