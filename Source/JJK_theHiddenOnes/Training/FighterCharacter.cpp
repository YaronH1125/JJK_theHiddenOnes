// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/FighterCharacter.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);



#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
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
#include "Training/ChargedBlastAbility.h"
#include "Training/CombatInputComponent.h"
#include "Training/CombatTypes.h"
#include "Training/DamageGameplayEffect.h"
#include "Training/ModifyAttributeGameplayEffect.h"
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
 UpdateSprintMovement();
	if (CanAct() || CanMoveDuringDodgeRecovery()) Super::DoMove(Right, Forward);
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
		// 咒力消耗视为咒力流动活动：重启回咒延迟（M6；恢复/自然回复不重启）
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UFighterAttributeSet::GetCursedEnergyAttribute())
			.AddUObject(this, &AFighterCharacter::OnCursedEnergyChanged);
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
	// 近战软锁（异人之下式索敌）：出招瞬间面向目标（MeleeAutoFace 总开关）
	if (GetStance() == EFighterStance::Melee && Definition && Definition->MeleeAutoFace)
	{
		if (auto* Target = GetPreferredTargetFighter())
		{
			const float Range = Definition ? Definition->MeleeAutoFaceRange : 600.f;
			const FVector D = Target->GetActorLocation() - GetActorLocation();
			if (Target->IsDead() == false && D.Size2D() <= Range && D.Size2D() > 1.f)
			{
				SetActorRotation(FVector(D.X, D.Y, 0.f).GetSafeNormal().Rotation());
			}
		}
	}
	const bool Activated = AbilitySystem != nullptr && AbilitySystem->TryActivateAbilityByClass(GetMeleeAttackAbilityClass());
	if (!Activated) PendingSequence.Reset();
	return Activated;
}

EActionRequestResult AFighterCharacter::ValidateRangedRequest() const
{
	if (!CombatInput->AreRequestsEnabled()) return EActionRequestResult::RejectedBlocked;
	if (IsDead()) return EActionRequestResult::RejectedDead;
	if (!IsStatsInitialized() || AbilitySystem == nullptr) return EActionRequestResult::RejectedNotInitialized;
	if (GetStance() != EFighterStance::Ranged) return EActionRequestResult::RejectedBlocked;
	if (IsBlastCharging()) return EActionRequestResult::RejectedAlreadyActive;
	if (HasCombatTag(TAG_State_HitStun) || HasCombatTag(TAG_State_KnockedDown)
		|| HasCombatTag(TAG_State_StanceSwitching) || HasCombatTag(TAG_State_GuardStun)
		|| HasCombatTag(TAG_State_DodgeInvulnerable) || HasCombatTag(TAG_State_DodgeRecovery) || IsThrowPaired())
	{
		return EActionRequestResult::RejectedBlocked;
	}
	return EActionRequestResult::Executed;
}

bool AFighterCharacter::RequestBlast(ECachedAction Action)
{
	if (IsDomainActive())
	{
		// 领域期内禁用手动远程炮（08：结束需新按下恢复）
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 领域期内禁用手动炮"), *GetName());
		return false;
	}
	if (ValidateRangedRequest() != EActionRequestResult::Executed)
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 蓄力炮请求被拒（%d）"), *GetName(), static_cast<int32>(Action));
		return false;
	}

	const UFighterDefinition* Def = GetDefinition();
	TSubclassOf<UGameplayAbility> AbilityClass = nullptr;
	if (Action == ECachedAction::MobileBlast && Def != nullptr)
	{
		AbilityClass = Def->MobileBlastAbility;
	}
	else if (Action == ECachedAction::SuperBlast && Def != nullptr)
	{
		AbilityClass = Def->StationaryBlastAbility;
	}
	if (AbilityClass == nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 蓄力炮能力未配置（%d）"), *GetName(), static_cast<int32>(Action));
		return false;
	}

	// 冷却/咒力不足由 GA 自身激活规则拒绝（TryActivate 返回 false）
	const bool Activated = AbilitySystem->TryActivateAbilityByClass(AbilityClass);
	if (!Activated)
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 蓄力炮激活失败（冷却/资源不足）"), *GetName());
	}
	return Activated;
}

bool AFighterCharacter::RequestDomain()
{
	if (IsDead()) return false;
	if (!IsStatsInitialized() || AbilitySystem == nullptr) return false;
	if (IsDomainActive())
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 领域已开启，忽略重复请求"), *GetName());
		return false;
	}

	const UFighterDefinition* Def = GetDefinition();
	if (Def == nullptr || Def->DomainExpansionAbility == nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 领域能力未配置"), *GetName());
		return false;
	}

	const bool Activated = AbilitySystem->TryActivateAbilityByClass(Def->DomainExpansionAbility);
	if (!Activated)
	{
		UE_LOG(LogTemp, Log, TEXT("[Combat] %s 领域展开激活失败（能量不足/冷却）"), *GetName());
	}
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

float AFighterCharacter::GetActionResource() const
{
	return AttributeSet ? AttributeSet->GetActionResource() : 0.f;
}

bool AFighterCharacter::SpendActionResource(float Amount)
{
	return ModifyActionResource(-FMath::Abs(Amount));
}

bool AFighterCharacter::TrySpendActionResource(float Amount)
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
	if (!TrySpendActionResource(Cost))
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
	// A03：切形态先中止持炮（已扣不退，超级炮按中断进冷却，无免费满蓄留存）
	CancelActiveBlast();
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
	SetAimIntent(false); // 08：切形态清除瞄准，恢复需新按下
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
	SetAimIntent(false);
	RestoreAimCamera();

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
	if (Definition->MobileBlastAbility != nullptr)
	{
		Abilities.AddUnique(Definition->MobileBlastAbility);
	}
	if (Definition->StationaryBlastAbility != nullptr)
	{
		Abilities.AddUnique(Definition->StationaryBlastAbility);
	}
	if (Definition->DomainExpansionAbility != nullptr)
	{
		Abilities.AddUnique(Definition->DomainExpansionAbility);
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
	SetSprintHeld(false);
	SetAimIntent(false);
	RestoreAimCamera();
	// 重置后回咒延迟重新计时：无活动窗口内不发生自然回充（M3 精确数值口径）
	LastCurseFlowActivityTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	LastDodgeSuccessTime = -1000.;
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
 // Attacks, guarding, hit reactions and death require a fresh Shift press afterward.
 if (IsGuardIntent() || (!CanAct() && !HasCombatTag(TAG_State_DodgeInvulnerable) && !HasCombatTag(TAG_State_DodgeRecovery))) bSprintHeld=false;
 UpdateSprintMovement();
 if (AbilitySystem)
 {
  if (IsGuarding() && !GuardEffect.IsValid()) GuardEffect = ApplyCombatState(TAG_State_Guarding, -1.f);
  else if (!IsGuarding() && GuardEffect.IsValid()) { AbilitySystem->RemoveActiveGameplayEffect(GuardEffect); GuardEffect.Invalidate(); }
 }
 const bool Locked = !CanAct() && !CanMoveDuringDodgeRecovery();
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

bool AFighterCharacter::CanMoveDuringDodgeRecovery() const
{
 return bSprintHeld && !LastMoveInputDirection.IsNearlyZero() && HasCombatTag(TAG_State_DodgeRecovery)
  && !HasCombatTag(TAG_State_DodgeInvulnerable) && !IsDead() && !IsAttacking() && !IsThrowPaired()
  && !HasCombatTag(TAG_State_HitStun) && !HasCombatTag(TAG_State_GuardStun)
  && !HasCombatTag(TAG_State_KnockedDown) && !HasCombatTag(TAG_State_StanceSwitching)
  && !IsGuardIntent() && CombatInput->AreRequestsEnabled();
}

void AFighterCharacter::SetSprintHeld(bool bHeld)
{
 bSprintHeld=bHeld;
 RefreshMovementControl();
}

void AFighterCharacter::UpdateSprintMovement()
{
 auto* Move=GetCharacterMovement();
 const bool bWanted=bSprintHeld && IsPlayerControlled() && !LastMoveInputDirection.IsNearlyZero()
  && Move->IsMovingOnGround() && CombatInput->AreRequestsEnabled() && !IsGuardIntent()
  && (CanAct() || CanMoveDuringDodgeRecovery());
 if(bWanted==bSprinting) return;
 // While combat owns the movement lock, update the speed to restore, never unlock it here.
 float& Speed=bMovementLocked ? SavedMaxWalkSpeed : Move->MaxWalkSpeed;
 if(bWanted) { PreSprintMaxWalkSpeed=Speed; Speed*=Definition ? Definition->SprintSpeedMultiplier : 1.5f; }
 else Speed=PreSprintMaxWalkSpeed;
 bSprinting=bWanted;
}

void AFighterCharacter::NotifyDodgeAvoided()
{
 if(HasCombatTag(TAG_State_DodgeInvulnerable)) LastDodgeSuccessTime=GetWorld()->GetTimeSeconds();
}

bool AFighterCharacter::HasRecentDodgeSuccess() const
{
 return GetWorld() && GetWorld()->GetTimeSeconds()-LastDodgeSuccessTime<.8;
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

// ---------- M6 蓄力炮/领域/瞄准 ----------

float AFighterCharacter::GetCursedEnergy() const
{
	return AttributeSet ? AttributeSet->GetCursedEnergy() : 0.f;
}

bool AFighterCharacter::ModifyCursedEnergy(float SignedAmount)
{
	if (!AbilitySystem) return false;
	if (SignedAmount < 0.f && AbilitySystem->HasInfiniteResources()) return true;
	const float Current = AttributeSet ? AttributeSet->GetCursedEnergy() : 0.f;
	if (SignedAmount < 0.f && Current + SignedAmount < -0.01f) return false;
	auto Spec = AbilitySystem->MakeOutgoingSpec(UModifyCursedEnergyGameplayEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid()) return false;
	Spec.Data->SetSetByCallerMagnitude(TAG_Data_Amount, SignedAmount);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	return true;
}

void AFighterCharacter::GainCursedEnergy(float Amount)
{
	ModifyCursedEnergy(FMath::Abs(Amount));
}

bool AFighterCharacter::IsBlastCharging() const
{
	return ActiveBlast.IsValid() && ActiveBlast->IsCharging();
}

float AFighterCharacter::GetBlastChargeAlpha() const
{
	return ActiveBlast.IsValid() ? ActiveBlast->GetPaidQ() : 0.f;
}

void AFighterCharacter::CancelActiveBlast()
{
	if (ActiveBlast.IsValid()) ActiveBlast->CancelFromOutside();
}

void AFighterCharacter::RegisterActiveBlast(UChargedBlastAbilityBase* Blast)
{
	ActiveBlast = Blast;
}

void AFighterCharacter::NotifyBlastRelease()
{
	if (ActiveBlast.IsValid()) ActiveBlast->NotifyExternalRelease();
}

void AFighterCharacter::NotifyBlastEnded(UChargedBlastAbilityBase* Blast)
{
	if (ActiveBlast == Blast) ActiveBlast = nullptr;
	LastCurseFlowActivityTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void AFighterCharacter::NotifyCurseFlowActivity()
{
	LastCurseFlowActivityTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void AFighterCharacter::OnCursedEnergyChanged(const FOnAttributeChangeData& Data)
{
	// 仅消耗（含 GE 成本路径）重启回咒延迟；恢复/自然回复不重启
	if (Data.NewValue < Data.OldValue)
	{
		NotifyCurseFlowActivity();
	}
}

void AFighterCharacter::SetDomainActive(bool bActive)
{
	bDomainActive = bActive;
	if (AbilitySystem)
	{
		if (bActive) AbilitySystem->AddLooseGameplayTag(TAG_State_DomainActive);
		else AbilitySystem->RemoveLooseGameplayTag(TAG_State_DomainActive);
	}
}

ETrainingContact AFighterCharacter::SettleRangedHitOn(AFighterCharacter* Target, const FRangedHitSettle& Settle)
{
	// 保护：死亡/倒地/被投不进入普通结算（与近战口径一致）
	if (!IsValid(Target) || Target == this || Target->IsDead() || Target->IsThrowPaired()
		|| Target->HasCombatTag(TAG_State_KnockedDown))
	{
		return ETrainingContact::Whiff;
	}
	auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;

	// 闪避无敌窗（仅可闪避攻击；领域球不可闪避）
	if (Settle.bDodgeable && Target->HasCombatTag(TAG_State_DodgeInvulnerable))
	{
		Target->NotifyDodgeAvoided();
		if (GM) GM->RecordContact(this, Target, ETrainingContact::Immune, Settle.Damage, 0.f, 0.f);
		UE_LOG(LogTemp, Log, TEXT("[RangedHit] %s 的远程命中被 %s 闪避免疫"), *GetNameSafe(this), *GetNameSafe(Target));
		return ETrainingContact::Immune;
	}

	// 正面防御：chip 伤 + 防御硬直；背面防御不生效
	const bool bFront = Target->IsAttackFromFront(this, Target->GetGuardFrontArcHalfAngle());
	const bool bGuarded = Target->IsGuarding() && Settle.bBlockable && bFront;
	if (bGuarded)
	{
		const FGuardConfig& Guard = Target->GetDefinition()->GuardConfig;
		ApplyCombatDamage(Target, Settle.Damage, Guard.bChipDamage ? Settle.Damage * Guard.ChipDamageRatio : 0.f, ETrainingContact::Guard);
		FCombatEvent GuardEvent;
		GuardEvent.Type = FCombatEvent::EType::GuardStun;
		GuardEvent.Instigator = this;
		GuardEvent.StunDuration = Settle.GuardStunDuration;
		GuardEvent.HitLocation = Target->GetActorLocation();
		Target->QueueCombatEvent(GuardEvent);
		UE_LOG(LogTemp, Log, TEXT("[RangedHit] %s 的远程命中被 %s 防御"), *GetNameSafe(this), *GetNameSafe(Target));
		return ETrainingContact::Guard;
	}

	// 普通命中：伤害 + 受击反应（致死入受击方延迟队列）
	if (Settle.bGrantCurse) RestoreCursedEnergyOnHit();
	ApplyCombatDamage(Target, Settle.Damage, Settle.Damage, ETrainingContact::Hit);
	// 非领域期有效结算伤害 5% 转领域能量（领域期由 GainDomainEnergy 拒绝）
	GainDomainEnergy(Settle.Damage, Settle.AttackInstanceId);
	const bool bLethal = Target->GetFighterAttributeSet()->GetHealth() <= 0.f;
	FCombatEvent Event;
	Event.Type = FCombatEvent::EType::HitReact;
	Event.Instigator = this;
	Event.InterruptLevel = Settle.InterruptLevel;
	Event.StunDuration = Settle.HitStunDuration;
	Event.bLethal = bLethal;
	Event.HitLocation = Target->GetActorLocation();
	Event.KnockbackStrength = Settle.KnockbackStrength;
	Event.KnockbackDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	Target->QueueCombatEvent(Event);
	UE_LOG(LogTemp, Log, TEXT("[RangedHit] %s 远程命中 %s（伤害 %.0f，致死=%d）"),
		*GetNameSafe(this), *GetNameSafe(Target), Settle.Damage, bLethal ? 1 : 0);
	return ETrainingContact::Hit;
}

void AFighterCharacter::GainDomainEnergy(float ResolvedDamage, uint64 AttackInstanceId)
{
	if (!Definition || !AbilitySystem) return;
	// 08 §182：领域期间暂停领域能量获取，不允许领域自己充满下一次领域
	if (IsDomainActive()) return;
	// 同一攻击实例只结算一次（每实例封顶由 Cap 保证）
	if (AttackInstanceId != 0)
	{
		if (LastGainInstanceId == AttackInstanceId) return;
		LastGainInstanceId = AttackInstanceId;
	}
	const float Gain = FMath::Min(ResolvedDamage * Definition->ResourceFlow.DomainEnergyGainRatio,
		Definition->ResourceFlow.DomainEnergyGainPerInstanceCap);
	if (Gain <= 0.f) return;
	ModifyEnergy(Gain);
}

FName AFighterCharacter::GetMuzzleSocketName() const
{
	return MuzzleSocket;
}

void AFighterCharacter::SetAimIntent(bool bNewAiming)
{
	if (bAimIntent == bNewAiming) return;
	bAimIntent = bNewAiming;
	UE_LOG(LogTemp, Log, TEXT("[Aim] %s 右键瞄准意图=%s（需远程形态生效）"), *GetName(), bNewAiming ? TEXT("开") : TEXT("关"));
}

bool AFighterCharacter::IsAimingEffective() const
{
	// 瞄准只是镜头状态（08 §127）：近战/死亡/倒地/闪避期/请求关闭均不生效；切形态清除意图需新按下
	return bAimIntent && GetStance() == EFighterStance::Ranged && !IsDead()
		&& !HasCombatTag(TAG_State_KnockedDown)
		&& !HasCombatTag(TAG_State_DodgeInvulnerable) && !HasCombatTag(TAG_State_DodgeRecovery)
		&& CombatInput && CombatInput->AreRequestsEnabled();
}

void AFighterCharacter::RestoreAimCamera()
{
	bAimIntent = false;
	bAiming = false;
	bUseControllerRotationYaw = false;
	if (Definition)
	{
		if (GetCameraBoom())
		{
			GetCameraBoom()->TargetArmLength = Definition->NormalArmLength;
			GetCameraBoom()->SocketOffset = FVector(0.f, Definition->NormalSocketOffsetY, Definition->NormalSocketOffsetZ);
		}
		if (auto* Cam = GetFollowCamera())
			Cam->SetFieldOfView(Definition->NormalFOV);
	}
}

AFighterCharacter* AFighterCharacter::GetPreferredTargetFighter() const
{
	auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
	return GM ? GM->GetOpponentOf(this) : nullptr;
}


bool AFighterCharacter::ModifyEnergy(float SignedAmount)
{
	if (!AbilitySystem) return false;
	if (SignedAmount < 0.f && AbilitySystem->HasInfiniteResources()) return true;
	const float Current = AttributeSet ? AttributeSet->GetEnergy() : 0.f;
	if (SignedAmount < 0.f && Current + SignedAmount < -0.01f) return false;
	auto Spec = AbilitySystem->MakeOutgoingSpec(UModifyDomainEnergyGameplayEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(TAG_Data_Amount, SignedAmount);
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		return true;
	}
	return false;
}

void AFighterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ProcessCombatEvents();
	TickAimCamera(DeltaSeconds);
	TickMeleeSoftLock(DeltaSeconds);
	TickCurseRegen(DeltaSeconds);
	TickThrowPair();
}

void AFighterCharacter::TickMeleeSoftLock(float DeltaSeconds)
{
	// 攻击中且目标在索敌范围 → 持续转向目标（异人之下式柔性锁定）
	if (GetStance() != EFighterStance::Melee || !IsAttacking() || !Definition || !Definition->MeleeAutoFace) return;
	auto* Target = GetPreferredTargetFighter();
	if (!Target || Target->IsDead()) return;
	const FVector D = Target->GetActorLocation() - GetActorLocation();
	const float Dist = D.Size2D();
	if (Dist > Definition->MeleeAutoFaceRange || Dist < 1.f) return;
	const float TargetYaw = FVector(D.X, D.Y, 0.f).GetSafeNormal().Rotation().Yaw;
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, TargetYaw, 0.f), DeltaSeconds, Definition->MeleeFaceInterpSpeed));
}

void AFighterCharacter::TickAimCamera(float DeltaSeconds)
{
	if (!Definition || !GetCameraBoom()) return;
	const bool bShouldAim = IsAimingEffective();
	if (bShouldAim != bAiming)
	{
		bAiming = bShouldAim;
		UE_LOG(LogTemp, Log, TEXT("[Aim] %s 有效瞄准=%s"), *GetName(), bAiming ? TEXT("开") : TEXT("关"));
	}
	auto* Boom = GetCameraBoom();
	const float Speed = Definition->AimInterpSpeed;
	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength,
		bAiming ? Definition->AimArmLength : Definition->NormalArmLength, DeltaSeconds, Speed);
	FVector Offset = Boom->SocketOffset;
	// TPS 惯例：待机即常驻右肩偏移（人物左侧），瞄准收紧到贴肩
	Offset.Y = FMath::FInterpTo(Offset.Y, bAiming ? Definition->AimSocketOffsetY : Definition->NormalSocketOffsetY, DeltaSeconds, Speed);
	Offset.Z = FMath::FInterpTo(Offset.Z, bAiming ? Definition->AimSocketOffsetZ : Definition->NormalSocketOffsetZ, DeltaSeconds, Speed);
	Boom->SocketOffset = Offset;
	// 瞄准收窄 FOV（PUBG/COD ADS 观感）
	if (auto* Cam = GetFollowCamera())
	{
		Cam->SetFieldOfView(FMath::FInterpTo(Cam->FieldOfView,
			bAiming ? Definition->AimFOV : Definition->NormalFOV, DeltaSeconds, Speed));
	}
	bUseControllerRotationYaw = bAiming;
}

void AFighterCharacter::TickCurseRegen(float DeltaSeconds)
{
	if (!AbilitySystem || !AttributeSet || IsDead()) return;
	if (ActiveBlast.IsValid()) return; // 炮击全生命周期（蓄力/前摇/恢复）暂停回咒（08：持蓄不回咒）
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - LastCurseFlowActivityTime < (Definition ? Definition->ResourceFlow.CurseRegenDelay : 2.0)) return;
	const float Curse = AttributeSet->GetCursedEnergy();
	const float MaxCurse = AttributeSet->GetMaxCursedEnergy();
	if (Curse < MaxCurse)
	{
		ModifyCursedEnergy(FMath::Min(Definition ? Definition->ResourceFlow.CurseRegenPerSecond : 6.f, MaxCurse - Curse) * DeltaSeconds);
	}
}

void UChargedBlastAbilityBase::ClearTimers()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ChargeTickHandle);
		GetWorld()->GetTimerManager().ClearTimer(PhaseTimerHandle);
		// 冷却计时器须在能力结束后继续走完（08：发射/中断后完整冷却），不在此清除
	}
}

double UChargedBlastAbilityBase::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}
