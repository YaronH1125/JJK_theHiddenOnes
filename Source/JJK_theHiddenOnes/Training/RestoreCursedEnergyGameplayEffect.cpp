#include "Training/RestoreCursedEnergyGameplayEffect.h"
#include "Training/FighterAttributeSet.h"
#include "Training/CombatTypes.h"
URestoreCursedEnergyGameplayEffect::URestoreCursedEnergyGameplayEffect()
{
 DurationPolicy = EGameplayEffectDurationType::Instant;
 FGameplayModifierInfo Modifier;
 Modifier.Attribute = UFighterAttributeSet::GetCursedEnergyAttribute();
 Modifier.ModifierOp = EGameplayModOp::Additive;
 FSetByCallerFloat Caller; Caller.DataTag = TAG_Data_Amount;
 Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Caller);
 Modifiers.Add(Modifier);
}
