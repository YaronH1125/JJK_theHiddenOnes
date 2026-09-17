// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/DomainExpansionAbility.h"

#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Training/TrainingGameMode.h"

UDomainExpansionAbility::UDomainExpansionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_Domain);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(TAG_State_Dead);
	ActivationBlockedTags.AddTag(TAG_State_HitStun);
	ActivationBlockedTags.AddTag(TAG_State_KnockedDown);
	ActivationBlockedTags.AddTag(TAG_State_StanceSwitching);
	ActivationBlockedTags.AddTag(TAG_State_GuardStun);
	ActivationBlockedTags.AddTag(TAG_State_DomainActive);
}

void UDomainExpansionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	auto* Fighter = Cast<AFighterCharacter>(GetAvatarActorFromActorInfo());
	if (!Fighter || !Fighter->GetDefinition())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 消耗领域能量 100（08 第 7.1 节）
	if (!Fighter->ModifyEnergy(-Fighter->GetDefinition()->DomainConfig.EnergyCost))
	{
		UE_LOG(LogTemp, Log, TEXT("[Domain] %s 领域能量不足"), *GetNameSafe(Fighter));
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	CachedFighter = Fighter;

	// 结印期：打上 DomainCasting 标签（期间可被打断 → 失败不退能量）
	if (auto* ASC = Fighter->GetFighterAbilitySystemComponent())
		ASC->AddLooseGameplayTag(TAG_State_DomainCasting);

	const float CastTime = Fighter->GetDefinition()->DomainConfig.CastTime;
	GetWorld()->GetTimerManager().SetTimer(CastTimerHandle, this,
		&UDomainExpansionAbility::HandleCastComplete, FMath::Max(CastTime, 0.1f), false);

	UE_LOG(LogTemp, Log, TEXT("[Domain] %s 开始结印（%.1fs）"), *GetNameSafe(Fighter), CastTime);
}

void UDomainExpansionAbility::HandleCastComplete()
{
	auto* Fighter = CachedFighter.Get();
	if (!Fighter) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false); return; }

	// 移除结印标签
	if (auto* ASC = Fighter->GetFighterAbilitySystemComponent())
		ASC->RemoveLooseGameplayTag(TAG_State_DomainCasting);

	// 通知 GameMode：捕获目标并登记领域会话
	auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
	if (GM && GM->TryOpenDomain(Fighter))
	{
		UE_LOG(LogTemp, Log, TEXT("[Domain] %s 领域展开成功"), *GetNameSafe(Fighter));
		// 领域激活期间角色持有 DomainActive 标签（GameMode 管理）
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[Domain] %s 领域展开失败（目标不可捕获）"), *GetNameSafe(Fighter));
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UDomainExpansionAbility::HandleFighterInterrupted()
{
	// 结印被打断：失败，不退能量
	UE_LOG(LogTemp, Log, TEXT("[Domain] %s 结印被打断，领域展开失败"), *GetNameSafe(CachedFighter.Get()));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UDomainExpansionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(CastTimerHandle);
	if (auto* ASC = CachedFighter.IsValid() ? CachedFighter->GetFighterAbilitySystemComponent() : nullptr)
		ASC->RemoveLooseGameplayTag(TAG_State_DomainCasting);
	CachedFighter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
