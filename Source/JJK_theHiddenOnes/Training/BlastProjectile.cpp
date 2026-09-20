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
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
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

void ABlastProjectile::ApplyChargeStrength(float InQ)
{
	ChargeQ = FMath::Clamp(InQ, 0.f, 1.f);
}

void ABlastProjectile::ApplyBeam(AFighterCharacter* InCaster, UParticleSystem* BeamSystem, float WidthMin, float WidthMax, float InChargeQ)
{
	if (InCaster == nullptr || BeamSystem == nullptr)
	{
		return;
	}
	BeamCaster = InCaster;
	ChargeQ = FMath::Clamp(InChargeQ, 0.f, 1.f);
	BeamWidthMin = FMath::Max(WidthMin, 0.05f);
	BeamWidthMax = FMath::Max(BeamWidthMin, WidthMax);

	// Cascade Beam2：源点=额头，目标点=弹体（每帧刷新）；"Beam Max Index" 控制光束条数（越多越粗）
	UParticleSystemComponent* Comp = NewObject<UParticleSystemComponent>(InCaster);
	Comp->SetTemplate(BeamSystem);
	Comp->bAutoDestroy = false;
	Comp->bAutoActivate = false;
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->RegisterComponent();
	InCaster->AddInstanceComponent(Comp);
	// 注意：不要 AttachToComponent。挂头部骨骼会把骨骼旋转带给模板的雾气层（朝上/朝歪喷）；
	// 位置跟随由 UpdateBeam 每帧 SetWorldLocation 到额头完成，旋转保持世界朝向（雾气竖直上升）。
	Comp->SetWorldLocation(GetBeamOrigin());
	BeamComp = Comp;

	UpdateBeam();
	// 粗细=条数：在配置宽度区间之上再乘蓄力增益，让"随蓄力变粗"肉眼可辨
	//（0 蓄：基础宽 ×0.6；满蓄：基础宽 ×1.5）
	const float BaseWidth = FMath::Lerp(BeamWidthMin, BeamWidthMax, ChargeQ);
	const float WidthScale = BaseWidth * FMath::Lerp(0.6f, 1.5f, ChargeQ);
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
	// Beam2：源点=额头（停留期持续跟随施术者）；目标点：飞行期=弹体，停留期=冻结的命中点
	const FVector SrcWorld = GetBeamOrigin();
	const FVector EndWorld = bLingering ? ImpactLocation : GetActorLocation();
	// 组件原点每帧同步到额头：模板的雾气/辉光层随人移动，但旋转保持世界朝向（雾气竖直升起，
	// 不随头部骨骼旋转——挂骨骼曾导致雾气朝上/朝歪喷）
	Comp->SetWorldLocation(SrcWorld);
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
	// 最大存在时间硬上限（用户要求）：飞行 + 停留总计超过即强制清理，
	// 防止任何路径（如朝天空发射、组件回收失败）导致特效/弹体永不销毁。
	if (GetGameTimeSinceCreation() >= MaxLifeTime)
	{
		ForceCleanup();
		return;
	}
	if (bLingering)
	{
		TickLinger(DeltaSeconds);
		return;
	}
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
	ImpactLocation = GetActorLocation();

	// 命中特效（Cascade）：命中与撞墙、寿命耗尽共用同一表现（伤害仍只在命中结算）
	// 持续时间与光束停留共用同一条蓄力曲线（整体偏短，用户口径），并以配置最长存活封顶。
	if (!ImpactEffect.ToSoftObjectPath().IsNull())
	{
		if (UParticleSystem* ImpactFx = ImpactEffect.LoadSynchronous())
		{
			const float Life = FMath::Clamp(FMath::Lerp(0.35f, 0.9f, ChargeQ), 0.2f, ImpactLife);
			UParticleSystemComponent* ImpactComp = UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(), ImpactFx, ImpactLocation, GetActorRotation(), FVector(ImpactScale), /*bAutoDestroy=*/true);
			if (ImpactComp != nullptr && Life > 0.f)
			{
				TWeakObjectPtr<UParticleSystemComponent> Weak = ImpactComp;
				FTimerHandle Handle;
				GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
				{
					if (UParticleSystemComponent* C = Weak.Get())
					{
						C->DestroyComponent();
					}
				}), Life, false);
			}
		}
	}

	// 有引导光束则进入停留态：端点冻结在命中点、起点跟随施术者额头，
	// 时长从命中起算（飞行不计）；到点熄灭并销毁。
	if (BeamComp.IsValid())
	{
		bLingering = true;
		LingerRemaining = FMath::Lerp(0.35f, 0.9f, ChargeQ);
		SetActorEnableCollision(false);
		MeshComp->SetVisibility(false);
		SetLifeSpan(0.f);   // 停留期由 TickLinger 自行收尾，禁用兜底寿命
		UpdateBeam();
		return;
	}

	Destroy();
}

void ABlastProjectile::TickLinger(float DeltaSeconds)
{
	LingerRemaining -= DeltaSeconds;
	// 起点与组件原点持续跟随施术者额头（移动中命中的光束不会脱锚悬空），
	// 终点冻结在命中点；到点熄灭并销毁
	UpdateBeam();
	if (LingerRemaining <= 0.f)
	{
		RecycleBeam();
	}
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
	Comp->DestroyComponent();
	Destroy();
}

void ABlastProjectile::ForceCleanup()
{
	// 硬上限兜底：无论处于飞行还是停留态，立即回收光束并销毁弹体
	if (UParticleSystemComponent* Comp = BeamComp.Get())
	{
		BeamComp = nullptr;
		Comp->DeactivateSystem();
		Comp->DestroyComponent();
	}
	Destroy();
}
