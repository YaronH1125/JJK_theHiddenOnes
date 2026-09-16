// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/CombatInputComponent.h"

#include "AbilitySystemComponent.h"
#include "Training/AttackDefinition.h"
#include "Training/CombatHitComponent.h"
#include "Training/CombatTypes.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Training/TargetingComponent.h"

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

double UCombatInputComponent::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

namespace
{
	const TCHAR* ToString(EActionRequestResult Result)
	{
		switch (Result)
		{
		case EActionRequestResult::Cached: return TEXT("Cached");
		case EActionRequestResult::Executed: return TEXT("Executed");
		case EActionRequestResult::RejectedDead: return TEXT("RejectedDead");
		case EActionRequestResult::RejectedNotInitialized: return TEXT("RejectedNotInitialized");
		case EActionRequestResult::RejectedAlreadyActive: return TEXT("RejectedAlreadyActive");
		case EActionRequestResult::RejectedBlocked: return TEXT("RejectedBlocked");
		case EActionRequestResult::RejectedAbilityMissing: return TEXT("RejectedAbilityMissing");
		case EActionRequestResult::HeavyIntentRecorded: return TEXT("HeavyIntentRecorded");
		case EActionRequestResult::RejectedStaleRelease: return TEXT("RejectedStaleRelease");
		case EActionRequestResult::RejectedRangedStance: return TEXT("RejectedRangedStance");
		case EActionRequestResult::RejectedStanceSwitchBusy: return TEXT("RejectedStanceSwitchBusy");
		default: return TEXT("Unknown");
		}
	}
}

void UCombatInputComponent::NotifyAttackPressed()
{
	if (!bRequestsEnabled) return;
	if (bSessionActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 忽略重复左键按下（会话 %d 活跃）"),
			*GetNameSafe(GetOwner()), ActiveSessionId);
		return;
	}
	++SessionCounter;
	ActiveSessionId = SessionCounter;
	bSessionActive = true;
	PressGameTime = Now();
	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 建立左键会话 %d"), *GetNameSafe(GetOwner()), ActiveSessionId);
}

void UCombatInputComponent::NotifyAttackReleased()
{
	if (!bSessionActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 左键松开被拒：无活跃会话（旧松键不补攻击）"), *GetNameSafe(GetOwner()));
		OnRequestResult.Broadcast(EActionRequestResult::RejectedStaleRelease, 0);
		return;
	}
	const double Held = Now() - PressGameTime;
	const int32 SessionId = ActiveSessionId;
	bSessionActive = false;
	ActiveSessionId = 0;

	if (Held < LightHeavyThreshold)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 左键会话 %d 持续 %.3fs：轻拳路径"), *GetNameSafe(GetOwner()), SessionId, Held);
		SubmitLightAttack();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 左键会话 %d 持续 %.3fs：重拳意图"), *GetNameSafe(GetOwner()), SessionId, Held);
		SubmitHeavyPunch();
	}
}

void UCombatInputComponent::NotifyKickPressed()
{
	if (!bRequestsEnabled) return;
	if (bKickSessionActive)
	{
		return;
	}
	bKickSessionActive = true;
	KickPressGameTime = Now();
	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 建立 Q 会话"), *GetNameSafe(GetOwner()));
}

void UCombatInputComponent::NotifyKickReleased()
{
	if (!bKickSessionActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s Q 松开被拒：无会话"), *GetNameSafe(GetOwner()));
		return;
	}
	const double Held = Now() - KickPressGameTime;
	bKickSessionActive = false;

	if (Held < LightHeavyThreshold)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s Q 持续 %.3fs：腿击路径"), *GetNameSafe(GetOwner()), Held);
		SubmitKick();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s Q 持续 %.3fs：重踢意图"), *GetNameSafe(GetOwner()), Held);
		SubmitHeavyKick();
	}
}

void UCombatInputComponent::NotifyGuardPressed()
{
	if (!bRequestsEnabled) return;
	bGuardIntent = true;
	if (auto* F = GetOwnerFighter()) F->RefreshMovementControl();
	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 防御意图：按住"), *GetNameSafe(GetOwner()));
}

void UCombatInputComponent::NotifyGuardReleased()
{
	if (bGuardIntent)
	{
		bGuardIntent = false;
		if (auto* F = GetOwnerFighter()) F->RefreshMovementControl();
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 防御意图：松开（硬直/菜单中松开同样生效）"), *GetNameSafe(GetOwner()));
	}
}

void UCombatInputComponent::NotifyDodgePressed(FVector Direction)
{
	AFighterCharacter* Fighter = GetOwnerFighter();
	if (Fighter == nullptr)
	{
		return;
	}
	// 有移动输入沿输入方向；无输入默认后撤（由角色侧兜底）
	const FVector Dir = Direction.IsNearlyZero() ? Fighter->GetLastDodgeDirection() : Direction;
	Fighter->RequestDodge(Dir);
}

void UCombatInputComponent::NotifyStanceSwitchPressed()
{
	AFighterCharacter* Fighter = GetOwnerFighter();
	if (Fighter == nullptr)
	{
		return;
	}
	Fighter->RequestStanceSwitch();
}

void UCombatInputComponent::InvalidateSession(const FText& Reason)
{
	if (bSessionActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 会话 %d 失效：%s"), *GetNameSafe(GetOwner()), ActiveSessionId, *Reason.ToString());
	}
	if (bKickSessionActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s Q 会话失效：%s"), *GetNameSafe(GetOwner()), *Reason.ToString());
	}
	bSessionActive = false;
	ActiveSessionId = 0;
	bKickSessionActive = false;
	ConsumeCache();
}

void UCombatInputComponent::ReleaseContinuousInputs()
{
	if (bGuardIntent)
	{
		bGuardIntent = false;
		if (auto* F = GetOwnerFighter()) F->RefreshMovementControl();
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 持续防御意图被释放（菜单/失焦/重置）"), *GetNameSafe(GetOwner()));
	}
}

ECachedAction UCombatInputComponent::PeekCachedAction() const
{
	const AFighterCharacter* Fighter = GetOwnerFighter();
	const float Lifetime = Fighter && Fighter->GetDefinition() ? Fighter->GetDefinition()->ComboCacheLifetime : 0.5f;
	if (bCacheHadTarget && (!CachedTarget.IsValid() || CachedTarget->IsDead())) return ECachedAction::None;
	if (CachedAction == ECachedAction::None)
	{
		return ECachedAction::None;
	}
	if (Now() - CachedActionTime > Lifetime)
	{
		return ECachedAction::None; // 过期丢弃
	}
	return CachedAction;
}

void UCombatInputComponent::ConsumeCache()
{
	CachedAction = ECachedAction::None;
}

void UCombatInputComponent::CacheAction(ECachedAction Action)
{
	CachedAction = Action;
	CachedActionTime = Now();
	if (auto* F = GetOwnerFighter()) { CachedDirection = F->GetLastDodgeDirection(); CachedTarget = F->GetTargeting()->GetCurrentTarget(); bCacheHadTarget = CachedTarget.IsValid(); }
	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 缓存动作 %d（单槽覆盖）"), *GetNameSafe(GetOwner()), static_cast<int32>(Action));
}

bool UCombatInputComponent::TryStartSequence(ECachedAction Action)
{
	AFighterCharacter* Fighter = GetOwnerFighter();
	if (Fighter == nullptr)
	{
		return false;
	}
	return Fighter->RequestAttackSequence(Action);
}

EActionRequestResult UCombatInputComponent::SubmitLightAttack()
{
	AFighterCharacter* Fighter = GetOwnerFighter();
	if (Fighter == nullptr)
	{
		return EActionRequestResult::RejectedNotInitialized;
	}

	EActionRequestResult Result = Fighter->ValidateAttackRequest();
	if (Result != EActionRequestResult::Executed)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 轻拳请求：%s"), *GetNameSafe(GetOwner()), ToString(Result));
		OnRequestResult.Broadcast(Result, GetActiveSessionId());
		return Result;
	}

	if (Fighter->IsAttacking())
	{
		// 连击中：进单槽缓存，由活动 GA 在衔接窗口消费
		CacheAction(ECachedAction::NextSegment);
		Result = EActionRequestResult::Cached;
	}
	else
	{
		Result = TryStartSequence(ECachedAction::NextSegment) ? EActionRequestResult::Executed
															  : EActionRequestResult::RejectedAbilityMissing;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 轻拳请求结果：%s"), *GetNameSafe(GetOwner()), ToString(Result));
	OnRequestResult.Broadcast(Result, GetActiveSessionId());
	return Result;
}

EActionRequestResult UCombatInputComponent::SubmitKick()
{
	AFighterCharacter* Fighter = GetOwnerFighter();
	if (Fighter == nullptr)
	{
		return EActionRequestResult::RejectedNotInitialized;
	}

	EActionRequestResult Result = Fighter->ValidateAttackRequest();
	if (Result != EActionRequestResult::Executed)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 腿击请求：%s"), *GetNameSafe(GetOwner()), ToString(Result));
		OnRequestResult.Broadcast(Result, GetActiveSessionId());
		return Result;
	}

	if (Fighter->IsAttacking())
	{
		CacheAction(ECachedAction::Kick);
		Result = EActionRequestResult::Cached;
	}
	else
	{
		Result = TryStartSequence(ECachedAction::Kick) ? EActionRequestResult::Executed
													   : EActionRequestResult::RejectedAbilityMissing;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 腿击请求结果：%s"), *GetNameSafe(GetOwner()), ToString(Result));
	OnRequestResult.Broadcast(Result, GetActiveSessionId());
	return Result;
}

EActionRequestResult UCombatInputComponent::SubmitHeavyPunch()
{
	AFighterCharacter* Fighter = GetOwnerFighter();
	if (Fighter == nullptr)
	{
		return EActionRequestResult::RejectedNotInitialized;
	}

	EActionRequestResult Result = Fighter->ValidateAttackRequest();
	if (Result != EActionRequestResult::Executed)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 重拳请求：%s"), *GetNameSafe(GetOwner()), ToString(Result));
		OnRequestResult.Broadcast(Result, GetActiveSessionId());
		return Result;
	}

	if (Fighter->IsAttacking())
	{
		CacheAction(ECachedAction::HeavyPunch);
		Result = EActionRequestResult::Cached;
	}
	else
	{
		Result = TryStartSequence(ECachedAction::HeavyPunch) ? EActionRequestResult::Executed
															 : EActionRequestResult::RejectedAbilityMissing;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 重拳请求结果：%s"), *GetNameSafe(GetOwner()), ToString(Result));
	OnRequestResult.Broadcast(Result, GetActiveSessionId());
	return Result;
}

EActionRequestResult UCombatInputComponent::SubmitHeavyKick()
{
	AFighterCharacter* Fighter = GetOwnerFighter();
	if (Fighter == nullptr)
	{
		return EActionRequestResult::RejectedNotInitialized;
	}

	EActionRequestResult Result = Fighter->ValidateAttackRequest();
	if (Result != EActionRequestResult::Executed)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 重踢请求：%s"), *GetNameSafe(GetOwner()), ToString(Result));
		OnRequestResult.Broadcast(Result, GetActiveSessionId());
		return Result;
	}

	if (Fighter->IsAttacking())
	{
		CacheAction(ECachedAction::HeavyKick);
		Result = EActionRequestResult::Cached;
	}
	else
	{
		Result = TryStartSequence(ECachedAction::HeavyKick) ? EActionRequestResult::Executed
															: EActionRequestResult::RejectedAbilityMissing;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatInput] %s 重踢请求结果：%s"), *GetNameSafe(GetOwner()), ToString(Result));
	OnRequestResult.Broadcast(Result, GetActiveSessionId());
	return Result;
}

void UCombatInputComponent::SetRequestsEnabled(bool bEnabled)
{
 bRequestsEnabled = bEnabled;
 if (!bEnabled) { InvalidateSession(FText::FromString(TEXT("停止请求"))); ReleaseContinuousInputs(); }
}
