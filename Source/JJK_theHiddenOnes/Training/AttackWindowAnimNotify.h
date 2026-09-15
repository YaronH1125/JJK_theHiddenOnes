// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AttackWindowAnimNotify.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

/**
 * 命中窗口开启通知（M2.2）：只报告事件，不扣血、不持有跨角色状态。
 * 事件由 CombatHitComponent 以当前攻击实例校验，旧 Montage 遗留通知被拒绝。
 */
UCLASS()
class UAnimNotify_AttackWindowOpen : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual bool Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const override;
};

/** 命中窗口关闭通知 */
UCLASS()
class UAnimNotify_AttackWindowClose : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual bool Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const override;
};
