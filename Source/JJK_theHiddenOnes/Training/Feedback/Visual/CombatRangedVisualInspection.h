#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatRangedVisualInspection.generated.h"

class UParticleSystem;

/** Cascade diagnostics and editor-only preparation of project-owned derived cues. */
UCLASS()
class UCombatRangedVisualInspection : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Feedback|Visual|Debug", meta=(DevelopmentOnly))
	static TArray<FString> DescribeCascade(const UParticleSystem* Effect);
	/** Refuses vendor assets; prepares a bounded natural particle tail or parameter-driven beam fade. */
	UFUNCTION(BlueprintCallable, Category="Feedback|Visual|Debug", meta=(DevelopmentOnly))
	static bool PrepareNaturalFade(UParticleSystem* Effect, float ParticleLife, bool bBeam);
};
