// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Training/BlastConfig.h"
#include "FighterDefinition.generated.h"

class UAttackDefinition;
class UGameplayAbility;
class UMaterialInterface;
class UMeleeComboAbility;
class UAnimMontage;

/** 防御规则（M3.3）：正面角度按防御者朝向判断，与镜头无关 */
USTRUCT(BlueprintType)
struct FGuardConfig
{
	GENERATED_BODY()

	/** 正面半角（度）：攻击来向与防御者朝向夹角小于该值判为正面 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard", meta = (ClampMin = "10.0", ClampMax = "180.0", ForceUnits = "deg"))
	float FrontArcHalfAngle = 90.f;

	/** 防御硬直时长（被防御命中后防御方短暂不可行动） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float GuardStunDuration = 0.35f;

	/** 防御削血开关（默认关闭；启用时按比例扣血） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard")
	bool bChipDamage = false;

	/** 防御削血比例 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChipDamageRatio = 0.1f;
};

/** 闪避规则（M3.3/M3.4）：方向闪避 + 取消闪避共用 */
USTRUCT(BlueprintType)
struct FDodgeConfig
{
	GENERATED_BODY()

	/** 闪避位移速度（沿方向或后撤） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "100.0", ForceUnits = "cm/s"))
	float DodgeSpeed = 900.f;

	/** 无敌窗口时长（从闪避开始） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float InvulnerableDuration = 0.25f;

	/** 无敌结束后恢复期时长（恢复期内不可再次闪避，可正常受击） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RecoveryDuration = 0.4f;

	/** 普通闪避行动资源消耗 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.0"))
	float DodgeCost = 1.f;

	/** 取消闪避总成本（替代普通成本扣一次，不叠加） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.0"))
	float CancelDodgeTotalCost = 2.f;

	/** 移动闪避=加速跑：无敌/恢复期间的速度倍率（不播前扑，直接起跑） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "1.0"))
	float RunDodgeSpeedMultiplier = 2.2f;
};

/** 行动资源恢复（M3.4） */
USTRUCT(BlueprintType)
struct FResourceRegenConfig
{
	GENERATED_BODY()

	/** 停止消耗后开始恢复的延迟 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RegenDelay = 1.f;

	/** 每秒恢复量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0.0"))
	float RegenPerSecond = 1.f;
};

/** 条件投技规则（M3.5） */
USTRUCT(BlueprintType)
struct FThrowConfig
{
	GENERATED_BODY()

	/** 可转投的最大距离（攻击者到目标中心） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throw", meta = (ClampMin = "50.0", ForceUnits = "cm"))
	float MaxDistance = 180.f;

	/** 投技配对时长（双方锁定） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throw", meta = (ClampMin = "0.2", ForceUnits = "s"))
	float PairDuration = 1.0f;

	/** 投技伤害（配对结束时一次结算） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throw", meta = (ClampMin = "0.0"))
	float Damage = 120.f;

	/** 配对期间双方间距 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throw", meta = (ClampMin = "50.0", ForceUnits = "cm"))
	float PairDistance = 90.f;

	/** 擂台半径（投技配对位置不出边界） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throw", meta = (ClampMin = "100.0", ForceUnits = "cm"))
	float ArenaRadius = 1100.f;
};

/**
 * 最小角色定义（02_架构设计.md 第 9 节）：只保存配置。
 * M1 仅含初始属性与外观标识；资源引用、能力列表和技能槽按后续阶段补齐。
 * 运行状态（连招段、目标、计时器等）禁止存入本类。
 */
UCLASS(BlueprintType)
class UFighterDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UFighterDefinition();

	/** 调试与训练面板显示用 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	/** 生命上限（>= 初始生命） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float MaxHealth = 1000.f;

	/** 初始生命 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float InitialHealth = 1000.f;

	/** 行动资源上限（08 种子值 5/5，闪避消耗 1、取消总计 2） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float MaxActionResource = 5.f;

	/** 初始行动资源 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float InitialActionResource = 5.f;

	/** 能量上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float MaxEnergy = 100.f;

	/** 初始能量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float InitialEnergy = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
 float MaxCursedEnergy = 100.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
 float InitialCursedEnergy = 100.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
 float MeleeCursedEnergyGain = 3.f;

	/** P1/P2 身份标识颜色（覆盖材质参数名 Tint） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FLinearColor MarkerColor = FLinearColor::White;

	/** 用于叠加染色的基础材质；为空则不做颜色标识 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	TSoftObjectPtr<UMaterialInterface> MarkerOverlayMaterial;

	/** 初始授予能力；M1 保持为空，授予具备防重复保护 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	/** M2 普攻能力类（共享动作请求入口按此激活）；默认 MeleeCombo 单段原型 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> MeleeAttackAbility;

	/** M2 单段攻击配置（动画、命中段、伤害、轨迹参数） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UAttackDefinition> AttackDefinition;

	// ---------- M3：连招与近战方案二 ----------

	/** 三段连击配置（A1/A2/A3）；MeleeAttackAbility 按序推进 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Combo")
	TArray<TObjectPtr<UAttackDefinition>> ComboSegments;

	/** 重拳（左键长按） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Combo")
	TObjectPtr<UAttackDefinition> HeavyPunchDefinition;

	/** 腿击（Q 点按） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Combo")
	TObjectPtr<UAttackDefinition> KickDefinition;

	/** 重踢（Q 长按，倒地） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Combo")
	TObjectPtr<UAttackDefinition> HeavyKickDefinition;

	/** 输入缓存槽寿命（超时丢弃） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Combo", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float ComboCacheLifetime = 0.5f;

	/** 切形态动作时长（E） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Stance", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float StanceSwitchDuration = 0.1f;

	/** 再次切形态的最小间隔 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Stance", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float StanceSwitchInterval = 0.1f;

	/** 防御规则 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Guard")
	FGuardConfig GuardConfig;

	/** 闪避规则 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Dodge")
	FDodgeConfig DodgeConfig;

 /** Presentation only; displacement and invulnerability remain owned by DodgeAbility. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement")
 TSoftObjectPtr<UAnimMontage> DodgeMontage;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement")
 TSoftObjectPtr<UAnimMontage> BackstepMontage;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1.0",ClampMax="3.0"))
 float SprintSpeedMultiplier = 1.5f;

	/** 行动资源恢复 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Resource")
	FResourceRegenConfig ResourceRegen;

	/** 条件投技规则 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Throw")
	FThrowConfig ThrowConfig;

	/** 倒地时长（倒地保护期；结束后起身恢复行动） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Hit", meta = (ClampMin = "0.2", ForceUnits = "s"))
	float KnockdownDuration = 1.5f;
 /** 起身属于倒地保护的最后一段，不提前恢复输入。 */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M3|Knockdown", meta = (ClampMin = "0.05"))
 float GetUpDuration = 0.4f;

	/** 闪避能力类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> DodgeAbility;

	/** 切形态能力类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> StanceSwitchAbility;

	/** 远程移动蓄力炮能力类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> MobileBlastAbility;

	/** 远程定点超级炮能力类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> StationaryBlastAbility;

	/** 领域展开能力类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> DomainExpansionAbility;

	/** 移动蓄力炮 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Blast")
	FMobileBlastConfig MobileBlast;

	/** 定点超级蓄力炮 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Blast")
	FSuperBlastConfig SuperBlast;

	/** 领域展开 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Domain")
	FDomainConfig DomainConfig;

	/** 咒力/领域能量流动 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Resource")
	FResourceFlowConfig ResourceFlow;

	/** 炮口 Socket（正式模型为 Muzzle_Head_Review；占位模型回退头部骨骼名） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Blast")
	FName MuzzleSocket = TEXT("Muzzle_Head_Review");

	/** 右键瞄准镜头臂长（枪战 TPS：收到右肩上方，避免模型挡准星） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Aim", meta = (ClampMin = "50.0", ForceUnits = "cm"))
	float AimArmLength = 150.f;

	/** 普通镜头臂长（松开右键恢复） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Aim", meta = (ClampMin = "50.0", ForceUnits = "cm"))
	float NormalArmLength = 400.f;

	/** 瞄准插值速度（FInterpTo 参数，参考旧工程 10） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Aim", meta = (ClampMin = "1.0"))
	float AimInterpSpeed = 12.f;

	/** 瞄准右肩偏移（Boom SocketOffset.Y：相机从右肩上方越过头顶看） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Aim", meta = (ForceUnits = "cm"))
	float AimSocketOffsetY = 60.f;

	/** 瞄准上抬偏移（Boom SocketOffset.Z：相机略高于头） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Aim", meta = (ForceUnits = "cm"))
	float AimSocketOffsetZ = 25.f;

	/** 待机右肩偏移（TPS 惯例：非瞄准相机也不在正后方，人物常驻屏幕左侧） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Aim", meta = (ForceUnits = "cm"))
	float NormalSocketOffsetY = 75.f;

	/** 待机上抬偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Aim", meta = (ForceUnits = "cm"))
	float NormalSocketOffsetZ = 35.f;

	/** 近战形态右肩偏移（异人之下式：人物基本居中） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Melee", meta = (ForceUnits = "cm"))
	float MeleeSocketOffsetY = 0.f;

	/** 近战形态上抬偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Melee", meta = (ForceUnits = "cm"))
	float MeleeSocketOffsetZ = 35.f;


	/** 柔性锁定：辅助生效的目标距离 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Melee", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MeleeAutoFaceRange = 600.f;

	/** 软锁总开关（M3/M5 方向性近战回归显式钉住 false） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Melee")
	bool MeleeAutoFace = true;

	/** 索敌键硬锁：锁定时面向目标的速率（FInterpTo） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Melee", meta = (ClampMin = "1.0"))
	float MeleeFaceInterpSpeed = 10.f;

	/** 出招磁吸：此距离外不吸附（08 攻击距离 ~200 + 余量） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Melee", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MagnetismRange = 350.f;

	/** 近战攻击有效距离（磁吸只补这段之外的空隙） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Melee", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AttackReach = 180.f;

	/** 磁吸滑步最大距离（“瞬移到敌人面前”的吸引感，实测 50–120cm） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Melee", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MagnetismLunge = 100.f;

	/** 远程开火朝向：蓄力/发射期间转向镜头 yaw 的限速（度/秒，PUBG 开火转身） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Aim", meta = (ClampMin = "0.0"))
	float FireFaceYawRate = 540.f;

	/** 瞄准 FOV（收窄视场：PUBG ADS 归一化 ≈70 vFOV） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Aim", meta = (ClampMin = "30.0", ClampMax = "120.0"))
	float AimFOV = 65.f;

	/** 普通 FOV */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M6|Aim", meta = (ClampMin = "30.0", ClampMax = "120.0"))
	float NormalFOV = 90.f;
};
