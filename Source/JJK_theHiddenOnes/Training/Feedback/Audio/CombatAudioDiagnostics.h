#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatAudioDiagnostics.generated.h"

/** Read-only device diagnostics and a reversible Development-only recording fixture. */
UCLASS()
class UCombatAudioDiagnostics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Feedback|Audio|Debug") static float GetBackgroundVolume();
	UFUNCTION(BlueprintCallable, Category="Feedback|Audio|Debug", meta=(DevelopmentOnly)) static void SetRecordingBackgroundVolume(float Gain);
	UFUNCTION(BlueprintCallable, Category="Feedback|Audio|Debug", meta=(DevelopmentOnly)) static TArray<FString> DescribeAudioDevice(const UObject* WorldContext);
};
