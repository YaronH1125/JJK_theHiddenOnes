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
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterDefinition.h"
#include "Training/MeleeComboAbility.h"
#include "Training/TargetingComponent.h"

AFighterCharacter::AFighterCharacter()
{
	AbilitySystem = CreateDefaultSubobject<UFighterAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);

	AttributeSet = CreateDefaultSubobject<UFighterAttributeSet>(TEXT("AttributeSet"));

	Targeting = CreateDefaultSubobject<UTargetingComponent>(TEXT("Targeting"));

	CombatInput = CreateDefaultSubobject<UCombatInputComponent>(TEXT("CombatInput"));
	CombatHit = CreateDefaultSubobject<UCombatHitComponent>(TEXT("CombatHit"));

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
	}

	if (FighterRole != EFighterRole::Unassigned && InitialTransform.GetLocation().IsZero())
	{
		// GameMode 未记录时兜底保存当前变换
		InitialTransform = GetActorTransform();
	}
}

void AFighterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 受击/死亡事件在本角色自身 Tick 开头处理：攻击方本帧已完成的接触仍属同一批次
	ProcessCombatEvents();
}

bool AFighterCharacter::IsDead() const
{
	return bDead || (AbilitySystem != nullptr && AbilitySystem->HasMatchingGameplayTag(TAG_State_Dead));
}

TSubclassOf<UGameplayAbility> AFighterCharacter::GetMeleeAttackAbilityClass() const
{
	return Definition ? Definition->MeleeAttackAbility : TSubclassOf<UGameplayAbility>();
}

void AFighterCharacter::QueueCombatEvent(const FCombatEvent& Event)
{
	PendingCombatEvents.Push(Event);
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
		}
		else
		{
			ApplyHitReactNow(Event.StunDuration, Event.Instigator.Get(), Event.HitLocation);
		}
	}
}

void AFighterCharacter::ApplyHitReactNow(float StunDuration, AActor* InInstigator, const FVector& HitLocation)
{
	if (IsDead())
	{
		return;
	}

	LastHitLocation = HitLocation;
	LastHitInstigator = InInstigator;

	// State.HitStun：硬直期内拒绝新动作请求（请求入口与能力激活双重检查）
	if (AbilitySystem != nullptr)
	{
		AbilitySystem->AddLooseGameplayTag(TAG_State_HitStun);
	}
	if (GetWorld() != nullptr && StunDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(HitStunTimerHandle, this,
			&AFighterCharacter::RemoveHitStun, FMath::Max(StunDuration, 0.01f), false);
	}

	// 受击打断攻击：按能力标签取消活动攻击（GA EndAbility 关窗清状态）
	if (AbilitySystem != nullptr)
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(TAG_Ability_MeleeAttack);
		AbilitySystem->CancelAbilities(&CancelTags);
	}

	CombatInput->InvalidateSession(FText::FromString(TEXT("受击中断")));

	if (UAnimMontage* ReactMontage = Definition && Definition->AttackDefinition
		? Definition->AttackDefinition->HitReactMontage.LoadSynchronous()
		: nullptr)
	{
		PlayAnimMontage(ReactMontage);
	}

	UE_LOG(LogTemp, Log, TEXT("[Combat] %s 受击硬直 %.2fs（来源 %s）"),
		*GetName(), StunDuration, *GetNameSafe(InInstigator));
}

void AFighterCharacter::RemoveHitStun()
{
	if (AbilitySystem != nullptr)
	{
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_HitStun);
	}
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
		AbilitySystem->AddLooseGameplayTag(TAG_State_Dead);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_HitStun);
		GetWorldTimerManager().ClearTimer(HitStunTimerHandle);

		// 死亡取消一切战斗行为；等待调试重置（M2.4 最小死亡处理）
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(TAG_Ability_MeleeAttack);
		AbilitySystem->CancelAbilities(&CancelTags);
	}

	CombatInput->InvalidateSession(FText::FromString(TEXT("死亡")));
	CombatHit->EndAttack();
	GetCharacterMovement()->StopMovementImmediately();

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

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Definition->GrantedAbilities)
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
	// 训练重置（M2.6 顺序：停请求 → 取消能力 → 清临时 → 复位 → 恢复属性）：
	// 已授予能力不重复授予；本函数可从任意战斗状态安全重入
	CombatInput->InvalidateSession(FText::FromString(TEXT("训练重置")));
	CombatHit->EndAttack();
	PendingCombatEvents.Reset();

	bDead = false;
	GetWorldTimerManager().ClearTimer(HitStunTimerHandle);
	if (AbilitySystem != nullptr)
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(TAG_Ability_MeleeAttack);
		AbilitySystem->CancelAbilities(&CancelTags);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_Dead);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_HitStun);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_Attacking);
	}

	// 训练重置的属性恢复同样走集中入口；位置与速度由本函数恢复
	if (Definition != nullptr)
	{
		ApplyDefinitionStats();
	}

	Targeting->ClearTarget();

	GetCharacterMovement()->StopMovementImmediately();
	TeleportTo(InitialTransform.GetLocation(), InitialTransform.Rotator());

	UE_LOG(LogTemplateCharacter, Log, TEXT("[%s] 训练重置完成"), *GetName());
}
