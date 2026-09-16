// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/DodgeAbility.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Training/CombatInputComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Training/CombatTypes.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterDefinition.h"
#include "AbilitySystemComponent.h"
#include "Training/FighterCharacter.h"

UDodgeAbility::UDodgeAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_Dodge);
	SetAssetTags(Tags);

	// 恢复期内不可再次闪避；死亡/硬直/倒地禁止
	ActivationBlockedTags.AddTag(TAG_State_Dead);
	ActivationBlockedTags.AddTag(TAG_State_HitStun);
	ActivationBlockedTags.AddTag(TAG_State_KnockedDown);
	ActivationBlockedTags.AddTag(TAG_State_DodgeRecovery);
	ActivationBlockedTags.AddTag(TAG_State_StanceSwitching);
}

void UDodgeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	// 方向：移动输入方向；无输入默认后撤（08 第 4 章）
	const AFighterCharacter::FDodgeRequest Dodge = Fighter->ConsumePendingDodge();
	if (!Dodge.bPending || !Fighter->GetDefinition()) { EndAbility(Handle, ActorInfo, ActivationInfo, false, true); return; }
	FVector Dir = Dodge.Direction;
	Dir.Z = 0.f;
	if (Dir.IsNearlyZero())
	{
		Dir = -Fighter->GetActorForwardVector();
		Dir.Z = 0.f;
	}
	Dir = Dir.GetSafeNormal();

	const FDodgeConfig& Config = Fighter->GetDefinition()->DodgeConfig;

	// 无敌窗口
	Fighter->GetFighterAbilitySystemComponent()->AddLooseGameplayTag(TAG_State_DodgeInvulnerable);

 // 通过检查之后提交取消，清除旧松键和缓存。单一 RootMotionSource 持有位移。
 Fighter->GetCombatInput()->InvalidateSession(FText::FromString(TEXT("闪避提交")));
 if (Dodge.bCancel)
 {
  FGameplayTagContainer CancelTags; CancelTags.AddTag(TAG_Ability_MeleeAttack);
  Fighter->GetFighterAbilitySystemComponent()->CancelAbilities(&CancelTags);
 }
 Fighter->RefreshMovementControl();
 MoveTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
  this, TEXT("DodgeMove"), Dir, Config.DodgeSpeed, Config.InvulnerableDuration, false,
  nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, true);
 MoveTask->ReadyForActivation();

	GetWorld()->GetTimerManager().SetTimer(InvulnTimerHandle, this,
		&UDodgeAbility::HandleInvulnEnd, Config.InvulnerableDuration, false);

	UE_LOG(LogTemp, Log, TEXT("[Dodge] %s 闪避（方向 %s，无敌 %.2fs）"),
		*GetNameSafe(Fighter), *Dir.ToString(), Config.InvulnerableDuration);
}

void UDodgeAbility::HandleInvulnEnd()
{
	AFighterCharacter* Fighter = CachedFighter.Get();
	if (Fighter == nullptr)
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(TAG_State_DodgeInvulnerable);
		ASC->AddLooseGameplayTag(TAG_State_DodgeRecovery);
	}
	const float Recovery = Fighter->GetDefinition() ? Fighter->GetDefinition()->DodgeConfig.RecoveryDuration : 0.4f;
	GetWorld()->GetTimerManager().SetTimer(RecoveryTimerHandle, this,
		&UDodgeAbility::HandleRecoveryEnd, FMath::Max(Recovery, 0.01f), false);
}

void UDodgeAbility::HandleRecoveryEnd()
{
	if (MoveTask) { MoveTask->EndTask(); MoveTask = nullptr; }
	if (AFighterCharacter* Fighter = CachedFighter.Get())
	{
		if (UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(TAG_State_DodgeRecovery);
		}
		UE_LOG(LogTemp, Log, TEXT("[Dodge] %s 恢复结束，可再次行动"), *GetNameSafe(Fighter));
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UDodgeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InvulnTimerHandle);
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	if (MoveTask) { MoveTask->EndTask(); MoveTask = nullptr; }
	if (AFighterCharacter* Fighter = CachedFighter.Get())
	{
		if (UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(TAG_State_DodgeInvulnerable);
			ASC->RemoveLooseGameplayTag(TAG_State_DodgeRecovery);
		}
	}
	if (AFighterCharacter* F = CachedFighter.Get()) F->RefreshMovementControl();
	CachedFighter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
