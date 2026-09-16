// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/FighterCharacter.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);



#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Training/AttackDefinition.h"
#include "Training/CombatHitComponent.h"
#include "Training/CombatInputComponent.h"
#include "Training/CombatTypes.h"
#include "Training/DamageGameplayEffect.h"
#include "Training/TrainingGameMode.h"
#include "Training/TrainingProbeAbility.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterDefinition.h"
#include "Training/MeleeComboAbility.h"
#include "Training/ModifyActionResourceGameplayEffect.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Training/CombatStateGameplayEffect.h"
#include "Training/RestoreCursedEnergyGameplayEffect.h"
#include "Training/TargetingComponent.h"

AFighterCharacter::AFighterCharacter()
{
	AbilitySystem = CreateDefaultSubobject<UFighterAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);

	AttributeSet = CreateDefaultSubobject<UFighterAttributeSet>(TEXT("AttributeSet"));

	Targeting = CreateDefaultSubobject<UTargetingComponent>(TEXT("Targeting"));

	CombatInput = CreateDefaultSubobject<UCombatInputComponent>(TEXT("CombatInput"));
	CombatHit = CreateDefaultSubobject<UCombatHitComponent>(TEXT("CombatHit"));

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	// 无控制器的静止对手仍需落地、保持移动物理与动画更新。
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
}

UAbilitySystemComponent* AFighterCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AFighterCharacter::RecordInitialTransform(const FTransform& InTransform)
{
	InitialTransform = InTransform;
}

void AFighterCharacter::DoMove(float Right, float Forward)
{

	if (const AController* C = GetController())
	{
		const FRotator Yaw(0.f, C->GetControlRotation().Yaw, 0.f);
		LastMoveInputDirection = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * Forward
			+ FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * Right;
		if (LastMoveInputDirection.SizeSquared() < 0.01f)
		{
			LastMoveInputDirection = FVector::ZeroVector;
		}
	}
	if (CanAct()) Super::DoMove(Right, Forward);
}

void AFighterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 对手无控制器，不经过 PossessedBy，这里补一次 ActorInfo 初始化；
	// 数值初始化的幂等保护保证双方各只执行一次
	InitAbilityActorInfo();
	InitializeFromDefinition();
	AddDefaultMappingContext();

	// 生命变化监听：致死伤害进入延迟队列，与受击共用换血批次语义（M2.5）
	if (AbilitySystem != nullptr && IsValid(AttributeSet))
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UFighterAttributeSet::GetHealthAttribute())
			.AddUObject(this, &AFighterCharacter::OnHealthChanged);
		// 默认近战形态
		AbilitySystem->SetLooseGameplayTagCount(TAG_Stance_Melee, 1);
	}

	// 行动资源恢复节拍
	GetWorldTimerManager().SetTimer(ResourceRegenTimerHandle, this,
		&AFighterCharacter::TickResourceRegen, ResourceRegenTimerInterval, true);

	if (FighterRole != EFighterRole::Unassigned && InitialTransform.GetLocation().IsZero())
	{
		// GameMode 未记录时兜底保存当前变换
		InitialTransform = GetActorTransform();
	}
}

bool AFighterCharacter::IsDead() const
{
	return bDead || (AbilitySystem != nullptr && AbilitySystem->HasMatchingGameplayTag(TAG_State_Dead));
}

bool AFighterCharacter::HasCombatTag(const FGameplayTag& Tag) const
{
	return AbilitySystem != nullptr && AbilitySystem->HasMatchingGameplayTag(Tag);
}

bool AFighterCharacter::IsAttacking() const
{
	return HasCombatTag(TAG_State_Attacking);
}

bool AFighterCharacter::IsGuardIntent() const
{
	return CombatInput && CombatInput->IsGuardIntent();
}

bool AFighterCharacter::IsGuarding() const
{
	if (!IsGuardIntent() || IsDead() || IsThrowPaired())
	{
		return false;
	}
	if (HasCombatTag(TAG_State_HitStun)
		|| HasCombatTag(TAG_State_KnockedDown) || HasCombatTag(TAG_State_DodgeInvulnerable)
		|| HasCombatTag(TAG_State_DodgeRecovery) || HasCombatTag(TAG_State_StanceSwitching))
	{
		return false;
	}
	return !IsAttacking();
}

bool AFighterCharacter::HasSuperArmor() const
{
	return HasCombatTag(TAG_State_SuperArmor);
}

TSubclassOf<UGameplayAbility> AFighterCharacter::GetMeleeAttackAbilityClass() const
{
	return Definition ? Definition->MeleeAttackAbility : TSubclassOf<UGameplayAbility>();
}

EActionRequestResult AFighterCharacter::ValidateAttackRequest() const
{
	if (!CombatInput->AreRequestsEnabled()) return EActionRequestResult::RejectedBlocked;
	if (IsDead())
	{
		return EActionRequestResult::RejectedDead;
	}
	if (!IsStatsInitialized() || AbilitySystem == nullptr)
	{
		return EActionRequestResult::RejectedNotInitialized;
	}
	if (GetStance() == EFighterStance::Ranged)
	{
		return EActionRequestResult::RejectedRangedStance;
	}
	if (HasCombatTag(TAG_State_HitStun) || HasCombatTag(TAG_State_KnockedDown)
		|| HasCombatTag(TAG_State_StanceSwitching) || HasCombatTag(TAG_State_GuardStun)
		|| HasCombatTag(TAG_State_DodgeInvulnerable) || HasCombatTag(TAG_State_DodgeRecovery) || IsThrowPaired() || (!IsAttacking() && IsGuardIntent()))
	{
		return EActionRequestResult::RejectedBlocked;
	}
	return EActionRequestResult::Executed;
}

UAttackDefinition* AFighterCharacter::ResolveAttackDefinition(ECachedAction Action) const
{
	const UFighterDefinition* Def = GetDefinition();
	if (Def == nullptr)
	{
		return nullptr;
	}
	switch (Action)
	{
	case ECachedAction::NextSegment:
		return Def->ComboSegments.Num() > 0 ? Def->ComboSegments[0] : Def->AttackDefinition;
	case ECachedAction::HeavyPunch:
		return Def->HeavyPunchDefinition;
	case ECachedAction::Kick:
		return Def->KickDefinition;
	case ECachedAction::HeavyKick:
		return Def->HeavyKickDefinition;
	default:
		return nullptr;
	}
}

bool AFighterCharacter::RequestAttackSequence(ECachedAction Action)
{
	if (ValidateAttackRequest() != EActionRequestResult::Executed || IsAttacking()) return false;
	UAttackDefinition* FirstDef = ResolveAttackDefinition(Action);
	if (FirstDef == nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 攻击序列缺少定义（%d）"), *GetName(), static_cast<int32>(Action));
		return false;
	}

	PendingSequence.Reset();
	PendingSequence.Add(FirstDef);

	// 连击链：A1 序列携带后续段，GA 在衔接窗口内推进
	if (Action == ECachedAction::NextSegment && GetDefinition() != nullptr)
	{
		for (UAttackDefinition* Seg : GetDefinition()->ComboSegments)
		{
			if (Seg != nullptr && Seg != FirstDef && !PendingSequence.Contains(Seg))
			{
				PendingSequence.Add(Seg);
			}
		}
	}

	PendingSegmentIndex = 0;
	const bool Activated = AbilitySystem != nullptr && AbilitySystem->TryActivateAbilityByClass(GetMeleeAttackAbilityClass());
	if (!Activated) PendingSequence.Reset();
	return Activated;
}

bool AFighterCharacter::ConsumePendingSequence(TArray<TObjectPtr<UAttackDefinition>>& OutSequence, int32& OutSegmentIndex)
{
	if (PendingSequence.IsEmpty())
	{
		return false;
	}
	OutSequence = PendingSequence;
	OutSegmentIndex = PendingSegmentIndex;
	PendingSequence.Reset();
	PendingSegmentIndex = 0;
	return true;
}

AFighterCharacter::FDodgeRequest AFighterCharacter::ConsumePendingDodge()
{
 const FDodgeRequest Result = PendingDodge;
 PendingDodge = {};
 return Result;
}

bool AFighterCharacter::SpendActionResource(float Amount)
{
	return ModifyActionResource(-FMath::Abs(Amount));
}

void AFighterCharacter::RestoreActionResource(float Amount)
{
	ModifyActionResource(FMath::Abs(Amount));
}

bool AFighterCharacter::ModifyActionResource(float SignedAmount)
{
	if (AbilitySystem == nullptr)
	{
		return false;
	}
	if (SignedAmount < 0.f && AbilitySystem->HasInfiniteResources()) return true;
	const float Current = AttributeSet ? AttributeSet->GetActionResource() : 0.f;
	if (SignedAmount < 0.f && Current + SignedAmount < -0.01f)
	{
		return false; // 不足
	}
	FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UModifyActionResourceGameplayEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(TAG_Data_Amount, SignedAmount);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	if (SignedAmount < 0.f) LastResourceSpendTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return true;
}

void AFighterCharacter::TickResourceRegen()
{
	if (IsDead() || !Definition) return;
	const auto& ResourceRegen = Definition->ResourceRegen;
	const float Current = AttributeSet ? AttributeSet->GetActionResource() : 0.f;
	const float Max = AttributeSet ? AttributeSet->GetMaxActionResource() : 0.f;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Current < Max && (Now - LastResourceSpendTime) >= ResourceRegen.RegenDelay)
	{
		RestoreActionResource(ResourceRegen.RegenPerSecond * ResourceRegenTimerInterval);
	}
}

bool AFighterCharacter::RequestDodge(FVector Direction)
{
	if (!CombatInput->AreRequestsEnabled() || !Definition || !AbilitySystem || !IsStatsInitialized() || !GetCharacterMovement()->IsMovingOnGround() || IsDead() || HasCombatTag(TAG_State_DodgeInvulnerable) || HasCombatTag(TAG_State_HitStun) || HasCombatTag(TAG_State_KnockedDown)
		|| HasCombatTag(TAG_State_GuardStun) || HasCombatTag(TAG_State_DodgeRecovery)
		|| HasCombatTag(TAG_State_StanceSwitching) || IsThrowPaired())
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 闪避请求被拒：状态阻止"), *GetName());
		return false;
	}

	// 取消闪避判定：攻击中且处于该段取消窗口
	const bool bCancel = IsAttacking();
	if (bCancel && (!CombatHit || !CombatHit->IsCancelWindowOpen())) return false;
	const float Cost = bCancel && Definition ? Definition->DodgeConfig.CancelDodgeTotalCost
											 : (Definition ? Definition->DodgeConfig.DodgeCost : 1.f);

	// 先全部检查（含资源），通过才停止旧 GA（M3.4）
	const bool bPaid = !AbilitySystem->HasInfiniteResources();
	const double PreviousSpendTime = LastResourceSpendTime;
	if (!SpendActionResource(Cost))
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 闪避被拒：行动资源不足（需 %.0f）"), *GetName(), Cost);
		return false;
	}

	PendingDodge.bPending = true;
	PendingDodge.Direction = Direction;
	PendingDodge.bCancel = bCancel;

	const bool bActivated = AbilitySystem != nullptr
		&& AbilitySystem->TryActivateAbilityByClass(Definition ? Definition->DodgeAbility : nullptr);
	if (!bActivated)
	{
		// 激活失败：退还资源，原动作继续
		PendingDodge = {};
		if (bPaid) RestoreActionResource(Cost);
		LastResourceSpendTime = PreviousSpendTime;
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 闪避激活失败，资源已退还"), *GetName());
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[Combat] %s 闪避开始（取消=%d，消耗 %.0f）"), *GetName(), bCancel ? 1 : 0, Cost);
	return true;
}

bool AFighterCharacter::RequestStanceSwitch()
{
	if (!CombatInput->AreRequestsEnabled() || CombatInput->IsSessionActive() || CombatInput->IsKickSessionActive() || !Definition || !AbilitySystem || !CanAct() || HasCombatTag(TAG_State_GuardStun) || IsDead() || IsAttacking() || HasCombatTag(TAG_State_HitStun)
		|| HasCombatTag(TAG_State_KnockedDown) || HasCombatTag(TAG_State_StanceSwitching)
		|| IsThrowPaired())
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 切形态被拒：动作/状态限制"), *GetName());
		return false;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - LastStanceSwitchTime < (Definition ? Definition->StanceSwitchInterval : 0.5))
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 切形态被拒：间隔未到"), *GetName());
		return false;
	}

	CombatInput->InvalidateSession(FText::FromString(TEXT("切换形态")));
	CombatInput->ReleaseContinuousInputs();
	bPendingStanceSwitch = true;
	return AbilitySystem != nullptr
		&& AbilitySystem->TryActivateAbilityByClass(Definition ? Definition->StanceSwitchAbility : nullptr);
}

void AFighterCharacter::QueueCombatEvent(const FCombatEvent& Event)
{
	PendingCombatEvents.Push(Event);
	// 确保事件在命中组件的 Tick 内、扫掠之后被处理（换血批次语义）
	CombatHit->NotifyEventsPending();
}

float AFighterCharacter::GetGuardFrontArcHalfAngle() const
{
	return Definition ? Definition->GuardConfig.FrontArcHalfAngle : 90.f;
}

bool AFighterCharacter::IsAttackFromFront(AActor* Attacker, float HalfAngleDeg) const
{
	if (Attacker == nullptr)
	{
		return false;
	}
	FVector ToAttacker = Attacker->GetActorLocation() - GetActorLocation();
	ToAttacker.Z = 0.f;
	if (ToAttacker.IsNearlyZero())
	{
		return true;
	}
	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.f;
	const double CosAngle = FVector::DotProduct(Forward.GetSafeNormal(), ToAttacker.GetSafeNormal());
	return CosAngle >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));
}

bool AFighterCharacter::CanThrowTarget(AFighterCharacter* Target, bool bAttackCanThrow)
{
 if (!bAttackCanThrow || !IsValid(Target) || !Definition || Target == this || IsDead() || Target->IsDead()) return false;
 if (IsThrowPaired() || Target->IsThrowPaired() || Target->HasCombatTag(TAG_State_KnockedDown)
  || Target->HasCombatTag(TAG_State_DodgeInvulnerable)) return false;
 if (!GetCharacterMovement()->IsMovingOnGround() || !Target->GetCharacterMovement()->IsMovingOnGround()) return false;
 const FVector Delta = Target->GetActorLocation() - GetActorLocation();
 return Delta.SizeSquared() <= FMath::Square(Definition->ThrowConfig.MaxDistance)
  && FVector::DotProduct(GetActorForwardVector(), Delta.GetSafeNormal2D()) >= FMath::Cos(FMath::DegreesToRadians(60.f));
}

bool AFighterCharacter::FindThrowPosition(AFighterCharacter* Partner, float PairDistance, FVector& OutPosition) const
{
 const FVector Direction = (Partner->GetActorLocation()-GetActorLocation()).GetSafeNormal2D();
 if (Direction.IsNearlyZero()) return false;
 const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
 if (PairDistance < Radius + Partner->GetCapsuleComponent()->GetScaledCapsuleRadius() + 2.f) return false;
 OutPosition = Partner->GetActorLocation() - Direction * PairDistance;
 OutPosition.Z = GetActorLocation().Z;
 const float MaxR = Definition->ThrowConfig.ArenaRadius - Radius;
 if (OutPosition.Size2D() > MaxR || Partner->GetActorLocation().Size2D() > MaxR) return false;
 FCollisionQueryParams Params(SCENE_QUERY_STAT(ThrowPosition), false, this);
 Params.AddIgnoredActor(Partner);
 const auto Shape = FCollisionShape::MakeCapsule(Radius, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-2.f);
 FHitResult Hit;
 if (GetWorld()->SweepSingleByChannel(Hit, GetActorLocation(), OutPosition, FQuat::Identity, ECC_Pawn, Shape, Params)) return false;
 if (GetWorld()->OverlapBlockingTestByChannel(OutPosition, FQuat::Identity, ECC_Pawn, Shape, Params)) return false;
 const FVector GroundEnd = OutPosition - FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+20.f);
 return GetWorld()->LineTraceSingleByChannel(Hit, OutPosition, GroundEnd, ECC_Visibility, Params) && Hit.ImpactNormal.Z > 0.7f;
}

bool AFighterCharacter::BeginThrowPair(AFighterCharacter* Partner, float Duration, float Damage, float PairDistance)
{
 FVector PairPosition;
 if (!CanThrowTarget(Partner, true) || !FindThrowPosition(Partner, PairDistance, PairPosition)) return false;
 UAnimMontage* AttackMontage = ResolveCurrentAttackDefinition() ? ResolveCurrentAttackDefinition()->Montage.LoadSynchronous() : nullptr;
 UAnimMontage* VictimMontage = Partner->GetHitReactMontage();
 if (!AttackMontage || !VictimMontage) return false;
 // 所有前置检查成功后才中止双方动作。白盒配对使用现有 Montage，时长拉伸。
 for (AFighterCharacter* F : {this, Partner})
 {
  F->AbilitySystem->CancelAllAbilities();
  F->CombatInput->InvalidateSession(FText::FromString(TEXT("投技配对")));
  F->CombatInput->ReleaseContinuousInputs();
  F->CombatHit->EndAttack();
  F->PendingCombatEvents.Reset();
  F->GetWorldTimerManager().ClearTimer(F->HitStunTimerHandle);
  F->ClearReactionEffects();
  F->PreThrowMovementMode = F->GetCharacterMovement()->MovementMode;
  F->GetCharacterMovement()->StopMovementImmediately();
  F->GetCharacterMovement()->SetMovementMode(MOVE_None);
 }
 ThrowPartner = Partner;
 Partner->ThrowPartner = this;
 ThrowEffect = ApplyCombatState(TAG_State_ThrowPaired, -1.f);
 Partner->ThrowEffect = Partner->ApplyCombatState(TAG_State_ThrowPaired, -1.f);
 bThrowDriver = true;
 Partner->bThrowDriver = false;
 SetActorLocation(PairPosition, false, nullptr, ETeleportType::TeleportPhysics);
 const FVector Direction = (Partner->GetActorLocation()-GetActorLocation()).GetSafeNormal2D();
 SetActorRotation(Direction.Rotation());
 Partner->SetActorRotation((-Direction).Rotation());
 RefreshMovementControl(); Partner->RefreshMovementControl();
 ThrowMontage = AttackMontage; Partner->ThrowMontage = VictimMontage;
 const float PlayDuration = FMath::Max(Duration, 0.2f) + 0.4f;
 if (PlayAnimMontage(AttackMontage, AttackMontage->GetPlayLength()/PlayDuration) <= 0.f
  || Partner->PlayAnimMontage(VictimMontage, VictimMontage->GetPlayLength()/PlayDuration) <= 0.f)
 { EndThrowPair(true); return false; }
 TWeakObjectPtr<AFighterCharacter> WeakPartner = Partner;
 FTimerDelegate Delegate;
 Delegate.BindWeakLambda(this, [this, WeakPartner, Damage]()
 {
  AFighterCharacter* Victim = WeakPartner.Get();
  if (!Victim || !IsThrowPaired() || IsDead() || Victim->IsDead()) { EndThrowPair(true); return; }
  ApplyCombatDamage(Victim, Damage, Damage, ETrainingContact::Throw);
  RestoreCursedEnergyOnHit();
  EndThrowPair(true);
 });
 GetWorldTimerManager().SetTimer(ThrowPairTimerHandle, Delegate, FMath::Max(Duration,0.2f), false);
 GetWorldTimerManager().SetTimer(ThrowWatchTimerHandle, this, &AFighterCharacter::TickThrowPair, 0.025f, true);
 return true;
}

void AFighterCharacter::TickThrowPair()
{
 AFighterCharacter* P = ThrowPartner.Get();
 if (!P || IsDead() || P->IsDead() || !GetMesh()->GetAnimInstance() || !P->GetMesh()->GetAnimInstance()
  || !GetMesh()->GetAnimInstance()->Montage_IsPlaying(ThrowMontage.Get())
  || !P->GetMesh()->GetAnimInstance()->Montage_IsPlaying(P->ThrowMontage.Get())) EndThrowPair(true);
}

void AFighterCharacter::EndThrowPair(bool bRestore)
{
 AFighterCharacter* Partner = ThrowPartner.Get();
 const bool WasPaired = Partner != nullptr || ThrowMontage.IsValid();
 ThrowPartner = nullptr;
 if (Partner) Partner->ThrowPartner = nullptr;
 for (AFighterCharacter* F : {this, Partner})
 {
  if (!F) continue;
  F->GetWorldTimerManager().ClearTimer(F->ThrowPairTimerHandle);
  F->GetWorldTimerManager().ClearTimer(F->ThrowWatchTimerHandle);
  if (F->ThrowMontage.IsValid()) F->StopAnimMontage(F->ThrowMontage.Get());
  if (F->AbilitySystem) F->AbilitySystem->RemoveActiveGameplayEffect(F->ThrowEffect);
  F->ThrowMontage = nullptr;
  F->bThrowDriver = false;
  if (WasPaired && bRestore) F->GetCharacterMovement()->SetMovementMode(F->IsDead() ? MOVE_None : EMovementMode(F->PreThrowMovementMode));
  F->RefreshMovementControl();
 }
}

void AFighterCharacter::NotifyStanceSwitched()
{
	Stance = Stance == EFighterStance::Melee ? EFighterStance::Ranged : EFighterStance::Melee;
	LastStanceSwitchTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void AFighterCharacter::ProcessCombatEvents()
{
	if (PendingCombatEvents.IsEmpty())
	{
		return;
	}
	TArray<FCombatEvent> EventsToProcess = MoveTemp(PendingCombatEvents);
	PendingCombatEvents.Reset();

	for (const FCombatEvent& Event : EventsToProcess)
	{
		if (Event.bLethal)
		{
			Die(Event.Instigator.Get());
			continue;
		}
		switch (Event.Type)
		{
		case FCombatEvent::EType::GuardStun:
			ApplyGuardStunNow(Event);
			break;
		case FCombatEvent::EType::Knockdown:
			ApplyKnockdownNow(Event);
			break;
		case FCombatEvent::EType::HitReact:
		default:
			ApplyHitReactNow(Event);
			break;
		}
	}
 OnCombatEventsProcessed.Broadcast();
}

void AFighterCharacter::ApplyHitReactNow(const FCombatEvent& Event)
{
	if (IsDead())
	{
		return;
	}

	LastHitLocation = Event.HitLocation;
	LastHitInstigator = Event.Instigator.Get();

	// 霸体：伤害已照常结算，仅抵抗打断（M3-T12）
	if (HasSuperArmor() && Event.InterruptLevel <= (ResolveCurrentAttackDefinition() ? ResolveCurrentAttackDefinition()->ArmorResistanceLevel : 0))
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 霸体抵抗打断（伤害已结算）"), *GetName());
		return;
	}

	// State.HitStun：硬直期内拒绝新动作请求（请求入口与能力激活双重检查）
	if (AbilitySystem != nullptr)
	{
		AbilitySystem->RemoveActiveGameplayEffect(StunEffect);
		StunEffect = ApplyCombatState(TAG_State_HitStun, FMath::Max(Event.StunDuration,0.01f));
	}
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().SetTimer(HitStunTimerHandle, this,
			&AFighterCharacter::RemoveHitStun, FMath::Max(Event.StunDuration, 0.01f), false);
	}

	// 受击打断攻击：按能力标签取消活动攻击（GA EndAbility 关窗清状态）
	if (AbilitySystem != nullptr)
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(TAG_Ability_MeleeAttack);
		CancelTags.AddTag(TAG_Ability_Dodge);
		CancelTags.AddTag(TAG_Ability_StanceSwitch);
		AbilitySystem->CancelAbilities(&CancelTags);
	}

	CombatInput->InvalidateSession(FText::FromString(TEXT("受击中断")));

	RefreshMovementControl();
	UAnimMontage* ReactMontage = nullptr;
	if (Definition != nullptr)
	{
		if (UAttackDefinition* Seg = ResolveCurrentAttackDefinition())
		{
			ReactMontage = Seg->HitReactMontage.LoadSynchronous();
		}
		if (ReactMontage == nullptr)
		{
			ReactMontage = Definition->AttackDefinition ? Definition->AttackDefinition->HitReactMontage.LoadSynchronous() : nullptr;
		}
	}
	if (ReactMontage != nullptr)
	{
		PlayAnimMontage(ReactMontage);
	}

	// 击退（配置强度，沿攻击者指向）
	if (Event.KnockbackStrength > 0.f)
	{
		FVector Dir = Event.KnockbackDirection.IsNearlyZero()
			? -GetActorForwardVector()
			: Event.KnockbackDirection;
		Dir.Z = 0.f;
		LaunchCharacter(Dir.GetSafeNormal() * Event.KnockbackStrength, false, true);
	}

	UE_LOG(LogTemp, Log, TEXT("[Combat] %s 受击硬直 %.2fs（来源 %s）"),
		*GetName(), Event.StunDuration, *GetNameSafe(Event.Instigator.Get()));
}

UAttackDefinition* AFighterCharacter::ResolveCurrentAttackDefinition() const
{
	return CombatHit ? const_cast<UAttackDefinition*>(CombatHit->GetActiveDefinition()) : nullptr;
}

void AFighterCharacter::ApplyGuardStunNow(const FCombatEvent& Event)
{
	if (IsDead())
	{
		return;
	}
	// 防御硬直：防御意图保持，恢复后继续防御（不强制松开）
	if (AbilitySystem != nullptr)
	{
		AbilitySystem->RemoveActiveGameplayEffect(StunEffect);
		StunEffect = ApplyCombatState(TAG_State_GuardStun, FMath::Max(Event.StunDuration,0.01f));
	}
	RefreshMovementControl();
	GetWorldTimerManager().SetTimer(HitStunTimerHandle, this,
		&AFighterCharacter::RemoveHitStun, FMath::Max(Event.StunDuration, 0.01f), false);
	UE_LOG(LogTemp, Log, TEXT("[Combat] %s 防御硬直 %.2fs"), *GetName(), Event.StunDuration);
}

void AFighterCharacter::ApplyKnockdownNow(const FCombatEvent& Event)
{
 if (HasSuperArmor() && ResolveCurrentAttackDefinition() && Event.InterruptLevel <= ResolveCurrentAttackDefinition()->ArmorResistanceLevel) return;
	if (IsDead())
	{
		return;
	}
	LastHitLocation = Event.HitLocation;
	LastHitInstigator = Event.Instigator.Get();

	if (AbilitySystem != nullptr)
	{
		ClearReactionEffects();
		KnockdownEffect = ApplyCombatState(TAG_State_KnockedDown, Definition ? Definition->KnockdownDuration : 1.5f);
	}
	CombatInput->InvalidateSession(FText::FromString(TEXT("倒地")));
	if (AbilitySystem != nullptr)
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(TAG_Ability_MeleeAttack);
		CancelTags.AddTag(TAG_Ability_Dodge);
		CancelTags.AddTag(TAG_Ability_StanceSwitch);
		AbilitySystem->CancelAbilities(&CancelTags);
	}

	FVector Dir = Event.KnockbackDirection.IsNearlyZero() ? -GetActorForwardVector() : Event.KnockbackDirection;
	Dir.Z = 0.f;
	LaunchCharacter(Dir.GetSafeNormal() * FMath::Max(Event.KnockbackStrength, 200.f) + FVector(0, 0, 250), false, true);

	RefreshMovementControl();
	const float Duration = Definition ? Definition->KnockdownDuration : 1.5f;
 GetWorldTimerManager().SetTimer(GetUpTimerHandle, this, &AFighterCharacter::BeginGetUp,
  FMath::Max(.01f, Duration - (Definition ? Definition->GetUpDuration : .4f)), false);
	GetWorldTimerManager().SetTimer(KnockdownTimerHandle, this,
		&AFighterCharacter::EndKnockdown, Duration, false);

	if (UAnimMontage* M = GetHitReactMontage()) PlayAnimMontage(M, M->GetPlayLength()/FMath::Max(Duration, .1f));
	UE_LOG(LogTemp, Log, TEXT("[Combat] %s 倒地 %.2fs（来源 %s）"), *GetName(), Duration, *GetNameSafe(Event.Instigator.Get()));
}

void AFighterCharacter::BeginGetUp()
{
 if (!IsDead() && HasCombatTag(TAG_State_KnockedDown))
  GetUpEffect = ApplyCombatState(TAG_State_GettingUp, Definition ? Definition->GetUpDuration : .4f);
}

void AFighterCharacter::EndKnockdown()
{
	if (IsDead())
	{
		return;
	}
	if (AbilitySystem != nullptr)
	{
		AbilitySystem->RemoveActiveGameplayEffect(KnockdownEffect);
		AbilitySystem->RemoveActiveGameplayEffect(GetUpEffect);
	}
	RefreshMovementControl();
	UE_LOG(LogTemp, Log, TEXT("[Combat] %s 起身，恢复行动"), *GetName());
}

UAnimMontage* AFighterCharacter::GetHitReactMontage() const
{
	if (Definition == nullptr)
	{
		return nullptr;
	}
	if (UAttackDefinition* Current = ResolveCurrentAttackDefinition())
	{
		if (TSoftObjectPtr<UAnimMontage> M = Current->HitReactMontage)
		{
			return M.LoadSynchronous();
		}
	}
	return Definition->AttackDefinition ? Definition->AttackDefinition->HitReactMontage.LoadSynchronous() : nullptr;
}

void AFighterCharacter::RemoveHitStun()
{
	if (AbilitySystem != nullptr)
	{
		AbilitySystem->RemoveActiveGameplayEffect(StunEffect);
	}
	RefreshMovementControl();
	UE_LOG(LogTemp, Log, TEXT("[Combat] %s 硬直结束，恢复行动"), *GetName());
}

void AFighterCharacter::Die(AActor* InInstigator)
{
	if (bDead)
	{
		return;
	}
	bDead = true;

	if (AbilitySystem != nullptr)
	{
		ClearReactionEffects();
		DeathEffect = ApplyCombatState(TAG_State_Dead, -1.f);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_HitStun);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_GuardStun);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_KnockedDown);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_DodgeInvulnerable);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_DodgeRecovery);
		GetWorldTimerManager().ClearTimer(HitStunTimerHandle);
		GetWorldTimerManager().ClearTimer(KnockdownTimerHandle);

		// 死亡取消一切战斗行为（攻击/闪避/切形态）；等待调试重置（M2.4 最小死亡处理）
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(TAG_Ability_MeleeAttack);
		CancelTags.AddTag(TAG_Ability_Dodge);
		CancelTags.AddTag(TAG_Ability_StanceSwitch);
		AbilitySystem->CancelAbilities(&CancelTags);
	}

	CombatInput->InvalidateSession(FText::FromString(TEXT("死亡")));
	CombatInput->ReleaseContinuousInputs();
	CombatHit->EndAttack();
	EndThrowPair(true);
	RefreshMovementControl();
	GetCharacterMovement()->StopMovementImmediately();

	if (UAnimMontage* M = GetHitReactMontage()) PlayAnimMontage(M);
	UE_LOG(LogTemp, Log, TEXT("[Combat] %s 死亡（来源 %s），禁止新动作，等待训练重置"),
		*GetName(), *GetNameSafe(InInstigator));
}

void AFighterCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.f && !IsDead())
	{
		// 致死伤害与受击同批次语义：进入自身队列，下一 Tick 统一死亡处理
		FCombatEvent Event;
		Event.bLethal = true;
		Event.Instigator = LastHitInstigator;
		Event.HitLocation = LastHitLocation;
		QueueCombatEvent(Event);
	}
}

void AFighterCharacter::JJKDebugForceHitReact()
{
	// 明确标记的异常注入：直接命中事件，不入正常检测统计
	FCombatEvent Event;
	Event.bLethal = false;
	Event.StunDuration = Definition && Definition->AttackDefinition ? Definition->AttackDefinition->HitStunDuration : 0.5f;
	Event.Instigator = nullptr;
	Event.HitLocation = GetActorLocation();
	QueueCombatEvent(Event);
	UE_LOG(LogTemp, Log, TEXT("[Combat] %s 调试注入受击（绕过检测窗口，异常注入标记）"), *GetName());
}

void AFighterCharacter::JJKDebugKill()
{
	// 明确标记的异常注入：致死伤害走延迟死亡队列
	FCombatEvent Event;
	Event.bLethal = true;
	Event.Instigator = nullptr;
	Event.HitLocation = GetActorLocation();
	QueueCombatEvent(Event);
	UE_LOG(LogTemp, Log, TEXT("[Combat] %s 调试注入致死伤害（异常注入标记）"), *GetName());
}

void AFighterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 重新 Possess 更新控制信息；数值与能力授予由幂等保护兜住
	InitAbilityActorInfo();
	InitializeFromDefinition();
}

void AFighterCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	InitAbilityActorInfo();
}

void AFighterCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	AddDefaultMappingContext();
}

void AFighterCharacter::InitAbilityActorInfo()
{
	if (AbilitySystem != nullptr && IsValid(AttributeSet))
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
}

void AFighterCharacter::InitializeFromDefinition()
{
	if (Definition == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("[%s] 未配置 FighterDefinition，跳过初始化"), *GetName());
		return;
	}

	if (StatsInitCount == 0)
	{
		ApplyDefinitionStats();
		GrantAbilities();
		++StatsInitCount;
		UE_LOG(LogTemplateCharacter, Log, TEXT("[%s] 首次按定义初始化完成（Health=%.0f/%.0f）"),
			*GetName(), GetFighterAttributeSet()->GetHealth(), GetFighterAttributeSet()->GetMaxHealth());
	}

	// 外观标识允许在配置变更后刷新，不属于数值初始化
	ApplyMarkerVisual();
}

void AFighterCharacter::ApplyDefinitionStats()
{
	if (AbilitySystem == nullptr || !IsValid(AttributeSet) || Definition == nullptr)
	{
		return;
	}

	const float MaxHealth = FMath::Max(Definition->MaxHealth, 1.f);
	const float Health = FMath::Clamp(FMath::Max(Definition->InitialHealth, 1.f), 1.f, MaxHealth);
	const float MaxAction = FMath::Max(Definition->MaxActionResource, 0.f);
	const float Action = FMath::Clamp(Definition->InitialActionResource, 0.f, MaxAction);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetMaxCursedEnergyAttribute(), Definition->MaxCursedEnergy);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetCursedEnergyAttribute(), FMath::Clamp(Definition->InitialCursedEnergy, 0.f, Definition->MaxCursedEnergy));
	const float MaxEnergy = FMath::Max(Definition->MaxEnergy, 0.f);
	const float Energy = FMath::Clamp(Definition->InitialEnergy, 0.f, MaxEnergy);

	// 集中初始化入口：仅初始化/训练重置允许直接写基础值
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetHealthAttribute(), Health);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetMaxActionResourceAttribute(), MaxAction);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetActionResourceAttribute(), Action);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetMaxEnergyAttribute(), MaxEnergy);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetEnergyAttribute(), Energy);
}

void AFighterCharacter::GrantAbilities()
{
	if (AbilitySystem == nullptr || Definition == nullptr)
	{
		return;
	}

 TArray<TSubclassOf<UGameplayAbility>> Abilities = Definition->GrantedAbilities;
#if !UE_BUILD_SHIPPING
 Abilities.AddUnique(UTrainingProbeAbility::StaticClass());
#endif
	if (Definition->MeleeAttackAbility != nullptr)
	{
		Abilities.AddUnique(Definition->MeleeAttackAbility);
	}
	if (Definition->DodgeAbility != nullptr)
	{
		Abilities.AddUnique(Definition->DodgeAbility);
	}
	if (Definition->StanceSwitchAbility != nullptr)
	{
		Abilities.AddUnique(Definition->StanceSwitchAbility);
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Abilities)
	{
		if (AbilityClass == nullptr || GrantedAbilityClasses.Contains(AbilityClass))
		{
			continue;
		}

		const FGameplayAbilitySpecHandle Handle =
			AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		if (Handle.IsValid())
		{
			GrantedAbilityClasses.Add(AbilityClass);
		}
	}
}

void AFighterCharacter::SetMarkerColor(const FLinearColor& Color)
{
	MarkerColorOverride = Color;
	ApplyMarkerVisual();
}

void AFighterCharacter::ApplyMarkerVisual()
{
	if (Definition == nullptr || GetMesh() == nullptr)
	{
		return;
	}

	UMaterialInterface* OverlayBase = Definition->MarkerOverlayMaterial.LoadSynchronous();
	if (OverlayBase == nullptr)
	{
		GetMesh()->SetOverlayMaterial(nullptr);
		return;
	}

	// 覆盖材质整体叠染，不依赖角色网格原始材质的参数名
	UMaterialInstanceDynamic* MarkerMID = UMaterialInstanceDynamic::Create(OverlayBase, this);
	MarkerMID->SetVectorParameterValue(TEXT("Tint"), MarkerColorOverride.Get(Definition->MarkerColor));
	GetMesh()->SetOverlayMaterial(MarkerMID);
}

void AFighterCharacter::AddDefaultMappingContext() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC == nullptr || DefaultMappingContext == nullptr)
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, MappingPriority);
		}
	}
}

void AFighterCharacter::ResetToInitialState()
{
	const bool bResumeRequests = CombatInput->AreRequestsEnabled();
	CombatInput->SetRequestsEnabled(false);
	// 训练重置（M2.6 顺序：停请求 → 取消能力 → 清临时 → 复位 → 恢复属性）：
	// 已授予能力不重复授予；本函数可从任意战斗状态安全重入
	CombatInput->InvalidateSession(FText::FromString(TEXT("训练重置")));
	CombatInput->ReleaseContinuousInputs();
	CombatHit->EndAttack();
	PendingCombatEvents.Reset();
	PendingSequence.Reset();
	PendingDodge.bPending = false;

	ClearReactionEffects();
	AbilitySystem->RemoveActiveGameplayEffect(DeathEffect);
	bDead = false;
	GetWorldTimerManager().ClearTimer(HitStunTimerHandle);
	GetWorldTimerManager().ClearTimer(KnockdownTimerHandle);
	GetWorldTimerManager().ClearTimer(ThrowPairTimerHandle);
	EndThrowPair(true);
	if (AbilitySystem != nullptr)
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(TAG_Ability_MeleeAttack);
		CancelTags.AddTag(TAG_Ability_Dodge);
		CancelTags.AddTag(TAG_Ability_StanceSwitch);
		AbilitySystem->CancelAbilities(&CancelTags);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_Dead);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_HitStun);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_GuardStun);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_KnockedDown);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_DodgeInvulnerable);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_DodgeRecovery);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_SuperArmor);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_Attacking);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_StanceSwitching);
		// 形态复位为近战
		Stance = EFighterStance::Melee;
		AbilitySystem->RemoveLooseGameplayTag(TAG_Stance_Ranged);
		AbilitySystem->SetLooseGameplayTagCount(TAG_Stance_Melee, 1);
	}

	// 训练重置的属性恢复同样走集中入口；位置与速度由本函数恢复
	if (Definition != nullptr)
	{
		ApplyDefinitionStats();
	}

	LastMoveInputDirection = FVector::ZeroVector;
	LastStanceSwitchTime = -1000;
	LastResourceSpendTime = -1000;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	RefreshMovementControl();
	StopAnimMontage();
	Targeting->ClearTarget();

	GetCharacterMovement()->StopMovementImmediately();
	TeleportTo(InitialTransform.GetLocation(), InitialTransform.Rotator());

	CombatInput->SetRequestsEnabled(bResumeRequests);
	UE_LOG(LogTemplateCharacter, Log, TEXT("[%s] 训练重置完成"), *GetName());
}

bool AFighterCharacter::CanAct() const
{
 return !IsDead() && !IsAttacking() && !IsThrowPaired() &&
 !HasCombatTag(TAG_State_HitStun) && !HasCombatTag(TAG_State_GuardStun) &&
 !HasCombatTag(TAG_State_KnockedDown) && !HasCombatTag(TAG_State_DodgeInvulnerable) &&
 !HasCombatTag(TAG_State_DodgeRecovery) && !HasCombatTag(TAG_State_StanceSwitching);
}

void AFighterCharacter::RefreshMovementControl()
{
 auto* Move = GetCharacterMovement();
 if (AbilitySystem)
 {
  if (IsGuarding() && !GuardEffect.IsValid()) GuardEffect = ApplyCombatState(TAG_State_Guarding, -1.f);
  else if (!IsGuarding() && GuardEffect.IsValid()) { AbilitySystem->RemoveActiveGameplayEffect(GuardEffect); GuardEffect.Invalidate(); }
 }
 const bool Locked = !CanAct();
 if (Locked && !bMovementLocked)
 {
  SavedMaxWalkSpeed = Move->MaxWalkSpeed;
  bSavedOrientToMovement = Move->bOrientRotationToMovement;
  Move->StopMovementImmediately();
  ConsumeMovementInputVector();
  Move->MaxWalkSpeed = 0.f;
  Move->bOrientRotationToMovement = false;
 }
 else if (!Locked && bMovementLocked)
 {
  Move->MaxWalkSpeed = SavedMaxWalkSpeed;
  Move->bOrientRotationToMovement = bSavedOrientToMovement;
  FTimerDelegate Recovered;
  Recovered.BindWeakLambda(this, [this]() { if (CanAct()) OnRecovered.Broadcast(); });
  GetWorldTimerManager().SetTimerForNextTick(Recovered);
 }
 bMovementLocked = Locked;
}

void AFighterCharacter::Jump()
{
 if (CanAct()) Super::Jump();
}

void AFighterCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
 EndThrowPair(true);
 if (AbilitySystem) AbilitySystem->CancelAllAbilities();
 GetWorldTimerManager().ClearAllTimersForObject(this);
 CombatInput->InvalidateSession(FText::FromString(TEXT("EndPlay")));
 CombatInput->ReleaseContinuousInputs();
 CombatHit->EndAttack();
 PendingCombatEvents.Reset();
 Super::EndPlay(Reason);
}

FActiveGameplayEffectHandle AFighterCharacter::ApplyCombatState(FGameplayTag Tag, float Duration)
{
 auto Spec = AbilitySystem->MakeOutgoingSpec(UCombatStateGameplayEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
 if (!Spec.IsValid()) return {};
 Spec.Data->DynamicGrantedTags.AddTag(Tag);
 Spec.Data->SetDuration(Duration, true);
 return AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}
void AFighterCharacter::ClearReactionEffects()
{
 if (!AbilitySystem) return;
 AbilitySystem->RemoveActiveGameplayEffect(StunEffect);
 AbilitySystem->RemoveActiveGameplayEffect(KnockdownEffect);
 AbilitySystem->RemoveActiveGameplayEffect(GetUpEffect);
 GetWorldTimerManager().ClearTimer(GetUpTimerHandle);
 GetWorldTimerManager().ClearTimer(HitStunTimerHandle);
 GetWorldTimerManager().ClearTimer(KnockdownTimerHandle);
}
void AFighterCharacter::RestoreCursedEnergyOnHit()
{
 if (!AbilitySystem || !Definition || IsDead()) return;
 auto Spec = AbilitySystem->MakeOutgoingSpec(URestoreCursedEnergyGameplayEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
 if (Spec.IsValid())
 {
  Spec.Data->SetSetByCallerMagnitude(TAG_Data_Amount, Definition->MeleeCursedEnergyGain);
  AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
 }
}

void AFighterCharacter::ApplyCombatDamage(AFighterCharacter* Target, float RawDamage, float ResolvedDamage, ETrainingContact Kind)
{
 if (!IsValid(Target) || !AbilitySystem || !Target->AbilitySystem || Target->IsDead()) return;
 auto* GM=GetWorld()->GetAuthGameMode<ATrainingGameMode>();
 const float Before=Target->AttributeSet->GetHealth();
 const float Resolved=FMath::Max(0.f,ResolvedDamage);
 const float Actual=(GM && GM->Settings.bInfiniteHealth) ? FMath::Min(Resolved,FMath::Max(0.f,Before-1.f)) : FMath::Min(Resolved,Before);
 auto Spec=AbilitySystem->MakeOutgoingSpec(UDamageGameplayEffect::StaticClass(),1,AbilitySystem->MakeEffectContext());
 if (Actual>0.f && Spec.IsValid()) { Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage,-Actual); AbilitySystem->ApplyGameplayEffectSpecToTarget(*Spec.Data,Target->AbilitySystem); }
 if (GM) GM->RecordContact(this,Target,Kind,RawDamage,Resolved,Before-Target->AttributeSet->GetHealth());
}
