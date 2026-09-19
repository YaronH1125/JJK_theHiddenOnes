// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Training/ChargedBlastAbility.h"
#include "MobileChargedBlastAbility.generated.h"

/** 远程左键：移动蓄力炮（M6.3） */
UCLASS()
class UMobileChargedBlastAbility : public UChargedBlastAbilityBase
{
	GENERATED_BODY()

public:
	UMobileChargedBlastAbility();

protected:
	virtual float GetMinCost() const override;
	virtual float GetMaxCost() const override;
	virtual float GetMinDamage() const override;
	virtual float GetMaxDamage() const override;
	virtual float GetChargeCapTime() const override;
	virtual float GetLockWindup() const override;
	virtual float GetRange() const override;
	virtual float GetRecoveryTime() const override;
	virtual bool HasMinChargeGate() const override { return false; }
	virtual float GetMinChargeTime() const override { return 0.f; }
	virtual float GetCooldown() const override { return 0.f; }
	virtual float GetMoveSpeedScale() const override;
	virtual bool LocksMovementWhileCharging() const override { return false; }
	virtual void FireBlast(float q, float Damage) override;
	virtual void GetProjectileFx(TSoftObjectPtr<class UNiagaraSystem>& OutTrail, float& OutTrailScale,
		TSoftObjectPtr<class UNiagaraSystem>& OutImpact, float& OutImpactScale, float& OutImpactLife) const override;
	virtual void ApplyChargeStateTags(class UFighterAbilitySystemComponent* ASC) override;
};
