#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "Training/CombatFeedbackTypes.h"
#include "CombatFeedbackProfile.generated.h"

/** Presentation only. B owns derived reaction assets; A assigns the fighter reference. */
UCLASS(BlueprintType)
class UCombatReactionLibrary : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimMontage> Light;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimMontage> Heavy;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimMontage> Guard;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimMontage> Left;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimMontage> Right;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimMontage> Back;
 UAnimMontage* Resolve(ECombatFeedbackTier Tier, bool bGuard, const FVector& LocalIncoming) const;
};

/** No damage, cost or cooldown fields. Hard references establish preload/Cook ownership. */
UCLASS(BlueprintType)
class UCombatFeedbackProfile : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float LightStop = .022f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float MediumStop = .042f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float FinisherStop = .060f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float HeavyStop = .090f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float GuardStop = .020f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float MobileStop = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float SuperStop = .035f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float DomainStop = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float ContinuousStopLimit = .100f;
 /** A sets this only after C's replacement consumer is bound. Prevents double impacts. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bExternalRangedImpact = false;
 /** Explicit package dependencies for configured consumer profiles and poses. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UObject>> ConsumerAssets;
 float StopFor(const FCombatContactFeedback& Contact) const;
};
