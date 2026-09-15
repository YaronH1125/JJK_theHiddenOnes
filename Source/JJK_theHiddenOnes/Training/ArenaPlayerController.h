// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArenaPlayerController.generated.h"

class AFighterCharacter;
class ATrainingGameMode;
class UInputAction;

/**
 * 训练场玩家控制器：输入入口与镜头协调。
 * 锁定/回正仅改变观察目标或一次性对准，不逐帧强制镜头跟随目标。
 */
UCLASS()
class AArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 切换锁定（按下触发） */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Training")
	TObjectPtr<UInputAction> LockTargetAction;

	/** 辅助回正：一次性把观察朝向对准当前目标（按下触发） */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Training")
	TObjectPtr<UInputAction> RecenterCameraAction;

	/** 攻击（左键）：按下建立会话，松开按轻重阈值提交（M2.1） */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Training")
	TObjectPtr<UInputAction> AttackAction;

	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 锁定/解除切换；有目标则解除，否则锁定最优对手 */
	UFUNCTION(BlueprintCallable, Category = "Training|Targeting")
	void ToggleLock();

	/** 一次性把控制旋转的水平朝向对准当前目标，保留俯仰 */
	UFUNCTION(BlueprintCallable, Category = "Training|Targeting")
	void RecenterCameraToTarget();

	UFUNCTION(BlueprintPure, Category = "Training")
	AFighterCharacter* GetPlayerFighter() const;

	UFUNCTION(BlueprintPure, Category = "Training")
	ATrainingGameMode* GetTrainingGameMode() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	void HandleLockInput();
	void HandleRecenterInput();
	void HandleAttackPressed();
	void HandleAttackReleased();

	/** 应用失焦：清攻击会话，恢复后要求重新按下（08 第 4.2 节） */
	UFUNCTION(BlueprintCallable, Category = "Training|Input")
	void HandleAppActivationChanged(bool bActive);

	/** 调试：打印双方 GAS 状态（Owner/Avatar/数值/初始化次数） */
	UFUNCTION(Exec, Category = "Training|Debug")
	void JJKFighters();

	/** 调试：对双方重复执行集中初始化入口，验证幂等 */
	UFUNCTION(Exec, Category = "Training|Debug")
	void JJKReinitFighters();

	/** 调试：训练重置双方 */
	UFUNCTION(Exec, Category = "Training|Debug")
	void JJKResetFighters();

	/** 调试：销毁当前目标（或未锁定时的对手），验证失效安全解除与重新分配 */
	UFUNCTION(Exec, Category = "Training|Debug")
	void JJKKillTarget();

	/** 补齐已销毁的角色并重新分配目标。 */
	UFUNCTION(Exec, Category = "Training|Debug")
	void JJKRespawnFighters();
};
