// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AttackDefinition.generated.h"

class UAnimMontage;

/**
 * 攻击动作配置（M2 单段）：只保存配置，不保存运行状态（02_架构设计.md 第 9 节）。
 * 命中窗口优先由动画通知驱动；Montage 无通知时可用时间窗口显式回退（记录为偏差）。
 */
UCLASS(BlueprintType)
class UAttackDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	/** 攻击类别名（M3 防御/投技判定扩展点；M2 仅记录） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FName AttackTypeName = TEXT("Punch");

	/** 命中段编号；同实例同段去重，新段/新攻击可再次命中 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	int32 SegmentId = 0;

	/** 伤害值；经 SetByCaller 传入伤害 GE */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float Damage = 35.f;

	/** 攻击 Montage（M2: 由 MM_Attack_01 生成） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> Montage;

	/** 受击占位 Montage（可空；为空时受击仅硬直标签+禁止输入） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> HitReactMontage;

	/** 拳部扫掠 Socket（M1 素材检查确认 SK_Mannequin 标准手部 Socket） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	FName TraceSocket = TEXT("hand_r");

	/** 扫掠球半径（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float TraceRadius = 15.f;

	/** 命中硬直时长（秒）；State.HitStun 持续期，期间拒绝新动作请求 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float HitStunDuration = 0.5f;

	/** 窗口来源：动画通知（默认，M2.2 规范） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	bool bWindowFromAnimNotifies = true;

	/** 回退窗口起点（Montage 开始后的秒数）；仅 bWindowFromAnimNotifies=false 时使用 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (EditCondition = "!bWindowFromAnimNotifies", ClampMin = "0.0", ForceUnits = "s"))
	float WindowStartTime = 0.3f;

	/** 回退窗口终点 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (EditCondition = "!bWindowFromAnimNotifies", ClampMin = "0.0", ForceUnits = "s"))
	float WindowEndTime = 0.5f;
};
