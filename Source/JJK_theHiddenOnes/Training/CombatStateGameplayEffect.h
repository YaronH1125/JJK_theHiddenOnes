#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "CombatStateGameplayEffect.generated.h"

/** 战斗限制效果：具体状态标签与时长由每次 Spec 提供，实例句柄归角色协调入口持有。 */
UCLASS()
class UCombatStateGameplayEffect : public UGameplayEffect
{
 GENERATED_BODY()
public:
 UCombatStateGameplayEffect();
};
