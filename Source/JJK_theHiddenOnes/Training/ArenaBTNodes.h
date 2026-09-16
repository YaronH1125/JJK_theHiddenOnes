#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "Training/FighterAIController.h"
#include "ArenaBTNodes.generated.h"

/** 序列首节点，按游戏时间限制决策频率。 */
UCLASS()
class UBTTask_ArenaDecide : public UBTTaskNode
{
 GENERATED_BODY()
public:
 UBTTask_ArenaDecide();
 virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& Owner, uint8* Memory) override;
 virtual void TickTask(UBehaviorTreeComponent& Owner, uint8* Memory, float Delta) override;
private:
 double ReadyAt = 0.;
};

UCLASS()
class UBTDecorator_ArenaBranch : public UBTDecorator
{
 GENERATED_BODY()
public:
 UPROPERTY() EAIBranch Branch = EAIBranch::Wait;
 virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& Owner, uint8* Memory) const override;
};

/** 每个树实例独立持有订阅；提交前绑定 ASC，避免同步完成丢回调。 */
UCLASS()
class UBTTask_ArenaAction : public UBTTaskNode
{
 GENERATED_BODY()
public:
 UBTTask_ArenaAction();
 UPROPERTY() EAIBranch Branch = EAIBranch::Wait;
 virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& Owner, uint8* Memory) override;
 virtual void TickTask(UBehaviorTreeComponent& Owner, uint8* Memory, float Delta) override;
 virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& Owner, uint8* Memory) override;
 virtual void OnTaskFinished(UBehaviorTreeComponent& Owner, uint8* Memory, EBTNodeResult::Type Result) override;
private:
 void Cleanup();
 TWeakObjectPtr<AFighterAIController> Controller;
 TWeakObjectPtr<class UFighterAbilitySystemComponent> ASC;
 TWeakObjectPtr<class UGameplayAbility> Ability;
 FDelegateHandle ActivatedHandle, EndedHandle;
 double Deadline = 0.;
 uint64 CacheId = 0;
 bool bEnded = false, bCancelled = false, bGuardOwned = false;
};
