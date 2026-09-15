// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/MeleeComboAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimSequenceBase.h"
#include "Training/AttackDefinition.h"
#include "Training/CombatHitComponent.h"
#include "Training/CombatTypes.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"

UMeleeComboAbility::UMeleeComboAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 单实例：已有活动攻击时再次激活在能力层即被拒绝（T04）
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_MeleeAttack);
	SetAssetTags(Tags);

	// 死亡/受击硬直禁止出招（能力层兜底；请求入口已先行检查）
	ActivationBlockedTags.AddTag(TAG_State_Dead);
	ActivationBlockedTags.AddTag(TAG_State_HitStun);
}

AFighterCharacter* UMeleeComboAbility::GetFighter() const
{
	return Cast<AFighterCharacter>(GetAvatarActorFromActorInfo());
}

void UMeleeComboAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	AFighterCharacter* Fighter = GetFighter();
	const UAttackDefinition* Definition = Fighter && Fighter->GetDefinition() ? Fighter->GetDefinition()->AttackDefinition : nullptr;
	if (Fighter == nullptr || Definition == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MeleeCombo] 激活失败：角色或 AttackDefinition 缺失"));
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	CachedFighter = Fighter;
	ActiveDefinition = Definition;
	AttackInstanceId = Fighter->GetCombatHit()->BeginAttack(Definition);
	bInstanceActive = true;
	Fighter->GetCombatHit()->SetPhase(EAttackPhase::Windup);

	// State.Attacking：唯一管理者为 ASC 标签（02_架构设计.md 第 4 节）；与 EndAbility 成对清理
	Fighter->GetFighterAbilitySystemComponent()->AddLooseGameTag(TAG_State_Attacking);
	bAttackTagApplied = true;

	UAnimMontage* Montage = Definition->Montage.LoadSynchronous();
	if (Montage == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MeleeCombo] Montage 未配置或加载失败，攻击结束"));
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 窗口来源：动画通知（默认）或配置时间回退（显式记录的偏差路径）
	if (Definition->bWindowFromAnimNotifies)
	{
		UE_LOG(LogTemp, Log, TEXT("[MeleeCombo] %s 实例 %llu 窗口由动画通知驱动"), *GetNameSafe(Fighter), AttackInstanceId);
	}
	else
	{
		TWeakObjectPtr<UCombatHitComponent> WeakHit = Fighter->GetCombatHit();
		const uint64 InstanceId = AttackInstanceId;
		const float Start = FMath::Max(Definition->WindowStartTime, 0.f);
		const float End = FMath::Max(Definition->WindowEndTime, Start + 0.01f);
		TWeakObjectPtr<UMeleeComboAbility> WeakAbility = this;
		auto MakeWindowLambda = [WeakHit, WeakAbility, InstanceId](bool bOpen)
		{
			if (WeakHit.IsValid() && WeakAbility.IsValid()
				&& WeakHit->GetActiveInstanceId() == InstanceId && WeakHit->HasActiveAttack())
			{
				WeakHit->HandleAnimWindowNotify(bOpen, nullptr);
			}
		};
		GetWorld()->GetTimerManager().SetTimer(WindowOpenTimerHandle,
			[MakeWindowLambda]() { MakeWindowLambda(true); }, Start, false);
		GetWorld()->GetTimerManager().SetTimer(WindowCloseTimerHandle,
			[MakeWindowLambda]() { MakeWindowLambda(false); }, End, false);
	}

	FinishMontageTask();
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, TEXT("MeleeAttack"), Montage, 1.f, NAME_None, true);
	MontageTask->OnCompleted.AddUniqueDynamic(this, &UMeleeComboAbility::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddUniqueDynamic(this, &UMeleeComboAbility::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddUniqueDynamic(this, &UMeleeComboAbility::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();

	UE_LOG(LogTemp, Log, TEXT("[MeleeCombo] %s 攻击激活（实例 %llu，Montage %s）"),
		*GetNameSafe(Fighter), AttackInstanceId, *GetNameSafe(Montage));
}

void UMeleeComboAbility::FinishMontageTask()
{
	if (IsValid(MontageTask))
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
}

void UMeleeComboAbility::HandleMontageCompleted()
{
	if (AFighterCharacter* Fighter = CachedFighter.Get())
	{
		UE_LOG(LogTemp, Log, TEXT("[MeleeCombo] %s 实例 %llu Montage 自然完成"), *GetNameSafe(Fighter), AttackInstanceId);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UMeleeComboAbility::HandleMontageInterrupted()
{
	if (AFighterCharacter* Fighter = CachedFighter.Get())
	{
		UE_LOG(LogTemp, Log, TEXT("[MeleeCombo] %s 实例 %llu Montage 被打断/取消"), *GetNameSafe(Fighter), AttackInstanceId);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UMeleeComboAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 所有退出路径统一清理：通知丢失/播放失败也不依赖最后的通知恢复角色（M2.2）
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindowOpenTimerHandle);
		World->GetTimerManager().ClearTimer(WindowCloseTimerHandle);
	}

	if (AFighterCharacter* Fighter = CachedFighter.Get())
	{
		if (bInstanceActive && Fighter->GetCombatHit()->GetActiveInstanceId() == AttackInstanceId)
		{
			Fighter->GetCombatHit()->EndAttack();
		}
		if (bAttackTagApplied)
		{
			if (UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent())
			{
				ASC->RemoveLooseGameTag(TAG_State_Attacking);
			}
			bAttackTagApplied = false;
		}
	}

	bInstanceActive = false;
	FinishMontageTask();
	CachedFighter = nullptr;
	ActiveDefinition = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
