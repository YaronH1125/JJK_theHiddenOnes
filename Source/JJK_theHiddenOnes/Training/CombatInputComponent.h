// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/CombatTypes.h"
#include "CombatInputComponent.generated.h"

class AFighterCharacter;
class UGameplayAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJJKOnRequestResult, EActionRequestResult, Result, int32, SessionId);

/**
 * 共享动作请求入口（M2.1）：玩家输入与调试对手程序化调用都从这里提交攻击意图，
 * 结果（执行/拒绝原因）通过返回值、日志与委托广播。本阶段不实现完整连招缓存。
 *
 * 轻重输入会话（08 第 4.2 节）：按下建立 InputSessionId；阈值前松开提交单段轻拳；
 * 达到阈值只记录 HeavyPunch 占位意图（无动作无伤害，不补发轻拳）。
 * 松开、中断、死亡、失焦、重置清会话；旧松键不补攻击。
 */
UCLASS(ClassGroup = (JJK), meta = (BlueprintSpawnableComponent))
class UCombatInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatInputComponent();

	/** 输入按下：建立会话（重复按下忽略，不重建） */
	void NotifyAttackPressed();

	/** 输入松开：按持续时长分流轻拳/重拳意图，随后清会话 */
	void NotifyAttackReleased();

	/** 会话失效：死亡/中断/失焦/重置调用；旧松键不再提交 */
	void InvalidateSession(const FText& Reason);

	/** 提交一段轻拳攻击（共享入口，玩家与调试对手一致） */
	EActionRequestResult SubmitLightAttack();

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetActiveSessionId() const { return bSessionActive ? ActiveSessionId : 0; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsSessionActive() const { return bSessionActive; }

	/** 请求结果广播（调试显示用） */
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FJJKOnRequestResult OnRequestResult;

	/** 轻重分界起测阈值（08 第 4.2 节建议 0.18 秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float LightHeavyThreshold = 0.18f;

protected:
	virtual void BeginPlay() override;

	AFighterCharacter* GetOwnerFighter() const;

	bool bSessionActive = false;
	int32 SessionCounter = 0;
	int32 ActiveSessionId = 0;
	double PressGameTime = 0.0;
};
