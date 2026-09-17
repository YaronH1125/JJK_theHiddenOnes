#include "Training/FighterAIController.h"
#include "Training/ArenaBTNodes.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterAttributeSet.h"
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
 for (EAIBranch Branch : {EAIBranch::Approach,EAIBranch::Strafe,EAIBranch::Retreat,EAIBranch::Attack,EAIBranch::Defend,EAIBranch::Dodge,EAIBranch::RangedAttack,EAIBranch::Domain,EAIBranch::Wait})
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
 // ABP_Unarmed requires both speed and acceleration to enter locomotion.
 // Direct-velocity path following otherwise leaves ShouldMove false while sliding.
 if (auto* Self=GetSelf()) Self->GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths=true;
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
 bActive=false; DebugBranch.Reset();
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
 Value.AttackChance=FMath::Clamp(Value.AttackChance,0.f,1.f);
 Value.HeavyChance=FMath::Clamp(Value.HeavyChance,0.f,1.f);
 Value.DodgeChance=FMath::Clamp(Value.DodgeChance,0.f,1.f);
 Value.DefendChance=FMath::Clamp(Value.DefendChance,0.f,1.f-Value.DodgeChance);
 Value.StrafeSwitchInterval=FMath::Max(.1f,Value.StrafeSwitchInterval);
 Params=Value;
}
void AFighterAIController::Tick(float Delta)
{
 Super::Tick(Delta); Observe();
 // 只撤销寻路，避免 StopMovementImmediately 抹掉已生效击退/闪避速度。
 if (!CanRun() || !GetSelf()->CanAct()) { StopPathKeepingVelocity(); ClearFocus(EAIFocusPriority::Gameplay); }
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
 auto* Self=GetSelf();
 if (CanRun() && Blackboard->GetValueAsBool(TEXT("CanAct")) && Self)
 {
  const float D=Blackboard->GetValueAsFloat(TEXT("Distance"));
  const bool Threat=ObservedAttackAt>=0. && Now()-ObservedAttackAt>=Params.ReactionDelay && D<=Params.AttackRangeExit+80.f;
  const float Roll=RandomFraction();
  const bool CanPayDodge=GetSelf()->GetFighterAbilitySystemComponent()->HasInfiniteResources() || Blackboard->GetValueAsFloat(TEXT("ActionResource"))>=GetSelf()->GetDefinition()->DodgeConfig.DodgeCost;
  // M6：远程/领域决策输入（总开关默认关：保持 M5 近战行为）
  const bool bRanged = Params.bEnableRangedCombat && Self->GetStance()==EFighterStance::Ranged;
  const UFighterDefinition* Def=Self->GetDefinition();
  const float BlastRange = Def ? Def->MobileBlast.Range : 1800.f;
  const float CaptureRange = Def ? Def->DomainConfig.CaptureRange : 1200.f;
  const auto* ASC=Self->GetFighterAbilitySystemComponent();
  const bool bDomainReady = Params.bEnableRangedCombat && ASC && !Self->IsDomainActive()
   && ASC->GetNumericAttribute(UFighterAttributeSet::GetEnergyAttribute())
      >= ASC->GetNumericAttribute(UFighterAttributeSet::GetMaxEnergyAttribute()) - 1.f;
  if (Threat && Roll<Params.DodgeChance && CanPayDodge) Next=EAIBranch::Dodge;
  else if (Threat && Roll<Params.DodgeChance+Params.DefendChance) Next=EAIBranch::Defend;
  else if (bDomainReady && D<=CaptureRange && Roll<Params.DomainChance) Next=EAIBranch::Domain;
  else if (bRanged)
  {
   if (D>BlastRange) Next=EAIBranch::Approach;
   else if (D>900.f) Next=Roll<.5f ? EAIBranch::RangedAttack : EAIBranch::Approach; // 远处边打边收
   else if (D>Params.RetreatDistance)
   {
    // 进入炮击范围内有概率切回近战（自然近远节奏）；不炮击时继续逼近
    if (D<=400.f && Roll<.35f && Self->RequestStanceSwitch()) LogDecision(TEXT("Ranged close: switch to melee"));
    Next=Roll<Params.RangedBlastChance ? EAIBranch::RangedAttack : (D>Params.AttackRange ? EAIBranch::Approach : EAIBranch::Strafe);
   }
   else
   {
    // 近距离远程：切回近战（攻击范围内即贴脸，不只 120 内）
    if (Params.bEnableRangedCombat && D<=Params.AttackRange && Self->RequestStanceSwitch()) LogDecision(TEXT("Ranged close: switch to melee"));
    Next=D<=Params.AttackRange ? EAIBranch::Retreat : EAIBranch::Retreat;
   }
  }
  else if (D<Params.RetreatDistance) Next=EAIBranch::Retreat;
  else if (D<=(Previous==EAIBranch::Attack ? Params.AttackRangeExit : Params.AttackRange)) Next=Roll<Params.AttackChance ? EAIBranch::Attack : EAIBranch::Strafe;
  else if (D>Params.AttackRangeExit+80.f)
  {
   Next=EAIBranch::Approach;
   // 中远距离：概率切入远程形态（下轮决策生效；仅远程开关开启时）
   if (Params.bEnableRangedCombat && D>Params.AttackRangeExit+120.f && RandomFraction()<.6f && Self->RequestStanceSwitch())
    LogDecision(TEXT("Long range: switch to ranged"));
  }
  else Next=EAIBranch::Strafe;
  const bool WasMoving=Previous==EAIBranch::Approach || Previous==EAIBranch::Strafe || Previous==EAIBranch::Retreat;
  if (!Threat && WasMoving && Next!=Previous && Now()-LastBranchSwitch<Params.MinBranchHoldTime) Next=Previous;
 }
#if !UE_BUILD_SHIPPING
 if (DebugBranch.IsSet() && CanRun()) { Next=DebugBranch.GetValue(); DebugBranch.Reset(); }
#endif
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

void AFighterAIController::StopPathKeepingVelocity()
{
 if (auto* Path=GetPathFollowingComponent())
  if (Path->GetStatus()!=EPathFollowingStatus::Idle)
   Path->AbortMove(*this,FPathFollowingResultFlags::ForcedScript,FAIRequestID::CurrentRequest,EPathFollowingVelocityMode::Keep);
}
void AFighterAIController::DebugRequestBranch(EAIBranch Branch)
{
#if !UE_BUILD_SHIPPING
 if (!bActive) return;
 if (auto* BT=Cast<UBehaviorTreeComponent>(BrainComponent))
 { BT->StopTree(EBTStopMode::Forced); DebugBranch=Branch; BT->StartTree(*ArenaTree); }
#endif
}
