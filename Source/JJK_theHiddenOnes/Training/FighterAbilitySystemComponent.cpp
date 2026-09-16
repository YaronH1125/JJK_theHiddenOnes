// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/FighterAbilitySystemComponent.h"

UFighterAbilitySystemComponent::UFighterAbilitySystemComponent()
{
	// 单机训练场：Full 模式便于调试观察全部 GameplayEffect
	ReplicationMode = EGameplayEffectReplicationMode::Full;
}

#include "Training/TrainingGameMode.h"
#include "Training/TrainingProbeAbility.h"
#include "Training/FighterAttributeSet.h"
#include "Training/CombatTypes.h"
bool UFighterAbilitySystemComponent::HasInfiniteResources() const
{
 const auto* GM=GetWorld()->GetAuthGameMode<ATrainingGameMode>();
 return GM && GM->Settings.bInfiniteResources;
}
bool UFighterAbilitySystemComponent::HasNoCooldown() const
{
 const auto* GM=GetWorld()->GetAuthGameMode<ATrainingGameMode>();
 return GM && GM->Settings.bNoCooldown;
}
bool UFighterAbilitySystemComponent::CanPayTrainingCost() const
{
 return HasInfiniteResources() || (GetNumericAttribute(UFighterAttributeSet::GetActionResourceAttribute())>=1.f && GetNumericAttribute(UFighterAttributeSet::GetCursedEnergyAttribute())>=10.f && GetNumericAttribute(UFighterAttributeSet::GetEnergyAttribute())>=5.f);
}
void UFighterAbilitySystemComponent::PayTrainingCost()
{
 if (HasInfiniteResources()) return;
 auto Spec=MakeOutgoingSpec(UTrainingCostEffect::StaticClass(),1,MakeEffectContext());
 if (Spec.IsValid()) ApplyGameplayEffectSpecToSelf(*Spec.Data);
}
float UFighterAbilitySystemComponent::GetTrainingCooldownRemaining() const
{
 FGameplayTagContainer Tags; Tags.AddTag(TAG_Cooldown_TrainingProbe);
 const auto Times=GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
 float Result=0.f; for(float Time:Times) Result=FMath::Max(Result,Time); return Result;
}
