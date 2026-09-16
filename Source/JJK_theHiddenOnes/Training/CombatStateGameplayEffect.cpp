#include "Training/CombatStateGameplayEffect.h"
UCombatStateGameplayEffect::UCombatStateGameplayEffect()
{
 DurationPolicy = EGameplayEffectDurationType::HasDuration;
 DurationMagnitude = FScalableFloat(1.f);
}
