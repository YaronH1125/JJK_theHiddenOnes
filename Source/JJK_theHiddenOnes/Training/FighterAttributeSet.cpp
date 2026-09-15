// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/FighterAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"
#include "Net/UnrealNetwork.h"

void UFighterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 当前仅约束生命范围；资源类属性的约束在 M2 引入对应 GE 时补充
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		const float Max = FMath::Max(GetMaxHealth(), 0.f);
		SetHealth(FMath::Clamp(GetHealth(), 0.f, Max));
	}
}

void UFighterAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFighterAttributeSet, Health, OldValue);
}

void UFighterAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFighterAttributeSet, MaxHealth, OldValue);
}

void UFighterAttributeSet::OnRep_ActionResource(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFighterAttributeSet, ActionResource, OldValue);
}

void UFighterAttributeSet::OnRep_MaxActionResource(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFighterAttributeSet, MaxActionResource, OldValue);
}

void UFighterAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFighterAttributeSet, Energy, OldValue);
}

void UFighterAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFighterAttributeSet, MaxEnergy, OldValue);
}

void UFighterAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UFighterAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFighterAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFighterAttributeSet, ActionResource, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFighterAttributeSet, MaxActionResource, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFighterAttributeSet, Energy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFighterAttributeSet, MaxEnergy, COND_None, REPNOTIFY_Always);
}
