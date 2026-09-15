// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/CombatInputComponent.h"

#include "AbilitySystemComponent.h"
#include "Training/CombatTypes.h"
#include "Training/FighterCharacter.h"

UCombatInputComponent::UCombatInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatInputComponent::BeginPlay()
{
	Super::BeginPlay();
}

AFighterCharacter* UCombatInputComponent::GetOwnerFighter() const
{
	return Cast<AFighterCharacter>(GetOwner());
}

namespace
{
	const TCHAR* ToString(EActionRequestResult Result)
	{
		switch (Result)
		{
		case EActionRequestResult::Executed: return TEXT("Executed");
		case EActionRequestResult::RejectedDead: return TEXT("RejectedDead");
		case EActionRequestResult::RejectedNotInitialized: return TEXT("RejectedNotInitialized");
		case EActionRequestResult::RejectedAlreadyActive: return TEXT("RejectedAlreadyActive");
		case EActionRequestResult::RejectedBlocked: return TEXT("RejectedBlocked");
		case EActionRequestResult::RejectedAbilityMissing: return TEXT("RejectedAbilityMissing");
		case EActionRequestResult::HeavyIntentRecorded: return TEXT("HeavyIntentRecorded");
		case EActionRequestResult::RejectedStaleRelease: return TEXT("RejectedStaleRelease");
		default: return TEXT("Unknown");
		}
	}
}

void UCombatInputComponent::NotifyAttackPressed()
{
	if (bSessionActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 忽略重复按下（会话 %d 仍活跃）"),
			*GetNameSafe(GetOwner()), ActiveSessionId);
		return;
	}
	++SessionCounter;
	ActiveSessionId = SessionCounter;
	bSessionActive = true;
	PressGameTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 建立输入会话 %d"), *GetNameSafe(GetOwner()), ActiveSessionId);
}

void UCombatInputComponent::NotifyAttackReleased()
{
	if (!bSessionActive)
	{
		// 旧松键不补攻击（08 第 4.2 节）
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 松开被拒：无活跃会话（旧松键不补攻击）"), *GetNameSafe(GetOwner()));
		OnRequestResult.Broadcast(EActionRequestResult::RejectedStaleRelease, 0);
		return;
	}

	const double Held = GetWorld() ? GetWorld()->GetTimeSeconds() - PressGameTime : 0.0;
	const int32 SessionId = ActiveSessionId;
	bSessionActive = false;
	ActiveSessionId = 0;

	if (Held < LightHeavyThreshold)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 会话 %d 持续 %.3fs < 阈值 %.2fs：提交轻拳"),
			*GetNameSafe(GetOwner()), SessionId, Held, LightHeavyThreshold);
		SubmitLightAttack();
	}
	else
	{
		// M2 占位：只记录重拳意图，无实际动作与伤害；不补发轻拳
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 会话 %d 持续 %.3fs >= 阈值 %.2fs：记录 HeavyPunch 占位意图（M2 无动作/伤害）"),
			*GetNameSafe(GetOwner()), SessionId, Held, LightHeavyThreshold);
		OnRequestResult.Broadcast(EActionRequestResult::HeavyIntentRecorded, SessionId);
	}
}

void UCombatInputComponent::InvalidateSession(const FText& Reason)
{
	if (!bSessionActive)
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 会话 %d 失效：%s"), *GetNameSafe(GetOwner()), ActiveSessionId, *Reason.ToString());
	bSessionActive = false;
	ActiveSessionId = 0;
}

EActionRequestResult UCombatInputComponent::SubmitLightAttack()
{
	const AFighterCharacter* Fighter = GetOwnerFighter();
	if (Fighter == nullptr)
	{
		return EActionRequestResult::RejectedNotInitialized;
	}

	EActionRequestResult Result = EActionRequestResult::Executed;

	if (Fighter->IsDead())
	{
		Result = EActionRequestResult::RejectedDead;
	}
	else if (!Fighter->IsStatsInitialized() || Fighter->GetFighterAbilitySystemComponent() == nullptr)
	{
		Result = EActionRequestResult::RejectedNotInitialized;
	}
	else
	{
		const UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent();
		if (ASC->HasMatchingGameplayTag(TAG_State_HitStun))
		{
			Result = EActionRequestResult::RejectedBlocked;
		}
		else if (ASC->HasMatchingGameplayTag(TAG_State_Attacking))
		{
			Result = EActionRequestResult::RejectedAlreadyActive;
		}
		else
		{
			TSubclassOf<UGameplayAbility> AbilityClass = Fighter->GetMeleeAttackAbilityClass();
			UAbilitySystemComponent* MutableASC = Fighter->GetFighterAbilitySystemComponent();
			if (AbilityClass == nullptr || !MutableASC->TryActivateAbilityByClass(AbilityClass))
			{
				Result = AbilityClass == nullptr ? EActionRequestResult::RejectedAbilityMissing
												 : EActionRequestResult::RejectedAlreadyActive;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 攻击请求结果：%s"), *GetNameSafe(GetOwner()), ToString(Result));
	OnRequestResult.Broadcast(Result, GetActiveSessionId());
	return Result;
}
