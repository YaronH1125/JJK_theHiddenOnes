// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/ChargedBlastAbility.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Training/CombatTypes.h"
#include "Training/DamageGameplayEffect.h"
#include "Training/TargetingComponent.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"

namespace
{
	float BlastDebugEnabled()
	{
		if (const auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("JJK.DebugHitFX")))
			return CVar->GetFloat();
		return 0.f;
	}
}

UChargedBlastAbilityBase::UChargedBlastAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_Blast);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(TAG_State_Dead);
	ActivationBlockedTags.AddTag(TAG_State_HitStun);
	ActivationBlockedTags.AddTag(TAG_State_KnockedDown);
	ActivationBlockedTags.AddTag(TAG_State_StanceSwitching);
	ActivationBlockedTags.AddTag(TAG_State_GuardStun);
}

AFighterCharacter* UChargedBlastAbilityBase::GetFighter() const
{
	return Cast<AFighterCharacter>(GetAvatarActorFromActorInfo());
}

bool UChargedBlastAbilityBase::PassesActivationChecks(FString& OutReason) const
{
	const auto* Fighter = GetFighter();
	if (!Fighter || !Fighter->GetDefinition())
	{
		OutReason = TEXT("角色/定义缺失");
		return false;
	}
	if (Fighter->IsDead())
	{
		OutReason = TEXT("已死亡");
		return false;
	}
	if (Fighter->GetStance() != EFighterStance::Ranged)
	{
		OutReason = TEXT("非远程形态");
		return false;
	}
	if (Fighter->IsBlastCharging())
	{
		OutReason = TEXT("已有持炮会话");
		return false;
	}
	return true;
}

void UChargedBlastAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	auto* Fighter = GetFighter();
	FString Reason;
	if (!PassesActivationChecks(Reason))
	{
		UE_LOG(LogTemp, Log, TEXT("[Blast] %s 激活被拒：%s"), *GetNameSafe(Fighter), *Reason);
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	CachedFighter = Fighter;

	if (GetCooldown() > 0.f && Fighter->HasCombatTag(TAG_State_SuperBlastCooldown))
	{
		UE_LOG(LogTemp, Log, TEXT("[Blast] %s 冷却中"), *GetNameSafe(Fighter));
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	if (!Fighter->ModifyCursedEnergy(-GetMinCost()))
	{
		UE_LOG(LogTemp, Log, TEXT("[Blast] %s 咒力不足以支付基础成本 %.0f"), *GetNameSafe(Fighter), GetMinCost());
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	PaidCost = GetMinCost();
	PaidQ = 0.f;
	ChargeStartTime = Now();

	ApplyChargeStateTags(Fighter->GetFighterAbilitySystemComponent());

	StoredBaseMoveSpeed = Fighter->GetCharacterMovement()->MaxWalkSpeed;
	ApplyMoveSpeedScale(GetMoveSpeedScale());
	Phase = EBlastPhase::Charging;
	Fighter->RegisterActiveBlast(this);
	StartChargeTick();

	UE_LOG(LogTemp, Log, TEXT("[Blast] %s 开始蓄力（成本 %.0f）"), *GetNameSafe(Fighter), GetMinCost());
}

float UChargedBlastAbilityBase::ComputeChargeQ(float ElapsedSeconds) const
{
	if (!HasMinChargeGate())
	{
		const float Cap = GetChargeCapTime();
		return Cap > KINDA_SMALL_NUMBER ? FMath::Clamp(ElapsedSeconds / Cap, 0.f, 1.f) : 1.f;
	}
	// 超级炮：最低门槛前进度为 0（只保持基础成本），之后按剩余时长增长
	const float MinT = GetMinChargeTime();
	const float Span = GetChargeCapTime() - MinT;
	if (ElapsedSeconds <= MinT) return 0.f;
	return Span > KINDA_SMALL_NUMBER ? FMath::Clamp((ElapsedSeconds - MinT) / Span, 0.f, 1.f) : 1.f;
}

void UChargedBlastAbilityBase::StartChargeTick()
{
	GetWorld()->GetTimerManager().SetTimer(ChargeTickHandle, this,
		&UChargedBlastAbilityBase::HandleChargeTick, 0.05f, true);
}

void UChargedBlastAbilityBase::HandleChargeTick()
{
	auto* Fighter = CachedFighter.Get();
	if (!Fighter || Phase != EBlastPhase::Charging) return;

	if (LocksMovementWhileCharging())
		Fighter->GetCharacterMovement()->StopMovementImmediately();

	UpdateChargeProgress();
}

void UChargedBlastAbilityBase::UpdateChargeProgress()
{
	auto* Fighter = CachedFighter.Get();
	if (!Fighter || Phase != EBlastPhase::Charging) return;

	const double Elapsed = Now() - ChargeStartTime;
	const float q = ComputeChargeQ(static_cast<float>(Elapsed));

	// 只支付累计目标成本与已支付成本的正差；资源耗尽时余额计入 PaidCost（强度与实际支付一致）
	const float TargetCost = GetMinCost() + (GetMaxCost() - GetMinCost()) * q;
	const float Increment = TargetCost - PaidCost;
	const float CostSpan = GetMaxCost() - GetMinCost();
	if (Increment > 0.01f)
	{
		const float Curse = Fighter->GetCursedEnergy();
		if (Curse >= Increment)
		{
			Fighter->ModifyCursedEnergy(-Increment);
			PaidCost = TargetCost;
		}
		else
		{
			if (Curse > 0.f)
			{
				Fighter->ModifyCursedEnergy(-Curse);
				PaidCost += Curse; // 余额全部计入累计成本，不吞掉
			}
			PaidQ = CostSpan > 1.f ? FMath::Clamp((PaidCost - GetMinCost()) / CostSpan, 0.f, 1.f) : 0.f;
			return;
		}
	}
	PaidQ = CostSpan > 1.f ? FMath::Clamp((PaidCost - GetMinCost()) / CostSpan, 0.f, 1.f) : 1.f;
}

void UChargedBlastAbilityBase::NotifyExternalRelease()
{
	if (Phase != EBlastPhase::Charging) return;
	bReleaseSignaled = true;
	HandleExternalRelease();
}

void UChargedBlastAbilityBase::HandleExternalRelease()
{
	const double Elapsed = Now() - ChargeStartTime;
	if (HasMinChargeGate() && Elapsed < GetMinChargeTime())
	{
		AbortBlast(TEXT("低于最低蓄力门槛"), true);
		return;
	}
	// 方向解耦：发射方向在松开瞬间固定，前摇不继续瞬时追随
	LockedAimPoint = ResolveAimPoint();
	bAimPointCaptured = true;
	// 松开瞬间补齐最后一段成本与强度（帧针脱落时保证成本=强度口径一致）
	UpdateChargeProgress();
	UE_LOG(LogTemp, Log, TEXT("[BlastDebug] %s 松开：Elapsed=%.3f PaidCost=%.3f PaidQ=%.3f"),
		*GetNameSafe(CachedFighter.Get()), Now() - ChargeStartTime, PaidCost, PaidQ);
	StartWindup();
}

void UChargedBlastAbilityBase::StartWindup()
{
	ClearTimers();
	Phase = EBlastPhase::Windup;
	ApplyMoveSpeedScale(IsStationaryBlast() ? 0.f : 0.2f);
	GetWorld()->GetTimerManager().SetTimer(PhaseTimerHandle, this,
		&UChargedBlastAbilityBase::HandleWindupDone, FMath::Max(GetLockWindup(), 0.01f), false);
}

void UChargedBlastAbilityBase::HandleWindupDone()
{
	if (Phase != EBlastPhase::Windup) return;
	UE_LOG(LogTemp, Log, TEXT("[BlastDebug] %s 发射：Elapsed=%.3f PaidCost=%.3f PaidQ=%.3f"),
		*GetNameSafe(CachedFighter.Get()), Now() - ChargeStartTime, PaidCost, PaidQ);
	FireOnce();
	if (HasMinChargeGate()) bSuperBlastFired = true;
	Phase = EBlastPhase::Recovery;
	ApplyMoveSpeedScale(1.f);
	GetWorld()->GetTimerManager().SetTimer(PhaseTimerHandle, this,
		&UChargedBlastAbilityBase::HandleRecoveryDone, FMath::Max(GetRecoveryTime(), 0.01f), false);
}

void UChargedBlastAbilityBase::HandleRecoveryDone()
{
	if (CachedFighter.IsValid())
		ClearChargeStateTags(CachedFighter->GetFighterAbilitySystemComponent());
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UChargedBlastAbilityBase::CancelFromOutside()
{
	if (Phase == EBlastPhase::Charging || Phase == EBlastPhase::Windup)
	{
		AbortBlast(TEXT("切形态中止持炮"), true);
	}
}

void UChargedBlastAbilityBase::AbortBlast(const TCHAR* Reason, bool bPaidInterrupt)
{
	UE_LOG(LogTemp, Log, TEXT("[Blast] %s 收招：%s"), *GetNameSafe(CachedFighter.Get()), Reason);
	if (bPaidInterrupt) ApplySuperBlastCooldown();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UChargedBlastAbilityBase::ApplyChargeStateTags(class UFighterAbilitySystemComponent* ASC)
{
	if (ASC) ASC->AddLooseGameplayTag(TAG_State_BlastCharging);
}

void UChargedBlastAbilityBase::ClearChargeStateTags(class UFighterAbilitySystemComponent* ASC)
{
	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(TAG_State_BlastCharging);
		ASC->RemoveLooseGameplayTag(TAG_State_RangedBlastCharging);
	}
}

void UChargedBlastAbilityBase::ApplySuperBlastCooldown()
{
	if (GetCooldown() <= 0.f) return;
	auto* Fighter = CachedFighter.Get();
	auto* ASC = Fighter ? Fighter->GetFighterAbilitySystemComponent() : nullptr;
	if (!ASC || !GetWorld()) return;
	// 以 ASC 实际标签为准（训练重置会摘标签并清实例残留状态）；已在冷却不刷新
	if (bCooldownTagApplied && !ASC->HasMatchingGameplayTag(TAG_State_SuperBlastCooldown))
		bCooldownTagApplied = false;
	if (bCooldownTagApplied) return;
	bCooldownTagApplied = true;
	ASC->AddLooseGameplayTag(TAG_State_SuperBlastCooldown);
	GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, this,
		&UChargedBlastAbilityBase::HandleCooldownDone, GetCooldown(), false);
	UE_LOG(LogTemp, Log, TEXT("[Blast] %s 超级炮冷却 %.0fs"), *GetNameSafe(Fighter), GetCooldown());
}

void UChargedBlastAbilityBase::HandleCooldownDone()
{
	if (auto* Fighter = CachedFighter.Get())
	{
		if (auto* ASC = Fighter->GetFighterAbilitySystemComponent())
			ASC->RemoveLooseGameplayTag(TAG_State_SuperBlastCooldown);
	}
	bCooldownTagApplied = false;
}

void UChargedBlastAbilityBase::ApplyMoveSpeedScale(float Scale)
{
	auto* Fighter = CachedFighter.Get();
	if (!Fighter || !Fighter->GetCharacterMovement()) return;
	if (!bMoveSpeedModified)
	{
		StoredBaseMoveSpeed = Fighter->GetCharacterMovement()->MaxWalkSpeed;
		bMoveSpeedModified = true;
	}
	Fighter->GetCharacterMovement()->MaxWalkSpeed = StoredBaseMoveSpeed * FMath::Clamp(Scale, 0.f, 1.f);
}

void UChargedBlastAbilityBase::RestoreMoveSpeed()
{
	auto* Fighter = CachedFighter.Get();
	if (Fighter && Fighter->GetCharacterMovement() && bMoveSpeedModified)
	{
		Fighter->GetCharacterMovement()->MaxWalkSpeed = StoredBaseMoveSpeed;
		bMoveSpeedModified = false;
	}
}

void UChargedBlastAbilityBase::FireOnce()
{
	auto* Fighter = CachedFighter.Get();
	if (!Fighter) return;

	const float Damage = GetMinDamage() + (GetMaxDamage() - GetMinDamage()) * PaidQ;
	const float Range = GetRange();

	FVector Muzzle = Fighter->GetActorLocation() + FVector(0, 0, 60);
	if (auto* Mesh = Fighter->GetMesh())
	{
		if (Mesh->DoesSocketExist(Fighter->GetMuzzleSocketName()))
			Muzzle = Mesh->GetSocketLocation(Fighter->GetMuzzleSocketName());
	}

	// 方向解耦（A02）：松开瞬间已捕获瞄准点；AI 非 held 路径实时解析
	FVector AimPoint = bAimPointCaptured ? LockedAimPoint : ResolveAimPoint();
	AimPoint = Muzzle + (AimPoint - Muzzle).GetSafeNormal() * FMath::Min(Range, FVector::Dist(Muzzle, AimPoint) + 1.f);

	FCollisionQueryParams QP(SCENE_QUERY_STAT(JJKBlast));
	QP.AddIgnoredActor(Fighter);

	// 全路径遮挡（A02）：世界阻挡（Visibility，胶囊不受影响）与 Pawn 命中（对象类型扫掠）取更近者
	FHitResult WallHit;
	const bool bWall = GetWorld()->LineTraceSingleByChannel(WallHit, Muzzle, AimPoint, ECC_Visibility, QP)
		&& !Cast<AFighterCharacter>(WallHit.GetActor());
	const float WallDist = bWall ? static_cast<float>(FVector::Dist(Muzzle, WallHit.ImpactPoint)) : TNumericLimits<float>::Max();

	// 炮口嵌墙安全拒绝：墙在炮口 30cm 内直接拒绝发射
	if (bWall && WallDist < 30.f)
	{
		UE_LOG(LogTemp, Log, TEXT("[Blast] %s 炮口嵌墙安全拒绝"), *GetNameSafe(Fighter));
		return;
	}

	FHitResult FireHit;
	AFighterCharacter* HitFighter = nullptr;
	FVector EndPoint = AimPoint;
	{
		FCollisionObjectQueryParams ObjectParams(ECC_Pawn);
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(15.f);
		if (GetWorld()->SweepSingleByObjectType(FireHit, Muzzle, AimPoint, FQuat::Identity, ObjectParams, Sphere, QP))
		{
			const float PawnDist = static_cast<float>(FVector::Dist(Muzzle, FireHit.ImpactPoint));
			if (!bWall || PawnDist < WallDist)
			{
				EndPoint = FireHit.ImpactPoint;
				HitFighter = Cast<AFighterCharacter>(FireHit.GetActor());
			}
			else
			{
				EndPoint = WallHit.ImpactPoint; // 中间墙体截断：命中无效
			}
		}
		else if (bWall)
		{
			EndPoint = WallHit.ImpactPoint;
		}
	}

	if (BlastDebugEnabled() > 0.f && GetWorld())
		DrawDebugLine(GetWorld(), Muzzle, EndPoint, FColor::Cyan, false, 2.f, 0, 2.f);

	// 共享攻防结算（A02）：闪避免疫/正面防御/命中与近战同口径，特效终点与结算一致
	if (HitFighter)
	{
		FRangedHitSettle Settle;
		Settle.Damage = Damage;
		Settle.bDodgeable = true;
		Settle.bBlockable = true;
		Settle.bGrantCurse = true;
		Settle.GuardStunDuration = GetGuardStunDuration();
		Settle.HitStunDuration = GetHitStunDuration();
		Settle.InterruptLevel = GetInterruptLevel();
		Settle.KnockbackStrength = GetKnockbackStrength();
		Settle.AttackInstanceId = (static_cast<uint64>(GetUniqueID()) << 20) | (++ShotCounter);
		const ETrainingContact Result = Fighter->SettleRangedHitOn(HitFighter, Settle);
		UE_LOG(LogTemp, Log, TEXT("[Blast] %s 炮击命中 %s（伤害 %.0f q=%.2f 结果=%d）"),
			*GetNameSafe(Fighter), *GetNameSafe(HitFighter), Damage, PaidQ, static_cast<int32>(Result));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[Blast] %s 炮击空放"), *GetNameSafe(Fighter));
	}
}

FVector UChargedBlastAbilityBase::ResolveAimPoint() const
{
	auto* Fighter = CachedFighter.Get();
	if (!Fighter) return FVector::ZeroVector;

	FVector Muzzle = Fighter->GetActorLocation() + FVector(0, 0, 60);
	FVector CamLoc = Muzzle;
	FRotator CamRot = Fighter->GetActorRotation();
	FVector AimPoint = Muzzle + Fighter->GetActorForwardVector() * GetRange();
	if (auto* PC = Cast<APlayerController>(Fighter->GetController()))
	{
		if (PC->PlayerCameraManager) { PC->GetPlayerViewPoint(CamLoc, CamRot); AimPoint = CamLoc + CamRot.Vector() * GetRange(); }
		// 锁定即瞄准：有效锁定目标优先于相机射线
		if (auto* Targeting = Fighter->GetTargeting())
			if (Targeting->IsTargetValid())
				AimPoint = Targeting->GetCurrentTarget()->GetActorLocation() + FVector(0, 0, 30);
	}
	else if (auto* Target = Fighter->GetPreferredTargetFighter())
	{
		AimPoint = Target->GetActorLocation() + FVector(0, 0, 30);
	}
	return AimPoint;
}

void UChargedBlastAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearTimers();
	RestoreMoveSpeed();
	if (auto* Fighter = CachedFighter.Get())
	{
		ClearChargeStateTags(Fighter->GetFighterAbilitySystemComponent());
		if (bWasCancelled || bSuperBlastFired) ApplySuperBlastCooldown();
		Fighter->NotifyBlastEnded(this);
	}
	CachedFighter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
