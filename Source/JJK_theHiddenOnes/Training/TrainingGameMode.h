// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Training/TrainingTypes.h"
#include "Training/TrainingSettings.h"
#include "GameplayEffectTypes.h"
#include "Training/FighterAIController.h"
#include "TrainingGameMode.generated.h"

class AFighterCharacter;
class AController;
class AFighterAIController;
class ADomainOrb;
class UFighterDefinition;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTrainingChanged);

/** AI 对战胜负（M5.6：只结算一次；同批次双亡为平局） */
UENUM(BlueprintType)
enum class EMatchOutcome : uint8
{
	None,
	PlayerWin,
	OpponentWin,
	Draw
};

/** 对手行为模式；Static 仅停止主动决策，角色 Tick/ASC/碰撞/动画保持正常 */
UENUM(BlueprintType)
enum class EOpponentMode : uint8
{
	/** 无主动行为木桩 */
	Static,
	FixedGuard,
	AI
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
 UPROPERTY(BlueprintReadOnly, Category="Training") FTrainingSettings Settings;
 UPROPERTY(BlueprintReadOnly, Category="Training") FTrainingStats PlayerStats;
 UPROPERTY(BlueprintReadOnly, Category="Training") FTrainingStats OpponentStats;
 UPROPERTY(BlueprintReadOnly, Category="Training") TArray<FString> InputHistory;
 UPROPERTY(BlueprintAssignable, Category="Training") FTrainingChanged OnTrainingChanged;
 UFUNCTION(BlueprintCallable, Category="Training") void SetTrainingSettings(FTrainingSettings Value);
 UFUNCTION(BlueprintCallable, Category="Training") bool SetOpponentMode(EOpponentMode Value);
 UFUNCTION(BlueprintPure, Category="Training") bool IsModeAvailable(EOpponentMode Value) const { return Value==EOpponentMode::Static || Value==EOpponentMode::FixedGuard || Value==EOpponentMode::AI; }
 UFUNCTION(BlueprintPure, Category="Training") EMatchOutcome GetMatchOutcome() const { return MatchOutcome; }
 UFUNCTION(BlueprintPure, Category="Training") bool IsMatchResolved() const { return bMatchResolved; }
 UFUNCTION(BlueprintCallable, Category="Training") void RestartMatch();
 UFUNCTION(BlueprintCallable, Category="Training") void SetTrainingMenuOpen(bool bOpen);
 UFUNCTION(BlueprintPure, Category="Training") bool IsTrainingMenuOpen() const { return bMenuOpen; }
 UFUNCTION(BlueprintCallable, Category="Training|Debug") bool RequestTrainingProbe(AFighterCharacter* Fighter);
 UFUNCTION(BlueprintPure, Category="Training|Debug") int32 GetAttributeBindingCount() const { return AttributeBindings.Num(); }
 UFUNCTION(BlueprintPure, Category="AI") AFighterAIController* GetOpponentAI() const { return OpponentAI; }
 UFUNCTION(BlueprintPure, Category="AI") int32 GetMatchResolutionCount() const { return MatchResolutionCount; }
 void RecordContact(AFighterCharacter* Source, AFighterCharacter* Target, ETrainingContact Kind, float Raw, float Resolved, float Lost);
 void RecordInput(AFighterCharacter* Source, const FString& Text);
 void NotifyTrainingChanged();
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;


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

	/**
	 * 训练重置（M2.6）：停请求 → 取消能力 → 清临时/事件 → 复位双方 →
	 * 恢复属性与命中统计 → 恢复目标与模式。供调试入口与后续训练面板复用。
	 */
	UFUNCTION(BlueprintCallable, Category = "Training")
	void ResetTraining();

	/** 领域展开：结印完成后由 DomainExpansionAbility 调用 */
	bool TryOpenDomain(AFighterCharacter* Caster);
	/** 清所有领域会话（训练重置用） */
	void ShutdownAllDomains();

	/** 调试：对手经共享请求入口提交一段轻拳（合法性与玩家一致） */
	UFUNCTION(Exec, Category = "Training|Debug")
	void JJKOpponentAttack();

	/** 调试：开关战斗 HUD（请求结果/阶段/标签/实例/命中/生命） */
	UFUNCTION(Exec, Category = "Training|Debug")
	void JJKDebugHud();
	UFUNCTION(Exec, Category = "Training|Debug")
	void JJKOpponentGuard(bool bHeld);
	UFUNCTION(Exec, Category = "Training|Debug")
	void JJKOpponentAction(int32 Action);

protected:
	virtual void Tick(float DeltaSeconds) override;

	void DrawCombatDebug() const;

	/** 调试 HUD 显隐 */
 bool bDebugHud = false;
 bool bMenuOpen = false, bResetting = false;
 struct FAttributeBinding { TWeakObjectPtr<class UFighterAbilitySystemComponent> ASC; FGameplayAttribute Attribute; FDelegateHandle Handle; };
 TArray<FAttributeBinding> AttributeBindings;
 TWeakObjectPtr<AFighterCharacter> BoundPlayer, BoundOpponent;
 FTimerHandle PlayerRecoveryTimer, OpponentRecoveryTimer, CooldownRefreshTimer;
 void BindFighters();
 void UnbindFighters();
 void ApplyOpponentMode();
 void StopActiveIntent(AFighterCharacter* Fighter);
 void EnsureOpponentAI();
 void ShutdownOpponentAI();
 void CheckMatchOutcome();
 void ResolveMatchOutcome(EMatchOutcome Outcome);
 void ResolveRecovery(AFighterCharacter* Victim);
 void ScheduleRecovery(AFighterCharacter* Victim);
 void ClearTrainingCooldowns(AFighterCharacter* Fighter);
 UFUNCTION() void OnPlayerRecovered();
 UFUNCTION() void OnOpponentRecovered();
 UFUNCTION() void OnFighterDestroyed(AActor* Actor);

	// ---------- M6 领域会话 ----------
	struct FDomainSessionData
	{
		int32 SessionId = 0;
		TWeakObjectPtr<AFighterCharacter> Caster;
		TWeakObjectPtr<AFighterCharacter> Victim;
		double EndTime = 0.0;
		double NextSpawnTime = 0.0;
		bool bSuppressed = false;
		/** 在飞咒球（弱引用：球自然销毁/被打掉后自动剔除） */
		TArray<TWeakObjectPtr<class ADomainOrb>> Orbs;
	};
	TArray<FDomainSessionData> DomainSessions;
	int32 NextDomainSessionId = 0;
	void TickDomainSessions();
	void EndDomainSession(int32 SessionId);
	void SpawnDomainOrb(FDomainSessionData& Session);


	AFighterCharacter* SpawnFighter(EFighterRole InRole, const FTransform& GroundTransform);

	/** 按地面变换与胶囊半高计算角色中心出生变换 */
	FTransform ResolveSpawnTransform(const FTransform& GroundTransform) const;

	UPROPERTY()
	TObjectPtr<AFighterCharacter> PlayerFighter;

	UPROPERTY()
	TObjectPtr<AFighterCharacter> OpponentFighter;

	UPROPERTY()
	TObjectPtr<AFighterAIController> OpponentAI;

	/** AI 对战胜负（M5.6：只结算一次） */
	UPROPERTY(BlueprintReadOnly, Category = "Training")
	EMatchOutcome MatchOutcome = EMatchOutcome::None;

	bool bMatchResolved = false;
 uint64 DeathObservedFrame = 0;
 int32 MatchResolutionCount = 0;
};
