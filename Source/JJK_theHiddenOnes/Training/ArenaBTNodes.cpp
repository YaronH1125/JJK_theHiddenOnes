#include "Training/ArenaBTNodes.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterDefinition.h"
#include "Training/CombatInputComponent.h"
#include "Abilities/GameplayAbility.h"

UBTTask_ArenaDecide::UBTTask_ArenaDecide()
{ NodeName=TEXT("Observe and decide (game time)"); bCreateNodeInstance=true; bNotifyTick=true; }
EBTNodeResult::Type UBTTask_ArenaDecide::ExecuteTask(UBehaviorTreeComponent& Owner,uint8*)
{
 auto* AI=Cast<AFighterAIController>(Owner.GetAIOwner()); if (!AI) return EBTNodeResult::Failed;
 ReadyAt=AI->Now()+AI->Params.DecisionInterval; return EBTNodeResult::InProgress;
}
void UBTTask_ArenaDecide::TickTask(UBehaviorTreeComponent& Owner,uint8*,float)
{
 auto* AI=Cast<AFighterAIController>(Owner.GetAIOwner());
 if (!AI) { FinishLatentTask(Owner,EBTNodeResult::Failed); return; }
 if (AI->Now()>=ReadyAt) { AI->Decide(); FinishLatentTask(Owner,EBTNodeResult::Succeeded); }
}
bool UBTDecorator_ArenaBranch::CalculateRawConditionValue(UBehaviorTreeComponent& Owner,uint8*) const
{ auto* AI=Cast<AFighterAIController>(Owner.GetAIOwner()); return AI && AI->GetCurrentBranch()==Branch; }
UBTTask_ArenaAction::UBTTask_ArenaAction()
{ NodeName=TEXT("Shared action / navigation"); bCreateNodeInstance=true; bNotifyTick=true; bNotifyTaskFinished=true; }
EBTNodeResult::Type UBTTask_ArenaAction::ExecuteTask(UBehaviorTreeComponent& Owner,uint8*)
{
 Cleanup(); Controller=Cast<AFighterAIController>(Owner.GetAIOwner());
 auto* AI=Controller.Get(); if (!AI) return EBTNodeResult::Failed;
 auto* Self=AI->GetSelf();
 bEnded=false; bCancelled=false; CacheId=0;
 Deadline=AI->Now()+AI->Params.DecisionInterval;
 if (!AI->CanRun()) { AI->SetTaskStatus(TEXT("Wait: target/mode unavailable")); return EBTNodeResult::InProgress; }
 if (Branch==EAIBranch::Approach || Branch==EAIBranch::Strafe || Branch==EAIBranch::Retreat)
 { AI->BeginMove(Branch); return EBTNodeResult::InProgress; }
 AI->StopMovement();
 if (Branch==EAIBranch::Wait) { AI->SetTaskStatus(TEXT("Wait: combat control")); return EBTNodeResult::InProgress; }
 AI->SetFocus(AI->GetTarget());
 Self->SetActorRotation((AI->GetTarget()->GetActorLocation()-Self->GetActorLocation()).GetSafeNormal2D().Rotation());
 auto* Input=Self->GetCombatInput();
 if (Branch==EAIBranch::Defend)
 {
  bGuardOwned=!Input->IsGuardIntent(); if (bGuardOwned) Input->NotifyGuardPressed();
  Deadline=AI->Now()+FMath::Clamp(AI->Params.DefendHoldTime,.05f,3.f);
  AI->SetTaskStatus(TEXT("Guard: holding")); return EBTNodeResult::InProgress;
 }
 ASC=Self->GetFighterAbilitySystemComponent();
 const TSubclassOf<UGameplayAbility> Class=Branch==EAIBranch::Attack ? Self->GetMeleeAttackAbilityClass() : Self->GetDefinition()->DodgeAbility;
 if (!Class || !ASC.IsValid()) { AI->SetTaskStatus(TEXT("Rejected: ability missing")); return EBTNodeResult::Failed; }
 // 先捕获已有实例（缓存路径），再在请求前绑定新激活与结束事件。
 if (auto* Spec=ASC->FindAbilitySpecFromClass(Class)) if (Spec->IsActive()) Ability=Spec->GetPrimaryInstance();
 ActivatedHandle=ASC->AbilityActivatedCallbacks.AddWeakLambda(this,[this,Class](UGameplayAbility* Activated)
 { if (Activated && Activated->IsA(Class)) Ability=Activated; });
 EndedHandle=ASC->OnAbilityEnded.AddWeakLambda(this,[this](const FAbilityEndedData& Data)
 { if (Ability.IsValid() && Data.AbilityThatEnded==Ability.Get()) { bEnded=true; bCancelled=Data.bWasCancelled; } });
 AI->AdjustActionBindings(2);
 EActionRequestResult Result=EActionRequestResult::RejectedBlocked;
 if (Branch==EAIBranch::Attack)
 {
  const float Roll=AI->RandomFraction();
  Result=Roll<AI->Params.HeavyChance ? Input->SubmitHeavyPunch() : (Roll>.8f ? Input->SubmitKick() : Input->SubmitLightAttack());
 }
 else Result=Self->RequestDodge(FVector::ZeroVector) ? EActionRequestResult::Executed : EActionRequestResult::RejectedBlocked;
 AI->SetTaskStatus(FString::Printf(TEXT("Request=%s"),*StaticEnum<EActionRequestResult>()->GetNameStringByValue(int64(Result))));
 if (Result!=EActionRequestResult::Executed && Result!=EActionRequestResult::Cached) return EBTNodeResult::Failed;
 if (Result==EActionRequestResult::Cached) CacheId=Input->GetCacheId();
 // 包括同步结束/取消/Commit 失败；不能在绑定之前调用请求。
 if (bEnded) return bCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
 if (!Ability.IsValid()) { AI->SetTaskStatus(TEXT("Request ended synchronously / no active instance")); return EBTNodeResult::Succeeded; }
 Deadline=AI->Now()+5.;
 return EBTNodeResult::InProgress;
}
void UBTTask_ArenaAction::TickTask(UBehaviorTreeComponent& Owner,uint8*,float)
{
 auto* AI=Controller.Get(); auto* Self=AI ? AI->GetSelf() : nullptr;
 if (!AI || !Self || !AI->CanRun()) { FinishLatentTask(Owner,EBTNodeResult::Failed); return; }
 auto* Input=Self->GetCombatInput();
 if (CacheId)
 {
  if (Input->GetConsumedCacheId()==CacheId) { CacheId=0; AI->SetTaskStatus(TEXT("Cache consumed: awaiting same ability end")); }
  else if (Input->GetCacheId()!=CacheId || Input->PeekCachedAction()==ECachedAction::None || bEnded)
  { AI->SetTaskStatus(TEXT("Cache expired/discarded/superseded")); FinishLatentTask(Owner,EBTNodeResult::Failed); return; }
 }
 if (bEnded) { AI->SetTaskStatus(bCancelled ? TEXT("Ability interrupted") : TEXT("Ability completed")); FinishLatentTask(Owner,bCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded); return; }
 const bool Moving=Branch==EAIBranch::Approach || Branch==EAIBranch::Strafe || Branch==EAIBranch::Retreat;
 if ((Moving || Branch==EAIBranch::Defend) && !Self->CanAct()) { AI->StopMovement(); FinishLatentTask(Owner,EBTNodeResult::Failed); return; }
 if (AI->Now()>=Deadline)
 { AI->SetTaskStatus(Ability.IsValid() ? TEXT("Ability timeout (subscription released)") : TEXT("Branch completed")); FinishLatentTask(Owner,Ability.IsValid() ? EBTNodeResult::Failed : EBTNodeResult::Succeeded); }
}
void UBTTask_ArenaAction::Cleanup()
{
 auto* AI=Controller.Get();
 if (ASC.IsValid()) { ASC->AbilityActivatedCallbacks.Remove(ActivatedHandle); ASC->OnAbilityEnded.Remove(EndedHandle); }
 if (AI)
 {
  if (ActivatedHandle.IsValid()) AI->AdjustActionBindings(-1);
  if (EndedHandle.IsValid()) AI->AdjustActionBindings(-1);
  if (auto* Self=AI->GetSelf())
  {
   if (bGuardOwned) Self->GetCombatInput()->NotifyGuardReleased();
   Self->GetCombatInput()->ClearOwnedCache(CacheId);
  }
  // 任务清理只停止自己的寻路；能力是否可取消仍由共享规则/模式事务决定。
  if (Branch==EAIBranch::Approach || Branch==EAIBranch::Strafe || Branch==EAIBranch::Retreat) AI->StopMovement();
 }
 ActivatedHandle.Reset(); EndedHandle.Reset(); ASC.Reset(); Ability.Reset(); Controller.Reset(); CacheId=0; bGuardOwned=false;
}
EBTNodeResult::Type UBTTask_ArenaAction::AbortTask(UBehaviorTreeComponent&,uint8*) { Cleanup(); return EBTNodeResult::Aborted; }
void UBTTask_ArenaAction::OnTaskFinished(UBehaviorTreeComponent& Owner,uint8* Memory,EBTNodeResult::Type Result)
{ Cleanup(); Super::OnTaskFinished(Owner,Memory,Result); }
