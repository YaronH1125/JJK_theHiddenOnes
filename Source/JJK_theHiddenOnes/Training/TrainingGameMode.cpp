// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/TrainingGameMode.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Training/TargetingComponent.h"

ATrainingGameMode::ATrainingGameMode()
{
	// 擂台默认出生配置：24m 擂台上 P1(-500,0) 朝 +X，P2(500,0) 朝 -X（07_擂台与视觉参考.md）
	PlayerSpawnGroundTransform = FTransform(FRotator(0.f, 0.f, 0.f), FVector(-500.f, 0.f, 0.f));
	OpponentSpawnGroundTransform = FTransform(FRotator(0.f, 180.f, 0.f), FVector(500.f, 0.f, 0.f));
}

void ATrainingGameMode::StartPlay()
{
	Super::StartPlay();
	EnsureFightersSpawned();
}

void ATrainingGameMode::RestartPlayer(AController* NewPlayer)
{
	// 默认 Pawn 不再生成：玩家控制器直接接管预生成的 P1
	APlayerController* PC = Cast<APlayerController>(NewPlayer);
	if (PC == nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[TrainingGM] RestartPlayer 忽略非玩家控制器 %s"), *GetNameSafe(NewPlayer));
		return;
	}

	EnsureFightersSpawned();

	if (PlayerFighter == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[TrainingGM] 玩家角色生成失败，无法接管"));
		return;
	}

	if (PlayerFighter->GetController() != nullptr && PlayerFighter->GetController() != PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainingGM] 玩家角色已被 %s 接管，拒绝重复 Possess"), *GetNameSafe(PlayerFighter->GetController()));
		return;
	}

	PC->Possess(PlayerFighter);
}

void ATrainingGameMode::EnsureFightersSpawned()
{
	if (PlayerFighter != nullptr && OpponentFighter != nullptr)
	{
		return;
	}

	if (FighterClass == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[TrainingGM] 未配置 FighterClass，无法生成双方"));
		return;
	}

	if (PlayerFighter == nullptr)
	{
		PlayerFighter = SpawnFighter(EFighterRole::Player, PlayerSpawnGroundTransform);
	}
	if (OpponentFighter == nullptr)
	{
		OpponentFighter = SpawnFighter(EFighterRole::Opponent, OpponentSpawnGroundTransform);
	}

	if (PlayerFighter != nullptr && OpponentFighter != nullptr)
	{
		// 分配身份对应的首选目标；是否锁定仍由玩家手动触发
		PlayerFighter->GetTargeting()->SetPreferredTarget(OpponentFighter);
		OpponentFighter->GetTargeting()->SetPreferredTarget(PlayerFighter);

		UE_LOG(LogTemp, Log, TEXT("[TrainingGM] 双方已生成：P1=%s P2=%s 模式=%s"),
			*GetNameSafe(PlayerFighter), *GetNameSafe(OpponentFighter),
			OpponentMode == EOpponentMode::Static ? TEXT("Static") : TEXT("Unknown"));
	}
}

AFighterCharacter* ATrainingGameMode::SpawnFighter(EFighterRole InRole, const FTransform& GroundTransform)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Name = InRole == EFighterRole::Player ? TEXT("Fighter_P1") : TEXT("Fighter_P2");

	AFighterCharacter* Fighter = World->SpawnActor<AFighterCharacter>(FighterClass, ResolveSpawnTransform(GroundTransform), Params);
	if (Fighter == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[TrainingGM] SpawnActor 失败：Role=%d"), static_cast<int32>(InRole));
		return nullptr;
	}

	Fighter->SetRole(InRole);
	Fighter->RecordInitialTransform(Fighter->GetActorTransform());
	if (Fighter->Definition == nullptr)
	{
		Fighter->Definition = FighterDefinition;
	}

	// 对手保持无控制器：木桩只停止主动决策，角色本体照常运行
	if (InRole == EFighterRole::Opponent && OpponentMode == EOpponentMode::Static)
	{
		UE_LOG(LogTemp, Log, TEXT("[TrainingGM] 对手以 Static 模式生成（无控制器）"));
	}

	return Fighter;
}

FTransform ATrainingGameMode::ResolveSpawnTransform(const FTransform& GroundTransform) const
{
	float CapsuleHalfHeight = 96.f;
	if (FighterClass != nullptr)
	{
		if (const AFighterCharacter* CDO = FighterClass->GetDefaultObject<AFighterCharacter>())
		{
			if (const UCapsuleComponent* Capsule = CDO->GetCapsuleComponent())
			{
				CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
			}
		}
	}

	const FVector GroundLocation = GroundTransform.GetLocation();
	const FVector CenterLocation(GroundLocation.X, GroundLocation.Y, GroundLocation.Z + CapsuleHalfHeight + SpawnSafetyMargin);
	return FTransform(GroundTransform.GetRotation(), CenterLocation, GroundTransform.GetScale3D());
}

AFighterCharacter* ATrainingGameMode::GetOpponentOf(const AFighterCharacter* Fighter) const
{
	if (Fighter == nullptr)
	{
		return nullptr;
	}
	return Fighter == PlayerFighter ? OpponentFighter.Get() : (Fighter == OpponentFighter ? PlayerFighter.Get() : nullptr);
}
