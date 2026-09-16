#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TrainingPanelWidget.generated.h"
class ATrainingGameMode;
class UTextBlock;
class UCheckBox;
class UComboBoxString;
class UButton;
class USpinBox;
class UVerticalBox;

/** UMG 控件树；显示直接读取 GameMode/ASC，更新由训练与属性通知触发。 */
UCLASS()
class UTrainingPanelWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Training") void Refresh();
 UFUNCTION(BlueprintPure, Category="Training|Debug") int32 GetRefreshCount() const { return RefreshCount; }
 UFUNCTION(BlueprintPure, Category="Training|Debug") FString GetDisplayedState() const;
 UFUNCTION(BlueprintPure, Category="Training|Debug") bool IsAIOptionDisabled() const;
protected:
 virtual void NativeOnInitialized() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
 TWeakObjectPtr<ATrainingGameMode> Mode;
 UPROPERTY() TObjectPtr<UTextBlock> Status;
 UPROPERTY() TObjectPtr<UTextBlock> History;
 UPROPERTY() TObjectPtr<UCheckBox> AutoHeal;
 UPROPERTY(BlueprintReadOnly, Category="Training|Controls", meta=(AllowPrivateAccess="true")) TObjectPtr<UCheckBox> InfiniteHealth;
 UPROPERTY(BlueprintReadOnly, Category="Training|Controls", meta=(AllowPrivateAccess="true")) TObjectPtr<UCheckBox> InfiniteResources;
 UPROPERTY(BlueprintReadOnly, Category="Training|Controls", meta=(AllowPrivateAccess="true")) TObjectPtr<UCheckBox> NoCooldown;
 UPROPERTY(BlueprintReadOnly, Category="Training|Controls", meta=(AllowPrivateAccess="true")) TObjectPtr<UComboBoxString> OpponentChoice;
 UPROPERTY() TObjectPtr<USpinBox> Delay;
 UPROPERTY() TObjectPtr<UButton> AIButton;
 bool bRefreshing=false;
 int32 RefreshCount=0;
 UFUNCTION() void SettingsChanged(bool bValue);
 UFUNCTION() void DelayChanged(float Value);
 UFUNCTION() void ModeChanged(FString Item,ESelectInfo::Type Type);
 UFUNCTION() void ResetClicked();
 UFUNCTION() void CloseClicked();
 UFUNCTION() void ProbeClicked();
};
