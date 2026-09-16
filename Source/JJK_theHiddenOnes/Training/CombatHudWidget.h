#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHudWidget.generated.h"
class UProgressBar;
class UTextBlock;
class UBorder;

/** Development HUD reads authoritative ASC values; never owns combat state. */
UCLASS()
class UCombatHudWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Training|UI") void Refresh();
 UFUNCTION(BlueprintPure, Category="Training|UI") FString GetDisplayedState() const { return DisplayedState; }
protected:
 virtual void NativeOnInitialized() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
private:
 UPROPERTY() TArray<TObjectPtr<UProgressBar>> Bars;
 UPROPERTY() TArray<TObjectPtr<UTextBlock>> Values;
 UPROPERTY() TArray<TObjectPtr<UTextBlock>> FighterStates;
 UPROPERTY() TArray<TObjectPtr<UBorder>> Cards;
 UPROPERTY() TObjectPtr<UTextBlock> ModeLabel;
 FString DisplayedState;
 FTimerHandle RefreshTimer;
};
