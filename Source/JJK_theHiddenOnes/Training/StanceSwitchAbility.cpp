// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/StanceSwitchAbility.h"

#include "Training/CombatTypes.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Training/FighterAbilitySystemComponent.h"

UStanceSwitchAbility::UStanceSwitchAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_StanceSwitch);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(TAG_State_Dead);
	ActivationBlockedTags.AddTag(TAG_State_HitStun);
	ActivationBlockedTags.AddTag(TAG_State_KnockedDown);
	// 注：StanceSwitching 不阻断再切——窗口内再按 E 由 RequestStanceSwitch 直接反向翻转（自由切换）
}

void UStanceSwitchAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AFighterCharacter* Fighter = Cast<AFighterCharacter>(GetAvatarActorFromActorInfo());
	if (Fighter == nullptr || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	CachedFighter = Fighter;

	// 自由切换：按下瞬间立即翻转形态（0 等待）；0.1s 标签仅作攻击拒绝的表现窗口
	Fighter->NotifyStanceSwitched();

	if (UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(TAG_State_StanceSwitching);
	}

	Fighter->RefreshMovementControl();
	const float Duration = Fighter->GetDefinition() ? Fighter->GetDefinition()->StanceSwitchDuration : 0.25f;
	GetWorld()->GetTimerManager().SetTimer(SwitchTimerHandle, this,
		&UStanceSwitchAbility::HandleSwitchFinished, Duration, false);

	UE_LOG(LogTemp, Log, TEXT("[Stance] %s 开始切形态（%.2fs）"), *GetNameSafe(Fighter), Duration);
}

void UStanceSwitchAbility::HandleSwitchFinished()
{
	if (AFighterCharacter* Fighter = CachedFighter.Get())
	{
		if (UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(TAG_State_StanceSwitching);
		}
		UE_LOG(LogTemp, Log, TEXT("[Stance] %s 切换表现结束，当前形态=%s"), *GetNameSafe(Fighter),
			Fighter->GetStance() == EFighterStance::Melee ? TEXT("近战") : TEXT("远程"));
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UStanceSwitchAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwitchTimerHandle);
	}
	if (AFighterCharacter* Fighter = CachedFighter.Get())
	{
		if (UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(TAG_State_StanceSwitching);
		}
	}
	if (AFighterCharacter* F = CachedFighter.Get()) F->RefreshMovementControl();
	CachedFighter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
