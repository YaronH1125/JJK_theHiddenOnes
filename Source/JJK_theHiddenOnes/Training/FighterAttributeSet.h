// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "FighterAttributeSet.generated.h"

class UFighterAbilitySystemComponent;
struct FGameplayEffectModCallbackData;

/**
 * 战斗角色属性集：生命、行动资源、能量及各自上限。
 * 数值语义与 02_架构设计.md 对齐；初始化与训练重置走 FighterCharacter 的集中入口，
 * 正常战斗中的增减应通过 Gameplay Effect 进入本类并在此处做范围约束。
 */
UCLASS()
class UFighterAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	/** 生命 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UFighterAttributeSet, Health)

	/** 生命上限 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UFighterAttributeSet, MaxHealth)

	/** 行动资源（连招取消/闪避等共用） */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ActionResource, Category = "Attributes")
	FGameplayAttributeData ActionResource;
	ATTRIBUTE_ACCESSORS_BASIC(UFighterAttributeSet, ActionResource)

	/** 行动资源上限 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxActionResource, Category = "Attributes")
	FGameplayAttributeData MaxActionResource;
	ATTRIBUTE_ACCESSORS_BASIC(UFighterAttributeSet, MaxActionResource)

	/** 能量（技能/绝技消耗） */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Energy, Category = "Attributes")
	FGameplayAttributeData Energy;
	ATTRIBUTE_ACCESSORS_BASIC(UFighterAttributeSet, Energy)

	/** 能量上限 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxEnergy, Category = "Attributes")
	FGameplayAttributeData MaxEnergy;
	ATTRIBUTE_ACCESSORS_BASIC(UFighterAttributeSet, MaxEnergy)

	//~ Begin UAttributeSet Interface
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet Interface

protected:
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_ActionResource(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaxActionResource(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_Energy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaxEnergy(const FGameplayAttributeData& OldValue);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
