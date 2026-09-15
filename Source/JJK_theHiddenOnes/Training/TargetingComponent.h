// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingComponent.generated.h"

class AFighterCharacter;

/** 目标变更（含从无到有、从 A 换到 B）；NewTarget 可能为 null 表示清除 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJJKOnTargetChanged, AFighterCharacter*, NewTarget);

/** 当前目标因死亡/销毁等原因失效被清除 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FJJKOnTargetInvalidated);

/**
 * 目标选择与验证（02_架构设计.md 第 2 节）：仅负责目标身份、距离/有效性检查。
 * 不逐帧驱动镜头；辅助回正由 ArenaPlayerController 按输入触发。
 * 仅有锁定目标时启用 Tick 检查距离；不写入任何镜头或角色旋转。
 */
UCLASS(ClassGroup = (JJK), meta = (BlueprintSpawnableComponent))
class UTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetingComponent();

	/** 目标变更事件 */
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FJJKOnTargetChanged OnTargetChanged;

	/** 目标失效事件（死亡/销毁导致清除时触发一次） */
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FJJKOnTargetInvalidated OnTargetInvalidated;

	/** 锁定申请距离上限（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting", meta = (ForceUnits = "cm"))
	float MaxLockRange = 2000.f;

	/** 锁定候选目标；成功后广播 OnTargetChanged */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	bool LockTarget(AFighterCharacter* Candidate);

	/** 锁定最优目标：优先 GameMode 分配的首选目标，否则取范围内最近的其他战斗角色 */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	bool LockBestTarget();

	/** 清除当前目标（主动解除） */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void ClearTarget();

	/** 当前目标；失效时返回 null */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	AFighterCharacter* GetCurrentTarget() const;

	/** 当前目标是否有效（存在、未销毁、非自身且仍在锁定范围内） */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsTargetValid() const;

	/** GameMode 分配的默认对手；仅作为锁定优先级，不自动锁定 */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetPreferredTarget(AFighterCharacter* Candidate);

	UFUNCTION(BlueprintPure, Category = "Targeting")
	AFighterCharacter* GetPreferredTarget() const;

	/** 目标是否满足锁定条件（非自身、存活、在范围内） */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsFighterLockable(const AFighterCharacter* Candidate) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	AFighterCharacter* GetOwnerFighter() const;
	AFighterCharacter* FindBestTarget() const;
	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);
	void ClearTargetInternal(bool bInvalidated);
	bool bHasTarget = false;

	/** 当前锁定目标 */
	TWeakObjectPtr<AFighterCharacter> CurrentTarget;

	/** GameMode 分配的首选目标 */
	TWeakObjectPtr<AFighterCharacter> PreferredTarget;
};
