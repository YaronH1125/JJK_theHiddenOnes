// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/ArenaPlayerController.h"

#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterAttributeSet.h"
#include "Training/TargetingComponent.h"
#include "Training/TrainingGameMode.h"

void AArenaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (LockTargetAction != nullptr)
		{
			EnhancedInputComponent->BindAction(LockTargetAction, ETriggerEvent::Started, this, &AArenaPlayerController::HandleLockInput);
		}
		if (RecenterCameraAction != nullptr)
		{
			EnhancedInputComponent->BindAction(RecenterCameraAction, ETriggerEvent::Started, this, &AArenaPlayerController::HandleRecenterInput);
		}
	}
}

void AArenaPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogTemp, Log, TEXT("[ArenaPC] %s 接管 %s"), *GetName(), *GetNameSafe(InPawn));
}

AFighterCharacter* AArenaPlayerController::GetPlayerFighter() const
{
	return Cast<AFighterCharacter>(GetPawn());
}

ATrainingGameMode* AArenaPlayerController::GetTrainingGameMode() const
{
	return GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
}

void AArenaPlayerController::HandleLockInput()
{
	ToggleLock();
}

void AArenaPlayerController::HandleRecenterInput()
{
	RecenterCameraToTarget();
}

void AArenaPlayerController::ToggleLock()
{
	AFighterCharacter* Fighter = GetPlayerFighter();
	if (Fighter == nullptr || Fighter->GetTargeting() == nullptr)
	{
		return;
	}

	if (Fighter->GetTargeting()->IsTargetValid())
	{
		Fighter->GetTargeting()->ClearTarget();
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 2.f, FColor::Silver, TEXT("目标已解除"));
	}
	else if (Fighter->GetTargeting()->LockBestTarget())
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 2.f, FColor::Yellow,
			FString::Printf(TEXT("锁定: %s"), *GetNameSafe(Fighter->GetTargeting()->GetCurrentTarget())));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 2.f, FColor::Silver, TEXT("无有效目标"));
	}
}

void AArenaPlayerController::RecenterCameraToTarget()
{
	AFighterCharacter* Fighter = GetPlayerFighter();
	if (Fighter == nullptr || Fighter->GetTargeting() == nullptr || !Fighter->GetTargeting()->IsTargetValid())
	{
		return;
	}

	const AFighterCharacter* Target = Fighter->GetTargeting()->GetCurrentTarget();
	const FVector ToTarget = Target->GetActorLocation() - Fighter->GetActorLocation();
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));

	const FRotator Current = GetControlRotation();
	SetControlRotation(FRotator(Current.Pitch, TargetYaw, 0.f));

	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) + 1, 1.5f, FColor::Silver, TEXT("镜头回正"));
}

void AArenaPlayerController::JJKFighters()
{
	const ATrainingGameMode* GM = GetTrainingGameMode();
	if (GM == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[JJKFighters] 当前 GameMode 不是 ATrainingGameMode"));
		return;
	}

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const AFighterCharacter* Fighter = Index == 0 ? GM->GetPlayerFighter() : GM->GetOpponentFighter();
		if (Fighter == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[JJKFighters] 槽位 %d 为空"), Index);
			continue;
		}
		const UFighterAttributeSet* Attributes = Fighter->GetFighterAttributeSet();
		const UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent();

		UE_LOG(LogTemp, Log, TEXT("[JJKFighters] %s 角色=%s Owner=%s Avatar=%s Health=%.1f/%.1f Action=%.1f/%.1f Energy=%.1f/%.1f 初始化次数=%d 目标=%s"),
			Index == 0 ? TEXT("玩家") : TEXT("对手"),
			*GetNameSafe(Fighter),
			ASC != nullptr ? *GetNameSafe(ASC->GetOwnerActor()) : TEXT("null"),
			ASC != nullptr ? *GetNameSafe(ASC->GetAvatarActor()) : TEXT("null"),
			Attributes ? Attributes->GetHealth() : -1.f,
			Attributes ? Attributes->GetMaxHealth() : -1.f,
			Attributes ? Attributes->GetActionResource() : -1.f,
			Attributes ? Attributes->GetMaxActionResource() : -1.f,
			Attributes ? Attributes->GetEnergy() : -1.f,
			Attributes ? Attributes->GetMaxEnergy() : -1.f,
			Fighter->IsStatsInitialized() ? 1 : 0,
			Fighter->GetTargeting() != nullptr ? *GetNameSafe(Fighter->GetTargeting()->GetCurrentTarget()) : TEXT("null"));
	}
}

void AArenaPlayerController::JJKReinitFighters()
{
	ATrainingGameMode* GM = GetTrainingGameMode();
	if (GM == nullptr)
	{
		return;
	}
	if (AFighterCharacter* P1Fighter = GM->GetPlayerFighter())
	{
		P1Fighter->InitializeFromDefinition();
	}
	if (AFighterCharacter* Opponent = GM->GetOpponentFighter())
	{
		Opponent->InitializeFromDefinition();
	}
	UE_LOG(LogTemp, Log, TEXT("[JJKReinitFighters] 已重复调用集中初始化入口（应无数值变化/重复授予）"));
	JJKFighters();
}

void AArenaPlayerController::JJKResetFighters()
{
	ATrainingGameMode* GM = GetTrainingGameMode();
	if (GM == nullptr)
	{
		return;
	}
	if (AFighterCharacter* P1Fighter = GM->GetPlayerFighter())
	{
		P1Fighter->ResetToInitialState();
	}
	if (AFighterCharacter* Opponent = GM->GetOpponentFighter())
	{
		Opponent->ResetToInitialState();
	}
	UE_LOG(LogTemp, Log, TEXT("[JJKResetFighters] 双方已重置"));
	JJKFighters();
}

void AArenaPlayerController::JJKKillTarget()
{
	AFighterCharacter* P1Fighter = GetPlayerFighter();
	ATrainingGameMode* GM = GetTrainingGameMode();
	if (P1Fighter == nullptr || GM == nullptr || P1Fighter->GetTargeting() == nullptr)
	{
		return;
	}

	AFighterCharacter* Victim = P1Fighter->GetTargeting()->GetCurrentTarget();
	if (Victim == nullptr)
	{
		Victim = GM->GetOpponentFighter();
	}
	if (Victim == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[JJKKillTarget] 无可销毁目标"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[JJKKillTarget] 调试销毁 %s"), *GetNameSafe(Victim));
	Victim->Destroy();
}
