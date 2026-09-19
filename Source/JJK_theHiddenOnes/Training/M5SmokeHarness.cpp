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
#include "EngineUtils.h"
#include "Training/DomainOrb.h"
#include "Training/TargetingComponent.h"
#include "Training/FighterDefinition.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Training/BlastProjectile.h"

UM5SmokeHarness::UM5SmokeHarness() { PrimaryComponentTick.bCanEverTick=true; }
void UM5SmokeHarness::Check(bool Condition,const TCHAR* Name)
{
 ++Checks; if (!Condition) ++Failures;
 const FString Line=FString::Printf(TEXT("%s %s"),Condition?TEXT("PASS"):TEXT("FAIL"),Name);
 Results.Add(Line); UE_LOG(LogTemp,Display,TEXT("[M5Smoke] %s"),*Line);
}
void UM5SmokeHarness::Complete()
{
 if(!FrameMilliseconds.IsEmpty())
 {
  FrameMilliseconds.Sort();
  double Sum=0.; for(double Value:FrameMilliseconds) Sum+=Value;
  const FString Perf=FString::Printf(TEXT("PERF workload=smoke frames=%d mean_ms=%.3f p95_ms=%.3f max_ms=%.3f mean_fps=%.2f"),
   FrameMilliseconds.Num(),Sum/FrameMilliseconds.Num(),FrameMilliseconds[FMath::Min(FrameMilliseconds.Num()-1,FMath::FloorToInt(FrameMilliseconds.Num()*.95))],FrameMilliseconds.Last(),FrameMilliseconds.Num()*1000./FMath::Max(Sum,.001));
  Results.Add(Perf); UE_LOG(LogTemp,Display,TEXT("[M5Smoke] %s"),*Perf);
 }
 Results.Add(FString::Printf(TEXT("checks=%d failures=%d"),Checks,Failures));
 FFileHelper::SaveStringArrayToFile(Results,*(FPaths::ProjectSavedDir()/TEXT("M5_PackageSmoke.txt")));
 UE_LOG(LogTemp,Display,TEXT("[M5Smoke] COMPLETE checks=%d failures=%d"),Checks,Failures);
 SetComponentTickEnabled(false); FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
}
void UM5SmokeHarness::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Delta,Type,Tick);
#if !UE_BUILD_SHIPPING
 const double WallTime=FPlatformTime::Seconds();
 if(LastFrameWallTime>0. && Stage>=1 && Stage<18) FrameMilliseconds.Add((WallTime-LastFrameWallTime)*1000.);
 LastFrameWallTime=WallTime;
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
  {
   FString ExpectedMap=TEXT("L_TrainingArena");
   FParse::Value(FCommandLine::Get(),TEXT("SmokeMap="),ExpectedMap);
   Results.Add(FString::Printf(TEXT("map=%s expected=%s game_mode=%s"),*GetWorld()->GetMapName(),*ExpectedMap,*GM->GetClass()->GetPathName()));
   Check(GetWorld()->GetMapName().EndsWith(ExpectedMap),TEXT("cooked training map"));
  }
  GM->SetOpponentMode(EOpponentMode::AI);
  InitialHealth=P1->GetFighterAttributeSet()->GetHealth(); Next(); break;
 case 1:
  if (Now-StageTime<2.) break;
  if ((!AI || !AI->IsNavigationReady()) && Now-StageTime<15.) break;
  Check(AI && AI->IsAIActive() && AI->ArenaTree && AI->ArenaTree->RootNode,TEXT("native BT and blackboard in package"));
  Check(AI && AI->IsNavigationReady(),TEXT("navigation loaded after cold start"));
  UE_LOG(LogTemp,Display,TEXT("[M5Smoke] scene P1=%s P2=%s levels=%d actors=%d"),*P1->GetActorLocation().ToString(),*P2->GetActorLocation().ToString(),GetWorld()->GetNumLevels(),GetWorld()->GetActorCount());
  FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Dojo_ColdStart.png"),true,false);
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
  // Skill checks need a fresh, stationary target. A live AI can interrupt charging,
  // dodge, guard or kill the shooter; those are valid gameplay, not timing noise.
  GM->SetOpponentMode(EOpponentMode::Static);
  Restart();
  // ---- M6：双炮/领域包内链路 ----
  if(auto* Inp=P1->GetCombatInput()) Inp->NotifyStanceSwitchPressed();
  Next(); break;
 case 11:
  // The stance tag flips immediately, but the 0.1s presentation window still blocks attacks.
  if(!P1->HasCombatTag(TAG_Stance_Ranged) || P1->HasCombatTag(TAG_State_StanceSwitching)) break;
  Check(true,TEXT("M6 stance switch in package"));
  P1->GetTargeting()->LockBestTarget();
  SuperHealthSnapshot=P2->GetFighterAttributeSet()->GetHealth();
  if(auto* Inp=P1->GetCombatInput()) Inp->NotifyAttackPressed();
  Next(); break;
 case 12:
  if(Now-StageTime<P1->GetDefinition()->MobileBlast.CapTime+.4f) break;
  if(auto* Inp=P1->GetCombatInput()) Inp->NotifyAttackReleased();
  Next(); break;
 case 13:
  if(Now-StageTime<2.0f) break; // Include projectile travel and recovery.
  if(P2->GetFighterAttributeSet()->GetHealth()>=SuperHealthSnapshot && Now-StageTime<6.f) break;
  BlastHealthSnapshot=P2->GetFighterAttributeSet()->GetHealth();
  Check(FMath::IsNearlyEqual(SuperHealthSnapshot-BlastHealthSnapshot,P1->GetDefinition()->MobileBlast.MaxDamage,1.f),TEXT("M6 mobile blast fires at cap"));
  if(auto* Inp=P1->GetCombatInput()) Inp->NotifyKickPressed();
  Next(); break;
 case 14:
  if(Now-StageTime<P1->GetDefinition()->SuperBlast.CapTime+.4f) break;
  SuperHealthSnapshot=P2->GetFighterAttributeSet()->GetHealth();
  if(auto* Inp=P1->GetCombatInput()) Inp->NotifyKickReleased();
  Next(); break;
 case 15:
  if(Now-StageTime<2.f) break; // Current recovery is 0.9s, plus windup and frame boundaries.
  if((P2->GetFighterAttributeSet()->GetHealth()>=SuperHealthSnapshot || !P1->HasCombatTag(TAG_State_SuperBlastCooldown)) && Now-StageTime<6.f) break;
  Check(FMath::IsNearlyEqual(SuperHealthSnapshot-P2->GetFighterAttributeSet()->GetHealth(),P1->GetDefinition()->SuperBlast.MaxDamage,1.f),TEXT("M6 super blast fires at cap"));
  Check(P1->HasCombatTag(TAG_State_SuperBlastCooldown),TEXT("M6 super cooldown in package"));
  // 领域：给 P2 补满能量后由对手侧展开，验证球体链路
  if(auto* P2F=GM->GetOpponentFighter()) P2F->ModifyEnergy(100.f);
  if(auto* P2F=GM->GetOpponentFighter())
   if(auto* Inp=P2F->GetCombatInput()) Inp->NotifyDomainPressed();
  Next(); break;
 case 16:
  {
   int32 OrbCount=0;
   for(TActorIterator<ADomainOrb> It(GetWorld());It;++It) ++OrbCount;
   auto* P2F=GM->GetOpponentFighter();
   const bool bDom=(P2F&&P2F->HasCombatTag(TAG_State_DomainActive))||OrbCount>0;
   if(!bDom) break;
   OrbHealthSnapshot=P1->GetFighterAttributeSet()->GetHealth(); Next(); break;
  }
 case 17:
  if(Now-StageTime<5.f && !P1->HasCombatTag(TAG_State_Dead))
  {
   if(P1->GetFighterAttributeSet()->GetHealth()<OrbHealthSnapshot)
    Check(true,TEXT("M6 domain orb contact damage"));
   else break;
  }
  else Check(P1->HasCombatTag(TAG_State_Dead),TEXT("M6 domain phase completed"));
  GM->RestartMatch(); Next(); break;
 case 18:
  {
   if(Now-StageTime<1.5f) break;
   int32 OrbCount=0;
   for(TActorIterator<ADomainOrb> It(GetWorld());It;++It) ++OrbCount;
   auto* P2F=GM->GetOpponentFighter();
   Check(OrbCount==0 && P2F && !P2F->HasCombatTag(TAG_State_DomainActive),TEXT("M6 domain cleanup after restart"));
   if(FParse::Param(FCommandLine::Get(),TEXT("DojoSweepRegression")))
   {
    // Real collision regression: force one long integration step across target AND wall.
    // No asset changes; normal play never runs this explicit diagnostic.
    GM->SetOpponentMode(EOpponentMode::Static); Restart();
    auto FireProbe=[&](bool bFromOutside)
    {
     FActorSpawnParameters SP; SP.Owner=P1; SP.Instigator=P1;
     SP.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
     const FVector Direction=bFromOutside ? -FVector::ForwardVector : FVector::ForwardVector;
     const FVector Start=P2->GetActorLocation()+(bFromOutside ? FVector(1000,0,0) : FVector(-200,0,0));
     FCollisionQueryParams QP; QP.AddIgnoredActor(P1); QP.AddIgnoredActor(P2);
     FHitResult Wall;
     const bool bWall=GetWorld()->LineTraceSingleByChannel(Wall,Start,Start+Direction*2000.f,ECC_Visibility,QP);
     const float WallDistance=FVector::Dist(Start,Wall.ImpactPoint);
     const bool bFixture=bWall && (bFromOutside ? WallDistance<1000.f : WallDistance>200.f);
     auto* Probe=GetWorld()->SpawnActor<ABlastProjectile>(ABlastProjectile::StaticClass(),Start,FRotator::ZeroRotator,SP);
     if(!Probe) return false;
     FRangedHitSettle Hit; Hit.Damage=30.f; Hit.bBlockable=false; Hit.bDodgeable=false;
     Hit.bGrantCurse=false; Hit.KnockbackStrength=0.f;
     Probe->InitBlast(P1,Direction,20000.f,22.f,3.f,Hit);
     Probe->Tick(.1f);
     if(IsValid(Probe)) Probe->Destroy();
     return bFixture;
    };
    const float Before=P2->GetFighterAttributeSet()->GetHealth();
    const bool bBehindWall=FireProbe(false);
    Check(bBehindWall && FMath::IsNearlyEqual(Before-P2->GetFighterAttributeSet()->GetHealth(),30.f,.01f),TEXT("sweep hits nearer fighter before background wall"));
    Restart();
    const float GuardedHealth=P2->GetFighterAttributeSet()->GetHealth();
    const bool bForegroundWall=FireProbe(true);
    Check(bForegroundWall && FMath::IsNearlyEqual(P2->GetFighterAttributeSet()->GetHealth(),GuardedHealth,.01f),TEXT("sweep preserves foreground wall occlusion"));
    GM->ResetTraining();
   }
   // 包内实际 UMG 与角色画面；有图形运行才会生成此图。
   FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("M5_PackageResult.png"),true,false);
   Complete(); break;
  }
 }
#endif
}
