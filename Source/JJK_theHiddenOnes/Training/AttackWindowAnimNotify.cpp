// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/AttackWindowAnimNotify.h"

#include "Training/CombatHitComponent.h"
#include "Training/FighterCharacter.h"

namespace
{
	UCombatHitComponent* FindCombatHit(const USkeletalMeshComponent* MeshComp)
	{
		if (MeshComp == nullptr)
		{
			return nullptr;
		}
		if (AFighterCharacter* Fighter = Cast<AFighterCharacter>(MeshComp->GetOwner()))
		{
			return Fighter->GetCombatHit();
		}
		return nullptr;
	}
}

void UAnimNotify_AttackWindowOpen::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UCombatHitComponent* Hit = FindCombatHit(MeshComp))
	{
		Hit->HandleAnimWindowNotify(true, Animation);
	}
}

void UAnimNotify_AttackWindowClose::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UCombatHitComponent* Hit = FindCombatHit(MeshComp))
	{
		Hit->HandleAnimWindowNotify(false, Animation);
	}
}
