#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaMenuWidget.generated.h"

class SWebBrowserView;
class FJsonObject;
class USoundClass;
class USoundMix;

/** The approved menu artwork, rendered locally; all gameplay is owned by Unreal. */
UCLASS()
class UArenaMenuWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UArenaMenuWidget(const FObjectInitializer& ObjectInitializer);
 virtual void ReleaseSlateResources(bool bReleaseChildren) override;
 void ShowPage(const FString& Page, const FString& Tab = TEXT("graphics"));
 void FocusBrowser();
 void NavigateBack();
 void CaptureScene();
 UFUNCTION(BlueprintCallable, Category="Menu") FString Command(const FString& Payload);
 UFUNCTION(BlueprintCallable, Category="Menu|Debug", meta=(DevelopmentOnly)) void DebugJavascript(const FString& Script);
 UFUNCTION(BlueprintCallable, Category="Menu|Debug", meta=(DevelopmentOnly)) bool DebugCaptureScreenshot(const FString& Filename);
 UFUNCTION(BlueprintPure, Category="Menu") bool IsReady() const { return bReady; }
 UFUNCTION(BlueprintPure, Category="Menu") FString GetCurrentPage() const { return CurrentPage; }
 UFUNCTION(BlueprintPure, Category="Menu|Debug") FString GetLastProbe() const { return LastProbe; }
 float GetLookSensitivity() const;
 bool IsLookInverted() const;

protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
 virtual void NativeDestruct() override;

private:
 TSharedPtr<SWebBrowserView> Browser;
 TSharedPtr<FJsonObject> SavedSettings;
 FString CurrentPage = TEXT("main"), PendingTab = TEXT("graphics"), LastProbe;
 bool bReady = false;
 UPROPERTY() TObjectPtr<USoundClass> MusicSoundClass;
 UPROPERTY() TObjectPtr<USoundMix> MusicMix;
 bool bMusicMixPushed = false;
 void LoadSettings();
 void ApplySettings(const TSharedPtr<FJsonObject>& Values, bool bGraphics, bool bPersist);
 void RestoreBrightness();
 void Loaded();
 FString SettingsJson() const;
 void SynchronizeSettings();
};
