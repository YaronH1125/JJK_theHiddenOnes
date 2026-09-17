// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/BlastProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Training/FighterCharacter.h"

ABlastProjectile::ABlastProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(12.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Sphere;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComp->SetStaticMesh(SphereMesh.Object);
		MeshComp->SetWorldScale3D(FVector(0.24f));
		MeshComp->SetVisibility(true);
	}
}

void ABlastProjectile::InitBlast(AFighterCharacter* InCaster, const FVector& Direction, float InSpeed,
	float InRadius, float InLife, const FRangedHitSettle& InSettle)
{
	Caster = InCaster;
	Settle = InSettle;
	LifeRemaining = InLife;
	Sphere->InitSphereRadius(InRadius);
	Velocity = Direction.GetSafeNormal() * InSpeed;
	PrevPosition = GetActorLocation();
	SetLifeSpan(InLife + 1.f);
}

void ABlastProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished) return;

	// 卡帧下限制单步积分（与领域球同口径）
	DeltaSeconds = FMath::Min(DeltaSeconds, 0.1f);
	LifeRemaining -= DeltaSeconds;
	if (LifeRemaining <= 0.f)
	{
		Finish();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		Finish();
		return;
	}

	FCollisionQueryParams QP(SCENE_QUERY_STAT(JJKBlastProjectile));
	QP.AddIgnoredActor(this);
	if (auto* C = Caster.Get()) QP.AddIgnoredActor(C);

	const FVector NewPos = GetActorLocation() + Velocity * DeltaSeconds;

	// 世界阻挡：撞墙销毁（不穿墙、不隐藏补伤害）
	{
		FHitResult WallHit;
		if (World->LineTraceSingleByChannel(WallHit, PrevPosition, NewPos, ECC_Visibility, QP))
		{
			if (!Cast<AFighterCharacter>(WallHit.GetActor()))
			{
				UE_LOG(LogTemp, Log, TEXT("[BlastProj] 撞到 %s 销毁（无伤害）"), *GetNameSafe(WallHit.GetActor()));
				Finish();
				return;
			}
		}
	}

	// Pawn 命中：共享攻防结算（A02 口径），结算一次即销毁
	FHitResult SweepHit;
	if (World->SweepSingleByChannel(SweepHit, PrevPosition, NewPos, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(Sphere->GetScaledSphereRadius()), QP))
	{
		if (auto* HitFighter = Cast<AFighterCharacter>(SweepHit.GetActor()))
		{
			if (auto* C = Caster.Get())
			{
				const ETrainingContact Result = C->SettleRangedHitOn(HitFighter, Settle);
				UE_LOG(LogTemp, Log, TEXT("[BlastProj] 命中 %s（伤害 %.0f 结果=%d）"),
					*GetNameSafe(HitFighter), Settle.Damage, static_cast<int32>(Result));
			}
			Finish();
			return;
		}
	}

	PrevPosition = NewPos;
	SetActorLocation(NewPos, true);
}

void ABlastProjectile::Finish()
{
	if (bFinished) return;
	bFinished = true;
	Destroy();
}
