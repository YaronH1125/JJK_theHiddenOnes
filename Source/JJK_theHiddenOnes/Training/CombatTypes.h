// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "CombatTypes.generated.h"

class AActor;

/** 命中引起的战斗事件：受击方向受击方队列投递，受击方在其命中组件 Tick 内、扫掠之后统一处理（M2.5 换血保证） */
struct FCombatEvent
{
	/** 事件类型 */
	enum class EType : uint8
	{
		HitReact,
		GuardStun,
		Knockdown
	};

	EType Type = EType::HitReact;
	TWeakObjectPtr<AActor> Instigator;
	int32 InterruptLevel = 1;
	float StunDuration = 0.f;
	float KnockbackStrength = 0.f;
	bool bLethal = false;
	FVector HitLocation = FVector::ZeroVector;
	FVector KnockbackDirection = FVector::ZeroVector;
};

/** 动作请求结果：玩家与调试对手共用入口，拒绝必须带原因（M2.1） */
UENUM(BlueprintType)
enum class EActionRequestResult : uint8
{
	/** 已执行：能力已激活 */
	Executed,
	Cached,
	/** 已死亡，禁止一切动作请求 */
	RejectedDead,
	/** ASC 未初始化（M1 幂等保护未完成） */
	RejectedNotInitialized,
	/** 已有同能力活动实例，重复按键不产生第二个实例 */
	RejectedAlreadyActive,
	/** 被状态标签阻止（受击硬直/倒地/切形态中等） */
	RejectedBlocked,
	/** 能力未授予（配置缺失） */
	RejectedAbilityMissing,
	/** 长按达到阈值：记录重击意图（重拳/重踢按当前形态与位置解析） */
	HeavyIntentRecorded,
	/** 无会话的松开（旧松键不补攻击） */
	RejectedStaleRelease,
	/** 远程形态：M3 不发炮，攻击请求被拒 */
	RejectedRangedStance,
	/** 切形态条件不满足（动作中/间隔未到） */
	RejectedStanceSwitchBusy
};

/** 攻击动作阶段：起手 → 有效（检测窗口）→ 恢复 */
UENUM(BlueprintType)
enum class EAttackPhase : uint8
{
	None,
	Windup,
	Active,
	Recovery
};

/** 角色形态（M3：E 切换；远程不发炮） */
UENUM(BlueprintType)
enum class EFighterStance : uint8
{
	Melee,
	Ranged
};

/** 输入缓存槽可缓存的动作（单槽，后到覆盖；消费时按当前攻击的允许集校验） */
UENUM(BlueprintType)
enum class ECachedAction : uint8
{
	None,
	/** 连击下一段（A1→A2→A3） */
	NextSegment,
	/** 重拳（左键长按；仅配置允许的段可转） */
	HeavyPunch,
	/** 腿击（Q 点按） */
	Kick,
	/** 重踢（Q 长按；造成倒地） */
	HeavyKick,
	/** 移动蓄力炮（远形态 LMB：按下开始蓄力，松开发射） */
	MobileBlast,
	/** 原地超蓄力炮（远形态 Q：按下开始蓄力，松开发射） */
	SuperBlast,
	/** 领域展开（R：瞬发结印） */
	Domain
};

/** 命中去重键：攻击实例 + 命中段 + 目标，保持到该段结束 */
struct FCombatHitDedupKey
{
	uint64 AttackInstanceId = 0;
	int32 SegmentId = 0;
	TWeakObjectPtr<const AActor> Target;

	bool operator==(const FCombatHitDedupKey& Other) const
	{
		return AttackInstanceId == Other.AttackInstanceId && SegmentId == Other.SegmentId && Target == Other.Target;
	}

	friend uint32 GetTypeHash(const FCombatHitDedupKey& Key)
	{
		uint32 Hash = ::GetTypeHash(Key.AttackInstanceId);
		Hash = HashCombine(Hash, ::GetTypeHash(Key.SegmentId));
		Hash = HashCombine(Hash, ::GetTypeHash(Key.Target));
		return Hash;
	}
};

/** 远程命中共享结算参数（A02：手动炮/领域球与近战同口径） */
struct FRangedHitSettle
{
	float Damage = 0.f;
	bool bDodgeable = true;   /* 可被闪避无敌窗免疫 */
	bool bBlockable = true;   /* 可被正面防御（chip 伤 + 防御硬直） */
	bool bGrantCurse = true;  /* 命中回咒（领域球不回咒） */
	uint64 AttackInstanceId = 0; /* 领域能量按攻击实例去重（08 §178/182） */
	float GuardStunDuration = 0.6f;
	float HitStunDuration = 0.4f;
	float InterruptLevel = 1.f;
	float KnockbackStrength = 500.f;
};

/** 本模块原生 GameplayTag（定义见 CombatNativeTags.cpp） */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_GettingUp);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ThrowPaired);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Dead);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_HitStun);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Attacking);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Guarding);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_GuardStun);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_KnockedDown);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_SuperArmor);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_DodgeInvulnerable);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_DodgeRecovery);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_StanceSwitching);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_SuperBlastCooldown);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_BlastCharging);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_DomainActive);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_DomainCasting);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_RangedBlastCharging);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stance_Melee);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Stance_Ranged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_MeleeAttack);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Blast);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Dodge);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Domain);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_StanceSwitch);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Amount);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_TrainingProbe);
