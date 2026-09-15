// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FighterDefinition.generated.h"

class UGameplayAbility;
class UMaterialInterface;

/**
 * 最小角色定义（02_架构设计.md 第 9 节）：只保存配置。
 * M1 仅含初始属性与外观标识；资源引用、能力列表和技能槽按后续阶段补齐。
 * 运行状态（连招段、目标、计时器等）禁止存入本类。
 */
UCLASS(BlueprintType)
class UFighterDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 调试与训练面板显示用 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	/** 生命上限（>= 初始生命） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float MaxHealth = 1000.f;

	/** 初始生命 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float InitialHealth = 1000.f;

	/** 行动资源上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float MaxActionResource = 100.f;

	/** 初始行动资源 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float InitialActionResource = 100.f;

	/** 能量上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float MaxEnergy = 100.f;

	/** 初始能量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0"))
	float InitialEnergy = 0.f;

	/** P1/P2 身份标识颜色（覆盖材质参数名 Tint） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FLinearColor MarkerColor = FLinearColor::White;

	/** 用于叠加染色的基础材质；为空则不做颜色标识 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	TSoftObjectPtr<UMaterialInterface> MarkerOverlayMaterial;

	/** 初始授予能力；M1 保持为空，授予具备防重复保护 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;
};
