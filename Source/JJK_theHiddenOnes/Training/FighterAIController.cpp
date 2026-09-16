#include "Training/FighterAIController.h"
#include "Training/ArenaBTNodes.h"
#include "Training/FighterCharacter.h"
#include "Training/CombatInputComponent.h"
#include "Training/TrainingGameMode.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AFighterAIController::AFighterAIController()
{
 PrimaryActorTick.bCanEverTick = true;
 bSetControlRotationFromPawnOrientation = false;
}
void AFighterAIController::BuildTree()
{
 if (ArenaTree) return;
 // 原生树结构随模块打包，不依赖编辑器生成图或硬编码 Content 路径。
 ArenaBlackboard = NewObject<UBlackboardData>(this, TEXT("BB_Arena"));
 auto AddKey = [this](FName Name, UBlackboardKeyType* Type)
 { FBlackboardEntry E; E.EntryName=Name; E.KeyType=Type; ArenaBlackboard->Keys.Add(E); };
 auto* TargetType=NewObject<UBlackboardKeyType_Object>(ArenaBlackboard);
 TargetType->BaseClass=AFighterCharacter::StaticClass();
 AddKey(TEXT("Target"),TargetType);
 AddKey(TEXT("Distance"),NewObject<UBlackboardKeyType_Float>(ArenaBlackboard));
 AddKey(TEXT("CanAct"),NewObject<UBlackboardKeyType_Bool>(ArenaBlackboard));
 AddKey(TEXT("TargetAttacking"),NewObject<UBlackboardKeyType_Bool>(ArenaBlackboard));
 AddKey(TEXT("ActionResource"),NewObject<UBlackboardKeyType_Float>(ArenaBlackboard));
 AddKey(TEXT("Branch"),NewObject<UBlackboardKeyType_Int>(ArenaBlackboard));
 ArenaTree=NewObject<UBehaviorTree>(this,TEXT("BT_Arena"));
 ArenaTree->BlackboardAsset=ArenaBlackboard;
 auto* Root=NewObject<UBTComposite_Sequence>(ArenaTree,TEXT("DecisionCycle"));
 ArenaTree->RootNode=Root;
 FBTCompositeChild DecideChild; DecideChild.ChildTask=NewObject<UBTTask_ArenaDecide>(Root); Root->Children.Add(DecideChild);
 auto* Selector=NewObject<UBTComposite_Selector>(Root,TEXT("CombatBranches"));
 FBTCompositeChild SelectChild; SelectChild.ChildComposite=Selector; Root->Children.Add(SelectChild);
 for (EAIBranch Branch : {EAIBranch::Approach,EAIBranch::Strafe,EAIBranch::Retreat,EAIBranch::Attack,EAIBranch::Defend,EAIBranch::Dodge,EAIBranch::Wait})
 {
  FBTCompositeChild Child;
  auto* Task=NewObject<UBTTask_ArenaAction>(Selector); Task->Branch=Branch; Child.ChildTask=Task;
  auto* Gate=NewObject<UBTDecorator_ArenaBranch>(Selector); Gate->Branch=Branch; Child.Decorators.Add(Gate);
  Selector->Children.Add(Child);
 }
}
AFighterCharacter* AFighterAIController::GetSelf() const { return Cast<AFighterCharacter>(GetPawn()); }
AFighterCharacter* AFighterAIController::GetTarget() const { return Blackboard ? Cast<AFighterCharacter>(Blackboard->GetValueAsObject(TEXT("Target"))) : nullptr; }
double AFighterAIController::Now() const { return GetWorld()->GetTimeSeconds(); }
bool AFighterAIController::CanRun() const
{
 auto* GM=GetWorld()->GetAuthGameMode<ATrainingGameMode>();
 auto* Self=GetSelf(); auto* Target=GetTarget();
 return bActive && GM && GM->OpponentMode==EOpponentMode::AI && !GM->IsTrainingMenuOpen() && !GM->IsMatchResolved()
  && IsValid(Self) && IsValid(Target) && !Self->IsDead() && !Target->IsDead() && Self->GetCombatInput()->AreRequestsEnabled();
}
bool AFighterAIController::IsAIActive() const
{ auto* BT=Cast<UBehaviorTreeComponent>(BrainComponent); return bActive && BT && BT->IsRunning(); }
EAIBranch AFighterAIController::GetCurrentBranch() const
{ return Blackboard ? static_cast<EAIBranch>(Blackboard->GetValueAsInt(TEXT("Branch"))) : EAIBranch::Wait; }
void AFighterAIController::ActivateAI(AFighterCharacter* Target)
{
 if (bActive && GetTarget()==Target) return;
 DeactivateAI(); BuildTree();
 UBlackboardComponent* BB=Blackboard;
 if (!UseBlackboard(ArenaBlackboard,BB)) return;
 Blackboard->SetValueAsObject(TEXT("Target"),Target);
 RandomStream.Initialize(Params.RandomSeed);
 LastBranchSwitch=-1000.; ObservedAttackAt=-1.; NextMoveAttempt=0.; LastStrafeFlip=Now();
 bActive=true;
 if (!RunBehaviorTree(ArenaTree)) { bActive=false; LogDecision(TEXT("TreeStartFailed")); return; }
 SetActorTickEnabled(true); LogDecision(TEXT("Started BT_Arena / BB_Arena"));
}
void AFighterAIController::DeactivateAI()
{
 bActive=false;
 if (auto* BT=Cast<UBehaviorTreeComponent>(BrainComponent)) BT->StopTree(EBTStopMode::Forced);
 StopMovement(); ClearFocus(EAIFocusPriority::Gameplay);
 if (auto* F=GetSelf()) { F->GetCombatInput()->InvalidateSession(FText::FromString(TEXT("AI停止"))); F->GetCombatInput()->ReleaseContinuousInputs(); }
 if (Blackboard) Blackboard->SetValueAsInt(TEXT("Branch"),int32(EAIBranch::Wait));
 TaskStatus=TEXT("Stopped"); SetActorTickEnabled(false);
}
void AFighterAIController::OnUnPossess() { DeactivateAI(); Super::OnUnPossess(); }
void AFighterAIController::EndPlay(const EEndPlayReason::Type Reason) { DeactivateAI(); Super::EndPlay(Reason); }
void AFighterAIController::SetParams(FArenaAIParams Value)
{
 Value.DecisionInterval=FMath::Clamp(Value.DecisionInterval,.05f,5.f);
 Value.ReactionDelay=FMath::Max(0.f,Value.ReactionDelay);
 Value.MinBranchHoldTime=FMath::Max(0.f,Value.MinBranchHoldTime);
 Value.AttackRange=FMath::Max(90.f,Value.AttackRange);
 Value.AttackRangeExit=FMath::Max(Value.AttackRange,Value.AttackRangeExit);
 Value.RetreatDistance=FMath::Clamp(Value.RetreatDistance,0.f,Value.AttackRange);
 if (Params.RandomSeed!=Value.RandomSeed) RandomStream.Initialize(Value.RandomSeed);
 Params=Value;
}
void AFighterAIController::Tick(float Delta)
{
 Super::Tick(Delta); Observe();
 // 只撤销寻路，避免 StopMovementImmediately 抹掉已生效击退/闪避速度。
 if (!CanRun() || !GetSelf()->CanAct()) { StopMovement(); ClearFocus(EAIFocusPriority::Gameplay); }
}
void AFighterAIController::Observe()
{
 if (!Blackboard) return;
 auto* Self=GetSelf(); auto* Target=GetTarget();
 const bool Valid=IsValid(Self) && IsValid(Target) && !Target->IsDead();
 Blackboard->SetValueAsFloat(TEXT("Distance"),Valid ? FVector::Dist2D(Self->GetActorLocation(),Target->GetActorLocation()) : MAX_flt);
 Blackboard->SetValueAsBool(TEXT("CanAct"),CanRun() && Self->CanAct());
 const bool Attacking=Valid && Target->IsAttacking();
 if (Attacking && ObservedAttackAt<0.) ObservedAttackAt=Now();
 if (!Attacking) ObservedAttackAt=-1.;
 Blackboard->SetValueAsBool(TEXT("TargetAttacking"),Attacking);
 Blackboard->SetValueAsFloat(TEXT("ActionResource"),Self ? Self->GetActionResource() : 0.f);
}
void AFighterAIController::Decide()
{
 Observe(); const EAIBranch Previous=GetCurrentBranch(); EAIBranch Next=EAIBranch::Wait;
 if (CanRun() && Blackboard->GetValueAsBool(TEXT("CanAct")))
 {
  const float D=Blackboard->GetValueAsFloat(TEXT("Distance"));
  const bool Threat=ObservedAttackAt>=0. && Now()-ObservedAttackAt>=Params.ReactionDelay && D<=Params.AttackRangeExit+80.f;
  const float Roll=RandomFraction();
  if (Threat && Roll<Params.DodgeChance && Blackboard->GetValueAsFloat(TEXT("ActionResource"))>=1.f) Next=EAIBranch::Dodge;
  else if (Threat && Roll<Params.DodgeChance+Params.DefendChance) Next=EAIBranch::Defend;
  else if (D<Params.RetreatDistance) Next=EAIBranch::Retreat;
  else if (D<=(Previous==EAIBranch::Attack ? Params.AttackRangeExit : Params.AttackRange)) Next=Roll<Params.AttackChance ? EAIBranch::Attack : EAIBranch::Strafe;
  else if (D>Params.AttackRangeExit+80.f) Next=EAIBranch::Approach;
  else Next=EAIBranch::Strafe;
  const bool WasMoving=Previous==EAIBranch::Approach || Previous==EAIBranch::Strafe || Previous==EAIBranch::Retreat;
  if (!Threat && WasMoving && Next!=Previous && Now()-LastBranchSwitch<Params.MinBranchHoldTime) Next=Previous;
 }
 if (Next!=Previous) LastBranchSwitch=Now();
 Blackboard->SetValueAsInt(TEXT("Branch"),int32(Next));
 LogDecision(FString::Printf(TEXT("Branch=%s Distance=%.1f ObservedFor=%.2f"),*StaticEnum<EAIBranch>()->GetNameStringByValue(int64(Next)),Blackboard->GetValueAsFloat(TEXT("Distance")),ObservedAttackAt<0. ? -1. : Now()-ObservedAttackAt));
}
bool AFighterAIController::ProjectDestination(const FVector& Point,FVector& Out) const
{
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()); auto* Self=GetSelf(); if (!Nav || !Self) return false;
 FNavLocation P; const auto& Agent=Self->GetCharacterMovement()->GetNavAgentPropertiesRef();
 if (!Nav->ProjectPointToNavigation(Point,P,FVector(60,60,250),&Agent)) return false;
 // 白盒安全区半径 1100 cm，墙外点不能投影成合法目标。
 if (FVector2D(Point).Size()>1100.f || FVector2D(P.Location).Size()>1100.f || FVector::Dist2D(Point,P.Location)>65.f) return false;
 if (FVector::Dist2D(Self->GetActorLocation(),P.Location)<2.f) { Out=P.Location; return true; }
 auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Self->GetActorLocation(),P.Location,GetPawn());
 if (!Path || !Path->IsValid() || Path->IsPartial()) return false;
 Out=P.Location; return true;
}
bool AFighterAIController::IsLegalDestination(FVector Point) const { FVector P; return ProjectDestination(Point,P); }
bool AFighterAIController::IsNavigationReady() const { return GetSelf() && IsLegalDestination(GetSelf()->GetActorLocation()); }
bool AFighterAIController::BeginMove(EAIBranch Branch)
{
 if (!CanRun() || !GetSelf()->CanAct() || Now()<NextMoveAttempt) return false;
 if (Now()-LastStrafeFlip>=Params.StrafeSwitchInterval) { StrafeSign*=-1.f; LastStrafeFlip=Now(); }
 const FVector Start=GetSelf()->GetActorLocation(), Target=GetTarget()->GetActorLocation();
 const FVector Toward=(Target-Start).GetSafeNormal2D(); FVector Desired=Target-Toward*(Params.AttackRange*.8f);
 if (Branch==EAIBranch::Strafe) Desired=Start+FVector(-Toward.Y,Toward.X,0.f)*StrafeSign*180.f;
 if (Branch==EAIBranch::Retreat) Desired=Start-Toward*200.f;
 FVector Projected;
 if (!ProjectDestination(Desired,Projected))
 {
  Desired=Start+FVector(-Toward.Y,Toward.X,0.f)*(-StrafeSign)*180.f;
  if (Branch==EAIBranch::Approach || !ProjectDestination(Desired,Projected))
  { NextMoveAttempt=Now()+.6; SetTaskStatus(TEXT("NavigationRejected / retry 0.6s")); return false; }
 }
 SetFocus(GetTarget());
 const auto Result=MoveToLocation(Projected,25.f,false,true,false,false,nullptr,false);
 NextMoveAttempt=Now()+.15;
 SetTaskStatus(Result==EPathFollowingRequestResult::Failed ? TEXT("MoveFailed / retry") : TEXT("MoveRequested"));
 return Result!=EPathFollowingRequestResult::Failed;
}
void AFighterAIController::SetTaskStatus(const FString& Status) { TaskStatus=Status; LogDecision(Status); }
FString AFighterAIController::GetDebugState() const
{ return FString::Printf(TEXT("BT %s | %s | %s | bindings=%d"),IsAIActive()?TEXT("Running"):TEXT("Stopped"),*StaticEnum<EAIBranch>()->GetNameStringByValue(int64(GetCurrentBranch())),*TaskStatus,ActionBindingCount); }
void AFighterAIController::LogDecision(const FString& Text)
{
 const FString Line=FString::Printf(TEXT("%.3f %s"),Now(),*Text);
 DecisionLog.Add(Line); if (DecisionLog.Num()>64) DecisionLog.RemoveAt(0);
 UE_LOG(LogTemp,Log,TEXT("[ArenaAI] %s"),*Line);
}
