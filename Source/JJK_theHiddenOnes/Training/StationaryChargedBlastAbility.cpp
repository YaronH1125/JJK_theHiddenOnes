// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/StationaryChargedBlastAbility.h"

#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"

UStationaryChargedBlastAbility::UStationaryChargedBlastAbility()
{
	float Dummy = 0.f; (void)Dummy;
}

float UStationaryChargedBlastAbility::GetMinCost() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->SuperBlast.MinCost : 45.f;
}
float UStationaryChargedBlastAbility::GetMaxCost() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->SuperBlast.MaxCost : 65.f;
}
float UStationaryChargedBlastAbility::GetMinDamage() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->SuperBlast.MinDamage : 150.f;
}
float UStationaryChargedBlastAbility::GetMaxDamage() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->SuperBlast.MaxDamage : 220.f;
}
float UStationaryChargedBlastAbility::GetChargeCapTime() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->SuperBlast.CapTime : 2.4f;
}
float UStationaryChargedBlastAbility::GetLockWindup() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->SuperBlast.LockWindup : 0.25f;
}
float UStationaryChargedBlastAbility::GetRange() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->SuperBlast.Range : 2400.f;
}
float UStationaryChargedBlastAbility::GetRecoveryTime() const
{
	return 0.9f;
}
float UStationaryChargedBlastAbility::GetMinChargeTime() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->SuperBlast.MinChargeTime : 1.2f;
}
float UStationaryChargedBlastAbility::GetCooldown() const
{
	auto* F = GetFighter();
	return F && F->GetDefinition() ? F->GetDefinition()->SuperBlast.Cooldown : 10.f;
}

void UStationaryChargedBlastAbility::GetProjectileFx(TSoftObjectPtr<UNiagaraSystem>& OutTrail, float& OutTrailScale,
	TSoftObjectPtr<UNiagaraSystem>& OutImpact, float& OutImpactScale, float& OutImpactLife) const
{
	auto* F = GetFighter();
	const FSuperBlastConfig* Cfg = F && F->GetDefinition() ? &F->GetDefinition()->SuperBlast : nullptr;
	OutTrail = Cfg ? Cfg->TrailEffect : nullptr;
	OutTrailScale = Cfg ? Cfg->TrailEffectScale : 1.f;
	OutImpact = Cfg ? Cfg->ImpactEffect : nullptr;
	OutImpactScale = Cfg ? Cfg->ImpactEffectScale : 1.f;
	OutImpactLife = Cfg ? Cfg->ImpactEffectLife : 1.2f;
}

void UStationaryChargedBlastAbility::FireBlast(float q, float Damage)
{
}
