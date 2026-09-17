// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/TrainingGameMode.h"
#include "Training/M5SmokeHarness.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Training/FighterAIController.h"
#include "Training/ArenaPlayerController.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/CombatHitComponent.h"
#include "Training/CombatInputComponent.h"
#include "Training/CombatTypes.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Training/TargetingComponent.h"
#include "Training/TrainingProbeAbility.h"
#include "Training/DomainOrb.h"
#include "Training/ArenaCrosshairHud.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATrainingGameMode::ATrainingGameMode()
{
	HUDClass = AArenaCrosshairHud::StaticClass();
	PrimaryActorTick.bCanEverTick = true;

	// 擂台默认出生配置：24m 擂台上 P1(-500,0) 朝 +X，P2(500,0) 朝 -X（07_擂台与视觉参考.md）
	PlayerSpawnGroundTransform = FTransform(FRotator(0.f, 0.f, 0.f), FVector(-500.f, 0.f, 0.f));
	OpponentSpawnGroundTransform = FTransform(FRotator(0.f, 180.f, 0.f), FVector(500.f, 0.f, 0.f));

	// 玩家控制器使用训练场实现（锁定/回正输入语义与调试命令）
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void ATrainingGameMode::StartPlay()
{
	Super::StartPlay();
	EnsureFightersSpawned();
#if !UE_BUILD_SHIPPING
 if(FParse::Param(FCommandLine::Get(),TEXT("M5SmokeTest")))
 {
  auto* Harness=NewObject<UM5SmokeHarness>(this); Harness->RegisterComponent();
 }
#endif
}

void ATrainingGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDebugHud)
	{
		DrawCombatDebug();
	}
	CheckMatchOutcome();
	TickDomainSessions();
}

void ATrainingGameMode::DrawCombatDebug() const
{
	int32 Line = 0;
	auto DrawFighter = [this, &Line](const TCHAR* Label, const AFighterCharacter* Fighter)
	{
		if (Fighter == nullptr)
		{
			GEngine->AddOnScreenDebugMessage(100 + Line++, 0.f, FColor::Silver,
				FString::Printf(TEXT("%s: 未生成"), Label));
			return;
		}
		const UCombatHitComponent* Hit = Fighter->GetCombatHit();
		const UCombatInputComponent* Input = Fighter->GetCombatInput();
		const UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent();
		const TCHAR* PhaseNames[] = {TEXT("None"), TEXT("Windup"), TEXT("Active"), TEXT("Recovery")};

		FString Status;
		if (Fighter->IsDead()) { Status = TEXT("死亡"); }
		else if (ASC && ASC->HasMatchingGameplayTag(TAG_State_HitStun)) { Status = TEXT("受击硬直"); }
		else if (Hit->HasActiveAttack()) { Status = FString::Printf(TEXT("攻击中(%s)"), PhaseNames[static_cast<int32>(Hit->GetPhase())]); }
		else { Status = TEXT("待机"); }

		GEngine->AddOnScreenDebugMessage(100 + Line++, 0.f,
			Fighter == PlayerFighter ? FColor::Cyan : FColor::Orange,
			FString::Printf(TEXT("%s [%s] HP=%.0f 阶段=%s 实例=%llu 命中=%d 会话=%d"),
				Label, *Status,
				Fighter->GetFighterAttributeSet() ? Fighter->GetFighterAttributeSet()->GetHealth() : -1.f,
				PhaseNames[static_cast<int32>(Hit->GetPhase())],
				Hit->GetActiveInstanceId(), Hit->GetHitCount(),
				Input->GetActiveSessionId()));
        FGameplayTagContainer Tags;
        ASC->GetOwnedGameplayTags(Tags);
        GEngine->AddOnScreenDebugMessage(100 + Line++, 0.f, FColor::White,
            FString::Printf(TEXT("AR %.2f / CE %.0f | Segment %d Cache %d | %s"),
                Fighter->GetFighterAttributeSet()->GetActionResource(), Fighter->GetFighterAttributeSet()->GetCursedEnergy(),
                Hit->GetSegmentId(), int32(Input->PeekCachedAction()), *Tags.ToStringSimple()));
	};

	DrawFighter(TEXT("P1"), PlayerFighter);
	DrawFighter(TEXT("P2"), OpponentFighter);
	if (IsValid(OpponentAI))
	{
		GEngine->AddOnScreenDebugMessage(120, 0.f, FColor::Yellow, OpponentAI->GetDebugState());
	}
}

void ATrainingGameMode::RestartPlayer(AController* NewPlayer)
{
	// 默认 Pawn 不再生成：玩家控制器直接接管预生成的 P1
	APlayerController* PC = Cast<APlayerController>(NewPlayer);
	if (PC == nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[TrainingGM] RestartPlayer 忽略非玩家控制器 %s"), *GetNameSafe(NewPlayer));
		return;
	}

	EnsureFightersSpawned();

	if (PlayerFighter == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[TrainingGM] 玩家角色生成失败，无法接管"));
		return;
	}

	if (PlayerFighter->GetController() != nullptr && PlayerFighter->GetController() != PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainingGM] 玩家角色已被 %s 接管，拒绝重复 Possess"), *GetNameSafe(PlayerFighter->GetController()));
		return;
	}

	PC->Possess(PlayerFighter);
}

void ATrainingGameMode::EnsureFightersSpawned()
{
	if (IsValid(PlayerFighter) && IsValid(OpponentFighter))
	{
		return;
	}

	if (FighterClass == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[TrainingGM] 未配置 FighterClass，无法生成双方"));
		return;
	}

	if (!IsValid(PlayerFighter))
	{
		PlayerFighter = SpawnFighter(EFighterRole::Player, PlayerSpawnGroundTransform);
		if (PlayerFighter)
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->Possess(PlayerFighter);
		}
	}
	if (!IsValid(OpponentFighter))
	{
		OpponentFighter = SpawnFighter(EFighterRole::Opponent, OpponentSpawnGroundTransform);
	}

	if (PlayerFighter != nullptr && OpponentFighter != nullptr)
	{
		BindFighters();
		ApplyOpponentMode();
		// 分配身份对应的首选目标；是否锁定仍由玩家手动触发
		PlayerFighter->GetTargeting()->SetPreferredTarget(OpponentFighter);
		OpponentFighter->GetTargeting()->SetPreferredTarget(PlayerFighter);

		// 训练场身份颜色（角色定义仍是双方共享配置）
		PlayerFighter->SetMarkerColor(PlayerMarkerColor);
		OpponentFighter->SetMarkerColor(OpponentMarkerColor);

		UE_LOG(LogTemp, Log, TEXT("[TrainingGM] 双方已生成：P1=%s P2=%s 模式=%s"),
			*GetNameSafe(PlayerFighter), *GetNameSafe(OpponentFighter),
			OpponentMode == EOpponentMode::Static ? TEXT("Static") : TEXT("Unknown"));
	}
}

AFighterCharacter* ATrainingGameMode::SpawnFighter(EFighterRole InRole, const FTransform& GroundTransform)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Name = InRole == EFighterRole::Player ? TEXT("Fighter_P1") : TEXT("Fighter_P2");
	// 已销毁对象可能尚未 GC；身份不依赖 UObject 名，重生允许唯一后缀。
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Params.bDeferConstruction = true;

	AFighterCharacter* Fighter = World->SpawnActor<AFighterCharacter>(FighterClass, ResolveSpawnTransform(GroundTransform), Params);
	if (Fighter == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[TrainingGM] SpawnActor 失败：Role=%d"), static_cast<int32>(InRole));
		return nullptr;
	}

	Fighter->SetRole(InRole);
	if (FighterDefinition != nullptr)
	{
		Fighter->Definition = FighterDefinition;
	}
	// 配置、身份与静止模式必须先于 Construction/BeginPlay 初始化。
	if (InRole == EFighterRole::Opponent)
	{
		Fighter->AutoPossessAI = EAutoPossessAI::Disabled;
		Fighter->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	}
	Fighter->FinishSpawning(Fighter->GetActorTransform());
	Fighter->RecordInitialTransform(Fighter->GetActorTransform());

	// 对手保持无控制器：木桩只停止主动决策，角色本体照常运行
	if (InRole == EFighterRole::Opponent)
	{
		UE_LOG(LogTemp, Log, TEXT("[TrainingGM] 对手以 Static 模式生成（无控制器）"));
	}

	return Fighter;
}

FTransform ATrainingGameMode::ResolveSpawnTransform(const FTransform& GroundTransform) const
{
	float CapsuleHalfHeight = 96.f;
	if (FighterClass != nullptr)
	{
		if (const AFighterCharacter* CDO = FighterClass->GetDefaultObject<AFighterCharacter>())
		{
			if (const UCapsuleComponent* Capsule = CDO->GetCapsuleComponent())
			{
				CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
			}
		}
	}

	const FVector GroundLocation = GroundTransform.GetLocation();
	const FVector CenterLocation(GroundLocation.X, GroundLocation.Y, GroundLocation.Z + CapsuleHalfHeight + SpawnSafetyMargin);
	return FTransform(GroundTransform.GetRotation(), CenterLocation, GroundTransform.GetScale3D());
}

AFighterCharacter* ATrainingGameMode::GetOpponentOf(const AFighterCharacter* Fighter) const
{
	if (Fighter == nullptr)
	{
		return nullptr;
	}
	return Fighter == PlayerFighter ? OpponentFighter.Get() : (Fighter == OpponentFighter ? PlayerFighter.Get() : nullptr);
}

void ATrainingGameMode::ResetTraining()
{
 if (bResetting) return;
 bResetting=true;
 ShutdownAllDomains();
 JJKClearBlockers();
 if(IsValid(OpponentAI)) OpponentAI->DeactivateAI();
 EnsureFightersSpawned();
 GetWorldTimerManager().ClearTimer(PlayerRecoveryTimer);
 GetWorldTimerManager().ClearTimer(OpponentRecoveryTimer);
 GetWorldTimerManager().ClearTimer(CooldownRefreshTimer);
 // 先同时关闭双方请求，避免一方恢复时另一方仍投技/提交动作。
 for(auto* F : {PlayerFighter.Get(),OpponentFighter.Get()}) if(IsValid(F)) F->GetCombatInput()->SetRequestsEnabled(false);
 for(auto* F : {PlayerFighter.Get(),OpponentFighter.Get()}) if(IsValid(F))
 {
  F->GetFighterAbilitySystemComponent()->CancelAllAbilities();
  F->ResetToInitialState();
  ClearTrainingCooldowns(F);
 }
 PlayerStats={}; OpponentStats={}; InputHistory.Reset();
 if(IsValid(PlayerFighter) && IsValid(OpponentFighter))
 {
  PlayerFighter->GetTargeting()->SetPreferredTarget(OpponentFighter);
  OpponentFighter->GetTargeting()->SetPreferredTarget(PlayerFighter);
 }
 bMatchResolved=false; MatchOutcome=EMatchOutcome::None; DeathObservedFrame=0;
 bResetting=false;
 SetTrainingMenuOpen(bMenuOpen);
 NotifyTrainingChanged();
}

void ATrainingGameMode::JJKOpponentAttack()
{
	if (OpponentFighter == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainingGM] 对手未生成"));
		return;
	}
	// 调试对手走同一共享请求入口（M2.1）：合法性与玩家完全一致
	OpponentFighter->GetCombatInput()->SubmitLightAttack();
}

void ATrainingGameMode::JJKDebugHud()
{
	bDebugHud = !bDebugHud;
	UE_LOG(LogTemp, Log, TEXT("[TrainingGM] 调试 HUD = %d"), bDebugHud ? 1 : 0);
}

void ATrainingGameMode::JJKOpponentGuard(bool bHeld)
{
 if (!IsValid(OpponentFighter)) return;
 if (bHeld) OpponentFighter->GetCombatInput()->NotifyGuardPressed();
 else OpponentFighter->GetCombatInput()->NotifyGuardReleased();
}

void ATrainingGameMode::JJKOpponentAction(int32 Action)
{
 if (!IsValid(OpponentFighter)) return;
 auto* Input = OpponentFighter->GetCombatInput();
 switch (Action)
 {
 case 1: Input->SubmitHeavyPunch(); break;
 case 2: Input->SubmitKick(); break;
 case 3: Input->SubmitHeavyKick(); break;
 case 4: Input->NotifyDodgePressed(FVector::ZeroVector); break;
 default: Input->SubmitLightAttack(); break;
 }
}

void ATrainingGameMode::NotifyTrainingChanged()
{
 if (!bResetting) OnTrainingChanged.Broadcast();
}
void ATrainingGameMode::StopActiveIntent(AFighterCharacter* F)
{
 if(!IsValid(F)) return;
 F->GetCombatInput()->SetRequestsEnabled(false);
 F->SetSprintHeld(false);
 F->GetFighterAbilitySystemComponent()->CancelAllAbilities();
 F->GetCharacterMovement()->StopMovementImmediately();
 F->ConsumeMovementInputVector();
 F->LastMoveInputDirection=FVector::ZeroVector;
 // 被动受击/倒地/配对不通过取消 GA 强行解除。
}
void ATrainingGameMode::ApplyOpponentMode()
{
 if(!IsValid(OpponentFighter) || bResetting) return;
 auto* Input=OpponentFighter->GetCombatInput();
 Input->SetRequestsEnabled(!bMenuOpen && !bMatchResolved);
 if(OpponentMode==EOpponentMode::AI)
 {
  EnsureOpponentAI();
  if(IsValid(OpponentAI))
  {
   if(bMenuOpen || bMatchResolved) OpponentAI->DeactivateAI();
   else OpponentAI->ActivateAI(PlayerFighter);
  }
  return;
 }
 ShutdownOpponentAI();
 if(!bMenuOpen && !bMatchResolved && OpponentMode==EOpponentMode::FixedGuard && OpponentFighter->CanAct() && !OpponentFighter->HasPendingCombatEvents() && !OpponentFighter->IsGuardIntent()) Input->NotifyGuardPressed();
}

void ATrainingGameMode::EnsureOpponentAI()
{
 if(IsValid(OpponentAI) && OpponentAI->GetPawn()==OpponentFighter) return;
 if(IsValid(OpponentAI)) OpponentAI->Destroy();
 OpponentAI = GetWorld() ? GetWorld()->SpawnActor<AFighterAIController>(AFighterAIController::StaticClass()) : nullptr;
 if(IsValid(OpponentAI) && IsValid(OpponentFighter)) OpponentAI->Possess(OpponentFighter);
}

void ATrainingGameMode::ShutdownOpponentAI()
{
 if(IsValid(OpponentAI))
 {
  OpponentAI->DeactivateAI();
  OpponentAI->Destroy();
 }
 OpponentAI=nullptr;
}

void ATrainingGameMode::CheckMatchOutcome()
{
 // M5.6：AI 对战中一方死亡后结算一次；同批次双亡为平局。
 // 至少跨过死亡观察帧且双方待处理接触队列清空，才发布结果。
 if(bMatchResolved || OpponentMode!=EOpponentMode::AI) return;
 if(!IsValid(PlayerFighter) || !IsValid(OpponentFighter)) return;
 const bool bPlayerDead=PlayerFighter->IsDead();
 const bool bOpponentDead=OpponentFighter->IsDead();
 if(!bPlayerDead && !bOpponentDead) { DeathObservedFrame=0; return; }
 if(!DeathObservedFrame) { DeathObservedFrame=GFrameCounter; return; }
 if(GFrameCounter<=DeathObservedFrame || PlayerFighter->HasPendingCombatEvents() || OpponentFighter->HasPendingCombatEvents()) return;
 ResolveMatchOutcome(bPlayerDead&&bOpponentDead ? EMatchOutcome::Draw
   : (bPlayerDead ? EMatchOutcome::OpponentWin : EMatchOutcome::PlayerWin));
}

void ATrainingGameMode::ResolveMatchOutcome(EMatchOutcome InOutcome)
{
 if(bMatchResolved) return;
 bMatchResolved=true;
 MatchOutcome=InOutcome; ++MatchResolutionCount;
 // 停止双方新主动行为（受击等被动流程照常），AI 决策随之冻结
 StopActiveIntent(PlayerFighter);
 StopActiveIntent(OpponentFighter);
 if(IsValid(OpponentAI)) OpponentAI->DeactivateAI();
 const TCHAR* OutcomeNames[]={TEXT("无"),TEXT("玩家胜利"),TEXT("对手胜利"),TEXT("平局")};
 const int32 Idx=FMath::Clamp(static_cast<int32>(InOutcome),0,3);
 UE_LOG(LogTemp,Log,TEXT("[TrainingGM] 对局结束：%s（只结算一次）"),OutcomeNames[Idx]);
 RecordInput(PlayerFighter,FString::Printf(TEXT("对局结束：%s"),OutcomeNames[Idx]));
 if(auto* PC=Cast<AArenaPlayerController>(GetWorld()->GetFirstPlayerController())) PC->SetTrainingPanelOpen(true);
 NotifyTrainingChanged();
}

void ATrainingGameMode::RestartMatch()
{
 // M5.6 重开：M4 统一重置事务 + 清对局结束状态；AI 按当前模式恢复
 bMatchResolved=false;
 MatchOutcome=EMatchOutcome::None;
 ResetTraining();
}
bool ATrainingGameMode::SetOpponentMode(EOpponentMode Value)
{
 if(!IsModeAvailable(Value)) return false;
 if(IsValid(OpponentAI)) OpponentAI->DeactivateAI();
 StopActiveIntent(OpponentFighter);
 OpponentMode=Value;
 if(Value!=EOpponentMode::AI && bMatchResolved)
 {
  bMatchResolved=false; MatchOutcome=EMatchOutcome::None; DeathObservedFrame=0;
  if(IsValid(PlayerFighter)) PlayerFighter->GetCombatInput()->SetRequestsEnabled(!bMenuOpen);
 }
 ApplyOpponentMode();
 NotifyTrainingChanged();
 return true;
}
void ATrainingGameMode::ClearTrainingCooldowns(AFighterCharacter* F)
{
 if(!IsValid(F)) return;
 FGameplayTagContainer CooldownTags; CooldownTags.AddTag(TAG_Cooldown_TrainingProbe);
 F->GetFighterAbilitySystemComponent()->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags));
 // 超级炮冷却走松标签+能力计时器：重置时能力已取消，计时器不会回落，直接摘标签
 F->GetFighterAbilitySystemComponent()->RemoveLooseGameplayTag(TAG_State_SuperBlastCooldown);
}
void ATrainingGameMode::SetTrainingSettings(FTrainingSettings Value)
{
 Value.RecoveryDelay=FMath::Clamp(Value.RecoveryDelay,0.1f,30.f);
 const bool bResourcesChanged=Settings.bInfiniteResources!=Value.bInfiniteResources;
 Settings=Value;
 for(auto* F : {PlayerFighter.Get(),OpponentFighter.Get()}) if(IsValid(F))
 {
  if(bResourcesChanged) { StopActiveIntent(F); F->GetCombatInput()->SetRequestsEnabled(!bMenuOpen && !bMatchResolved); }
  if(Settings.bNoCooldown) ClearTrainingCooldowns(F);
 }
 GetWorldTimerManager().ClearTimer(PlayerRecoveryTimer);
 GetWorldTimerManager().ClearTimer(OpponentRecoveryTimer);
 if(Settings.bAutoRecoverHealth) { ScheduleRecovery(PlayerFighter); ScheduleRecovery(OpponentFighter); }
 ApplyOpponentMode();
 NotifyTrainingChanged();
}
void ATrainingGameMode::SetTrainingMenuOpen(bool bOpen)
{
 bMenuOpen=bOpen;
 for(auto* F : {PlayerFighter.Get(),OpponentFighter.Get()}) if(IsValid(F))
 {
  if(bOpen) StopActiveIntent(F);
  else F->GetCombatInput()->SetRequestsEnabled(!bMatchResolved);
 }
 if(auto* PC=Cast<AArenaPlayerController>(GetWorld()->GetFirstPlayerController())) PC->SetCombatInputEnabled(!bOpen && !bMatchResolved);
 ApplyOpponentMode();
 NotifyTrainingChanged();
}
void ATrainingGameMode::RecordInput(AFighterCharacter* Source,const FString& Text)
{
 if(bResetting || !IsValid(Source)) return;
 InputHistory.Insert(FString::Printf(TEXT("%.2f %s %s"),GetWorld()->GetTimeSeconds(),Source==PlayerFighter ? TEXT("P1") : TEXT("P2"),*Text),0);
 InputHistory.SetNum(FMath::Min(InputHistory.Num(),8));
 NotifyTrainingChanged();
}
void ATrainingGameMode::RecordContact(AFighterCharacter* Source,AFighterCharacter* Target,ETrainingContact Kind,float Raw,float Resolved,float Lost)
{
 if(bResetting || (Source!=PlayerFighter && Source!=OpponentFighter)) return;
 auto& Stats=Source==PlayerFighter ? PlayerStats : OpponentStats;
 if(Kind==ETrainingContact::Whiff) ++Stats.Whiffs;
 else
 {
  Stats.RawDamage+=FMath::Max(0.f,Raw); Stats.ResolvedDamage+=Resolved; Stats.HealthLost+=Lost;
  if(Kind==ETrainingContact::Guard) ++Stats.Guards;
  else if(Kind==ETrainingContact::Immune) ++Stats.Immunes;
  else { ++Stats.Hits; ++Stats.ComboHits; Stats.ComboDamage+=Resolved; }
  if(Kind!=ETrainingContact::Immune && IsValid(Target))
  {
   GetWorldTimerManager().ClearTimer(Target==PlayerFighter ? PlayerRecoveryTimer : OpponentRecoveryTimer);
   // 接触先于本帧被动事件处理；下一 Tick 才判断恢复，不能按扣血时的 CanAct 提前断连。
   TWeakObjectPtr<AFighterCharacter> Weak=Target;
   FTimerDelegate D; D.BindWeakLambda(this,[this,Weak]() { if(Weak.IsValid()) ResolveRecovery(Weak.Get()); });
   GetWorldTimerManager().SetTimerForNextTick(D);
  }
 }
 NotifyTrainingChanged();
}
void ATrainingGameMode::ResolveRecovery(AFighterCharacter* Victim)
{
 if(!IsValid(Victim) || bResetting || Victim->HasPendingCombatEvents()) return;
 if(!Victim->CanAct() && !Victim->IsDead() && !Victim->HasCombatTag(TAG_State_KnockedDown)) return;
 auto& Stats=Victim==OpponentFighter ? PlayerStats : OpponentStats;
 if(Stats.ComboHits>0) { Stats.LastComboHits=Stats.ComboHits; Stats.LastComboDamage=Stats.ComboDamage; Stats.ComboHits=0; Stats.ComboDamage=0; }
 ScheduleRecovery(Victim);
 if(Victim==OpponentFighter) ApplyOpponentMode();
 NotifyTrainingChanged();
}
void ATrainingGameMode::ScheduleRecovery(AFighterCharacter* Victim)
{
 if(!Settings.bAutoRecoverHealth || !IsValid(Victim) || !Victim->CanAct() || Victim->HasPendingCombatEvents()) return;
 auto& Timer=Victim==PlayerFighter ? PlayerRecoveryTimer : OpponentRecoveryTimer;
 if(GetWorldTimerManager().IsTimerActive(Timer)) return;
 if(Victim->GetFighterAttributeSet()->GetHealth()>=Victim->GetFighterAttributeSet()->GetMaxHealth()) return;
 TWeakObjectPtr<AFighterCharacter> Weak=Victim;
 FTimerDelegate D; D.BindWeakLambda(this,[this,Weak]()
 {
  auto* F=Weak.Get();
  if(!F || !Settings.bAutoRecoverHealth || F->IsDead() || !F->CanAct() || F->HasPendingCombatEvents()) return;
  auto* ASC=F->GetFighterAbilitySystemComponent();
  auto Spec=ASC->MakeOutgoingSpec(UTrainingHealEffect::StaticClass(),1,ASC->MakeEffectContext());
  Spec.Data->SetSetByCallerMagnitude(TAG_Data_Amount,F->GetFighterAttributeSet()->GetMaxHealth());
  ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
 });
 GetWorldTimerManager().SetTimer(Timer,D,Settings.RecoveryDelay,false);
}
void ATrainingGameMode::OnPlayerRecovered() { ResolveRecovery(PlayerFighter); }
void ATrainingGameMode::OnOpponentRecovered() { ResolveRecovery(OpponentFighter); }
void ATrainingGameMode::UnbindFighters()
{
 for(auto& B:AttributeBindings) if(B.ASC.IsValid()) B.ASC->GetGameplayAttributeValueChangeDelegate(B.Attribute).Remove(B.Handle);
 AttributeBindings.Reset();
 if(auto* F=BoundPlayer.Get()) { F->OnRecovered.RemoveDynamic(this,&ATrainingGameMode::OnPlayerRecovered); F->OnCombatEventsProcessed.RemoveDynamic(this,&ATrainingGameMode::OnPlayerRecovered); F->OnDestroyed.RemoveDynamic(this,&ATrainingGameMode::OnFighterDestroyed); }
 if(auto* F=BoundOpponent.Get()) { F->OnRecovered.RemoveDynamic(this,&ATrainingGameMode::OnOpponentRecovered); F->OnCombatEventsProcessed.RemoveDynamic(this,&ATrainingGameMode::OnOpponentRecovered); F->OnDestroyed.RemoveDynamic(this,&ATrainingGameMode::OnFighterDestroyed); }
 BoundPlayer.Reset(); BoundOpponent.Reset();
}
void ATrainingGameMode::BindFighters()
{
 UnbindFighters();
 BoundPlayer=PlayerFighter; BoundOpponent=OpponentFighter;
 for(auto* F : {PlayerFighter.Get(),OpponentFighter.Get()}) if(IsValid(F))
 {
  F->GetCombatInput()->SetRequestsEnabled(!bMenuOpen && !bResetting && !bMatchResolved);
  auto* ASC=F->GetFighterAbilitySystemComponent();
  for(auto Attr : {UFighterAttributeSet::GetHealthAttribute(),UFighterAttributeSet::GetMaxHealthAttribute(),UFighterAttributeSet::GetActionResourceAttribute(),UFighterAttributeSet::GetMaxActionResourceAttribute(),UFighterAttributeSet::GetCursedEnergyAttribute(),UFighterAttributeSet::GetMaxCursedEnergyAttribute(),UFighterAttributeSet::GetEnergyAttribute(),UFighterAttributeSet::GetMaxEnergyAttribute()})
  {
   auto Handle=ASC->GetGameplayAttributeValueChangeDelegate(Attr).AddWeakLambda(this,[this](const FOnAttributeChangeData&) { NotifyTrainingChanged(); });
   AttributeBindings.Add({ASC,Attr,Handle});
  }
  F->OnDestroyed.AddUniqueDynamic(this,&ATrainingGameMode::OnFighterDestroyed);
 }
 if(IsValid(PlayerFighter)) { PlayerFighter->OnRecovered.AddUniqueDynamic(this,&ATrainingGameMode::OnPlayerRecovered); PlayerFighter->OnCombatEventsProcessed.AddUniqueDynamic(this,&ATrainingGameMode::OnPlayerRecovered); }
 if(IsValid(OpponentFighter)) { OpponentFighter->OnRecovered.AddUniqueDynamic(this,&ATrainingGameMode::OnOpponentRecovered); OpponentFighter->OnCombatEventsProcessed.AddUniqueDynamic(this,&ATrainingGameMode::OnOpponentRecovered); }
 NotifyTrainingChanged();
}
void ATrainingGameMode::OnFighterDestroyed(AActor* Actor)
{
 GetWorldTimerManager().ClearTimer(PlayerRecoveryTimer); GetWorldTimerManager().ClearTimer(OpponentRecoveryTimer);
 if(Actor==PlayerFighter) PlayerFighter=nullptr;
 if(Actor==OpponentFighter) OpponentFighter=nullptr;
 PlayerStats.ComboHits=OpponentStats.ComboHits=0;
 PlayerStats.ComboDamage=OpponentStats.ComboDamage=0;
 BindFighters();
}
bool ATrainingGameMode::RequestTrainingProbe(AFighterCharacter* Fighter)
{
 if(bMenuOpen || bResetting || bMatchResolved || !IsValid(Fighter) || (Fighter!=PlayerFighter && Fighter!=OpponentFighter)) return false;
 const bool Result=Fighter->GetFighterAbilitySystemComponent()->TryActivateAbilityByClass(UTrainingProbeAbility::StaticClass());
 RecordInput(Fighter,Result ? TEXT("开发测试技能：执行") : TEXT("开发测试技能：拒绝"));
 if(Result && !Settings.bNoCooldown)
 {
  GetWorldTimerManager().SetTimer(CooldownRefreshTimer,FTimerDelegate::CreateWeakLambda(this,[this]()
  {
   NotifyTrainingChanged();
   const bool Active=(IsValid(PlayerFighter) && PlayerFighter->GetFighterAbilitySystemComponent()->GetTrainingCooldownRemaining()>0.f) || (IsValid(OpponentFighter) && OpponentFighter->GetFighterAbilitySystemComponent()->GetTrainingCooldownRemaining()>0.f);
   if(!Active) GetWorldTimerManager().ClearTimer(CooldownRefreshTimer);
  }),0.1f,true);
 }
 return Result;
}
void ATrainingGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
 ShutdownOpponentAI();
 UnbindFighters(); GetWorldTimerManager().ClearAllTimersForObject(this);
 Super::EndPlay(Reason);
}

void ATrainingGameMode::JJKSpawnBlocker(float X, float Y, float SX, float SY, float SZ)
{
	if (!GetWorld()) return;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) return;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FTransform T(FRotator::ZeroRotator, FVector(X, Y, SZ * 50.f));
	AActor* Wall = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), T, Params);
	if (!Wall) return;
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Wall, TEXT("BlockerMesh"));
	Mesh->SetStaticMesh(Cube);
	Mesh->SetWorldScale3D(FVector(SX, SY, SZ));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Wall->SetRootComponent(Mesh);
	Mesh->RegisterComponent();
	TestBlockers.Add(Wall);
	UE_LOG(LogTemp, Log, TEXT("[Blocker] 生成验证墙 (%.0f, %.0f) 尺度 %.1f x %.1f x %.1f"), X, Y, SX, SY, SZ);
}

void ATrainingGameMode::JJKClearBlockers()
{
	for (TObjectPtr<AActor>& Wall : TestBlockers)
	{
		if (IsValid(Wall)) Wall->Destroy();
	}
	TestBlockers.Reset();
	UE_LOG(LogTemp, Log, TEXT("[Blocker] 清除全部验证墙"));
}

// ---------- M6 领域会话 ----------

bool ATrainingGameMode::TryOpenDomain(AFighterCharacter* Caster)
{
	if (!IsValid(Caster) || Caster->IsDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Domain] TryOpenDomain 拒绝：施法者无效或已死亡"));
		return false;
	}

	// 捕获目标：与身份一致（玩家↔对手）
	AFighterCharacter* Victim = (Caster == PlayerFighter.Get()) ? OpponentFighter.Get() : PlayerFighter.Get();
	if (!IsValid(Victim) || Victim->IsDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Domain] TryOpenDomain 拒绝：目标无效或已死亡"));
		return false;
	}

	const UFighterDefinition* Def = Caster->GetDefinition();
	const float CaptureRange = Def ? Def->DomainConfig.CaptureRange : 1200.f;
	const FVector From = Caster->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector To = Victim->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	if (FVector::DistXY(Caster->GetActorLocation(), Victim->GetActorLocation()) > CaptureRange)
	{
		// 目标超出捕获范围：结印完成但领域不开（ability 走失败分支结束）
		UE_LOG(LogTemp, Log, TEXT("[Domain] TryOpenDomain 拒绝：目标超出捕获范围 %.0f > %.0f"),
			FVector::DistXY(Caster->GetActorLocation(), Victim->GetActorLocation()), CaptureRange);
		return false;
	}

	// 捕获需视线可达：世界几何遮挡则无法捕获（A05）
	FCollisionQueryParams VisionQP(SCENE_QUERY_STAT(DomainCapture));
	VisionQP.AddIgnoredActor(Caster);
	VisionQP.AddIgnoredActor(Victim);
	FHitResult VisionHit;
	if (GetWorld()->LineTraceSingleByChannel(VisionHit, From, To, ECC_Visibility, VisionQP))
	{
		UE_LOG(LogTemp, Log, TEXT("[Domain] TryOpenDomain 拒绝：捕获视线被 %s 遮挡"),
			*GetNameSafe(VisionHit.GetActor()));
		return false;
	}

	// 同一施法者已有领域则不重复开启
	if (DomainSessions.ContainsByPredicate([&Caster](const FDomainSessionData& S) { return S.Caster.Get() == Caster; }))
	{
		return true;
	}

	FDomainSessionData Session;
	Session.SessionId = ++NextDomainSessionId;
	Session.Caster = Caster;
	Session.Victim = Victim;
	const double Now = GetWorld()->GetTimeSeconds();
	Session.EndTime = Now + (Def ? Def->DomainConfig.Duration : 6.f);
	Session.NextSpawnTime = Now + (Def ? Def->DomainConfig.FirstOrbDelay : 0.3f);
	DomainSessions.Add(Session);

	Caster->SetDomainActive(true);
	UE_LOG(LogTemp, Log, TEXT("[Domain] 会话 %d 开启：Caster=%s Victim=%s 持续 %.1fs"),
		Session.SessionId, *Caster->GetName(), *Victim->GetName(), Session.EndTime - Now);
	return true;
}

void ATrainingGameMode::TickDomainSessions()
{
	if (DomainSessions.Num() == 0) return;
	const double Now = GetWorld()->GetTimeSeconds();

	// ≥2 领域并存 → 相互压制：全部暂停出球与球体移动
	const bool bSuppressed = DomainSessions.Num() >= 2;

	for (int32 i = DomainSessions.Num() - 1; i >= 0; --i)
	{
		FDomainSessionData& S = DomainSessions[i];
		AFighterCharacter* Caster = S.Caster.Get();
		AFighterCharacter* Victim = S.Victim.Get();

		// 任一方死亡 / 引用失效 / 到期 → 结束会话
		if (!IsValid(Caster) || Caster->IsDead() || !IsValid(Victim) || Victim->IsDead() || Now >= S.EndTime)
		{
			EndDomainSession(S.SessionId);
			continue;
		}

		S.bSuppressed = bSuppressed;
		// 压制（A05/SLL-T24）：销毁已在飞的球，保留原调度点；恢复后不补发
		if (bSuppressed)
		{
			for (const TWeakObjectPtr<ADomainOrb>& OrbPtr : S.Orbs)
			{
				if (ADomainOrb* Orb = OrbPtr.Get()) Orb->Destroy();
			}
			S.Orbs.RemoveAll([](const TWeakObjectPtr<ADomainOrb>& P) { return !P.IsValid(); });
			continue;
		}

		// 术者受击/倒地/被投 → 仅暂停新炮发射，已在飞球继续（08 §212）
		const bool bCasterBusy = Caster->HasCombatTag(TAG_State_HitStun) || Caster->HasCombatTag(TAG_State_KnockedDown)
			|| Caster->HasCombatTag(TAG_State_GuardStun) || Caster->IsThrowPaired();
		if (!bCasterBusy && Now >= S.NextSpawnTime)
		{
			SpawnDomainOrb(S);
			if (const UFighterDefinition* Def = Caster->GetDefinition())
			{
				S.NextSpawnTime = Now + Def->DomainConfig.OrbInterval;
			}
		}
	}
}

void ATrainingGameMode::EndDomainSession(int32 SessionId)
{
	const int32 Idx = DomainSessions.IndexOfByPredicate(
		[SessionId](const FDomainSessionData& S) { return S.SessionId == SessionId; });
	if (Idx == INDEX_NONE) return;

	const FDomainSessionData S = DomainSessions[Idx];
	DomainSessions.RemoveAt(Idx);

	// 剩余在飞球体直接销毁
	for (const TWeakObjectPtr<ADomainOrb>& OrbPtr : S.Orbs)
	{
		if (ADomainOrb* Orb = OrbPtr.Get()) Orb->Destroy();
	}

	if (AFighterCharacter* Caster = S.Caster.Get())
	{
		Caster->SetDomainActive(false);
	}
	UE_LOG(LogTemp, Log, TEXT("[Domain] 会话 %d 结束"), SessionId);
}

void ATrainingGameMode::SpawnDomainOrb(FDomainSessionData& Session)
{
	AFighterCharacter* Caster = Session.Caster.Get();
	AFighterCharacter* Victim = Session.Victim.Get();
	if (!IsValid(Caster) || !IsValid(Victim)) return;
	const UFighterDefinition* Def = Caster->GetDefinition();
	if (!Def) return;

	const FDomainConfig& Cfg = Def->DomainConfig;

	// 在飞上限（剔除已销毁的弱引用后计数）
	Session.Orbs.RemoveAll([](const TWeakObjectPtr<ADomainOrb>& P) { return !P.IsValid(); });
	if (Session.Orbs.Num() >= Cfg.MaxOrbsInFlight) return;

	// 咒力不足 → 本次跳过（不消耗；下次到期再试）
	if (Caster->GetCursedEnergy() < Cfg.OrbCost)
	{
		UE_LOG(LogTemp, Log, TEXT("[Domain] 会话 %d 咒力不足，跳过出球"), Session.SessionId);
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = Caster;
	Params.Instigator = Caster;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLoc = Caster->GetActorLocation() + FVector(0.f, 0.f, 80.f);
	ADomainOrb* Orb = GetWorld()->SpawnActor<ADomainOrb>(ADomainOrb::StaticClass(), SpawnLoc, FRotator::ZeroRotator, Params);
	if (!Orb)
	{
		// 生成失败不吞咒力：未扣费即失败（成本只在成功后提交一次）
		UE_LOG(LogTemp, Log, TEXT("[Domain] 会话 %d 球体生成失败（未扣费）"), Session.SessionId);
		return;
	}

	// 生成成功才最终提交成本
	Caster->ModifyCursedEnergy(-Cfg.OrbCost);
	Orb->InitOrb(Victim, Cfg.OrbDamage, Cfg.OrbLife, Cfg.OrbSpeed, Cfg.OrbRadius);
	Session.Orbs.Add(Orb);
}

void ATrainingGameMode::ShutdownAllDomains()
{
	if (DomainSessions.Num() == 0) return;
	TArray<int32> Ids;
	for (const FDomainSessionData& S : DomainSessions) Ids.Add(S.SessionId);
	for (int32 Id : Ids) EndDomainSession(Id);
	DomainSessions.Reset();
}
