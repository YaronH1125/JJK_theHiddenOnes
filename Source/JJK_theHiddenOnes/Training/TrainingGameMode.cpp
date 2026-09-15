// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/TrainingGameMode.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Training/ArenaPlayerController.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/CombatHitComponent.h"
#include "Training/CombatInputComponent.h"
#include "Training/CombatTypes.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Training/TargetingComponent.h"

ATrainingGameMode::ATrainingGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	// 擂台默认出生配置：24m 擂台上 P1(-500,0) 朝 +X，P2(500,0) 朝 -X（07_擂台与视觉参考.md）
	PlayerSpawnGroundTransform = FTransform(FRotator(0.f, 0.f, 0.f), FVector(-500.f, 0.f, 0.f));
	OpponentSpawnGroundTransform = FTransform(FRotator(0.f, 180.f, 0.f), FVector(500.f, 0.f, 0.f));

	// 玩家控制器使用训练场实现（锁定/回正输入语义与调试命令）
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void ATrainingGameMode::StartPlay()
{
	Super::StartPlay();
	EnsureFightersSpawned();
}

void ATrainingGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDebugHud)
	{
		DrawCombatDebug();
	}
}

void ATrainingGameMode::DrawCombatDebug() const
{
	int32 Line = 0;
	auto DrawFighter = [this, &Line](const TCHAR* Label, const AFighterCharacter* Fighter)
	{
		if (Fighter == nullptr)
		{
			GEngine->AddOnScreenDebugMessage(100 + Line++, 0.f, FColor::Silver,
				FString::Printf(TEXT("%s: 未生成"), Label));
			return;
		}
		const UCombatHitComponent* Hit = Fighter->GetCombatHit();
		const UCombatInputComponent* Input = Fighter->GetCombatInput();
		const UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent();
		const TCHAR* PhaseNames[] = {TEXT("None"), TEXT("Windup"), TEXT("Active"), TEXT("Recovery")};

		FString Status;
		if (Fighter->IsDead()) { Status = TEXT("死亡"); }
		else if (ASC && ASC->HasMatchingGameplayTag(TAG_State_HitStun)) { Status = TEXT("受击硬直"); }
		else if (Hit->HasActiveAttack()) { Status = FString::Printf(TEXT("攻击中(%s)"), PhaseNames[static_cast<int32>(Hit->GetPhase())]); }
		else { Status = TEXT("待机"); }

		GEngine->AddOnScreenDebugMessage(100 + Line++, 0.f,
			Fighter == PlayerFighter ? FColor::Cyan : FColor::Orange,
			FString::Printf(TEXT("%s [%s] HP=%.0f 阶段=%s 实例=%llu 命中=%d 会话=%d"),
				Label, *Status,
				Fighter->GetFighterAttributeSet() ? Fighter->GetFighterAttributeSet()->GetHealth() : -1.f,
				PhaseNames[static_cast<int32>(Hit->GetPhase())],
				Hit->GetActiveInstanceId(), Hit->GetHitCount(),
				Input->GetActiveSessionId()));
	};

	DrawFighter(TEXT("P1"), PlayerFighter);
	DrawFighter(TEXT("P2"), OpponentFighter);
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
	if (IsValid(PlayerFighter) && IsValid(OpponentFighter))
	{
		return;
	}

	if (FighterClass == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[TrainingGM] 未配置 FighterClass，无法生成双方"));
		return;
	}

	if (!IsValid(PlayerFighter))
	{
		PlayerFighter = SpawnFighter(EFighterRole::Player, PlayerSpawnGroundTransform);
	}
	if (!IsValid(OpponentFighter))
	{
		OpponentFighter = SpawnFighter(EFighterRole::Opponent, OpponentSpawnGroundTransform);
	}

	if (PlayerFighter != nullptr && OpponentFighter != nullptr)
	{
		// 分配身份对应的首选目标；是否锁定仍由玩家手动触发
		PlayerFighter->GetTargeting()->SetPreferredTarget(OpponentFighter);
		OpponentFighter->GetTargeting()->SetPreferredTarget(PlayerFighter);

		// 训练场身份颜色（角色定义仍是双方共享配置）
		PlayerFighter->SetMarkerColor(PlayerMarkerColor);
		OpponentFighter->SetMarkerColor(OpponentMarkerColor);

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
	// 已销毁对象可能尚未 GC；身份不依赖 UObject 名，重生允许唯一后缀。
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Params.bDeferConstruction = true;

	AFighterCharacter* Fighter = World->SpawnActor<AFighterCharacter>(FighterClass, ResolveSpawnTransform(GroundTransform), Params);
	if (Fighter == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[TrainingGM] SpawnActor 失败：Role=%d"), static_cast<int32>(InRole));
		return nullptr;
	}

	Fighter->SetRole(InRole);
	if (FighterDefinition != nullptr)
	{
		Fighter->Definition = FighterDefinition;
	}
	// 配置、身份与静止模式必须先于 Construction/BeginPlay 初始化。
	if (InRole == EFighterRole::Opponent && OpponentMode == EOpponentMode::Static)
	{
		Fighter->AutoPossessAI = EAutoPossessAI::Disabled;
		Fighter->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	}
	Fighter->FinishSpawning(Fighter->GetActorTransform());
	Fighter->RecordInitialTransform(Fighter->GetActorTransform());

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

void ATrainingGameMode::ResetTraining()
{
	// 训练重置顺序（02_架构设计.md 第 10 节 / M2.6）：
	// 1.停请求 2.取消能力 3.清临时对象/事件 4.复位双方 5.恢复属性统计 6.恢复目标与模式
	UE_LOG(LogTemp, Log, TEXT("[TrainingGM] 训练重置开始"));
	for (AFighterCharacter* Fighter : {PlayerFighter.Get(), OpponentFighter.Get()})
	{
		if (Fighter != nullptr)
		{
			Fighter->ResetToInitialState();
		}
	}

	// 恢复目标分配与身份标识（6）
	if (IsValid(PlayerFighter) && IsValid(OpponentFighter))
	{
		PlayerFighter->GetTargeting()->SetPreferredTarget(OpponentFighter);
		OpponentFighter->GetTargeting()->SetPreferredTarget(PlayerFighter);
	}
	UE_LOG(LogTemp, Log, TEXT("[TrainingGM] 训练重置完成"));
}

void ATrainingGameMode::JJKOpponentAttack()
{
	if (OpponentFighter == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainingGM] 对手未生成"));
		return;
	}
	// 调试对手走同一共享请求入口（M2.1）：合法性与玩家完全一致
	OpponentFighter->GetCombatInput()->SubmitLightAttack();
}

void ATrainingGameMode::JJKDebugHud()
{
	bDebugHud = !bDebugHud;
	UE_LOG(LogTemp, Log, TEXT("[TrainingGM] 调试 HUD = %d"), bDebugHud ? 1 : 0);
}
