// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/CombatTypes.h"

// 战斗状态：唯一管理者为 ASC 标签（02_架构设计.md 第 4 节）
UE_DEFINE_GAMEPLAY_TAG(TAG_State_GettingUp, "State.GettingUp");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_ThrowPaired, "State.ThrowPaired");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dead, "State.Dead");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_HitStun, "State.HitStun");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Attacking, "State.Attacking");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Guarding, "State.Guarding");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_GuardStun, "State.GuardStun");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_KnockedDown, "State.KnockedDown");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_SuperArmor, "State.SuperArmor");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_DodgeInvulnerable, "State.DodgeInvulnerable");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_DodgeRecovery, "State.DodgeRecovery");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_StanceSwitching, "State.StanceSwitching");
UE_DEFINE_GAMEPLAY_TAG(TAG_Stance_Melee, "Stance.Melee");
UE_DEFINE_GAMEPLAY_TAG(TAG_Stance_Ranged, "Stance.Ranged");

// M3 能力标识
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_MeleeAttack, "Ability.Melee.Attack");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Dodge, "Ability.Dodge");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_StanceSwitch, "Ability.StanceSwitch");

// GE SetByCaller 数据键
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage, "Data.Damage");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Amount, "Data.Amount");
