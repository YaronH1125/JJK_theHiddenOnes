#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "Training/CombatFeedbackTypes.h"
#include "CombatCameraFeedbackModifier.generated.h"

class AFighterCharacter;

/** One render-only roll envelope. Never writes control rotation, spring arm or aim direction. */
UCLASS()
class UCombatCameraFeedbackModifier : public UCameraModifier
{
 GENERATED_BODY()
public:
 UCombatCameraFeedbackModifier();
 virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
 void ConsumeContact(const FCombatContactFeedback& Event, AFighterCharacter* Player);
 void ClearPulse();
 void ClearForSource(AFighterCharacter* Source);
 static int32 GetStrength();
 static void SetStrength(int32 Value);
 UPROPERTY(BlueprintReadOnly, Category="Feedback|Camera") float AppliedRoll = 0.f;
 UPROPERTY(BlueprintReadOnly, Category="Feedback|Camera") float PeakRoll = 0.f;
 UPROPERTY(BlueprintReadOnly, Category="Feedback|Camera") float PulseSeconds = 0.f;
 UPROPERTY(BlueprintReadOnly, Category="Feedback|Camera") int32 PulsePriority = 0;
 UPROPERTY(BlueprintReadOnly, Category="Feedback|Camera") int32 StartedCount = 0;
 UPROPERTY(BlueprintReadOnly, Category="Feedback|Camera") int32 MergedCount = 0;
 UPROPERTY(BlueprintReadOnly, Category="Feedback|Camera") int32 DroppedCount = 0;
private:
 double PulseStart = -100., PulseEnd = -100.;
 double BurstStart = -100.;
 double LastStart = -100.;
 ECombatFeedbackTier LastTier = ECombatFeedbackTier::Light;
 ECombatFeedbackResult LastResult = ECombatFeedbackResult::None;
 int32 LastPriority = 0;
 TWeakObjectPtr<AFighterCharacter> PulseSource;
};
