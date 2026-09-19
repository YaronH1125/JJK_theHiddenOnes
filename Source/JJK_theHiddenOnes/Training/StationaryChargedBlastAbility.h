// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Training/ChargedBlastAbility.h"
#include "StationaryChargedBlastAbility.generated.h"

/** 远程 Q：定点超级蓄力炮（M6.4） */
UCLASS()
class UStationaryChargedBlastAbility : public UChargedBlastAbilityBase
{
	GENERATED_BODY()

public:
	UStationaryChargedBlastAbility();

protected:
	virtual float GetMinCost() const override;
	virtual float GetMaxCost() const override;
	virtual float GetMinDamage() const override;
	virtual float GetMaxDamage() const override;
	virtual float GetChargeCapTime() const override;
	virtual float GetLockWindup() const override;
	virtual float GetRange() const override;
	virtual float GetRecoveryTime() const override;
	virtual bool HasMinChargeGate() const override { return true; }
	virtual float GetProjectileSpeed() const override { return 4500.f; }
	virtual float GetProjectileRadius() const override { return 22.f; }
	virtual float GetMinChargeTime() const override;
	virtual float GetCooldown() const override;
	virtual float GetMoveSpeedScale() const override { return 0.f; }
	virtual bool LocksMovementWhileCharging() const override { return true; }
	virtual void FireBlast(float q, float Damage) override;
	virtual void GetProjectileFx(TSoftObjectPtr<class UNiagaraSystem>& OutTrail, float& OutTrailScale,
		TSoftObjectPtr<class UNiagaraSystem>& OutImpact, float& OutImpactScale, float& OutImpactLife) const override;
};
