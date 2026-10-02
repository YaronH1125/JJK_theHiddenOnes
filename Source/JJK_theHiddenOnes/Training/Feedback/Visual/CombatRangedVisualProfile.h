#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatRangedVisualProfile.generated.h"

class UParticleSystem;

/** A preloaded, bounded Cascade cue. It has no gameplay collision or damage. */
USTRUCT(BlueprintType)
struct FCombatRangedVisualCue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UParticleSystem> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01"))
	float Scale = 1.f;

	/** Stop spawning at this time; existing particles retain their authored fade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05", ForceUnits="s"))
	float MaxLife = .4f;
	/** Bounded tail after deactivation. Reset/abnormal ends still clear immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05", ClampMax="1.0", ForceUnits="s"))
	float FadeLife = .45f;
};

/** Project-owned references establish preload and Cook dependencies through the consumer class. */
UCLASS(BlueprintType)
class UCombatRangedVisualProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FCombatRangedVisualCue MobileHit;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FCombatRangedVisualCue SuperHit;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FCombatRangedVisualCue DomainHit;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FCombatRangedVisualCue Guard;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FCombatRangedVisualCue Immune;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FCombatRangedVisualCue WorldImpact;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FCombatRangedVisualCue Expire;
};
