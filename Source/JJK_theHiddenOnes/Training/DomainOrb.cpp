// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/DomainOrb.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Training/DamageGameplayEffect.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterCharacter.h"
#include "Training/CombatFeedbackComponent.h"
#include "Training/TrainingTypes.h"

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
 Feedback.Source = Cast<AFighterCharacter>(GetInstigator());
 Feedback.Target = InTarget; Feedback.RoundId = UCombatFeedbackComponent::Round(GetWorld());
 Feedback.SourceGeneration = Feedback.Source.IsValid() ? Feedback.Source->GetCombatFeedback()->GetGeneration() : 0;
 Feedback.PaidQ = 1.f; Feedback.MoveId = TEXT("DomainOrb"); Feedback.Tier = ECombatFeedbackTier::DomainOrb; Feedback.bRanged = true; Feedback.AttackInstanceId = Feedback.Source.IsValid() ? Feedback.Source->GetCombatFeedback()->AllocateAttackId() : GetUniqueID();
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
 if (!Feedback.Source.IsValid() || Feedback.RoundId != UCombatFeedbackComponent::Round(GetWorld())) { DestroySelf(ECombatFeedbackEnd::Reset); return; }
 // In-flight gameplay is independent of input/presentation session cleanup.
 Feedback.SourceGeneration = Feedback.Source->GetCombatFeedback()->GetGeneration();
	if (bHasHit || bPausedMovement) return;

	// 卡帧/大帧距下限制单步积分（≤0.1s），防止轨迹穿透或坠地误判（T21 帧率无关性）
	DeltaSeconds = FMath::Min(DeltaSeconds, 0.1f);
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
    Feedback.Result = ECombatFeedbackResult::WorldImpact; Feedback.Target.Reset();
    Feedback.Location = WallHit.ImpactPoint; Feedback.Normal = WallHit.ImpactNormal; Feedback.Direction = Velocity.GetSafeNormal();
    if (Feedback.Source.IsValid()) Feedback.Source->GetCombatFeedback()->PublishContact(Feedback);
    SetActorLocation(WallHit.ImpactPoint, false);
    DestroySelf(ECombatFeedbackEnd::Completed);
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

			if (Caster)
			{
				// 共享攻防结算（A02）：领域球必中（不可闪避/不可防御），保护分支已在上方拦截
				FRangedHitSettle Settle;
				Settle.Damage = Damage;
				Settle.bDodgeable = false;
				Settle.bBlockable = false;
				Settle.bGrantCurse = false;
				Settle.KnockbackStrength = 0.f;
				Settle.AttackInstanceId = Feedback.AttackInstanceId;
    Settle.bHasContactGeometry = true;
    Settle.Feedback = Feedback; Settle.Feedback.Location = SweepHit.ImpactPoint;
    Settle.Feedback.Normal = SweepHit.ImpactNormal; Settle.Feedback.Direction = Velocity.GetSafeNormal();
    const auto Result = Caster->SettleRangedHitOn(HitFighter, Settle);
    bHasHit = Result == ETrainingContact::Hit || Result == ETrainingContact::Guard;
    SetActorLocation(SweepHit.ImpactPoint, false);
			}
			UE_LOG(LogTemp, Log, TEXT("[Orb] 命中 %s（伤害 %.0f）"), *GetNameSafe(HitFighter), Damage);
			DestroySelf(ECombatFeedbackEnd::Completed);
		}
	}
}

void ADomainOrb::DestroySelf(ECombatFeedbackEnd Reason)
{
 if (!bFeedbackEnded && Feedback.Source.IsValid()) Feedback.Source->GetCombatFeedback()->Lifecycle(Feedback.AttackInstanceId, Reason, GetActorLocation());
 bFeedbackEnded = true;
 Destroy();
}
void ADomainOrb::EndPlay(const EEndPlayReason::Type Reason)
{
 if (!bFeedbackEnded && Feedback.Source.IsValid()) Feedback.Source->GetCombatFeedback()->Lifecycle(Feedback.AttackInstanceId,
  Feedback.RoundId == UCombatFeedbackComponent::Round(GetWorld()) ? ECombatFeedbackEnd::Cancel : ECombatFeedbackEnd::Reset, GetActorLocation());
 bFeedbackEnded = true;
 Super::EndPlay(Reason);
}
