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

	/** 注入本次蓄力强度 0..1：命中特效持续时间与光束停留时长都随它缩放 */
	void ApplyChargeStrength(float InQ);

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
	void TickLinger(float DeltaSeconds);
	void ForceCleanup();

	/** 最大存在时间（秒，自生成起算，覆盖飞行+停留；硬上限兜底防特效永不销毁） */
	UPROPERTY(EditAnywhere, Category = "Blast", meta = (ClampMin = "1.0", ForceUnits = "s"))
	float MaxLifeTime = 6.f;

	TWeakObjectPtr<AFighterCharacter> Caster;
	FRangedHitSettle Settle;
	FVector Velocity = FVector::ZeroVector;
	FVector PrevPosition = FVector::ZeroVector;
	float LifeRemaining = 3.f;
	bool bFinished = false;
	/** 本次发射的蓄力强度 0..1（PaidQ） */
	float ChargeQ = 0.f;
	/** 命中后光束停留态：端点冻结在命中点、起点跟随施术者，到时熄灭（从命中起算，飞行时间不计） */
	bool bLingering = false;
	float LingerRemaining = 0.f;
	FVector ImpactLocation = FVector::ZeroVector;

	TWeakObjectPtr<UNiagaraComponent> TrailComp;
	TSoftObjectPtr<UParticleSystem> ImpactEffect;
	float ImpactScale = 1.f;
	float ImpactLife = 1.2f;

	TWeakObjectPtr<UParticleSystemComponent> BeamComp;
	TWeakObjectPtr<AFighterCharacter> BeamCaster;
	float BeamWidthMin = 1.f;
	float BeamWidthMax = 1.f;

	/** 环形束笼：挂在 BeamComp 下的完整光束，半径随蓄力拉大（UpdateBeam）；Transient=每发重建不序列化 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UParticleSystemComponent>> RingComps;
};
