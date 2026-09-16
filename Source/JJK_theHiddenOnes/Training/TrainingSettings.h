#pragma once
#include "CoreMinimal.h"
#include "TrainingSettings.generated.h"

USTRUCT(BlueprintType)
struct FTrainingSettings
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAutoRecoverHealth = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInfiniteHealth = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInfiniteResources = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bNoCooldown = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="30")) float RecoveryDelay = 2.f;
};

UENUM(BlueprintType)
enum class ETrainingContact : uint8 { Hit, Guard, Immune, Whiff, Throw };

/** 按攻击方累计；结算伤害含溢出与削血，实际扣血受剩余生命和训练开关限制。 */
USTRUCT(BlueprintType)
struct FTrainingStats
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) float RawDamage = 0.f;
 UPROPERTY(BlueprintReadOnly) float ResolvedDamage = 0.f;
 UPROPERTY(BlueprintReadOnly) float HealthLost = 0.f;
 UPROPERTY(BlueprintReadOnly) int32 Hits = 0;
 UPROPERTY(BlueprintReadOnly) int32 Guards = 0;
 UPROPERTY(BlueprintReadOnly) int32 Immunes = 0;
 UPROPERTY(BlueprintReadOnly) int32 Whiffs = 0;
 UPROPERTY(BlueprintReadOnly) int32 ComboHits = 0;
 UPROPERTY(BlueprintReadOnly) float ComboDamage = 0.f;
 UPROPERTY(BlueprintReadOnly) int32 LastComboHits = 0;
 UPROPERTY(BlueprintReadOnly) float LastComboDamage = 0.f;
};
