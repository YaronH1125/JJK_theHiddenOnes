// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Training/CombatTypes.h"
#include "BlastProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
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

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Blast")
	USphereComponent* Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Blast")
	UStaticMeshComponent* MeshComp;

private:
	void Finish();

	TWeakObjectPtr<AFighterCharacter> Caster;
	FRangedHitSettle Settle;
	FVector Velocity = FVector::ZeroVector;
	FVector PrevPosition = FVector::ZeroVector;
	float LifeRemaining = 3.f;
	bool bFinished = false;
};
