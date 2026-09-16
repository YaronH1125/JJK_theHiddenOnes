// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DomainOrb.generated.h"

class AFighterCharacter;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 领域自动炮曲线追踪球（M6.6）：
 * 弧线飞行、连续修正朝目标收束，扫掠接触命中一次后销毁。
 * 不使用 LineTrace 伤害；倒地/死亡/重置保护仍拒绝。
 */
UCLASS()
class ADomainOrb : public AActor
{
	GENERATED_BODY()

public:
	ADomainOrb();

	void InitOrb(AFighterCharacter* InTarget, float InDamage, float InLife, float InSpeed, float InRadius);
	virtual void Tick(float DeltaSeconds) override;
	void SetOrbPaused(bool bPaused) { bPausedMovement = bPaused; }
	bool HasHit() const { return bHasHit; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Orb")
	USphereComponent* Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Orb")
	UStaticMeshComponent* MeshComp;

private:
	TWeakObjectPtr<AFighterCharacter> Target;
	float Damage = 220.f;
	float Speed = 1200.f;
	float Radius = 15.f;
	float LifeRemaining = 3.f;
	bool bHasHit = false;
	bool bPausedMovement = false;
	FVector Velocity = FVector::ZeroVector;
	FVector PrevPosition = FVector::ZeroVector;

	void SteerTowardTarget(float DT);
	void CheckContact();
	void DestroySelf();
};
