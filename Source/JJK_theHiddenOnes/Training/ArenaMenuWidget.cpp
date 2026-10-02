#include "Training/ArenaMenuWidget.h"
#include "Training/ArenaPlayerController.h"
#include "Training/TrainingGameMode.h"
#include "Training/Feedback/Camera/CombatCameraFeedbackModifier.h"
#include "SWebBrowserView.h"
#include "WebBrowserModule.h"
#include "Framework/Application/SlateApplication.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Json.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"
#include "Widgets/Text/STextBlock.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/ConstructorHelpers.h"

namespace ArenaMenu
{
 FString Serialize(const TSharedPtr<FJsonObject>& Value)
 {
  FString Result;
  const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
  FJsonSerializer::Serialize(Value.ToSharedRef(), Writer);
  return Result;
 }
 FString SettingsFile() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Menu/UserSettings.json")); }
 double Number(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default, double Min, double Max)
 {
  double N = Default;
  if (O) O->TryGetNumberField(Key, N);
  return FMath::IsFinite(N) ? FMath::Clamp(N, Min, Max) : Default;
 }
 bool Boolean(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, bool Default)
 {
  bool B = Default;
  if (O) O->TryGetBoolField(Key, B);
  return B;
 }
 FString Choice(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FString& Default, const TArray<FString>& Options)
 {
  FString S;
  return O && O->TryGetStringField(Key, S) && Options.Contains(S) ? S : Default;
 }
 EOpponentMode Opponent(const FString& S)
 {
  return S == TEXT("AI 对战") ? EOpponentMode::AI : S == TEXT("固定防御") ? EOpponentMode::FixedGuard : EOpponentMode::Static;
 }
 FString OpponentName(EOpponentMode M)
 {
  return M == EOpponentMode::AI ? TEXT("AI 对战") : M == EOpponentMode::FixedGuard ? TEXT("固定防御") : TEXT("静止木桩");
 }
}

UArenaMenuWidget::UArenaMenuWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
 SetIsFocusable(true);
 static ConstructorHelpers::FObjectFinder<USoundClass> Music(TEXT("/Engine/EngineSounds/Music.Music"));
 if (Music.Succeeded()) MusicSoundClass=Music.Object;
}

void UArenaMenuWidget::NativeDestruct()
{
 if (bMusicMixPushed && MusicMix) UGameplayStatics::PopSoundMixModifier(this,MusicMix);
 bMusicMixPushed=false;
 Super::NativeDestruct();
}

void UArenaMenuWidget::LoadSettings()
{
 SavedSettings = MakeShared<FJsonObject>();
 // Read the real display configuration; opening a menu must never change resolution.
 const auto* U = GEngine ? GEngine->GetGameUserSettings() : nullptr;
 SavedSettings->SetStringField(TEXT("display"), !U || U->GetFullscreenMode() == EWindowMode::WindowedFullscreen ? TEXT("无边框窗口") : U->GetFullscreenMode() == EWindowMode::Fullscreen ? TEXT("全屏") : TEXT("窗口"));
 const FIntPoint R = U ? U->GetScreenResolution() : FIntPoint(1280,720);
 SavedSettings->SetStringField(TEXT("resolution"), FString::Printf(TEXT("%d × %d"), R.X, R.Y));
 const TCHAR* Qualities[] = {TEXT("低"),TEXT("中"),TEXT("高"),TEXT("极高")};
 SavedSettings->SetStringField(TEXT("quality"), Qualities[FMath::Clamp(U ? U->GetOverallScalabilityLevel() : 2,0,3)]);
 SavedSettings->SetBoolField(TEXT("vsync"), U && U->IsVSyncEnabled());
 const float Fps = U ? U->GetFrameRateLimit() : 60;
 SavedSettings->SetStringField(TEXT("fps"), Fps <= 0 ? TEXT("无限制") : FString::FromInt(FMath::RoundToInt(Fps)));
 SavedSettings->SetNumberField(TEXT("brightness"),100);
 SavedSettings->SetNumberField(TEXT("master"),75);
 SavedSettings->SetNumberField(TEXT("music"),55);
 SavedSettings->SetNumberField(TEXT("sfx"),80);
 SavedSettings->SetNumberField(TEXT("sensitivity"),50);
 SavedSettings->SetBoolField(TEXT("invert"),false);
 SavedSettings->SetBoolField(TEXT("shake"),true);
 FTrainingSettings T;
 if (const auto* PC = Cast<AArenaPlayerController>(GetOwningPlayer()))
  if (const auto* GM = PC->GetTrainingGameMode()) T = GM->Settings;
 SavedSettings->SetBoolField(TEXT("autoHeal"),T.bAutoRecoverHealth);
 SavedSettings->SetBoolField(TEXT("infiniteHealth"),T.bInfiniteHealth);
 SavedSettings->SetBoolField(TEXT("infiniteEnergy"),T.bInfiniteResources);
 SavedSettings->SetBoolField(TEXT("noCooldown"),T.bNoCooldown);
 SavedSettings->SetNumberField(TEXT("healDelay"),T.RecoveryDelay);
 SavedSettings->SetStringField(TEXT("opponent"),TEXT("静止木桩"));
 FString Json;
 TSharedPtr<FJsonObject> Existing;
 if (FFileHelper::LoadFileToString(Json,*ArenaMenu::SettingsFile()) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Existing) && Existing)
  ApplySettings(Existing, false, false);
 else ApplySettings(SavedSettings, false, false);
}

TSharedRef<SWidget> UArenaMenuWidget::RebuildWidget()
{
 LoadSettings();
 FString Html;
 const FString Path = FPaths::Combine(FPaths::ProjectContentDir(),TEXT("UI/Menu/menu-ui.html"));
 if (!FFileHelper::LoadFileToString(Html,*Path))
 {
  UE_LOG(LogTemp, Error, TEXT("[ArenaMenu] Missing staged menu: %s"), *Path);
  return SNew(STextBlock).Text(FText::FromString(TEXT("菜单资源未加载，请检查 Content/UI/Menu/menu-ui.html")));
 }
 // A direct Slate browser does not have WebBrowserWidget's module startup hook.
 // Load the runtime explicitly before SWebBrowserView checks IsAvailable().
 IWebBrowserModule::Get();
 SAssignNew(Browser,SWebBrowserView)
  .InitialURL(TEXT("file:///jjk-menu/menu-ui.html"))
  .ContentsToLoad(Html)
  .SupportsThumbMouseButtonNavigation(false)
  .OnSuppressContextMenu_Lambda([](){ return true; })
  .BackgroundColor(FColor::Black).BrowserFrameRate(60)
  .OnLoadError_Lambda([](){ UE_LOG(LogTemp,Error,TEXT("[ArenaMenu] Local menu failed to load")); })
  .OnLoadCompleted(FSimpleDelegate::CreateUObject(this,&UArenaMenuWidget::Loaded));
 Browser->BindUObject(TEXT("menu"),this,true);
 return Browser.ToSharedRef();
}

void UArenaMenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
 if (Browser) Browser->UnbindUObject(TEXT("menu"),this,true);
 Browser.Reset(); bReady=false;
 Super::ReleaseSlateResources(bReleaseChildren);
}

FString UArenaMenuWidget::SettingsJson() const { return SavedSettings ? ArenaMenu::Serialize(SavedSettings) : TEXT("{}"); }

void UArenaMenuWidget::Loaded()
{
 UE_LOG(LogTemp,Log,TEXT("[ArenaMenu] Document loaded; initializing game bridge"));
 if (Browser) Browser->ExecuteJavascript(TEXT("window.menuUE && window.menuUE.boot(") + SettingsJson() + TEXT(");"));
}

void UArenaMenuWidget::FocusBrowser()
{
 if (Browser && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Browser,EFocusCause::SetDirectly);
}

void UArenaMenuWidget::NavigateBack()
{
 if (Browser) Browser->ExecuteJavascript(TEXT("window.menuUE && window.menuUE.back();"));
}

void UArenaMenuWidget::CaptureScene()
{
 auto* Client=GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
 if (!bReady || !Browser || !Client || !Client->Viewport) return;
 const FIntPoint Size=Client->Viewport->GetSizeXY();
 TArray<FColor> Pixels;
 if (Size.X<1 || Size.Y<1 || !Client->Viewport->ReadPixels(Pixels)) return;
 for (auto& Pixel : Pixels) Pixel.A=255;
 TArray64<uint8> Png;
 FImageUtils::PNGCompressImageArray(Size.X,Size.Y,MakeArrayView(Pixels),Png);
 if (Png.Num()>0 && Png.Num()<16*1024*1024)
 {
  const FString Uri=TEXT("data:image/png;base64,")+FBase64::Encode(Png.GetData(),int32(Png.Num()));
  Browser->ExecuteJavascript(TEXT("window.menuUE.scene('")+Uri+TEXT("');"));
 }
}

void UArenaMenuWidget::ShowPage(const FString& Page,const FString& Tab)
{
 CurrentPage=Page; PendingTab=Tab;
 SetVisibility(ESlateVisibility::Visible);
 if (bReady && Browser)
 {
  FString Opponent=TEXT("静止木桩");
  if (auto* PC=Cast<AArenaPlayerController>(GetOwningPlayer()))
   if (auto* GM=PC->GetTrainingGameMode()) Opponent=ArenaMenu::OpponentName(GM->OpponentMode);
  const FString Script=FString::Printf(TEXT("window.menuUE.open('%s','%s','%s');"),*Page,*Tab,*Opponent);
  Browser->ExecuteJavascript(Script);
 }
}

FReply UArenaMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
 if (Event.GetKey()==EKeys::Escape)
 {
  NavigateBack();
  return FReply::Handled();
 }
 return Super::NativeOnPreviewKeyDown(Geometry,Event);
}

void UArenaMenuWidget::DebugJavascript(const FString& Script)
{
#if !UE_BUILD_SHIPPING
 if (Browser) Browser->ExecuteJavascript(Script);
#endif
}

bool UArenaMenuWidget::DebugCaptureScreenshot(const FString& Filename)
{
#if !UE_BUILD_SHIPPING
 auto* Client=GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
 const auto Window=Client ? Client->GetWindow() : nullptr;
 if (!Window) return false;
 TArray<FColor> Pixels;
 FIntVector Size;
 if (!FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)) return false;
 TArray64<uint8> Png;
 FImageUtils::PNGCompressImageArray(Size.X,Size.Y,MakeArrayView(Pixels),Png);
 return FFileHelper::SaveArrayToFile(Png,*Filename);
#else
 return false;
#endif
}

void UArenaMenuWidget::SynchronizeSettings()
{
 if (Browser) Browser->ExecuteJavascript(TEXT("window.menuUE.sync(")+SettingsJson()+TEXT(");"));
}

void UArenaMenuWidget::RestoreBrightness()
{
 if (auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.TonemapperGamma")))
  C->Set(float(2.2*SavedSettings->GetNumberField(TEXT("brightness"))/100.),ECVF_SetByGameSetting);
}

void UArenaMenuWidget::ApplySettings(const TSharedPtr<FJsonObject>& Values,bool bGraphics,bool bPersist)
{
 using namespace ArenaMenu;
 const auto Prev = SavedSettings;
 auto Out=MakeShared<FJsonObject>();
 const TArray<FString> Displays={TEXT("无边框窗口"),TEXT("全屏"),TEXT("窗口")};
 const TArray<FString> Resolutions={TEXT("1280 × 720"),TEXT("1600 × 900"),TEXT("1920 × 1080"),TEXT("2560 × 1440")};
 const TArray<FString> Qualities={TEXT("低"),TEXT("中"),TEXT("高"),TEXT("极高")};
 Out->SetStringField(TEXT("display"),Choice(Values,TEXT("display"),Prev->GetStringField(TEXT("display")),Displays));
 Out->SetStringField(TEXT("resolution"),Choice(Values,TEXT("resolution"),Prev->GetStringField(TEXT("resolution")),Resolutions));
 Out->SetStringField(TEXT("quality"),Choice(Values,TEXT("quality"),TEXT("高"),Qualities));
 Out->SetStringField(TEXT("fps"),Choice(Values,TEXT("fps"),TEXT("60"),{TEXT("30"),TEXT("60"),TEXT("120"),TEXT("144"),TEXT("无限制")}));
 Out->SetStringField(TEXT("opponent"),Choice(Values,TEXT("opponent"),TEXT("静止木桩"),{TEXT("静止木桩"),TEXT("固定防御"),TEXT("AI 对战")}));
 Out->SetNumberField(TEXT("brightness"),Number(Values,TEXT("brightness"),100,50,150));
 Out->SetNumberField(TEXT("master"),Number(Values,TEXT("master"),75,0,100));
 Out->SetNumberField(TEXT("music"),Number(Values,TEXT("music"),55,0,100));
 Out->SetNumberField(TEXT("sfx"),Number(Values,TEXT("sfx"),80,0,100));
 Out->SetNumberField(TEXT("sensitivity"),Number(Values,TEXT("sensitivity"),50,1,100));
 Out->SetNumberField(TEXT("healDelay"),Number(Values,TEXT("healDelay"),2,.1,10));
 for (const TCHAR* K : {TEXT("vsync"),TEXT("invert"),TEXT("shake"),TEXT("autoHeal"),TEXT("infiniteHealth"),TEXT("infiniteEnergy"),TEXT("noCooldown")})
  Out->SetBoolField(K,Boolean(Values,K,Prev->GetBoolField(K)));
 SavedSettings=Out;
 if (bGraphics && GEngine && GEngine->GetGameUserSettings())
 {
  auto* U=GEngine->GetGameUserSettings();
  const FString Display=Out->GetStringField(TEXT("display"));
  U->SetFullscreenMode(Display==TEXT("全屏") ? EWindowMode::Fullscreen : Display==TEXT("窗口") ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen);
  const FString Res=Out->GetStringField(TEXT("resolution"));
  const FIntPoint Size=Resolutions.Contains(Res) ? TArray<FIntPoint>{FIntPoint(1280,720),FIntPoint(1600,900),FIntPoint(1920,1080),FIntPoint(2560,1440)}[Resolutions.IndexOfByKey(Res)] : U->GetScreenResolution();
  U->SetScreenResolution(Size);
  U->SetOverallScalabilityLevel(Qualities.IndexOfByKey(Out->GetStringField(TEXT("quality"))));
  U->SetVSyncEnabled(Out->GetBoolField(TEXT("vsync")));
  U->SetFrameRateLimit(Out->GetStringField(TEXT("fps"))==TEXT("无限制") ? 0.f : FCString::Atof(*Out->GetStringField(TEXT("fps"))));
  if (GetWorld()->WorldType!=EWorldType::PIE) U->ApplySettings(false);
  else U->ApplyNonResolutionSettings();
  U->ConfirmVideoMode(); U->SaveSettings();
 }
 if (GetWorld())
 {
  if (auto Audio=GetWorld()->GetAudioDevice()) Audio->SetTransientPrimaryVolume(float(Out->GetNumberField(TEXT("master"))/100.));
  if (MusicSoundClass)
  {
   if (!MusicMix) MusicMix=NewObject<USoundMix>(this);
   UGameplayStatics::SetSoundMixClassOverride(this,MusicMix,MusicSoundClass,float(Out->GetNumberField(TEXT("music"))/100.),1.f,0.f,true);
   if (!bMusicMixPushed) { UGameplayStatics::PushSoundMixModifier(this,MusicMix); bMusicMixPushed=true; }
  }
 }
 if (auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("JJK.Feedback.AudioGain"))) C->Set(float(Out->GetNumberField(TEXT("sfx"))/100.),ECVF_SetByGameSetting);
 UCombatCameraFeedbackModifier::SetStrength(Out->GetBoolField(TEXT("shake")) ? 2 : 0);
 RestoreBrightness();
 if (auto* PC=Cast<AArenaPlayerController>(GetOwningPlayer()))
  if (auto* GM=PC->GetTrainingGameMode())
  {
   FTrainingSettings T;
   T.bAutoRecoverHealth=Out->GetBoolField(TEXT("autoHeal")); T.bInfiniteHealth=Out->GetBoolField(TEXT("infiniteHealth"));
   T.bInfiniteResources=Out->GetBoolField(TEXT("infiniteEnergy")); T.bNoCooldown=Out->GetBoolField(TEXT("noCooldown"));
   T.RecoveryDelay=float(Out->GetNumberField(TEXT("healDelay")));
   GM->SetTrainingSettings(T);
   // A saved training preference must not turn a newly started AI match into a dummy match.
   if (bPersist && CurrentPage==TEXT("settings")) GM->SetOpponentMode(Opponent(Out->GetStringField(TEXT("opponent"))));
  }
 if (bPersist)
 {
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(SettingsFile()),true);
  if (!FFileHelper::SaveStringToFile(SettingsJson(),*SettingsFile(),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
   UE_LOG(LogTemp, Warning, TEXT("[ArenaMenu] Unable to persist menu settings"));
 }
}

float UArenaMenuWidget::GetLookSensitivity() const { return SavedSettings ? float(SavedSettings->GetNumberField(TEXT("sensitivity"))/50.) : 1.f; }
bool UArenaMenuWidget::IsLookInverted() const { return SavedSettings && SavedSettings->GetBoolField(TEXT("invert")); }

FString UArenaMenuWidget::Command(const FString& Payload)
{
 TSharedPtr<FJsonObject> Message;
 if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Payload),Message) || !Message) return TEXT("invalid");
 FString Action;
 if (!Message->TryGetStringField(TEXT("action"),Action)) return TEXT("invalid");
 auto* PC=Cast<AArenaPlayerController>(GetOwningPlayer());
 auto* GM=PC ? PC->GetTrainingGameMode() : nullptr;
 if (!PC || !GM) return TEXT("unavailable");
 if (Action==TEXT("ready"))
 {
  bReady=true; ShowPage(CurrentPage,PendingTab); if (PC->IsGameMenuOpen()) FocusBrowser();
  UE_LOG(LogTemp,Log,TEXT("[ArenaMenu] Ready: approved local design loaded"));
#if !UE_BUILD_SHIPPING
  FString SmokeOut;
  if (FParse::Value(FCommandLine::Get(),TEXT("JJKMenuSmokeOut="),SmokeOut))
  {
   IFileManager::Get().MakeDirectory(*SmokeOut,true);
   FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this,SmokeOut](float)
   {
    auto* Owner=Cast<AArenaPlayerController>(GetOwningPlayer());
    auto Result=MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("ready"),bReady);
    Result->SetStringField(TEXT("page"),CurrentPage);
    Result->SetBoolField(TEXT("paused"),Owner && Owner->IsPaused());
    Result->SetBoolField(TEXT("screenshot"),DebugCaptureScreenshot(FPaths::Combine(SmokeOut,TEXT("main.png"))));
    Result->SetStringField(TEXT("map"),GetWorld()->GetMapName());
    FFileHelper::SaveStringToFile(ArenaMenu::Serialize(Result),*FPaths::Combine(SmokeOut,TEXT("report.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UKismetSystemLibrary::QuitGame(this,Owner,EQuitPreference::Quit,false);
    return false;
   }),3.f);
  }
#endif
 }
 else if (Action==TEXT("screen"))
 {
  FString Page;
  if (!Message->TryGetStringField(TEXT("screen"),Page) || !TArray<FString>{TEXT("main"),TEXT("start"),TEXT("pause"),TEXT("settings"),TEXT("controls"),TEXT("battle"),TEXT("exit")}.Contains(Page)) return TEXT("invalid");
  CurrentPage=Page;
  if (Page!=TEXT("settings")) RestoreBrightness();
  PC->SetGameMenuOpen(Page!=TEXT("battle"));
  UE_LOG(LogTemp,Log,TEXT("[ArenaMenu] Page=%s Paused=%d"),*Page,PC->IsPaused());
 }
 else if (Action==TEXT("start"))
 {
  FString Mode, Name;
  Message->TryGetStringField(TEXT("mode"),Mode); Message->TryGetStringField(TEXT("opponent"),Name);
  GM->SetOpponentMode(Mode==TEXT("ai") ? EOpponentMode::AI : ArenaMenu::Opponent(Name));
  GM->RestartMatch();
 }
 else if (Action==TEXT("restart")) GM->RestartMatch();
 else if (Action==TEXT("apply"))
 {
  const TSharedPtr<FJsonObject>* Settings=nullptr;
  if (!Message->TryGetObjectField(TEXT("settings"),Settings) || !Settings || !Settings->IsValid()) return TEXT("invalid");
  ApplySettings(*Settings,true,true); SynchronizeSettings();
 }
 else if (Action==TEXT("brightness"))
 {
  if (CurrentPage==TEXT("settings"))
   if (auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.TonemapperGamma"))) C->Set(float(2.2*ArenaMenu::Number(Message,TEXT("value"),100,50,150)/100.),ECVF_SetByGameSetting);
 }
 else if (Action==TEXT("quit")) UKismetSystemLibrary::QuitGame(this,PC,EQuitPreference::Quit,false);
 else if (Action==TEXT("probe"))
 {
#if !UE_BUILD_SHIPPING
  Message->TryGetStringField(TEXT("value"),LastProbe);
#endif
 }
 return SettingsJson();
}
