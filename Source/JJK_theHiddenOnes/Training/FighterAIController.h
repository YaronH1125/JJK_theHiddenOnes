// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "FighterAIController.generated.h"

class AFighterCharacter;
class ATrainingGameMode;
class UCombatInputComponent;

/** 决策分支（与行为树分支一一对应：接近/侧移/后退/攻击/防御/闪避/等待） */
UENUM(BlueprintType)
enum class EAIBranch : uint8
{
	Wait,
	Approach,
	Strafe,
	Retreat,
	Attack,
	Defend,
	Dodge,
	/** 远程形态：移动蓄力炮（M6） */
	RangedAttack,
	/** 领域展开（M6：能量满且目标在捕获范围） */
	Domain
};

/** 行为参数（反应延迟/决策间隔/距离阈值/保持时间/动作偏好） */
USTRUCT(BlueprintType)
struct FArenaAIParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI") float AttackRange = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI") float AttackRangeExit = 260.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI") float RetreatDistance = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.1")) float DecisionInterval = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0")) float ReactionDelay = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0")) float MinBranchHoldTime = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI") float StrafeSwitchInterval = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "1.0")) float AttackChance = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "1.0")) float HeavyChance = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "1.0")) float DefendChance = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "1.0")) float DodgeChance = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "1.0")) float RangedBlastChance = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "1.0")) float DomainChance = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "1.0")) float StanceSwitchChance = 0.4f;
	/** M6 总开关：启用后 AI 才有远程炮/领域/切形态决策（默认关闭，保持 M5 近战行为不变） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI") bool bEnableRangedCombat = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI") float DefendHoldTime = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI") int32 RandomSeed = 1337;
};

/** 原生 Behavior Tree 控制器；Tick 只更新可观察快照及停止冲突寻路。 */
UCLASS()
class AFighterAIController : public AAIController
{
 GENERATED_BODY()
public:
 AFighterAIController();
 UFUNCTION(BlueprintCallable, Category="AI") void ActivateAI(AFighterCharacter* Target);
 UFUNCTION(BlueprintCallable, Category="AI") void DeactivateAI();
 UFUNCTION(BlueprintPure, Category="AI") bool IsAIActive() const;
 UFUNCTION(BlueprintPure, Category="AI") EAIBranch GetCurrentBranch() const;
 UFUNCTION(BlueprintPure, Category="AI") FArenaAIParams GetParams() const { return Params; }
 UFUNCTION(BlueprintCallable, Category="AI") void SetParams(FArenaAIParams Value);
 UFUNCTION(BlueprintPure, Category="AI") TArray<FString> GetDecisionLog() const { return DecisionLog; }
 UFUNCTION(BlueprintPure, Category="AI") FString GetDebugState() const;
 UFUNCTION(BlueprintPure, Category="AI") int32 GetActionBindingCount() const { return ActionBindingCount; }
 UFUNCTION(BlueprintPure, Category="AI") bool IsNavigationReady() const;
 UFUNCTION(BlueprintPure, Category="AI") bool IsLegalDestination(FVector Point) const;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI") FArenaAIParams Params;
 UPROPERTY(Transient, BlueprintReadOnly, Category="AI") TObjectPtr<class UBehaviorTree> ArenaTree;
 UPROPERTY(Transient, BlueprintReadOnly, Category="AI") TObjectPtr<class UBlackboardData> ArenaBlackboard;
 /** 开发复现入口：下一次树决策指定分支，仍经过完整共享请求规则。Shipping 无效。 */
 UFUNCTION(BlueprintCallable, Category="AI|Debug") void DebugRequestBranch(EAIBranch Branch);
 void StopPathKeepingVelocity();
 void Observe();
 void Decide();
 bool BeginMove(EAIBranch Branch);
 bool CanRun() const;
 AFighterCharacter* GetSelf() const;
 AFighterCharacter* GetTarget() const;
 double Now() const;
 float RandomFraction() { return RandomStream.FRand(); }
 void SetTaskStatus(const FString& Status);
 void AdjustActionBindings(int32 Delta) { ActionBindingCount += Delta; }
 virtual void Tick(float DeltaSeconds) override;
protected:
 virtual void OnUnPossess() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 void BuildTree();
 void LogDecision(const FString& Text);
 bool ProjectDestination(const FVector& Point, FVector& Out) const;
 bool bActive = false;
 TOptional<EAIBranch> DebugBranch;
 FRandomStream RandomStream;
 TArray<FString> DecisionLog;
 FString TaskStatus = TEXT("Stopped");
 int32 ActionBindingCount = 0;
 double LastBranchSwitch = -1000., ObservedAttackAt = -1., NextMoveAttempt = 0., LastStrafeFlip = 0.;
 float StrafeSign = 1.f;
};
