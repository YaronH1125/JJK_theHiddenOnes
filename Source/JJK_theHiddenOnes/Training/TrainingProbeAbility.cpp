#include "Training/TrainingProbeAbility.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterAttributeSet.h"
#include "Training/CombatInputComponent.h"
#include "Training/CombatStateGameplayEffect.h"
#include "Training/CombatTypes.h"

UTrainingProbeAbility::UTrainingProbeAbility()
{
 InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}
bool UTrainingProbeAbility::CanActivateAbility(const FGameplayAbilitySpecHandle H, const FGameplayAbilityActorInfo* A, const FGameplayTagContainer* S, const FGameplayTagContainer* T, FGameplayTagContainer* R) const
{
 const auto* F = A ? Cast<AFighterCharacter>(A->AvatarActor.Get()) : nullptr;
 return F && !F->HasPendingCombatEvents() && F->CanAct() && !F->IsGuardIntent() && F->GetCombatInput()->AreRequestsEnabled() && Super::CanActivateAbility(H,A,S,T,R);
}
bool UTrainingProbeAbility::CheckCost(const FGameplayAbilitySpecHandle H, const FGameplayAbilityActorInfo* A, FGameplayTagContainer* R) const
{
 const auto* ASC = A ? Cast<UFighterAbilitySystemComponent>(A->AbilitySystemComponent.Get()) : nullptr;
 return ASC && ASC->CanPayTrainingCost();
}
void UTrainingProbeAbility::ApplyCost(const FGameplayAbilitySpecHandle H, const FGameplayAbilityActorInfo* A, const FGameplayAbilityActivationInfo I) const
{
 CastChecked<UFighterAbilitySystemComponent>(A->AbilitySystemComponent.Get())->PayTrainingCost();
}
bool UTrainingProbeAbility::CheckCooldown(const FGameplayAbilitySpecHandle H, const FGameplayAbilityActorInfo* A, FGameplayTagContainer* R) const
{
 const auto* ASC = A ? Cast<UFighterAbilitySystemComponent>(A->AbilitySystemComponent.Get()) : nullptr;
 return ASC && (ASC->HasNoCooldown() || !ASC->HasMatchingGameplayTag(TAG_Cooldown_TrainingProbe));
}
void UTrainingProbeAbility::ApplyCooldown(const FGameplayAbilitySpecHandle H, const FGameplayAbilityActorInfo* A, const FGameplayAbilityActivationInfo I) const
{
 auto* ASC = CastChecked<UFighterAbilitySystemComponent>(A->AbilitySystemComponent.Get());
 if (ASC->HasNoCooldown()) return;
 auto Spec = ASC->MakeOutgoingSpec(UCombatStateGameplayEffect::StaticClass(),1,ASC->MakeEffectContext());
 Spec.Data->SetDuration(3.f,true);
 Spec.Data->DynamicGrantedTags.AddTag(TAG_Cooldown_TrainingProbe);
 ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}
void UTrainingProbeAbility::ActivateAbility(const FGameplayAbilitySpecHandle H, const FGameplayAbilityActorInfo* A, const FGameplayAbilityActivationInfo I, const FGameplayEventData* E)
{
 const bool bCommitted = CommitAbility(H,A,I);
 EndAbility(H,A,I,false,!bCommitted);
}
UTrainingCostEffect::UTrainingCostEffect()
{
 DurationPolicy=EGameplayEffectDurationType::Instant;
 auto Add=[this](FGameplayAttribute Attribute,float Amount) { FGameplayModifierInfo M; M.Attribute=Attribute; M.ModifierOp=EGameplayModOp::Additive; M.ModifierMagnitude=FScalableFloat(Amount); Modifiers.Add(M); };
 Add(UFighterAttributeSet::GetActionResourceAttribute(),-1.f);
 Add(UFighterAttributeSet::GetCursedEnergyAttribute(),-10.f);
 Add(UFighterAttributeSet::GetEnergyAttribute(),-5.f);
}
UTrainingHealEffect::UTrainingHealEffect()
{
 DurationPolicy=EGameplayEffectDurationType::Instant;
 FGameplayModifierInfo M; M.Attribute=UFighterAttributeSet::GetHealthAttribute(); M.ModifierOp=EGameplayModOp::Additive;
 FSetByCallerFloat C; C.DataTag=TAG_Data_Amount; M.ModifierMagnitude=FGameplayEffectModifierMagnitude(C); Modifiers.Add(M);
}
