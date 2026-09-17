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
 * 共享动作请求入口（M2/M3）：玩家输入与调试对手程序化调用都从这里提交，
 * 结果（执行/拒绝原因）经返回值、日志与委托广播。
 *
 * 输入缓存（M3.2）：攻击中提交的动作进入单槽缓存（后到覆盖，带寿命），
 * 由活动 GA 在衔接窗口内消费并重新校验；过期/死亡/重置丢弃。
 * 持续防御（F）独立记录按下意图；菜单/失焦/死亡/重置释放。
 */
UCLASS(ClassGroup = (JJK), meta = (BlueprintSpawnableComponent))
class UCombatInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatInputComponent();

	/** 左键按下：建立会话（重复按下忽略） */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyAttackPressed();

	/** 左键松开：按持续时长分流轻拳（连段）/重拳意图 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyAttackReleased();

	/** Q 按下：腿击会话 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyKickPressed();

	/** Q 松开：按持续时长分流腿击/重踢 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyKickReleased();

	/** F 按下：持续防御意图（能否生效由受击结算按状态判定） */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyGuardPressed();

	/** F 松开：清除防御意图 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyGuardReleased();

	/** Shift 按下：闪避/闪避取消请求；Direction 为移动输入方向（世界空间，可零向量=后撤） */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyDodgePressed(FVector Direction);

	/** E 按下：切换形态（近/远程；M3 远程不发炮） */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyStanceSwitchPressed();

	/** R 按下：领域展开（瞬发结印；近远程形态均可） */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyDomainPressed();

	/** 会话失效：死亡/中断/失焦/重置调用；旧松键不再提交 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void InvalidateSession(const FText& Reason);

	/** 释放持续输入意图（防御等）；菜单/失焦/重置调用 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ReleaseContinuousInputs();
	bool AreRequestsEnabled() const { return bRequestsEnabled; }
 void SetRequestsEnabled(bool bEnabled);
	bool IsKickSessionActive() const { return bKickSessionActive; }

	/** 远程路径在按：左键/Q 按下已走蓄力炮，松开走发射而非近战分流 */
	bool IsRangedLmbSession() const { return bRangedLmbSession; }
	bool IsRangedQSession() const { return bRangedQSession; }

	/** 提交一段轻拳攻击（共享入口；连击中自动进缓存） */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	EActionRequestResult SubmitLightAttack();

	/** 提交腿击（共享入口） */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	EActionRequestResult SubmitKick();

	/** 提交重拳（共享入口） */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	EActionRequestResult SubmitHeavyPunch();

	/** 提交重踢（共享入口） */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	EActionRequestResult SubmitHeavyKick();

	/** 防御意图（按住 F） */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsGuardIntent() const { return bGuardIntent; }

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

	// ---------- 供 GA 消费的输入缓存（M3.2 单槽） ----------

	/** 当前缓存动作；无缓存或已过期返回 None */
	UFUNCTION(BlueprintPure, Category = "Combat")
	ECachedAction PeekCachedAction() const;

	/** 消费缓存（仅当动作与期望一致；寿命与 GA 允许集由 GA 校验） */
	void ConsumeCache(bool bConsumed = false);
 uint64 GetCacheId() const { return CacheCounter; }
 uint64 GetConsumedCacheId() const { return ConsumedCacheId; }
 void ClearOwnedCache(uint64 Id) { if (Id && Id == CacheCounter) ConsumeCache(); }

protected:
	virtual void BeginPlay() override;

	AFighterCharacter* GetOwnerFighter() const;
	double Now() const;

	void CacheAction(ECachedAction Action);
	bool TryStartSequence(ECachedAction Action);

	void HandleLightRelease();
	void HandleKickRelease();

	/** 左键会话 */
	bool bSessionActive = false;
	int32 SessionCounter = 0;
	int32 ActiveSessionId = 0;
	double PressGameTime = 0.0;
	/** Q 会话 */
	bool bKickSessionActive = false;
	double KickPressGameTime = 0.0;
	/** 远程形态按下标记：LMB/Q 走蓄力炮，松开只负责发射 */
	bool bRangedLmbSession = false;
	bool bRangedQSession = false;

	/** 防御意图（按住 F） */
	bool bGuardIntent = false;

	/** 单槽输入缓存 */
	bool bRequestsEnabled = true;
	FVector CachedDirection = FVector::ZeroVector;
	TWeakObjectPtr<AFighterCharacter> CachedTarget;
	bool bCacheHadTarget = false;
	uint64 CacheCounter = 0, ConsumedCacheId = 0;
	ECachedAction CachedAction = ECachedAction::None;
	double CachedActionTime = 0.0;
};