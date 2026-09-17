// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/DomainOrb.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Training/DamageGameplayEffect.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterCharacter.h"

ADomainOrb::ADomainOrb()
{
	PrimaryActorTick.bCanEverTick = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(15.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Sphere;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComp->SetStaticMesh(SphereMesh.Object);
		MeshComp->SetWorldScale3D(FVector(0.3f));
	}
}

void ADomainOrb::InitOrb(AFighterCharacter* InTarget, float InDamage, float InLife, float InSpeed, float InRadius)
{
	Target = InTarget;
	Damage = InDamage;
	LifeRemaining = InLife;
	Speed = InSpeed;
	Radius = InRadius;
	PrevPosition = GetActorLocation();
	bPrevValid = true;

	FVector ToTarget = InTarget ? (InTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal() : FVector::ForwardVector;
	Velocity = FVector(ToTarget.X, ToTarget.Y, 0.5f).GetSafeNormal() * Speed;
}

void ADomainOrb::BeginPlay()
{
	Super::BeginPlay();
	PrevPosition = GetActorLocation();
	bPrevValid = true;
}

void ADomainOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bHasHit || bPausedMovement) return;

	LifeRemaining -= DeltaSeconds;
	if (LifeRemaining <= 0.f)
	{
		DestroySelf();
		return;
	}
	SteerTowardTarget(DeltaSeconds);
	CheckContact();
}

void ADomainOrb::SteerTowardTarget(float DT)
{
	if (!Target.IsValid()) return;

	const FVector Desired = (Target->GetActorLocation() + FVector(0, 0, 30) - GetActorLocation()).GetSafeNormal();
	const FVector Current = Velocity.GetSafeNormal();

	const double Dot = FVector::DotProduct(Current, Desired);
	const float Angle = FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0));
	if (Angle > 3.0f * DT)
	{
		const FVector Axis = FVector::CrossProduct(Current, Desired).GetSafeNormal();
		const FVector NewDir = Current.RotateAngleAxis(FMath::RadiansToDegrees(3.0f * DT), Axis).GetSafeNormal();
		Velocity = NewDir * Speed;
	}
	else
	{
		Velocity = Desired * Speed;
	}

	PrevPosition = GetActorLocation();
	SetActorLocation(GetActorLocation() + Velocity * DT, true);
}

void ADomainOrb::CheckContact()
{
	if (!bPrevValid || !Target.IsValid() || bHasHit) return;

	auto* TargetFighter = Cast<AFighterCharacter>(Target.Get());
	if (!TargetFighter || TargetFighter->IsDead()) return;

	FCollisionQueryParams QP(SCENE_QUERY_STAT(DomainOrbSweep));
	QP.AddIgnoredActor(this);

	const FVector NewPos = GetActorLocation();

	// 撞墙销毁（A06）：世界几何遮挡即销毁，不穿墙、不隐藏补伤害
	{
		FHitResult WallHit;
		FCollisionQueryParams WallQP(SCENE_QUERY_STAT(DomainOrbWall));
		WallQP.AddIgnoredActor(this);
		if (GetWorld()->LineTraceSingleByChannel(WallHit, PrevPosition, NewPos, ECC_Visibility, WallQP))
		{
			if (!Cast<AFighterCharacter>(WallHit.GetActor()))
			{
				UE_LOG(LogTemp, Log, TEXT("[Orb] 撞到 %s 销毁（无伤害）"), *GetNameSafe(WallHit.GetActor()));
				bHasHit = true;
				DestroySelf();
				return;
			}
		}
	}

	FHitResult SweepHit;
	if (GetWorld()->SweepSingleByChannel(SweepHit, PrevPosition, NewPos,
		FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), QP))
	{
		// 命中校验：只结算所属会话的指定目标（A05）
		auto* HitFighter = Cast<AFighterCharacter>(SweepHit.GetActor());
		auto* Caster = GetInstigator() ? Cast<AFighterCharacter>(GetInstigator()) : nullptr;
		// 倒地/死亡保护仍生效（08：实际接触前不扣血，接触后按保护分支跳过）
		const bool bProtected = !HitFighter || HitFighter->IsDead() || HitFighter->HasCombatTag(TAG_State_KnockedDown);
		if (HitFighter && HitFighter == TargetFighter && !bProtected && !bHasHit)
		{
			bHasHit = true;
			if (Caster)
			{
				// 共享攻防结算（A02）：领域球必中（不可闪避/不可防御），保护分支已在上方拦截
				FRangedHitSettle Settle;
				Settle.Damage = Damage;
				Settle.bDodgeable = false;
				Settle.bBlockable = false;
				Settle.bGrantCurse = false;
				Settle.KnockbackStrength = 800.f;
				Settle.AttackInstanceId = GetUniqueID();
				Caster->SettleRangedHitOn(HitFighter, Settle);
			}
			UE_LOG(LogTemp, Log, TEXT("[Orb] 命中 %s（伤害 %.0f）"), *GetNameSafe(HitFighter), Damage);
			DestroySelf();
		}
	}
}

void ADomainOrb::DestroySelf()
{
	Destroy();
}
