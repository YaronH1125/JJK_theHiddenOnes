// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/CombatTypes.h"

// 战斗状态：唯一管理者为 ASC 标签（02_架构设计.md 第 4 节）
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dead, "State.Dead");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_HitStun, "State.HitStun");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Attacking, "State.Attacking");

// M2 普攻能力标识；死亡/重置按此标签取消活动攻击
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_MeleeAttack, "Ability.Melee.Attack");

// 伤害 GE 的 SetByCaller 数据键
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage, "Data.Damage");
