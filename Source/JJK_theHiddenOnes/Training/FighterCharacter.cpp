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
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterDefinition.h"
#include "Training/TargetingComponent.h"

AFighterCharacter::AFighterCharacter()
{
	AbilitySystem = CreateDefaultSubobject<UFighterAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);

	AttributeSet = CreateDefaultSubobject<UFighterAttributeSet>(TEXT("AttributeSet"));

	Targeting = CreateDefaultSubobject<UTargetingComponent>(TEXT("Targeting"));
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

	if (FighterRole != EFighterRole::Unassigned && InitialTransform.GetLocation().IsZero())
	{
		// GameMode 未记录时兜底保存当前变换
		InitialTransform = GetActorTransform();
	}
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
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetHealthAttribute(), Health);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetActionResourceAttribute(), Action);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetMaxActionResourceAttribute(), MaxAction);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetEnergyAttribute(), Energy);
	AbilitySystem->SetNumericAttributeBase(UFighterAttributeSet::GetMaxEnergyAttribute(), MaxEnergy);
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

void AFighterCharacter::ApplyMarkerVisual()
{
	if (Definition == nullptr || GetMesh() == nullptr)
	{
		return;
	}

	UMaterialInterface* OverlayBase = Definition->MarkerOverlayMaterial.LoadSynchronous();
	if (OverlayBase == nullptr)
	{
		return;
	}

	UMaterialInstanceDynamic* MarkerMID = GetMesh()->CreateAndSetMaterialInstanceDynamicFromMaterial(0, OverlayBase);
	if (MarkerMID != nullptr)
	{
		MarkerMID->SetVectorParameterValue(TEXT("Tint"), Definition->MarkerColor);
	}
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
