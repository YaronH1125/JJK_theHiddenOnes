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

	// ---------- M3：连招、攻防与状态 ----------

	/** 击退强度（0=无击退；A3/重拳配置，沿攻击者朝向推目标） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Attack", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float KnockbackStrength = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Attack")
 int32 InterruptLevel = 1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Attack")
 int32 ArmorResistanceLevel = 1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Attack")
 bool bDodgeable = true;

	/** 命中造成倒地（重踢）；倒地期间受倒地保护 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Attack")
	bool bKnockdown = false;

	/** 动作期间自身获得霸体（仍可扣血，只抵抗打断） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Attack")
	bool bGrantsSuperArmor = false;

	/** 该段命中防御目标时允许转投技（仅 A1 配置） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Throw")
	bool bCanThrow = false;

	/** 是否可被防御（近战默认可防御；M3 全部可防御） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Attack")
	bool bBlockable = true;

	/** 被防御时的防御方硬直时长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Attack", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float GuardStunDuration = 0.35f;

	/** 连击衔接窗口起点（本段开始后的秒数；此窗口内缓存的 NextSegment 可消费） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Combo", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ComboWindowStartTime = 0.45f;

	/** 连击衔接窗口终点 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Combo", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ComboWindowEndTime = 0.9f;

	/** 允许衔接下一段（A1→A2、A2→A3） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Combo")
	bool bAllowNextSegment = true;

	/** 允许转重拳（A2 指定衔接点） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Combo")
	bool bAllowHeavyTransition = false;

	/** 允许转腿击（A2→Q） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Combo")
	bool bAllowKickTransition = false;

	/** 允许 Shift 闪避取消的窗口起点（0=动作开始即可，含起手） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Cancel", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float CancelWindowStartTime = 0.f;

	/** 允许 Shift 闪避取消的窗口终点（相对动作开始；0=不开放取消） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Cancel", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float CancelWindowEndTime = 1.0f;
};
