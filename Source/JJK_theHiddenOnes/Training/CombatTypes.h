// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "CombatTypes.generated.h"

class AActor;

/** 命中引起的战斗事件：受击方向受击方队列投递，受击方自身下一 Tick 处理（M2.5 换血保证） */
struct FCombatEvent
{
	TWeakObjectPtr<AActor> Instigator;
	float StunDuration = 0.f;
	bool bLethal = false;
	FVector HitLocation = FVector::ZeroVector;
};

/** 动作请求结果：玩家与调试对手共用入口，拒绝必须带原因（M2.1） */
UENUM(BlueprintType)
enum class EActionRequestResult : uint8
{
	/** 已执行：能力已激活 */
	Executed,
	/** 已死亡，禁止一切动作请求 */
	RejectedDead,
	/** ASC 未初始化（M1 幂等保护未完成） */
	RejectedNotInitialized,
	/** 已有同能力活动实例，重复按键不产生第二个实例 */
	RejectedAlreadyActive,
	/** 被状态标签阻止（受击硬直等） */
	RejectedBlocked,
	/** 能力未授予（配置缺失） */
	RejectedAbilityMissing,
	/** 长按达到阈值：仅记录 HeavyPunch 占位意图（M2 无实际动作/伤害） */
	HeavyIntentRecorded,
	/** 无会话的松开（旧松键不补攻击） */
	RejectedStaleRelease
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

/** 命中去重键：攻击实例 + 命中段 + 目标（M2.3）；保持到该段结束 */
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

/** 本模块原生 GameplayTag（定义见 CombatNativeTags.cpp） */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Dead);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_HitStun);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Attacking);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_MeleeAttack);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage);
