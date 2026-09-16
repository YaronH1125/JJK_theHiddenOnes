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

	if (auto* ASC = Fighter->GetFighterAbilitySystemComponent())
		ASC->AddLooseGameplayTag(TAG_State_BlastCharging);

	StoredBaseMoveSpeed = Fighter->GetCharacterMovement()->MaxWalkSpeed;
	ApplyMoveSpeedScale(GetMoveSpeedScale());
	Phase = EBlastPhase::Charging;
	Fighter->RegisterActiveBlast(this);
	StartChargeTick();

	UE_LOG(LogTemp, Log, TEXT("[Blast] %s 开始蓄力（成本 %.0f）"), *GetNameSafe(Fighter), GetMinCost());
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

	const double Elapsed = Now() - ChargeStartTime;
	float q = FMath::Clamp(static_cast<float>(Elapsed) / GetChargeCapTime(), 0.f, 1.f);

	const float TargetCost = GetMinCost() + (GetMaxCost() - GetMinCost()) * q;
	const float Increment = TargetCost - PaidCost;
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
			if (Curse > 0.f) Fighter->ModifyCursedEnergy(-Curse);
			const float CostSpan = GetMaxCost() - GetMinCost();
			PaidQ = CostSpan > 1.f ? FMath::Clamp((PaidCost - GetMinCost()) / CostSpan, 0.f, 1.f) : 0.f;
			return;
		}
	}
	PaidQ = q;
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
	FireOnce();
	Phase = EBlastPhase::Recovery;
	ApplyMoveSpeedScale(1.f);
	GetWorld()->GetTimerManager().SetTimer(PhaseTimerHandle, this,
		&UChargedBlastAbilityBase::HandleRecoveryDone, FMath::Max(GetRecoveryTime(), 0.01f), false);
}

void UChargedBlastAbilityBase::HandleRecoveryDone()
{
	if (auto* ASC = CachedFighter.IsValid() ? CachedFighter->GetFighterAbilitySystemComponent() : nullptr)
		ASC->RemoveLooseGameplayTag(TAG_State_BlastCharging);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UChargedBlastAbilityBase::AbortBlast(const TCHAR* Reason, bool bPaidInterrupt)
{
	UE_LOG(LogTemp, Log, TEXT("[Blast] %s 收招：%s"), *GetNameSafe(CachedFighter.Get()), Reason);
	if (bPaidInterrupt) ApplySuperBlastCooldown();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UChargedBlastAbilityBase::ApplySuperBlastCooldown()
{
	if (bCooldownTagApplied || GetCooldown() <= 0.f) return;
	bCooldownTagApplied = true;
	if (auto* Fighter = CachedFighter.Get())
	{
		if (auto* ASC = Fighter->GetFighterAbilitySystemComponent())
			ASC->AddLooseGameplayTag(TAG_State_SuperBlastCooldown);
		GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, this,
			&UChargedBlastAbilityBase::HandleCooldownDone, GetCooldown(), false);
		UE_LOG(LogTemp, Log, TEXT("[Blast] %s 超级炮冷却 %.0fs"), *GetNameSafe(Fighter), GetCooldown());
	}
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

	FVector CamLoc = Muzzle;
	FRotator CamRot = Fighter->GetActorRotation();
	FVector AimPoint = Muzzle + Fighter->GetActorForwardVector() * Range;
	if (auto* PC = Cast<APlayerController>(Fighter->GetController()))
	{
		if (PC->PlayerCameraManager) { PC->GetPlayerViewPoint(CamLoc, CamRot); AimPoint = CamLoc + CamRot.Vector() * Range; }
	}
	else if (auto* Target = Fighter->GetPreferredTargetFighter())
	{
		AimPoint = Target->GetActorLocation() + FVector(0, 0, 30);
	}

	FCollisionQueryParams QP(SCENE_QUERY_STAT(JJKBlast));
	QP.AddIgnoredActor(Fighter);

	FHitResult AimHit;
	FVector TargetPoint = AimPoint;
	if (GetWorld()->LineTraceSingleByChannel(AimHit, CamLoc, AimPoint, ECC_Visibility, QP))
		TargetPoint = AimHit.ImpactPoint;

	FHitResult MuzzleHit;
	if (GetWorld()->LineTraceSingleByChannel(MuzzleHit, Muzzle,
		Muzzle + (TargetPoint - Muzzle).GetSafeNormal() * 30.f, ECC_Visibility, QP))
	{
		UE_LOG(LogTemp, Log, TEXT("[Blast] %s 炮口嵌墙安全拒绝"), *GetNameSafe(Fighter));
		return;
	}

	FHitResult FireHit;
	AFighterCharacter* HitFighter = nullptr;
	FVector EndPoint = TargetPoint;
	if (GetWorld()->LineTraceSingleByChannel(FireHit, Muzzle, TargetPoint, ECC_Visibility, QP))
	{
		EndPoint = FireHit.ImpactPoint;
		HitFighter = Cast<AFighterCharacter>(FireHit.GetActor());
	}

	if (BlastDebugEnabled() > 0.f && GetWorld())
		DrawDebugLine(GetWorld(), Muzzle, EndPoint, FColor::Cyan, false, 2.f, 0, 2.f);

	auto* AtkASC = Fighter->GetFighterAbilitySystemComponent();
	if (HitFighter && !HitFighter->IsDead() && AtkASC && HitFighter->GetFighterAbilitySystemComponent())
	{
		auto Spec = AtkASC->MakeOutgoingSpec(UDamageGameplayEffect::StaticClass(), 1.f, AtkASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage, -Damage);
			AtkASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, HitFighter->GetFighterAbilitySystemComponent());
		}
		UE_LOG(LogTemp, Log, TEXT("[Blast] %s 炮击命中 %s（伤害 %.0f q=%.2f）"),
			*GetNameSafe(Fighter), *GetNameSafe(HitFighter), Damage, PaidQ);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[Blast] %s 炮击空放"), *GetNameSafe(Fighter));
	}
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
		if (auto* ASC = Fighter->GetFighterAbilitySystemComponent())
			ASC->RemoveLooseGameplayTag(TAG_State_BlastCharging);
		if (bWasCancelled) ApplySuperBlastCooldown();
		Fighter->NotifyBlastEnded(this);
	}
	CachedFighter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
