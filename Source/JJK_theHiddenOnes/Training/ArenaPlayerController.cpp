// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/ArenaPlayerController.h"
#include "Training/ArenaMenuWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Framework/Application/IInputProcessor.h"
#include "Widgets/SViewport.h"
#include "Training/FighterDefinition.h"

#include "EnhancedInputComponent.h"
#include "InputCoreTypes.h"
#include "Training/TrainingPanelWidget.h"
#include "Training/CombatHudWidget.h"
#include "Training/ArenaCombatHudWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Training/CombatInputComponent.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterAttributeSet.h"
#include "Training/TargetingComponent.h"
#include "Training/TrainingGameMode.h"
#include "Training/CombatFeedbackComponent.h"
#include "Training/Feedback/Camera/CombatCameraFeedbackModifier.h"
#include "InputKeyEventArgs.h"

/** Route game-menu keys before editor PIE shortcuts, only within this player's UI/viewport. */
class FArenaMenuInputProcessor final : public IInputProcessor
{
 TWeakObjectPtr<AArenaPlayerController> Player;
public:
 explicit FArenaMenuInputProcessor(AArenaPlayerController* PC) : Player(PC) {}
 virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
 virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
 {
  auto* PC=Player.Get();
  if (!PC || !PC->GameMenu || Event.IsRepeat()) return false;
  const FKey Key=Event.GetKey();
  if (Key!=EKeys::Escape && Key!=EKeys::F1) return false;
  if (PC->IsGameMenuOpen())
  {
   if (!PC->GameMenu->TakeWidget()->HasAnyUserFocusOrFocusedDescendants()) return false;
   if (Key==EKeys::Escape) PC->GameMenu->NavigateBack();
   return true;
  }
  auto* Client=PC->GetWorld() ? PC->GetWorld()->GetGameViewport() : nullptr;
  const auto Viewport=Client ? Client->GetGameViewportWidget() : nullptr;
  if (!Viewport || !Viewport->HasAnyUserFocusOrFocusedDescendants()) return false;
  PC->OpenGameMenu(Key==EKeys::Escape ? TEXT("pause") : TEXT("settings"),Key==EKeys::F1 ? TEXT("training") : TEXT("graphics"));
  return true;
 }
};

void AArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();
 if(IsLocalController())
 {
  CombatHud=CreateWidget<UCombatHudWidget>(this);
  CombatHud->AddToViewport(5);
  // 正式战斗 HUD（14 §1/§8.1）：默认替换开发用数值卡片的观感；调试卡片保持已回归的类不动，
  // 仅由 JJKDebugHud 驱动显隐（并存，开关切换）
  ArenaHud=CreateWidget<UArenaCombatHudWidget>(this);
  ArenaHud->AddToViewport(6);
  CombatHud->SetVisibility(ESlateVisibility::Collapsed);
  GameMenu=CreateWidget<UArenaMenuWidget>(this);
  GameMenu->AddToViewport(100);
  MenuInputProcessor=MakeShared<FArenaMenuInputProcessor>(this);
  FSlateApplication::Get().RegisterInputPreProcessor(MenuInputProcessor,0);
  const bool bSkipMenu=FParse::Param(FCommandLine::Get(),TEXT("JJKSkipMenu"));
  GameMenu->ShowPage(bSkipMenu ? TEXT("battle") : TEXT("main"));
  GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this,bSkipMenu]()
  {
   SetGameMenuOpen(!bSkipMenu);
  }));
 }
	if (PlayerCameraManager != nullptr)
	{
		if (IsLocalController()) CombatCameraFeedback = Cast<UCombatCameraFeedbackModifier>(PlayerCameraManager->AddNewCameraModifier(UCombatCameraFeedbackModifier::StaticClass()));
		PlayerCameraManager->ViewPitchMin = -65.f;
		PlayerCameraManager->ViewPitchMax = 65.f;
	}

	// 失焦清会话：恢复后要求重新按下（08 第 4.2 节）
	FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(
		this, &AArenaPlayerController::HandleAppActivationChanged);
}

void AArenaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 UnbindCombatFeedback();
 ClearCombatFeedback(true);
 if (PlayerCameraManager && CombatCameraFeedback) PlayerCameraManager->RemoveCameraModifier(CombatCameraFeedback);
 CombatCameraFeedback = nullptr;
	if (FSlateApplication::IsInitialized())
	{
  if (MenuInputProcessor) FSlateApplication::Get().UnregisterInputPreProcessor(MenuInputProcessor);
  MenuInputProcessor.Reset();
		FSlateApplication::Get().OnApplicationActivationStateChanged().RemoveAll(this);
	}
	if(TrainingPanel) { TrainingPanel->RemoveFromParent(); TrainingPanel=nullptr; }
 if(GameMenu) { GameMenu->RemoveFromParent(); GameMenu=nullptr; }
 if(CombatHud) { CombatHud->RemoveFromParent(); CombatHud=nullptr; }
 if(ArenaHud) { ArenaHud->RemoveFromParent(); ArenaHud=nullptr; }
	Super::EndPlay(EndPlayReason);
}

void AArenaPlayerController::HandleAppActivationChanged(bool bActive)
{
 bFeedbackAppActive = bActive;
	if (!bActive)
	{
  ClearCombatFeedback();
		bDodgeHeld = false;
		ClearFrameInput();
		if (AFighterCharacter* Fighter = GetPlayerFighter())
		{
			if (UCombatInputComponent* Input = Fighter->GetCombatInput())
			{
				Input->InvalidateSession(FText::FromString(TEXT("应用失焦")));
				Input->ReleaseContinuousInputs();
				Fighter->LastMoveInputDirection = FVector::ZeroVector;
				Fighter->SetSprintHeld(false);
			}
		}
	}
}

void AArenaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::F1,IE_Pressed,this,&AArenaPlayerController::ToggleTrainingPanel);
 auto& PauseBinding=InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AArenaPlayerController::HandlePausePressed);
 PauseBinding.bExecuteWhenPaused=true;
	InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AArenaPlayerController::HandleDomainPressed);
	InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&AArenaPlayerController::HandleRestartPressed);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (LockTargetAction != nullptr)
		{
			EnhancedInputComponent->BindAction(LockTargetAction, ETriggerEvent::Started, this, &AArenaPlayerController::HandleLockInput);
		}
		if (RecenterCameraAction != nullptr)
		{
			EnhancedInputComponent->BindAction(RecenterCameraAction, ETriggerEvent::Started, this, &AArenaPlayerController::HandleRecenterInput);
		}
		if (AttackAction != nullptr)
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AArenaPlayerController::HandleAttackPressed);
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &AArenaPlayerController::HandleAttackReleased);
		}
		if (DodgeAction != nullptr)
		{
			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AArenaPlayerController::HandleDodgePressed);
   EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Completed, this, &AArenaPlayerController::HandleDodgeReleased);
   EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Canceled, this, &AArenaPlayerController::HandleDodgeReleased);
		}
		if (GuardAction != nullptr)
		{
			EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Started, this, &AArenaPlayerController::HandleGuardPressed);
			EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Completed, this, &AArenaPlayerController::HandleGuardReleased);
		}
		if (KickAction != nullptr)
		{
			EnhancedInputComponent->BindAction(KickAction, ETriggerEvent::Started, this, &AArenaPlayerController::HandleKickPressed);
			EnhancedInputComponent->BindAction(KickAction, ETriggerEvent::Completed, this, &AArenaPlayerController::HandleKickReleased);
		}
		if (StanceSwitchAction != nullptr)
		{
			EnhancedInputComponent->BindAction(StanceSwitchAction, ETriggerEvent::Started, this, &AArenaPlayerController::HandleStanceSwitchPressed);
		}
		if (AimAction != nullptr)
		{
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &AArenaPlayerController::HandleAimPressed);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &AArenaPlayerController::HandleAimReleased);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Canceled, this, &AArenaPlayerController::HandleAimReleased);
		}
		if (!AttackAction)
		{
			UE_LOG(LogTemp, Error, TEXT("[ArenaPC] AttackAction 未配置，攻击输入无法绑定；请检查 BP_ArenaPlayerController 与 IA_Attack 资产"));
		}
	}
}

void AArenaPlayerController::OnPossess(APawn* InPawn)
{
 UnbindCombatFeedback();
	Super::OnPossess(InPawn);
 ClearCombatFeedback(true);
 RefreshFeedbackBindings();
	SetCombatInputEnabled(bCombatInputEnabled);
	UE_LOG(LogTemp, Log, TEXT("[ArenaPC] %s 接管 %s"), *GetName(), *GetNameSafe(InPawn));
}

void AArenaPlayerController::OnUnPossess()
{
 UnbindCombatFeedback();
 ClearCombatFeedback(true);
 Super::OnUnPossess();
}

void AArenaPlayerController::JJKCameraStrength(int32 Level) { UCombatCameraFeedbackModifier::SetStrength(Level); }
int32 AArenaPlayerController::GetCameraFeedbackStrength() const { return UCombatCameraFeedbackModifier::GetStrength(); }
void AArenaPlayerController::GetCombatAimViewPoint(FVector& Location, FRotator& Rotation) const
{
 GetPlayerViewPoint(Location, Rotation);
 if (CombatCameraFeedback) Rotation.Roll -= CombatCameraFeedback->AppliedRoll;
}
bool AArenaPlayerController::CanPlayCombatFeedback() const
{
 const auto* GM = GetTrainingGameMode();
 const auto* Fighter = GetPlayerFighter();
 return IsLocalController() && bFeedbackAppActive && bCombatInputEnabled && IsValid(Fighter) && !Fighter->IsDead()
  && GM && !GM->IsMatchResolved() && !GM->IsTrainingMenuOpen() && !(TrainingPanel && TrainingPanel->IsInViewport());
}
int32 AArenaPlayerController::GetFeedbackBindingCount() const
{
 int32 Count = 0;
 for (const auto& Binding : FeedbackBindings) if (Binding.IsValid()) ++Count;
 return Count;
}
void AArenaPlayerController::UnbindCombatFeedback()
{
 for (auto& Binding : FeedbackBindings) if (auto* F = Binding.Get())
 {
  F->OnContact.RemoveDynamic(this, &AArenaPlayerController::HandleFeedbackContact);
  F->OnAction.RemoveDynamic(this, &AArenaPlayerController::HandleFeedbackAction);
  F->OnLifecycle.RemoveDynamic(this, &AArenaPlayerController::HandleFeedbackLifecycle);
 }
 FeedbackBindings.Reset(); FeedbackPlayer.Reset(); FeedbackOpponent.Reset();
 FeedbackContactKeys.Reset();
}
void AArenaPlayerController::ClearCombatFeedback(bool bRoundReset)
{
 if (CombatCameraFeedback) CombatCameraFeedback->ClearPulse();
 if (ArenaHud) ArenaHud->ClearFeedback(bRoundReset);
 if (bRoundReset) FeedbackContactKeys.Reset();
}
void AArenaPlayerController::RefreshFeedbackBindings()
{
 if (!IsLocalController()) return;
 auto* GM = GetTrainingGameMode();
 auto* LocalFighter = GetPlayerFighter();
 auto* Opponent = GM ? GM->GetOpponentFighter() : nullptr;
 if (!IsValid(LocalFighter))
 {
  UnbindCombatFeedback(); ClearCombatFeedback(); return;
 }
 const int32 Round = UCombatFeedbackComponent::Round(GetWorld());
 if (Round != FeedbackRound)
 {
  FeedbackRound = Round; ClearCombatFeedback(true);
 }
 if (LocalFighter == FeedbackPlayer.Get() && Opponent == FeedbackOpponent.Get()
  && GetFeedbackBindingCount() == (IsValid(LocalFighter) ? 1 : 0) + (IsValid(Opponent) && Opponent != LocalFighter ? 1 : 0)) return;
 UnbindCombatFeedback(); ClearCombatFeedback(true);
 FeedbackPlayer = LocalFighter; FeedbackOpponent = Opponent;
 for (auto* Fighter : { LocalFighter, Opponent }) if (IsValid(Fighter))
 {
  auto* F = Fighter->GetCombatFeedback();
  if (!F || FeedbackBindings.Contains(F)) continue;
  F->OnContact.AddUniqueDynamic(this, &AArenaPlayerController::HandleFeedbackContact);
  F->OnAction.AddUniqueDynamic(this, &AArenaPlayerController::HandleFeedbackAction);
  F->OnLifecycle.AddUniqueDynamic(this, &AArenaPlayerController::HandleFeedbackLifecycle);
  FeedbackBindings.Add(F);
 }
}
void AArenaPlayerController::PlayerTick(float DeltaTime)
{
 RefreshFeedbackBindings();
 if (!CanPlayCombatFeedback()) ClearCombatFeedback();
 Super::PlayerTick(DeltaTime);
}
void AArenaPlayerController::HandleFeedbackContact(const FCombatContactFeedback& E)
{
 auto* Source = E.Source.Get(); auto* Target = E.Target.Get();
 if (!CanPlayCombatFeedback() || !IsValid(Source) || !IsValid(Target)
  || !FeedbackBindings.Contains(Source->GetCombatFeedback())
  || !Source->GetCombatFeedback()->IsCurrent(E.RoundId, E.SourceGeneration)
  || !Target->GetCombatFeedback()->IsCurrent(E.RoundId, E.TargetGeneration)) return;
 const FString Key = FString::Printf(TEXT("%d:%s:%lld:%d:%s:%d"), E.RoundId, *Source->GetPathName(), E.AttackInstanceId,
  E.SegmentId, *Target->GetPathName(), static_cast<int32>(E.Result));
 if (FeedbackContactKeys.Contains(Key)) return;
 FeedbackContactKeys.Add(Key); ++FeedbackContactCount;
 if (CombatCameraFeedback) CombatCameraFeedback->ConsumeContact(E, GetPlayerFighter());
 if (ArenaHud && Source->GetCombatFeedback()->IsChannelEnabled(ECombatFeedbackChannel::HUD)) ArenaHud->ConsumeContact(E);
}
void AArenaPlayerController::DebugConsumeFeedbackContact(const FCombatContactFeedback& E)
{
#if !UE_BUILD_SHIPPING
 HandleFeedbackContact(E);
#endif
}
void AArenaPlayerController::HandleFeedbackAction(const FCombatActionFeedback& E)
{
 auto* Source = E.Source.Get();
 if (!IsValid(Source) || !FeedbackBindings.Contains(Source->GetCombatFeedback())
  || !Source->GetCombatFeedback()->IsCurrent(E.RoundId, E.Generation)) return;
 if (ArenaHud) ArenaHud->ConsumeAction(E);
}
void AArenaPlayerController::HandleFeedbackLifecycle(const FCombatLifecycleFeedback& E)
{
 auto* Source = E.Source.Get();
 if (!IsValid(Source) || !FeedbackBindings.Contains(Source->GetCombatFeedback())
  || !Source->GetCombatFeedback()->IsCurrent(E.RoundId, E.Generation)) return;
 if (E.Reason == ECombatFeedbackEnd::Death || E.Reason == ECombatFeedbackEnd::Reset
  || E.Reason == ECombatFeedbackEnd::Destroyed) ClearCombatFeedback(E.Reason == ECombatFeedbackEnd::Reset);
 else if (E.AttackInstanceId == 0 && CombatCameraFeedback) CombatCameraFeedback->ClearForSource(Source);
 if (ArenaHud) ArenaHud->ConsumeLifecycle(E);
}

AFighterCharacter* AArenaPlayerController::GetPlayerFighter() const
{
	return Cast<AFighterCharacter>(GetPawn());
}

ATrainingGameMode* AArenaPlayerController::GetTrainingGameMode() const
{
	return GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
}

void AArenaPlayerController::HandleLockInput()
{
	ToggleLock();
}

void AArenaPlayerController::HandleRecenterInput()
{
	RecenterCameraToTarget();
}

void AArenaPlayerController::HandleAttackPressed()
{
 if (bCombatInputEnabled) bAttackPressed = true;
}

void AArenaPlayerController::HandleAttackReleased()
{
 if (bCombatInputEnabled) bAttackReleased = true;
}

void AArenaPlayerController::HandleDodgePressed()
{
 if (bCombatInputEnabled) { bDodgePressed = true; bDodgeHeld = true; }
}

void AArenaPlayerController::HandleDodgeReleased()
{
 bDodgeHeld = false;
 if(auto* Fighter=GetPlayerFighter()) Fighter->SetSprintHeld(false);
}

void AArenaPlayerController::HandleGuardPressed()
{
	if (!bCombatInputEnabled) return;
	if (AFighterCharacter* Fighter = GetPlayerFighter())
	{
		if (UCombatInputComponent* Input = Fighter->GetCombatInput())
		{
			Input->NotifyGuardPressed();
		}
	}
}

void AArenaPlayerController::HandleGuardReleased()
{
	if (AFighterCharacter* Fighter = GetPlayerFighter())
	{
		if (UCombatInputComponent* Input = Fighter->GetCombatInput())
		{
			Input->NotifyGuardReleased();
		}
	}
}

void AArenaPlayerController::HandleKickPressed()
{
 if (bCombatInputEnabled) bKickPressed = true;
}

void AArenaPlayerController::HandleKickReleased()
{
 if (bCombatInputEnabled) bKickReleased = true;
}

void AArenaPlayerController::HandleStanceSwitchPressed()
{
 if (bCombatInputEnabled) bStancePressed = true;
}

void AArenaPlayerController::HandleDomainPressed()
{
 if (bCombatInputEnabled) bDomainPressed = true;
}

void AArenaPlayerController::HandleAimPressed()
{
	// 瞄准是镜头状态，不占用攻击槽；有效性与形态/状态校验在角色侧
	if (bCombatInputEnabled)
		if (auto* F = GetPlayerFighter()) F->SetAimIntent(true);
}

void AArenaPlayerController::HandleAimReleased()
{
	if (auto* F = GetPlayerFighter()) F->SetAimIntent(false);
}

void AArenaPlayerController::HandleRestartPressed()
{
	// 结果层提示的 [空格] 重新开始：只在已结算的对局生效，不影响战斗输入
	if (ATrainingGameMode* GM = GetTrainingGameMode())
	{
		if (GM->IsMatchResolved()) GM->RestartMatch();
	}
}

void AArenaPlayerController::SetDebugCardsVisible(bool bVisible)
{
	if (CombatHud) CombatHud->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void AArenaPlayerController::ToggleCombatHud()
{
	if (ArenaHud) ArenaHud->SetHudVisible(ArenaHud->GetVisibility() == ESlateVisibility::Collapsed);
}

void AArenaPlayerController::ToggleLock()
{
	AFighterCharacter* Fighter = GetPlayerFighter();
	if (Fighter == nullptr || Fighter->GetTargeting() == nullptr)
	{
		return;
	}

	if (Fighter->GetTargeting()->IsTargetValid())
	{
		Fighter->GetTargeting()->ClearTarget();
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 2.f, FColor::Silver, TEXT("目标已解除"));
	}
	else if (Fighter->GetTargeting()->LockBestTarget())
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 2.f, FColor::Yellow,
			FString::Printf(TEXT("锁定: %s"), *GetNameSafe(Fighter->GetTargeting()->GetCurrentTarget())));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 2.f, FColor::Silver, TEXT("无有效目标"));
	}
}

void AArenaPlayerController::RecenterCameraToTarget()
{
	AFighterCharacter* Fighter = GetPlayerFighter();
	if (Fighter == nullptr || Fighter->GetTargeting() == nullptr || !Fighter->GetTargeting()->IsTargetValid())
	{
		return;
	}

	const AFighterCharacter* Target = Fighter->GetTargeting()->GetCurrentTarget();
	const FVector ToTarget = Target->GetActorLocation() - Fighter->GetActorLocation();
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));

	const FRotator Current = GetControlRotation();
	SetControlRotation(FRotator(Current.Pitch, TargetYaw, 0.f));

	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) + 1, 1.5f, FColor::Silver, TEXT("镜头回正"));
}

void AArenaPlayerController::JJKFighters()
{
	const ATrainingGameMode* GM = GetTrainingGameMode();
	if (GM == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[JJKFighters] 当前 GameMode 不是 ATrainingGameMode"));
		return;
	}

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const AFighterCharacter* Fighter = Index == 0 ? GM->GetPlayerFighter() : GM->GetOpponentFighter();
		if (Fighter == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[JJKFighters] 槽位 %d 为空"), Index);
			continue;
		}
		const UFighterAttributeSet* Attributes = Fighter->GetFighterAttributeSet();
		const UAbilitySystemComponent* ASC = Fighter->GetFighterAbilitySystemComponent();

		UE_LOG(LogTemp, Log, TEXT("[JJKFighters] %s 角色=%s Owner=%s Avatar=%s Health=%.1f/%.1f Action=%.1f/%.1f Energy=%.1f/%.1f 初始化次数=%d 目标=%s"),
			Index == 0 ? TEXT("玩家") : TEXT("对手"),
			*GetNameSafe(Fighter),
			ASC != nullptr ? *GetNameSafe(ASC->GetOwnerActor()) : TEXT("null"),
			ASC != nullptr ? *GetNameSafe(ASC->GetAvatarActor()) : TEXT("null"),
			Attributes ? Attributes->GetHealth() : -1.f,
			Attributes ? Attributes->GetMaxHealth() : -1.f,
			Attributes ? Attributes->GetActionResource() : -1.f,
			Attributes ? Attributes->GetMaxActionResource() : -1.f,
			Attributes ? Attributes->GetEnergy() : -1.f,
			Attributes ? Attributes->GetMaxEnergy() : -1.f,
			Fighter->GetStatsInitCount(),
			Fighter->GetTargeting() != nullptr ? *GetNameSafe(Fighter->GetTargeting()->GetCurrentTarget()) : TEXT("null"));
	}
}

void AArenaPlayerController::JJKReinitFighters()
{
	ATrainingGameMode* GM = GetTrainingGameMode();
	if (GM == nullptr)
	{
		return;
	}
	if (AFighterCharacter* P1Fighter = GM->GetPlayerFighter())
	{
		P1Fighter->InitializeFromDefinition();
	}
	if (AFighterCharacter* Opponent = GM->GetOpponentFighter())
	{
		Opponent->InitializeFromDefinition();
	}
	UE_LOG(LogTemp, Log, TEXT("[JJKReinitFighters] 已重复调用集中初始化入口（应无数值变化/重复授予）"));
	JJKFighters();
}

void AArenaPlayerController::JJKResetFighters()
{
	ATrainingGameMode* GM = GetTrainingGameMode();
	if (GM == nullptr)
	{
		return;
	}
	// 统一走 GameMode 训练重置序列（停请求→取消能力→清临时→复位→恢复→目标/模式）
	GM->ResetTraining();
	UE_LOG(LogTemp, Log, TEXT("[JJKResetFighters] 双方已重置"));
	JJKFighters();
}

void AArenaPlayerController::JJKKillTarget()
{
	AFighterCharacter* P1Fighter = GetPlayerFighter();
	ATrainingGameMode* GM = GetTrainingGameMode();
	if (P1Fighter == nullptr || GM == nullptr || P1Fighter->GetTargeting() == nullptr)
	{
		return;
	}

	AFighterCharacter* Victim = P1Fighter->GetTargeting()->GetCurrentTarget();
	if (Victim == nullptr)
	{
		Victim = GM->GetOpponentFighter();
	}
	if (Victim == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[JJKKillTarget] 无可销毁目标"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[JJKKillTarget] 调试销毁 %s"), *GetNameSafe(Victim));
	Victim->Destroy();
}

void AArenaPlayerController::JJKRespawnFighters()
{
	if (ATrainingGameMode* GM = GetTrainingGameMode())
	{
		GM->RestartPlayer(this);
	}
}

void AArenaPlayerController::ClearFrameInput()
{
 bAttackPressed = bAttackReleased = bKickPressed = bKickReleased = bDodgePressed = bStancePressed = bDomainPressed = false;
}
void AArenaPlayerController::SetCombatInputEnabled(bool bEnabled)
{
 if (!bEnabled) ClearCombatFeedback();
 bCombatInputEnabled = bEnabled;
 if (auto* F = GetPlayerFighter()) F->GetCombatInput()->SetRequestsEnabled(bEnabled);
 ClearFrameInput();
 if (!bEnabled) HandleAppActivationChanged(false);
}
void AArenaPlayerController::PostProcessInput(float DeltaTime, bool bGamePaused)
{
 Super::PostProcessInput(DeltaTime, bGamePaused);
 AFighterCharacter* Fighter = GetPlayerFighter();
 if (!Fighter || !bCombatInputEnabled || bGamePaused) { ClearFrameInput(); return; }
 auto* Input = Fighter->GetCombatInput();
 // 先应用待处理死亡/强制中断，再按 Shift > 松键 > 新动作排序，与映射迭代顺序无关。
 Fighter->ProcessCombatEvents();
 // 移动中 Shift = 直接加速跑（不前扑）；原地 Shift = 后撤闪避（现状保留）
 bool Dodged=false;
 if(bDodgePressed)
 {
  if(Fighter->LastMoveInputDirection.IsNearlyZero())
  {
   Dodged=Fighter->RequestDodge(Fighter->GetLastDodgeDirection());
   if(Dodged) Fighter->SetSprintHeld(bDodgeHeld);
  }
  else
  {
   Fighter->SetSprintHeld(true);
  }
 }
 Fighter->RefreshMovementControl();
 if(bDodgePressed) if(auto* GM=GetTrainingGameMode()) GM->RecordInput(Fighter,Dodged ? TEXT("闪避：执行") : TEXT("闪避：拒绝"));
 if (!Dodged)
 {
  if (bAttackPressed) Input->NotifyAttackPressed();
  if (bKickPressed) Input->NotifyKickPressed();
  if (bAttackReleased) Input->NotifyAttackReleased();
  if (bKickReleased) Input->NotifyKickReleased();
  if (bStancePressed) Input->NotifyStanceSwitchPressed();
  if (bDomainPressed) Input->NotifyDomainPressed();
 }
 ClearFrameInput();
}

void AArenaPlayerController::SetTrainingPanelOpen(bool bOpen)
{
 if (bOpen) ClearCombatFeedback();
 auto* GM=GetTrainingGameMode(); if(!GM) return;
 const bool bPanelPresent = TrainingPanel && TrainingPanel->IsInViewport();
 if(bOpen==GM->IsTrainingMenuOpen() && bOpen==bPanelPresent) return;
 GM->SetTrainingMenuOpen(bOpen);
 ResetIgnoreMoveInput(); ResetIgnoreLookInput();
 SetIgnoreMoveInput(bOpen); SetIgnoreLookInput(bOpen);
 bShowMouseCursor=bOpen;
 if(bOpen)
 {
  if(!TrainingPanel) TrainingPanel=CreateWidget<UTrainingPanelWidget>(this,TrainingPanelClass ? TrainingPanelClass.Get() : UTrainingPanelWidget::StaticClass());
  if(TrainingPanel)
  {
   TrainingPanel->AddToViewport(50);
   FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(TrainingPanel->TakeWidget()); Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode); TrainingPanel->SetKeyboardFocus();
  }
 }
 else
 {
  if(TrainingPanel) TrainingPanel->RemoveFromParent();
  SetInputMode(FInputModeGameOnly());
 }
 FlushPressedKeys();
}
void AArenaPlayerController::ToggleTrainingPanel()
{
 if (GameMenu)
 {
  if (!bGameMenuOpen) OpenGameMenu(TEXT("settings"),TEXT("training"));
  return;
 }
 if(GetTrainingGameMode()) SetTrainingPanelOpen(!(TrainingPanel && TrainingPanel->IsInViewport()));
}

void AArenaPlayerController::SetGameMenuOpen(bool bOpen)
{
 if (!GameMenu) return;
 const bool bChanged=bGameMenuOpen!=bOpen;
 bGameMenuOpen=bOpen;
 if (TrainingPanel && TrainingPanel->IsInViewport()) TrainingPanel->RemoveFromParent();
 if (auto* GM=GetTrainingGameMode())
  if (bChanged || GM->IsTrainingMenuOpen()!=bOpen) GM->SetTrainingMenuOpen(bOpen);
 if (bOpen)
 {
  SetPause(true);
  ResetIgnoreMoveInput(); ResetIgnoreLookInput(); SetIgnoreMoveInput(true); SetIgnoreLookInput(true);
  if (ArenaHud) ArenaHud->SetVisibility(ESlateVisibility::Collapsed);
  if (CombatHud) CombatHud->SetVisibility(ESlateVisibility::Collapsed);
  GameMenu->SetVisibility(ESlateVisibility::Visible); bShowMouseCursor=true;
  FInputModeUIOnly Mode; Mode.SetWidgetToFocus(GameMenu->TakeWidget()); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
  GameMenu->FocusBrowser();
 }
 else
 {
  SetPause(false);
  ResetIgnoreMoveInput(); ResetIgnoreLookInput();
  GameMenu->SetVisibility(ESlateVisibility::Collapsed); bShowMouseCursor=false;
  if (ArenaHud) ArenaHud->SetVisibility(ESlateVisibility::HitTestInvisible);
  SetInputMode(FInputModeGameOnly());
  HandleAppActivationChanged(FSlateApplication::Get().IsActive());
 }
 if (bChanged) FlushPressedKeys();
}

void AArenaPlayerController::OpenGameMenu(const FString& Page,const FString& Tab)
{
 if (!GameMenu) return;
 if (!bGameMenuOpen) GameMenu->CaptureScene();
 SetGameMenuOpen(true); GameMenu->ShowPage(Page,Tab); GameMenu->FocusBrowser();
}
void AArenaPlayerController::HandlePausePressed()
{
 if (GameMenu && !bGameMenuOpen) OpenGameMenu(TEXT("pause"));
}
void AArenaPlayerController::JJKMenu(const FString& Page)
{
#if !UE_BUILD_SHIPPING
 if (Page==TEXT("battle")) { if (GameMenu) GameMenu->ShowPage(Page); SetGameMenuOpen(false); }
 else if (TArray<FString>{TEXT("main"),TEXT("pause"),TEXT("settings"),TEXT("controls")}.Contains(Page)) OpenGameMenu(Page);
#endif
}
float AArenaPlayerController::GetMenuLookSensitivity() const { return GameMenu ? GameMenu->GetLookSensitivity() : 1.f; }
bool AArenaPlayerController::IsMenuLookInverted() const { return GameMenu && GameMenu->IsLookInverted(); }
void AArenaPlayerController::DebugSendKey(FName KeyName, bool bPressed)
{
#if !UE_BUILD_SHIPPING
 if(!FSlateApplication::IsInitialized()) return;
 const FKey Key(KeyName);
 // Slate key events do not represent mouse button events. Send mouse buttons through
 // the native input gateway so Development tests exercise the existing EnhancedInput bindings.
 if (Key.IsMouseButton())
 {
  InputKey(FInputKeyEventArgs::CreateSimulated(Key, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.f : 0.f));
  return;
 }
 const FKeyEvent Event(Key,FModifierKeysState(),0,false,0,0);
 if(bPressed) FSlateApplication::Get().ProcessKeyDownEvent(Event);
 else FSlateApplication::Get().ProcessKeyUpEvent(Event);
#endif
}
bool AArenaPlayerController::IsWireframeView() const
{
 const auto* Viewport=GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
 return Viewport && Viewport->EngineShowFlags.Wireframe;
}
void AArenaPlayerController::RebuildTrainingPanel()
{
 auto* GM=GetTrainingGameMode(); const bool bWasOpen=GM && GM->IsTrainingMenuOpen();
 SetTrainingPanelOpen(false); TrainingPanel=nullptr;
 if(bWasOpen) SetTrainingPanelOpen(true);
}
