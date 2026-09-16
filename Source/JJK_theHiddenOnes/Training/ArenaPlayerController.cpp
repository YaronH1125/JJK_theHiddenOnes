// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/ArenaPlayerController.h"

#include "EnhancedInputComponent.h"
#include "InputCoreTypes.h"
#include "Training/TrainingPanelWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Training/CombatInputComponent.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterAttributeSet.h"
#include "Training/TargetingComponent.h"
#include "Training/TrainingGameMode.h"

void AArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->ViewPitchMin = -65.f;
		PlayerCameraManager->ViewPitchMax = 65.f;
	}

	// 失焦清会话：恢复后要求重新按下（08 第 4.2 节）
	FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(
		this, &AArenaPlayerController::HandleAppActivationChanged);
}

void AArenaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().RemoveAll(this);
	}
	if(TrainingPanel) { TrainingPanel->RemoveFromParent(); TrainingPanel=nullptr; }
	Super::EndPlay(EndPlayReason);
}

void AArenaPlayerController::HandleAppActivationChanged(bool bActive)
{
	if (!bActive)
	{
		ClearFrameInput();
		if (AFighterCharacter* Fighter = GetPlayerFighter())
		{
			if (UCombatInputComponent* Input = Fighter->GetCombatInput())
			{
				Input->InvalidateSession(FText::FromString(TEXT("应用失焦")));
				Input->ReleaseContinuousInputs();
				Fighter->LastMoveInputDirection = FVector::ZeroVector;
			}
		}
	}
}

void AArenaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::F1,IE_Pressed,this,&AArenaPlayerController::ToggleTrainingPanel);

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
		if (!AttackAction)
		{
			UE_LOG(LogTemp, Error, TEXT("[ArenaPC] AttackAction 未配置，攻击输入无法绑定；请检查 BP_ArenaPlayerController 与 IA_Attack 资产"));
		}
	}
}

void AArenaPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetCombatInputEnabled(bCombatInputEnabled);
	UE_LOG(LogTemp, Log, TEXT("[ArenaPC] %s 接管 %s"), *GetName(), *GetNameSafe(InPawn));
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
 if (bCombatInputEnabled) bDodgePressed = true;
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
 bAttackPressed = bAttackReleased = bKickPressed = bKickReleased = bDodgePressed = bStancePressed = false;
}
void AArenaPlayerController::SetCombatInputEnabled(bool bEnabled)
{
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
 const bool Dodged = bDodgePressed && Fighter->RequestDodge(Fighter->GetLastDodgeDirection());
 if(bDodgePressed) if(auto* GM=GetTrainingGameMode()) GM->RecordInput(Fighter,Dodged ? TEXT("闪避：执行") : TEXT("闪避：拒绝"));
 if (!Dodged)
 {
  if (bAttackPressed) Input->NotifyAttackPressed();
  if (bKickPressed) Input->NotifyKickPressed();
  if (bAttackReleased) Input->NotifyAttackReleased();
  if (bKickReleased) Input->NotifyKickReleased();
  if (bStancePressed) Input->NotifyStanceSwitchPressed();
 }
 ClearFrameInput();
}

void AArenaPlayerController::SetTrainingPanelOpen(bool bOpen)
{
 auto* GM=GetTrainingGameMode(); if(!GM) return;
 if(bOpen==GM->IsTrainingMenuOpen() && (!bOpen || (TrainingPanel && TrainingPanel->IsInViewport()))) return;
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
void AArenaPlayerController::ToggleTrainingPanel() { if(auto* GM=GetTrainingGameMode()) SetTrainingPanelOpen(!GM->IsTrainingMenuOpen()); }
void AArenaPlayerController::RebuildTrainingPanel()
{
 auto* GM=GetTrainingGameMode(); const bool bWasOpen=GM && GM->IsTrainingMenuOpen();
 SetTrainingPanelOpen(false); TrainingPanel=nullptr;
 if(bWasOpen) SetTrainingPanelOpen(true);
}
