#include "Training/M5SmokeHarness.h"
#include "Training/TrainingGameMode.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterAttributeSet.h"
#include "Training/CombatInputComponent.h"
#include "Training/ArenaPlayerController.h"
#include "Training/TrainingPanelWidget.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"

UM5SmokeHarness::UM5SmokeHarness() { PrimaryComponentTick.bCanEverTick=true; }
void UM5SmokeHarness::Check(bool Condition,const TCHAR* Name)
{
 ++Checks; if (!Condition) ++Failures;
 const FString Line=FString::Printf(TEXT("%s %s"),Condition?TEXT("PASS"):TEXT("FAIL"),Name);
 Results.Add(Line); UE_LOG(LogTemp,Display,TEXT("[M5Smoke] %s"),*Line);
}
void UM5SmokeHarness::Complete()
{
 Results.Add(FString::Printf(TEXT("checks=%d failures=%d"),Checks,Failures));
 FFileHelper::SaveStringArrayToFile(Results,*(FPaths::ProjectSavedDir()/TEXT("M5_PackageSmoke.txt")));
 UE_LOG(LogTemp,Display,TEXT("[M5Smoke] COMPLETE checks=%d failures=%d"),Checks,Failures);
 SetComponentTickEnabled(false); FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
}
void UM5SmokeHarness::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Delta,Type,Tick);
#if !UE_BUILD_SHIPPING
 auto* GM=Cast<ATrainingGameMode>(GetOwner()); if (!GM) return;
 auto* P1=GM->GetPlayerFighter(); auto* P2=GM->GetOpponentFighter();
 auto* PC=Cast<AArenaPlayerController>(GetWorld()->GetFirstPlayerController());
 const double Now=GetWorld()->GetTimeSeconds();
 if (!Started) Started=Now;
 if (Now-Started>120.) { Check(false,TEXT("scenario timeout")); Complete(); return; }
 if (!P1 || !P2 || !PC) return;
 auto* AI=GM->GetOpponentAI();
 auto Next=[&]() { ++Stage; StageTime=Now; };
 auto Restart=[&]() { GM->RestartMatch(); PC->SetTrainingPanelOpen(false); };
 switch(Stage)
 {
 case 0:
  if (Now-Started<3.) break;
  Check(GetWorld()->GetMapName().Contains(TEXT("L_TrainingArena")),TEXT("cooked training map"));
  GM->SetOpponentMode(EOpponentMode::AI);
  InitialHealth=P1->GetFighterAttributeSet()->GetHealth(); Next(); break;
 case 1:
  if (Now-StageTime<2.) break;
  Check(AI && AI->IsAIActive() && AI->ArenaTree && AI->ArenaTree->RootNode,TEXT("native BT and blackboard in package"));
  Check(AI && AI->IsNavigationReady(),TEXT("navigation loaded after cold start"));
  Next(); break;
 case 2:
  if (FVector::Dist2D(P1->GetActorLocation(),P2->GetActorLocation())>300.f) break;
  Check(true,TEXT("pathfinding approach"));
  if(AI) { auto P=AI->Params; P.AttackChance=1.f; P.RetreatDistance=0.f; AI->SetParams(P); }
  Next(); break;
 case 3:
  if(P1->GetFighterAttributeSet()->GetHealth()>=InitialHealth) break;
  Check(true,TEXT("shared GAS contact caused actual damage"));
  P1->JJKDebugKill(); Next(); break;
 case 4:
  if(!GM->IsMatchResolved()) break;
  Check(GM->GetMatchOutcome()==EMatchOutcome::OpponentWin,TEXT("opponent victory"));
  Check(GM->IsTrainingMenuOpen() && AI && !AI->IsAIActive() && AI->GetActionBindingCount()==0,TEXT("result panel and task cleanup"));
  Restart(); Next(); break;
 case 5:
  if(Now-StageTime<1.) break;
  Check(!P1->IsDead() && !P2->IsDead() && AI && AI->IsAIActive(),TEXT("restart 1"));
  P2->JJKDebugKill(); Next(); break;
 case 6:
  if(!GM->IsMatchResolved()) break;
  Check(GM->GetMatchOutcome()==EMatchOutcome::PlayerWin,TEXT("player victory"));
  Restart(); Next(); break;
 case 7:
  if(Now-StageTime<1.) break;
  Check(AI && AI->IsAIActive(),TEXT("restart 2"));
  P1->JJKDebugKill(); P2->JJKDebugKill(); Next(); break;
 case 8:
  if(!GM->IsMatchResolved()) break;
  Check(GM->GetMatchOutcome()==EMatchOutcome::Draw && GM->GetMatchResolutionCount()==3,TEXT("draw / exactly three results"));
  // 包内实际 UMG 与角色画面；有图形运行才会生成此图。
  FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("M5_PackageResult.png"),true,false);
  Next(); break;
 case 9:
  if(Now-StageTime<2.) break;
  Restart(); GM->SetOpponentMode(EOpponentMode::FixedGuard);
  Check(P2->IsGuardIntent() && !GM->GetOpponentAI(),TEXT("fixed guard after restart 3"));
  GM->SetOpponentMode(EOpponentMode::Static);
  Check(!P2->IsGuardIntent() && !P2->GetController(),TEXT("static has no old controller/guard"));
  GM->SetOpponentMode(EOpponentMode::AI); Next(); break;
 case 10:
  if(Now-StageTime<1.) break;
  Check(AI && AI->IsAIActive() && AI->IsNavigationReady(),TEXT("AI reentry"));
  Complete(); break;
 }
#endif
}
