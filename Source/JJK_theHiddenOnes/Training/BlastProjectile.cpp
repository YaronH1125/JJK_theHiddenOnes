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
				Comp->SetWorldScale3D(FVector(FMath::Max(InTrailScale, 0.01f) * FMath::Lerp(0.6f, 1.9f, ChargeQ)));
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
	// 弹体本体随蓄力放大（差值拉大：0 蓄 0.6×，满蓄 1.9×）
	if (MeshComp != nullptr)
	{
		MeshComp->SetWorldScale3D(FVector(0.24f * FMath::Lerp(0.6f, 1.9f, ChargeQ)));
	}
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
	// UpdateBeam 同步额头位置和光束三维朝向；俯仰来自实际光束方向，不继承头骨旋转。
	Comp->SetWorldLocation(GetBeamOrigin());
	BeamComp = Comp;

	UpdateBeam();
	// 粗细双通道：主束目标点圆周（UpdateBeam 每帧刷新）+ 环形束笼（8 条完整光束，半径 3→55cm 随蓄力）
	Comp->SetFloatParameter(TEXT("Beam Max Index"), 5.f);
	UE_LOG(LogTemp, Log, TEXT("[BlastBeam] 发射 Q=%.2f → 8 条环束半径 3→55cm（Q 插值），主束点数 1→9"), ChargeQ);
	Comp->SetFloatParameter(TEXT("Beam Flow Speed"), 1.f);
	Comp->ActivateSystem();
	// 光束自带发光核心，隐藏占位小球
	MeshComp->SetVisibility(false);

	// 环形束笼：8 条完整光束（烟雾层关闭：折射材质最贵，雾气只留轴心主束）
	RingComps.Reset();
	for (int32 i = 0; i < 8; ++i)
	{
		UParticleSystemComponent* Ring = NewObject<UParticleSystemComponent>(InCaster);
		Ring->SetTemplate(BeamSystem);
		Ring->bAutoDestroy = false;
		Ring->bAutoActivate = false;
		Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ring->RegisterComponent();
		Ring->AttachToComponent(Comp, FAttachmentTransformRules::KeepRelativeTransform);
		Ring->SetFloatParameter(TEXT("Beam Max Index"), 2.f);
		Ring->SetFloatParameter(TEXT("Beam Flow Speed"), 1.f);
		Ring->SetEmitterEnable(TEXT("Smoke"), false);
		RingComps.Add(Ring);
	}
	// 首次生成粒子前先定位环束，避免默认原点/朝向产生一帧错误喷射。
	UpdateBeam();
	for (UParticleSystemComponent* Ring : RingComps)
	{
		Ring->ActivateSystem();
	}
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
	// Frost 的 Smoke Velocity 使用局部 +X 初速度和局部空间模拟。
	// 局部 +X 对齐额头到弹体的三维方向，雾气随光束一起偏航、俯仰。
	// Beam2 的 UserSet 源/目标仍传世界坐标，不要再用组件变换旋转端点。
	FVector Facing = (EndWorld - SrcWorld).GetSafeNormal();
	if (Facing.IsNearlyZero())
	{
		Facing = Velocity.GetSafeNormal();
		if (Facing.IsNearlyZero() && BeamCaster.IsValid())
		{
			Facing = BeamCaster->GetActorForwardVector();
		}
	}
	const FRotator BeamRotation = Facing.IsNearlyZero() ? Comp->GetComponentRotation() : Facing.Rotation();
	Comp->SetWorldLocationAndRotation(SrcWorld, BeamRotation);
	for (int32 EmitterIndex = 0; EmitterIndex < 6; ++EmitterIndex)
	{
		Comp->SetBeamSourcePoint(EmitterIndex, SrcWorld, 0);
		for (int32 TargetIndex = 0; TargetIndex < 9; ++TargetIndex)
		{
			Comp->SetBeamTargetPoint(EmitterIndex, EndWorld, TargetIndex);
		}
	}

	// 环形束笼：半径随蓄力拉大（0 蓄 3cm 合拢，满蓄 55cm），圆周均匀、垂直于光束轴；
	// 每条环束源/目标同步外扩，端点跟随命中/飞行状态
	const int32 RingCount = RingComps.Num();
	if (RingCount > 0)
	{
		const FVector Dir = (EndWorld - SrcWorld).GetSafeNormal();
		if (Dir.IsNearlyZero() == false)
		{
			const FVector Ref = FMath::Abs(Dir.Z) < 0.95f ? FVector::UpVector : FVector::YAxisVector;
			const FVector Right = FVector::CrossProduct(Ref, Dir).GetSafeNormal();
			const FVector RingUp = FVector::CrossProduct(Dir, Right).GetSafeNormal();
			const float Radius = FMath::Lerp(3.f, 55.f, ChargeQ);
			for (int32 i = 0; i < RingCount; ++i)
			{
				if (UParticleSystemComponent* Ring = RingComps[i].Get())
				{
					const float Angle = static_cast<float>(i) / static_cast<float>(RingCount) * 2.f * PI;
					const FVector Offset = (Right * FMath::Cos(Angle) + RingUp * FMath::Sin(Angle)) * Radius;
					const FVector RingSrc = SrcWorld + Offset;
					const FVector RingEnd = EndWorld + Offset;
					Ring->SetWorldLocation(RingSrc);
					Ring->SetBeamSourcePoint(0, RingSrc, 0);
					Ring->SetBeamTargetPoint(0, RingEnd, 0);
				}
			}
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
		LingerRemaining = FMath::Lerp(0.6f, 1.5f, ChargeQ);
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
	// 停留期只固定命中端；额头起点、雾气朝向和环束仍随施术者移动更新。
	LingerRemaining -= DeltaSeconds;
	if (LingerRemaining <= 0.f)
	{
		RecycleBeam();
		return;
	}
	UpdateBeam();
}

void ABlastProjectile::RecycleBeam()
{
	UParticleSystemComponent* Comp = BeamComp.Get();
	if (Comp == nullptr)
	{
		return;
	}
	// 环束逐条销毁（正式成员列表——主组件销毁不会级联销毁附件子组件，会泄漏常驻渲染）
	for (UParticleSystemComponent* Ring : RingComps)
	{
		if (Ring != nullptr)
		{
			Ring->DeactivateSystem();
			Ring->DestroyComponent();
		}
	}
	RingComps.Reset();
	BeamComp = nullptr;
	Comp->DeactivateSystem();
	Comp->DestroyComponent();
	Destroy();
}

void ABlastProjectile::ForceCleanup()
{
	// 硬上限兜底：无论处于飞行还是停留态，立即回收光束并销毁弹体
	for (UParticleSystemComponent* Ring : RingComps)
	{
		if (Ring != nullptr)
		{
			Ring->DeactivateSystem();
			Ring->DestroyComponent();
		}
	}
	RingComps.Reset();
	if (UParticleSystemComponent* Comp = BeamComp.Get())
	{
		BeamComp = nullptr;
		Comp->DeactivateSystem();
		Comp->DestroyComponent();
	}
	Destroy();
}
