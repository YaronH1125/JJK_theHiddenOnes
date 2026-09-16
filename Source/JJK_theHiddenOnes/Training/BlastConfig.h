// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlastConfig.generated.h"

/** 移动蓄力炮（远程左键）配置；数值种子来自 08 第 7.2 节，全部为调试参数 */
USTRUCT(BlueprintType)
struct FMobileBlastConfig
{
	GENERATED_BODY()

	/** 基础咒力成本（合法激活时提交） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.0"))
	float MinCost = 8.f;

	/** 满蓄总成本 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.0"))
	float MaxCost = 24.f;

	/** 最低伤害（q=0） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.0"))
	float MinDamage = 30.f;

	/** 满蓄伤害（q=1） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.0"))
	float MaxDamage = 90.f;

	/** 蓄力封顶时间（秒；q=clamp(t/1.2,0,1)） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float CapTime = 1.2f;

	/** 松开后的锁向前摇（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float LockWindup = 0.15f;

	/** 射程（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "100.0", ForceUnits = "cm"))
	float Range = 1800.f;

	/** 蓄力期间移动速度倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MoveSpeedScale = 0.55f;
};

/** 定点超级蓄力炮（远程 Q）配置 */
USTRUCT(BlueprintType)
struct FSuperBlastConfig
{
	GENERATED_BODY()

	/** 基础咒力成本（合法激活时提交；不足则拒绝起手） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.0"))
	float MinCost = 45.f;

	/** 满蓄总成本 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.0"))
	float MaxCost = 65.f;

	/** 最低发射门槛伤害（q=0，需满 1.2s） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.0"))
	float MinDamage = 150.f;

	/** 满蓄伤害（2.4s 封顶） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.0"))
	float MaxDamage = 220.f;

	/** 最低蓄力时长（秒；未达松开=收招不发射） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float MinChargeTime = 1.2f;

	/** 伤害封顶时间（秒；q=clamp((t-1.2)/1.2,0,1)） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float CapTime = 2.4f;

	/** 松开后的锁向前摇（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float LockWindup = 0.25f;

	/** 射程（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "100.0", ForceUnits = "cm"))
	float Range = 2400.f;

	/** 冷却时长（发射或已付费中断时启动一次） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blast", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float Cooldown = 10.f;
};

/** 领域展开配置（M6.6；数值为 08 v0.3 种子） */
USTRUCT(BlueprintType)
struct FDomainConfig
{
	GENERATED_BODY()

	/** 结印时长（可被打断） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float CastTime = 1.f;

	/** 领域持续时长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "1.0", ForceUnits = "s"))
	float Duration = 6.f;

	/** 展开消耗的领域能量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "0.0"))
	float EnergyCost = 100.f;

	/** 目标捕获最大距离 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "100.0", ForceUnits = "cm"))
	float CaptureRange = 1200.f;

	/** 首发自动炮延迟（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float FirstOrbDelay = 0.3f;

	/** 自动炮发射间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float OrbInterval = 2.f;

	/** 单发自动炮伤害（满威力超级炮） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "0.0"))
	float OrbDamage = 220.f;

	/** 单发自动炮咒力成本（生成时扣除） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "0.0"))
	float OrbCost = 65.f;

	/** 同时在途球上限（每术者） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "1"))
	int32 MaxOrbsInFlight = 3;

	/** 球体寿命（秒；超时销毁不补命中） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "0.5", ForceUnits = "s"))
	float OrbLife = 3.f;

	/** 球体伤害扫掠半径 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "5.0", ForceUnits = "cm"))
	float OrbRadius = 15.f;

	/** 球体飞行速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domain", meta = (ClampMin = "100.0", ForceUnits = "cm/s"))
	float OrbSpeed = 1200.f;
};

/** 咒力与领域能量流动（M6.5） */
USTRUCT(BlueprintType)
struct FResourceFlowConfig
{
	GENERATED_BODY()

	/** 近战有效命中恢复咒力 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0.0"))
	float MeleeHitCurseRestore = 3.f;

	/** 停止炮击/蓄力后开始恢复咒力的延迟 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float CurseRegenDelay = 2.f;

	/** 咒力每秒恢复量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0.0"))
	float CurseRegenPerSecond = 6.f;

	/** 非领域期有效结算伤害转领域能量比例 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DomainEnergyGainRatio = 0.05f;

	/** 单攻击实例领域能量获取上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0.0"))
	float DomainEnergyGainPerInstanceCap = 10.f;
};
