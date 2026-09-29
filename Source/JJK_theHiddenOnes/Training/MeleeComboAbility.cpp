// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/MeleeComboAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Training/AttackDefinition.h"
#include "Training/CombatHitComponent.h"
#include "Training/CombatInputComponent.h"
#include "Training/CombatTypes.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"

UMeleeComboAbility::UMeleeComboAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_MeleeAttack);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(TAG_State_Dead);
	ActivationBlockedTags.AddTag(TAG_State_HitStun);
	ActivationBlockedTags.AddTag(TAG_State_KnockedDown);
	ActivationBlockedTags.AddTag(TAG_State_StanceSwitching);
}

AFighterCharacter* UMeleeComboAbility::GetFighter() const
{
	return Cast<AFighterCharacter>(GetAvatarActorFromActorInfo());
}

UAttackDefinition* UMeleeComboAbility::CurrentDef() const
{
	return SegmentSequence.IsValidIndex(SegmentIndex) ? SegmentSequence[SegmentIndex].Get() : nullptr;
}

bool UMeleeComboAbility::InstanceMatches() const
{
	const AFighterCharacter* Fighter = CachedFighter.Get();
	const UCombatHitComponent* Hit = Fighter ? Fighter->GetCombatHit() : nullptr;
	return Hit != nullptr && bInstanceActive && Hit->GetActiveInstanceId() == AttackInstanceId;
}

void UMeleeComboAbility::ClearTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindowOpenTimerHandle);
		World->GetTimerManager().ClearTimer(WindowCloseTimerHandle);
		World->GetTimerManager().ClearTimer(ComboWindowOpenTimerHandle);
		World->GetTimerManager().ClearTimer(ComboWindowCloseTimerHandle);
		World->GetTimerManager().ClearTimer(ComboCachePollTimerHandle);
		World->GetTimerManager().ClearTimer(ChargeTimerHandle);
	}
	bComboWindowOpen = false;
}

void UMeleeComboAbility::FinishMontageTask()
{
	if (IsValid(MontageTask))
	{
		MontageTask->OnCompleted.RemoveAll(this);
		MontageTask->OnInterrupted.RemoveAll(this);
		MontageTask->OnCancelled.RemoveAll(this);
		if (AFighterCharacter* F = CachedFighter.Get())
		{
			if (UAttackDefinition* Def = CurrentDef()) F->StopAnimMontage(Def->Montage.Get());
		}
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
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
	if (Fighter == nullptr || Fighter->GetDefinition() == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	CachedFighter = Fighter;

	// 消费共享入口设置的攻击序列（A1 链 / 重拳 / 腿击 / 重踢）
	if (!Fighter->ConsumePendingSequence(SegmentSequence, SegmentIndex))
	{
		const TArray<TObjectPtr<UAttackDefinition>>& Segments = Fighter->GetDefinition()->ComboSegments;
		if (Segments.Num() > 0)
		{
			for (const TObjectPtr<UAttackDefinition>& Seg : Segments)
			{
				SegmentSequence.Add(Seg);
			}
			SegmentIndex = 0;
		}
		else if (Fighter->GetDefinition()->AttackDefinition != nullptr)
		{
			SegmentSequence.Add(Fighter->GetDefinition()->AttackDefinition);
			SegmentIndex = 0;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[MeleeCombo] 无可用攻击配置"));
			EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
			return;
		}
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = Fighter->GetAbilitySystemComponent())
	{
		AbilitySystemComponent->AddLooseGameplayTag(TAG_State_Attacking);
		bAttackTagApplied = true;
	}

	const auto* Definition = Fighter->GetDefinition();
	SequenceAction = (CurrentDef() == Definition->KickDefinition || Definition->KickSegments.Contains(CurrentDef()))
		? ECachedAction::Kick : ECachedAction::NextSegment;
	AdvanceToSegment(SegmentIndex);
}

void UMeleeComboAbility::AdvanceToSegment(int32 Index)
{
	AFighterCharacter* Fighter = GetFighter();
	UAttackDefinition* Def = SegmentSequence.IsValidIndex(Index) ? SegmentSequence[Index].Get() : nullptr;
	if (Fighter == nullptr || Def == nullptr)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
		return;
	}

	// 段切换：清理上一段的窗口与轮询计时，重开攻击实例（每段独立命中记录）
	ClearTimers();
	FinishMontageTask();
	if (bInstanceActive)
	{
		Fighter->GetCombatHit()->EndAttack();
	}

	SegmentIndex = Index;
	AttackInstanceId = Fighter->GetCombatHit()->BeginAttack(Def);
	bInstanceActive = true;
	Fighter->GetCombatHit()->SetPhase(EAttackPhase::Windup);

	UAbilitySystemComponent* AbilitySystemComponent = Fighter->GetAbilitySystemComponent();
	if (bSuperArmorApplied) { AbilitySystemComponent->RemoveLooseGameplayTag(TAG_State_SuperArmor); bSuperArmorApplied = false; }
	Fighter->RefreshMovementControl();
	// 霸体（按段配置；动作期间生效，End 成对移除）
	if (Def->bGrantsSuperArmor && !bSuperArmorApplied && AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(TAG_State_SuperArmor);
		bSuperArmorApplied = true;
	}

	UAnimMontage* Montage = Def->Montage.LoadSynchronous();
	if (Montage == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MeleeCombo] %s 段 %d Montage 加载失败"), *GetNameSafe(Fighter), Index);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
		return;
	}

	ChargeAction = Def == Fighter->GetDefinition()->HeavyPunchDefinition ? ECachedAction::HeavyPunch
		: Def == Fighter->GetDefinition()->HeavyKickDefinition ? ECachedAction::HeavyKick : ECachedAction::None;
	bCharging = Def->ChargeHoldTime > 0.f && Fighter->GetCombatInput()->IsHoldingMeleeCharge(ChargeAction);
	if (bCharging)
	{
		Fighter->GetCombatInput()->RegisterMeleeCharge(this);
		GetWorld()->GetTimerManager().SetTimer(ChargeTimerHandle, this, &UMeleeComboAbility::TickHeldCharge, 0.01f, true);
	}
	else StartAttackWindows();

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, TEXT("MeleeAttack"), Montage, 1.f, NAME_None, true);
	MontageTask->OnCompleted.AddUniqueDynamic(this, &UMeleeComboAbility::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddUniqueDynamic(this, &UMeleeComboAbility::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddUniqueDynamic(this, &UMeleeComboAbility::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UMeleeComboAbility::StartAttackWindows(float MontageOffset)
{
	AFighterCharacter* Fighter = GetFighter();
	UAttackDefinition* Def = CurrentDef();
	if (!Fighter || !Def) return;
	// All timers use gameplay seconds after release, not wall time spent holding.
	TWeakObjectPtr<UCombatHitComponent> WeakHit = Fighter->GetCombatHit();
	const uint64 InstanceId = AttackInstanceId;
	TWeakObjectPtr<UMeleeComboAbility> WeakAbility = this;
	auto WindowLambda = [WeakHit, WeakAbility, InstanceId](bool bOpen)
	{
		if (WeakAbility.IsValid() && WeakHit.IsValid()
			&& WeakHit->GetActiveInstanceId() == InstanceId && WeakHit->HasActiveAttack())
		{
			WeakHit->HandleAnimWindowNotify(bOpen, nullptr);
		}
	};
	const float Start = FMath::Max(Def->WindowStartTime - MontageOffset, 0.001f);
	const float End = FMath::Max(Def->WindowEndTime - MontageOffset, Start + 0.01f);
	Fighter->ApplyMeleeMagnetism(Start);
	if (!Def->bWindowFromAnimNotifies)
	{
	GetWorld()->GetTimerManager().SetTimer(WindowOpenTimerHandle,
		[WindowLambda]() { WindowLambda(true); }, Start, false);
	GetWorld()->GetTimerManager().SetTimer(WindowCloseTimerHandle,
		[WindowLambda]() { WindowLambda(false); }, End, false);
	}

	// 连击衔接窗口：轮询输入缓存（缓存消费点）
	const bool bHasNext = SegmentIndex + 1 < SegmentSequence.Num();
	const bool bAnyTransition = Def->bAllowNextSegment && bHasNext
		|| Def->bAllowHeavyTransition || Def->bAllowKickTransition;
	if (Def->ComboWindowEndTime > Def->ComboWindowStartTime && bAnyTransition)
	{
		const float ComboStart = FMath::Max(Def->ComboWindowStartTime - MontageOffset, 0.001f);
		const float ComboEnd = FMath::Max(Def->ComboWindowEndTime - MontageOffset, ComboStart + 0.05f);
		GetWorld()->GetTimerManager().SetTimer(ComboWindowOpenTimerHandle,
			[WeakAbility, InstanceId]() { if (WeakAbility.IsValid() && WeakAbility->AttackInstanceId == InstanceId) WeakAbility->HandleWindowOpen(); }, ComboStart, false);
		GetWorld()->GetTimerManager().SetTimer(ComboWindowCloseTimerHandle,
			[WeakAbility, InstanceId]() { if (WeakAbility.IsValid() && WeakAbility->AttackInstanceId == InstanceId) WeakAbility->HandleWindowClose(); }, ComboEnd, false);
		GetWorld()->GetTimerManager().SetTimer(ComboCachePollTimerHandle,
			[WeakAbility]() { if (WeakAbility.IsValid()) WeakAbility->HandleCachePoll(); }, 0.016f, true);
	}

}

void UMeleeComboAbility::TickHeldCharge()
{
	AFighterCharacter* Fighter = GetFighter();
	UAttackDefinition* Def = CurrentDef();
	UAnimInstance* Anim = Fighter && Fighter->GetMesh() ? Fighter->GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = Def ? Def->Montage.Get() : nullptr;
	if (!bCharging || !Anim || !Montage) { CancelHeldCharge(); return; }
	if (Anim->Montage_GetPosition(Montage) + KINDA_SMALL_NUMBER < Def->ChargeHoldTime) return;
	Anim->Montage_SetPosition(Montage, Def->ChargeHoldTime);
	if (Fighter->GetCombatInput()->IsHoldingMeleeCharge(ChargeAction))
	{
		Anim->Montage_Pause(Montage);
		return;
	}
	bCharging = false;
	Fighter->GetCombatInput()->ClearMeleeCharge(this);
	GetWorld()->GetTimerManager().ClearTimer(ChargeTimerHandle);
	// Reset the hit instance clock so cancellation and hit timing cannot include the hold.
	Fighter->GetCombatHit()->EndAttack();
	AttackInstanceId = Fighter->GetCombatHit()->BeginAttack(Def, Def->ChargeHoldTime);
	Fighter->GetCombatHit()->SetPhase(EAttackPhase::Windup);
	Anim->Montage_Resume(Montage);
	StartAttackWindows(Def->ChargeHoldTime);
}

void UMeleeComboAbility::CancelHeldCharge()
{
	if (IsActive() && bCharging) CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UMeleeComboAbility::HandleCachePoll()
{
	if (!bComboWindowOpen) return;
	AFighterCharacter* Fighter = GetFighter();
	UAttackDefinition* Def = CurrentDef();
	UCombatInputComponent* Input = Fighter ? Fighter->GetCombatInput() : nullptr;
	UFighterDefinition* FighterDef = Fighter ? Fighter->GetDefinition() : nullptr;
	if (Fighter == nullptr || Def == nullptr || Input == nullptr || FighterDef == nullptr)
	{
		return;
	}
	if (Fighter->ValidateAttackRequest() != EActionRequestResult::Executed) { Input->ConsumeCache(true); return; }
	if (!InstanceMatches())
	{
		return;
	}

	const ECachedAction Cached = Input->PeekCachedAction();
	switch (Cached)
	{
	case ECachedAction::NextSegment:
		if (SequenceAction == ECachedAction::NextSegment && Def->bAllowNextSegment && SegmentIndex + 1 < SegmentSequence.Num())
		{
			Input->ConsumeCache(true);
			AdvanceToSegment(SegmentIndex + 1);
		}
		break;
	case ECachedAction::HeavyPunch:
		if (Def->bAllowHeavyTransition && FighterDef->HeavyPunchDefinition != nullptr)
		{
			Input->ConsumeCache(true);
			FinishMontageTask();
			SegmentSequence = {TObjectPtr<UAttackDefinition>(FighterDef->HeavyPunchDefinition)};
			SequenceAction = ECachedAction::HeavyPunch;
			AdvanceToSegment(0);
		}
		break;
	case ECachedAction::Kick:
		if (SequenceAction == ECachedAction::Kick && Def->bAllowNextSegment && SegmentIndex + 1 < SegmentSequence.Num())
		{
			Input->ConsumeCache(true);
			AdvanceToSegment(SegmentIndex + 1);
		}
		else if (SequenceAction != ECachedAction::Kick && Def->bAllowKickTransition && FighterDef->KickDefinition != nullptr)
		{
			Input->ConsumeCache(true);
			FinishMontageTask();
			SegmentSequence = FighterDef->KickSegments;
			if (SegmentSequence.IsEmpty()) SegmentSequence.Add(FighterDef->KickDefinition);
			SequenceAction = ECachedAction::Kick;
			AdvanceToSegment(0);
		}
		break;
	case ECachedAction::HeavyKick:
		if (Def->bAllowKickTransition && FighterDef->HeavyKickDefinition != nullptr)
		{
			Input->ConsumeCache(true);
			FinishMontageTask();
			SegmentSequence = {TObjectPtr<UAttackDefinition>(FighterDef->HeavyKickDefinition)};
			SequenceAction = ECachedAction::HeavyKick;
			AdvanceToSegment(0);
		}
		break;
	default:
		break;
	}
}

void UMeleeComboAbility::HandleWindowOpen()
{
 bComboWindowOpen = true;
 HandleCachePoll();
}
void UMeleeComboAbility::HandleWindowClose()
{
 const uint64 OldInstance = AttackInstanceId;
 HandleCachePoll();
 if (OldInstance == AttackInstanceId) bComboWindowOpen = false;
}

void UMeleeComboAbility::HandleMontageCompleted()
{
	if (AFighterCharacter* Fighter = CachedFighter.Get())
	{
		UE_LOG(LogTemp, Log, TEXT("[MeleeCombo] %s 段 %d Montage 自然完成"), *GetNameSafe(Fighter), SegmentIndex);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UMeleeComboAbility::HandleMontageInterrupted()
{
	if (AFighterCharacter* Fighter = CachedFighter.Get())
	{
		UE_LOG(LogTemp, Log, TEXT("[MeleeCombo] %s 段 %d Montage 被打断/取消"), *GetNameSafe(Fighter), SegmentIndex);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UMeleeComboAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 所有退出路径统一清理（M3.1：段间切换不残留上一段的检测记录）
	ClearTimers();
	FinishMontageTask();

	if (AFighterCharacter* Fighter = CachedFighter.Get())
	{
		Fighter->GetCombatInput()->ClearMeleeCharge(this);
		Fighter->ClearMeleeMagnetism();
		if (bCharging) Fighter->GetCombatInput()->InvalidateSession(FText::FromString(TEXT("Melee charge ended")));
		bCharging = false;
		if (bInstanceActive && Fighter->GetCombatHit()->GetActiveInstanceId() == AttackInstanceId)
		{
			Fighter->GetCombatHit()->EndAttack();
		}
		Fighter->GetCombatInput()->ConsumeCache();
		if (bAttackTagApplied)
		{
			if (UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent())
			{
				ASC->RemoveLooseGameplayTag(TAG_State_Attacking);
			}
			bAttackTagApplied = false;
		}
		if (bSuperArmorApplied)
		{
			if (UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent())
			{
				ASC->RemoveLooseGameplayTag(TAG_State_SuperArmor);
			}
			bSuperArmorApplied = false;
		}
	}

	TWeakObjectPtr<AFighterCharacter> FinishedFighter = CachedFighter;
	const bool bHadSequence = bInstanceActive;
	if (AFighterCharacter* F = CachedFighter.Get()) F->RefreshMovementControl();
	bInstanceActive = false;
	SegmentSequence.Reset();
	SegmentIndex = INDEX_NONE;
	CachedFighter = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	if (bHadSequence && FinishedFighter.IsValid()) FinishedFighter->OnComboEnded.Broadcast();
}
