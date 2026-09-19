// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/BlastProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
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

void ABlastProjectile::ApplyFx(TSoftObjectPtr<UNiagaraSystem> InTrail, float InTrailScale)
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

}

void ABlastProjectile::ApplyImpact(TSoftObjectPtr<UParticleSystem> InImpact, float InImpactScale, float InImpactLife)
{
	ImpactEffect = InImpact;
	ImpactScale = FMath::Max(InImpactScale, 0.01f);
	ImpactLife = FMath::Max(InImpactLife, 0.1f);
}

void ABlastProjectile::ApplyBeam(AFighterCharacter* InCaster, UParticleSystem* BeamSystem, float WidthMin, float WidthMax, float ChargeQ)
{
	if (InCaster == nullptr || BeamSystem == nullptr)
	{
		return;
	}
	BeamCaster = InCaster;
	BeamWidthMin = FMath::Max(WidthMin, 0.05f);
	BeamWidthMax = FMath::Max(WidthMax, BeamWidthMin);
	BeamChargeQ = FMath::Clamp(ChargeQ, 0.f, 1.f);

	// Cascade Beam2：源点=额头，目标点=弹体（每帧刷新）；"Beam Max Index" 控制光束条数（越多越粗）
	UParticleSystemComponent* Comp = NewObject<UParticleSystemComponent>(InCaster);
	Comp->SetTemplate(BeamSystem);
	Comp->bAutoDestroy = false;
	Comp->bAutoActivate = false;
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->RegisterComponent();
	InCaster->AddInstanceComponent(Comp);
	BeamComp = Comp;

	UpdateBeam();
	const float WidthScale = FMath::Lerp(BeamWidthMin, BeamWidthMax, BeamChargeQ);
	const int32 MaxIndex = FMath::Clamp(FMath::RoundToInt(WidthScale * 3.f), 1, 9);
	Comp->SetFloatParameter(TEXT("Beam Max Index"), static_cast<float>(MaxIndex));
	Comp->SetFloatParameter(TEXT("Beam Flow Speed"), 1.f);
	Comp->ActivateSystem();
	// 光束自带发光核心，隐藏占位小球
	MeshComp->SetVisibility(false);
}

FVector ABlastProjectile::GetBeamOrigin() const
{
	// 额头：优先头部骨骼（上移），缺省回落 actor 上方
	const AFighterCharacter* C = BeamCaster.Get();
	if (C != nullptr && C->GetMesh() != nullptr && C->GetMesh()->DoesSocketExist(TEXT("head")))
	{
		return C->GetMesh()->GetSocketLocation(TEXT("head")) + FVector(0, 0, 12.f);
	}
	if (C != nullptr)
	{
		return C->GetActorLocation() + FVector(0, 0, 150.f);
	}
	return GetActorLocation();
}

void ABlastProjectile::UpdateBeam()
{
	UParticleSystemComponent* Comp = BeamComp.Get();
	if (Comp == nullptr)
	{
		return;
	}
	// Beam2：源点=额头；目标点覆盖所有光束索引（弹体位置，世界坐标）
	const FVector SrcWorld = GetBeamOrigin();
	const FVector EndWorld = GetActorLocation();
	for (int32 EmitterIndex = 0; EmitterIndex < 6; ++EmitterIndex)
	{
		Comp->SetBeamSourcePoint(EmitterIndex, SrcWorld, 0);
		for (int32 TargetIndex = 0; TargetIndex < 9; ++TargetIndex)
		{
			Comp->SetBeamTargetPoint(EmitterIndex, EndWorld, TargetIndex);
		}
	}
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
	UpdateBeam();
}

void ABlastProjectile::Finish()
{
	if (bFinished) return;
	bFinished = true;
	RecycleBeam();

	// 撞击/消失特效（Cascade）：命中与撞墙、寿命耗尽共用同一表现（伤害仍只在命中结算）
	if (!ImpactEffect.ToSoftObjectPath().IsNull())
	{
		if (UParticleSystem* ImpactFx = ImpactEffect.LoadSynchronous())
		{
			UParticleSystemComponent* ImpactComp = UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(), ImpactFx, GetActorLocation(), GetActorRotation(), FVector(ImpactScale), /*bAutoDestroy=*/true);
			if (ImpactComp != nullptr && ImpactLife > 0.f)
			{
				// 兜底回收：正常由 Cascade 完成时自毁，超时强制销毁
				TWeakObjectPtr<UParticleSystemComponent> Weak = ImpactComp;
				FTimerHandle Handle;
				GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
				{
					if (UParticleSystemComponent* C = Weak.Get())
					{
						C->DestroyComponent();
					}
				}), ImpactLife, false);
			}
		}
	}

	Destroy();
}

void ABlastProjectile::RecycleBeam()
{
	UParticleSystemComponent* Comp = BeamComp.Get();
	if (Comp == nullptr)
	{
		return;
	}
	BeamComp = nullptr;
	Comp->DeactivateSystem();
	TWeakObjectPtr<UParticleSystemComponent> Weak = Comp;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
	{
		if (UParticleSystemComponent* C = Weak.Get())
		{
			C->DestroyComponent();
		}
	}), 0.8f, false);
}
