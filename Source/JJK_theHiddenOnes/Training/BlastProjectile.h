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

	/** 注入弹体表现（发射后由能力侧从角色定义配置取；空项跳过） */
	void ApplyFx(TSoftObjectPtr<UNiagaraSystem> InTrail, float InTrailScale,
		TSoftObjectPtr<UNiagaraSystem> InImpact, float InImpactScale, float InImpactLife);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Blast")
	USphereComponent* Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Blast")
	UStaticMeshComponent* MeshComp;

private:
	void Finish();

	/** 生成一次性撞击特效并在到期后强制回收（循环型系统也能清理）；供撞击点复用 */
	UNiagaraComponent* SpawnOneShotFx(UWorld* World, UNiagaraSystem* System, const FTransform& Xf, float LifeSeconds);

	TWeakObjectPtr<AFighterCharacter> Caster;
	FRangedHitSettle Settle;
	FVector Velocity = FVector::ZeroVector;
	FVector PrevPosition = FVector::ZeroVector;
	float LifeRemaining = 3.f;
	bool bFinished = false;

	TWeakObjectPtr<UNiagaraComponent> TrailComp;
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;
	float ImpactScale = 1.f;
	float ImpactLife = 1.2f;
};
