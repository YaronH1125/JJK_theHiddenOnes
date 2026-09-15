// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Training/TrainingTypes.h"
#include "TrainingGameMode.generated.h"

class AFighterCharacter;
class AController;
class UFighterDefinition;

/** 对手行为模式；Static 仅停止主动决策，角色 Tick/ASC/碰撞/动画保持正常 */
UENUM(BlueprintType)
enum class EOpponentMode : uint8
{
	/** 无主动行为木桩 */
	Static
};

/**
 * 训练场 GameMode：生成双方、分配身份与首选目标、留存训练配置。
 * 生成是唯一入口：本类负责创建玩家与对手，RestartPlayer 被覆写为接管
 * 预生成的玩家角色，避免默认 Pawn 与手工生成并存造成重复角色。
 */
UCLASS()
class ATrainingGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATrainingGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** 玩家角色类（薄蓝图） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
	TSubclassOf<AFighterCharacter> FighterClass;

	/** 双方共用的角色定义（只读配置） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
	TObjectPtr<UFighterDefinition> FighterDefinition;

	/** 玩家出生地面变换（Z 为地面高度，朝向 +X） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
	FTransform PlayerSpawnGroundTransform;

	/** 对手出生地面变换（Z 为地面高度，朝向 -X） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
	FTransform OpponentSpawnGroundTransform;

	/** 出生安全余量：Capsule 半高之上再加的高度（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SpawnSafetyMargin = 4.f;

	/** 对手模式（M1 仅 Static） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
	EOpponentMode OpponentMode = EOpponentMode::Static;

	/** 玩家身份标识颜色 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
	FLinearColor PlayerMarkerColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);

	/** 对手身份标识颜色 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
	FLinearColor OpponentMarkerColor = FLinearColor(1.0f, 0.35f, 0.2f, 1.0f);

	UFUNCTION(BlueprintPure, Category = "Training")
	AFighterCharacter* GetPlayerFighter() const { return PlayerFighter; }

	UFUNCTION(BlueprintPure, Category = "Training")
	AFighterCharacter* GetOpponentFighter() const { return OpponentFighter; }

	/** 取某角色的对手；未生成或传空返回 null */
	UFUNCTION(BlueprintPure, Category = "Training")
	AFighterCharacter* GetOpponentOf(const AFighterCharacter* Fighter) const;

	/** 生成双方（幂等）；并记录初始变换、分配身份与首选目标 */
	UFUNCTION(BlueprintCallable, Category = "Training")
	void EnsureFightersSpawned();

protected:
	AFighterCharacter* SpawnFighter(EFighterRole InRole, const FTransform& GroundTransform);

	/** 按地面变换与胶囊半高计算角色中心出生变换 */
	FTransform ResolveSpawnTransform(const FTransform& GroundTransform) const;

	UPROPERTY()
	TObjectPtr<AFighterCharacter> PlayerFighter;

	UPROPERTY()
	TObjectPtr<AFighterCharacter> OpponentFighter;
};
