// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Training/CombatTypes.h"
#include "BlastProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UParticleSystem;
class UParticleSystemComponent;
class AFighterCharacter;

/** M7 表现占位：手动蓄力炮的可见小圆球投射体（直线飞行；结算走共享攻防口径）。 */
UCLASS()
class ABlastProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABlastProjectile();

	void InitBlast(AFighterCharacter* InCaster, const FVector& Direction, float InSpeed, float InRadius,
		float InLife, const FRangedHitSettle& InSettle);

	/** 注入弹体拖尾（发射后由能力侧从角色定义配置取；空项跳过） */
	void ApplyFx(TSoftObjectPtr<UNiagaraSystem> InTrail, float InTrailScale);

	/** 注入撞击特效（Cascade；命中/撞墙/寿命耗尽时生成） */
	void ApplyImpact(TSoftObjectPtr<UParticleSystem> InImpact, float InImpactScale, float InImpactLife);

	/** 注入引导光束：额头→弹体（Cascade Beam2），条数随蓄力增加（视觉变粗） */
	void ApplyBeam(AFighterCharacter* InCaster, UParticleSystem* BeamSystem, float WidthMin, float WidthMax, float ChargeQ);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Blast")
	USphereComponent* Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Blast")
	UStaticMeshComponent* MeshComp;

private:
	void Finish();
	void UpdateBeam();
	FVector GetBeamOrigin() const;
	void RecycleBeam();

	TWeakObjectPtr<AFighterCharacter> Caster;
	FRangedHitSettle Settle;
	FVector Velocity = FVector::ZeroVector;
	FVector PrevPosition = FVector::ZeroVector;
	float LifeRemaining = 3.f;
	bool bFinished = false;

	TWeakObjectPtr<UNiagaraComponent> TrailComp;
	TSoftObjectPtr<UParticleSystem> ImpactEffect;
	float ImpactScale = 1.f;
	float ImpactLife = 1.2f;

	TWeakObjectPtr<UParticleSystemComponent> BeamComp;
	TWeakObjectPtr<AFighterCharacter> BeamCaster;
	float BeamWidthMin = 1.f;
	float BeamWidthMax = 1.f;
	float BeamChargeQ = 0.f;
};
