#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "TrainingProbeAbility.generated.h"

/** 仅训练开发验收：消耗行动资源 1、咒力 10、领域能量 5，冷却 3 秒，无攻击伤害。 */
UCLASS()
class UTrainingProbeAbility : public UGameplayAbility
{
 GENERATED_BODY()
public:
 UTrainingProbeAbility();
 virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr, FGameplayTagContainer* OptionalRelevantTags=nullptr) const override;
 virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags=nullptr) const override;
 virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
 virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags=nullptr) const override;
 virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
 virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
UCLASS()
class UTrainingCostEffect : public UGameplayEffect
{
 GENERATED_BODY()
public: UTrainingCostEffect();
};
UCLASS()
class UTrainingHealEffect : public UGameplayEffect
{
 GENERATED_BODY()
public: UTrainingHealEffect();
};
