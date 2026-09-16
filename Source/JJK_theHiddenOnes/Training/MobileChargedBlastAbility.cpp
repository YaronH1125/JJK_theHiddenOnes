// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/MobileChargedBlastAbility.h"

#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"

UMobileChargedBlastAbility::UMobileChargedBlastAbility()
{
	float Dummy = 0.f; (void)Dummy;
}

float UMobileChargedBlastAbility::GetMinCost() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->MobileBlast.MinCost : 8.f;
}
float UMobileChargedBlastAbility::GetMaxCost() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->MobileBlast.MaxCost : 24.f;
}
float UMobileChargedBlastAbility::GetMinDamage() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->MobileBlast.MinDamage : 30.f;
}
float UMobileChargedBlastAbility::GetMaxDamage() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->MobileBlast.MaxDamage : 90.f;
}
float UMobileChargedBlastAbility::GetChargeCapTime() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->MobileBlast.CapTime : 1.2f;
}
float UMobileChargedBlastAbility::GetLockWindup() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->MobileBlast.LockWindup : 0.15f;
}
float UMobileChargedBlastAbility::GetRange() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->MobileBlast.Range : 1800.f;
}
float UMobileChargedBlastAbility::GetRecoveryTime() const
{
	return 0.45f;
}
float UMobileChargedBlastAbility::GetMoveSpeedScale() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->MobileBlast.MoveSpeedScale : 0.55f;
}

void UMobileChargedBlastAbility::FireBlast(float q, float Damage)
{
	// FireOnce 在基类实现检测和 GE；此处可加发射特效
}
