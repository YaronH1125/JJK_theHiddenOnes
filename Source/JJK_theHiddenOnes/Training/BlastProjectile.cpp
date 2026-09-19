// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/BlastProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
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

void ABlastProjectile::ApplyFx(TSoftObjectPtr<UNiagaraSystem> InTrail, float InTrailScale,
	TSoftObjectPtr<UNiagaraSystem> InImpact, float InImpactScale, float InImpactLife)
{
	if (TrailComp.IsValid() == false && !InTrail.ToSoftObjectPath().IsNull())
	{
		if (UNiagaraSystem* TrailFx = InTrail.LoadSynchronous())
		{
			if (UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
				TrailFx, RootComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true))
			{
				Comp->SetWorldScale3D(FVector(FMath::Max(InTrailScale, 0.01f)));
				TrailComp = Comp;
				// 有拖尾特效时隐藏占位小球
				MeshComp->SetVisibility(false);
			}
		}
	}

	ImpactEffect = InImpact;
	ImpactScale = FMath::Max(InImpactScale, 0.01f);
	ImpactLife = FMath::Max(InImpactLife, 0.1f);
}

UNiagaraComponent* ABlastProjectile::SpawnOneShotFx(UWorld* World, UNiagaraSystem* System, const FTransform& Xf, float LifeSeconds)
{
	if (World == nullptr || System == nullptr)
	{
		return nullptr;
	}
	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, System, Xf.GetLocation(), Xf.Rotator(), Xf.GetScale3D(), /*bAutoDestroy=*/false, /*bAutoActivate=*/true);
	if (Comp == nullptr)
	{
		return nullptr;
	}
	// 循环型系统永不自动完结，用定时器强制回收（训练场口径：表现不影响结算）
	TWeakObjectPtr<UNiagaraComponent> Weak = Comp;
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
	{
		if (UNiagaraComponent* C = Weak.Get())
		{
			C->Deactivate();
			C->DestroyComponent();
		}
	}), LifeSeconds, /*bLoop=*/false);
	return Comp;
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

	// Compare contacts along the SAME segment. At high speed / on a hitch the segment
	// can contain both a fighter and the wall behind them; world-first early return
	// would incorrectly swallow the nearer fighter hit. Ties favor world occlusion.
	FHitResult WallHit;
	const bool bWallHit = World->LineTraceSingleByChannel(WallHit, PrevPosition, NewPos, ECC_Visibility, QP)
		&& !Cast<AFighterCharacter>(WallHit.GetActor());
	FHitResult SweepHit;
	const bool bFighterHit = World->SweepSingleByChannel(SweepHit, PrevPosition, NewPos, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(Sphere->GetScaledSphereRadius()), QP)
		&& Cast<AFighterCharacter>(SweepHit.GetActor());
	if (bWallHit && (!bFighterHit || WallHit.Time <= SweepHit.Time))
	{
		UE_LOG(LogTemp, Log, TEXT("[BlastProj] 撞到 %s 销毁（无伤害）"), *GetNameSafe(WallHit.GetActor()));
		Finish();
		return;
	}

	// Pawn 命中：共享攻防结算（A02 口径），结算一次即销毁
	if (bFighterHit)
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

	// 撞击/消失特效：命中与撞墙、寿命耗尽共用同一表现（伤害仍只在命中结算）
	if (!ImpactEffect.ToSoftObjectPath().IsNull())
	{
		if (UNiagaraSystem* ImpactFx = ImpactEffect.LoadSynchronous())
		{
			const FTransform Xf(GetActorRotation(), GetActorLocation(), FVector(ImpactScale));
			SpawnOneShotFx(GetWorld(), ImpactFx, Xf, ImpactLife);
		}
	}

	Destroy();
}
