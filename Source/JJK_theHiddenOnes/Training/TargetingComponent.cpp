// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/TargetingComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Training/FighterCharacter.h"

UTargetingComponent::UTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTargetingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTarget();
	SetPreferredTarget(nullptr);
	Super::EndPlay(EndPlayReason);
}

AFighterCharacter* UTargetingComponent::GetOwnerFighter() const
{
	return Cast<AFighterCharacter>(GetOwner());
}

void UTargetingComponent::HandleTargetDestroyed(AActor* DestroyedActor)
{
	if (bHasTarget && (!CurrentTarget.IsValid() || DestroyedActor == CurrentTarget.Get()))
	{
		UE_LOG(LogTemp, Log, TEXT("[Targeting] %s 的目标 %s 已销毁，安全解除"), *GetNameSafe(GetOwner()), *GetNameSafe(DestroyedActor));
		ClearTargetInternal(true);
	}
}

void UTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bHasTarget && !IsFighterLockable(CurrentTarget.Get()))
	{
		ClearTargetInternal(true);
	}
}

bool UTargetingComponent::IsFighterLockable(const AFighterCharacter* Candidate) const
{
	const AFighterCharacter* OwnerFighter = GetOwnerFighter();
	if (!IsValid(OwnerFighter) || !IsValid(Candidate) || Candidate == OwnerFighter || Candidate->GetWorld() != GetWorld())
	{
		return false;
	}
	if (Candidate->IsActorBeingDestroyed() || !Candidate->HasActorBegunPlay() || Candidate->IsPendingKillPending())
	{
		return false;
	}
	// M1 无死亡标签；死亡判定接入 GAS 后在此扩展
	if (OwnerFighter != nullptr)
	{
		const float DistSq = FVector::DistSquared(OwnerFighter->GetActorLocation(), Candidate->GetActorLocation());
		if (DistSq > FMath::Square(MaxLockRange))
		{
			return false;
		}
	}
	return true;
}

bool UTargetingComponent::LockTarget(AFighterCharacter* Candidate)
{
	if (!IsFighterLockable(Candidate))
	{
		UE_LOG(LogTemp, Log, TEXT("[Targeting] %s 锁定失败：候选 %s 无效"), *GetNameSafe(GetOwner()), *GetNameSafe(Candidate));
		return false;
	}

	if (CurrentTarget.Get() == Candidate)
	{
		return true;
	}

	AFighterCharacter* OldTarget = CurrentTarget.Get();
	if (OldTarget != nullptr)
	{
		OldTarget->OnDestroyed.RemoveAll(this);
	}

	CurrentTarget = Candidate;
	bHasTarget = true;
	SetComponentTickEnabled(true);
	Candidate->OnDestroyed.AddUniqueDynamic(this, &UTargetingComponent::HandleTargetDestroyed);

	UE_LOG(LogTemp, Log, TEXT("[Targeting] %s 锁定 %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Candidate));
	OnTargetChanged.Broadcast(Candidate);
	return true;
}

AFighterCharacter* UTargetingComponent::FindBestTarget() const
{
	// 首选 GameMode 分配的对手；不可用时退化到范围内最近的其他战斗角色
	if (IsFighterLockable(PreferredTarget.Get()))
	{
		return PreferredTarget.Get();
	}

	AFighterCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<AFighterCharacter> It(GetWorld()); It; ++It)
	{
		AFighterCharacter* Other = *It;
		if (!IsFighterLockable(Other))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(GetOwnerFighter()->GetActorLocation(), Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Other;
		}
	}
	return Best;
}

bool UTargetingComponent::LockBestTarget()
{
	AFighterCharacter* Best = FindBestTarget();
	return Best != nullptr && LockTarget(Best);
}

void UTargetingComponent::ClearTarget()
{
	ClearTargetInternal(false);
}

void UTargetingComponent::ClearTargetInternal(bool bInvalidated)
{
	AFighterCharacter* OldTarget = CurrentTarget.Get();
	if (!bHasTarget)
	{
		return;
	}
	if (OldTarget != nullptr)
	{
		OldTarget->OnDestroyed.RemoveAll(this);
	}
	CurrentTarget = nullptr;
	bHasTarget = false;
	SetComponentTickEnabled(false);
	if (bInvalidated)
	{
		OnTargetInvalidated.Broadcast();
	}
	OnTargetChanged.Broadcast(nullptr);
	UE_LOG(LogTemp, Log, TEXT("[Targeting] %s 解除目标 %s"), *GetNameSafe(GetOwner()), *GetNameSafe(OldTarget));
}

AFighterCharacter* UTargetingComponent::GetCurrentTarget() const
{
	return IsTargetValid() ? CurrentTarget.Get() : nullptr;
}

bool UTargetingComponent::IsTargetValid() const
{
	return IsFighterLockable(CurrentTarget.Get());
}

void UTargetingComponent::SetPreferredTarget(AFighterCharacter* Candidate)
{
	PreferredTarget = Candidate;
}

AFighterCharacter* UTargetingComponent::GetPreferredTarget() const
{
	return PreferredTarget.Get();
}
