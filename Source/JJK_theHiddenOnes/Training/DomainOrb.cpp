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
	FHitResult SweepHit;
	if (GetWorld()->SweepSingleByChannel(SweepHit, PrevPosition, NewPos,
		FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), QP))
	{
		auto* HitFighter = Cast<AFighterCharacter>(SweepHit.GetActor());
		auto* Caster = GetInstigator() ? Cast<AFighterCharacter>(GetInstigator()) : nullptr;
		// 倒地/死亡保护仍生效（08：实际接触前不扣血，接触后按保护分支跳过）
		const bool bProtected = !HitFighter || HitFighter->IsDead() || HitFighter->HasCombatTag(TAG_State_KnockedDown);
		if (HitFighter && HitFighter != Caster && !bProtected && !bHasHit)
		{
			bHasHit = true;
			auto* ASC = Caster ? Caster->GetFighterAbilitySystemComponent() : nullptr;
			if (ASC && HitFighter->GetFighterAbilitySystemComponent())
			{
				auto Spec = ASC->MakeOutgoingSpec(UDamageGameplayEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage, -Damage);
					ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, HitFighter->GetFighterAbilitySystemComponent());
				}
				// 命中获得领域能量（按比例、按实例封顶）
				Caster->GainDomainEnergy(Damage, GetUniqueID());
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
