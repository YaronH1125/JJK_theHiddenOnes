#include "Training/TrainingPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Training/TrainingGameMode.h"
#include "Training/ArenaPlayerController.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "InputCoreTypes.h"

void UTrainingPanelWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized(); SetIsFocusable(true);
 auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
 auto* Border=WidgetTree->ConstructWidget<UBorder>(); Border->SetBrushColor(FLinearColor(.018f,.027f,.05f,.96f)); Border->SetPadding(FMargin(22));
 auto* PanelSlot=Canvas->AddChildToCanvas(Border); PanelSlot->SetAnchors(FAnchors(.5f,.5f)); PanelSlot->SetAlignment(FVector2D(.5f,.5f)); PanelSlot->SetAutoSize(true);
 auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Border->SetContent(Box);
 auto Text=[this,Box](const FString& Value,int Size=16)
 {
  auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Value)); auto Font=T->GetFont(); Font.Size=Size; T->SetFont(Font);
  T->SetColorAndOpacity(FSlateColor(FLinearColor(.9f,.94f,1.f))); Box->AddChildToVerticalBox(T)->SetPadding(FMargin(0,3)); return T;
 };
 Text(TEXT("训练设置  /  F1 返回战斗"),24);
 Text(TEXT("双方通用 · 开关关闭后恢复正常规则，不补扣过去消耗"),13);
 OpponentChoice=WidgetTree->ConstructWidget<UComboBoxString>(); OpponentChoice->AddOption(TEXT("静止木桩")); OpponentChoice->AddOption(TEXT("固定防御"));
 Box->AddChildToVerticalBox(OpponentChoice)->SetPadding(FMargin(0,6)); OpponentChoice->OnSelectionChanged.AddDynamic(this,&UTrainingPanelWidget::ModeChanged);
 AIButton=WidgetTree->ConstructWidget<UButton>(); auto* AILabel=WidgetTree->ConstructWidget<UTextBlock>(); AILabel->SetText(FText::FromString(TEXT("AI 对战：M5 未接入"))); AIButton->AddChild(AILabel); AIButton->SetIsEnabled(false); Box->AddChildToVerticalBox(AIButton);
 auto Check=[this,Box](const TCHAR* Label)
 {
  auto* C=WidgetTree->ConstructWidget<UCheckBox>(); auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Label)); auto Font=T->GetFont(); Font.Size=16; T->SetFont(Font); C->AddChild(T);
  Box->AddChildToVerticalBox(C)->SetPadding(FMargin(0,3)); C->OnCheckStateChanged.AddDynamic(this,&UTrainingPanelWidget::SettingsChanged); return C;
 };
 AutoHeal=Check(TEXT("自动恢复生命（恢复行动后延迟，死亡不复活）"));
 InfiniteHealth=Check(TEXT("无限生命（保留受击，生命最低 1）"));
 InfiniteResources=Check(TEXT("无限资源（行动资源 / 咒力 / 领域能量）"));
 NoCooldown=Check(TEXT("无技能冷却（保留起手、后摇和切形态限制）"));
 auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>(); Box->AddChildToVerticalBox(Row);
 auto* Label=WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(FText::FromString(TEXT("恢复延迟（秒）  "))); Row->AddChildToHorizontalBox(Label);
 Delay=WidgetTree->ConstructWidget<USpinBox>(); Delay->SetMinDesiredWidth(80.f); Delay->SetMinValue(.1f); Delay->SetMaxValue(30.f); Delay->SetMinSliderValue(.1f); Delay->SetMaxSliderValue(10.f); Row->AddChildToHorizontalBox(Delay); Delay->OnValueChanged.AddDynamic(this,&UTrainingPanelWidget::DelayChanged);
 Status=Text(TEXT(""),17); History=Text(TEXT(""),14);
 Text(TEXT("远程炮击、瞄准、蓄力与领域：M6 未接入"),12);
 auto Button=[this,Box](const TCHAR* Label)
 {
  auto* B=WidgetTree->ConstructWidget<UButton>(); auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Label)); auto Font=T->GetFont(); Font.Size=16; T->SetFont(Font); B->AddChild(T); Box->AddChildToVerticalBox(B)->SetPadding(FMargin(0,4)); return B;
 };
 Button(TEXT("快速重置（保留设置与模式）"))->OnClicked.AddDynamic(this,&UTrainingPanelWidget::ResetClicked);
#if !UE_BUILD_SHIPPING
 Button(TEXT("开发测试技能：关闭面板并尝试消耗 1 / 10 / 5，冷却 3 秒"))->OnClicked.AddDynamic(this,&UTrainingPanelWidget::ProbeClicked);
#endif
 Button(TEXT("返回战斗  [F1]"))->OnClicked.AddDynamic(this,&UTrainingPanelWidget::CloseClicked);
}
void UTrainingPanelWidget::NativeConstruct()
{
 Super::NativeConstruct();
 Mode=GetWorld()->GetAuthGameMode<ATrainingGameMode>();
 if(Mode.IsValid()) Mode->OnTrainingChanged.AddUniqueDynamic(this,&UTrainingPanelWidget::Refresh);
 Refresh();
}
void UTrainingPanelWidget::NativeDestruct()
{
 if(Mode.IsValid()) Mode->OnTrainingChanged.RemoveDynamic(this,&UTrainingPanelWidget::Refresh);
 Mode.Reset(); Super::NativeDestruct();
}
void UTrainingPanelWidget::Refresh()
{
 auto* GM=Mode.Get(); if(!GM || !Status || bRefreshing) return;
 bRefreshing=true; ++RefreshCount;
 AutoHeal->SetIsChecked(GM->Settings.bAutoRecoverHealth); InfiniteHealth->SetIsChecked(GM->Settings.bInfiniteHealth);
 InfiniteResources->SetIsChecked(GM->Settings.bInfiniteResources); NoCooldown->SetIsChecked(GM->Settings.bNoCooldown); Delay->SetValue(GM->Settings.RecoveryDelay);
 OpponentChoice->SetSelectedOption(GM->OpponentMode==EOpponentMode::FixedGuard ? TEXT("固定防御") : TEXT("静止木桩"));
 FString Lines;
 auto Fighter=[&Lines](const TCHAR* Name,AFighterCharacter* F,const FTrainingStats& S)
 {
  if(!IsValid(F)) { Lines+=FString(Name)+TEXT("：待重生\n"); return; }
  auto* A=F->GetFighterAttributeSet();
  Lines+=FString::Printf(TEXT("%s  生命 %.0f/%.0f  行动 %.1f/%.0f  咒力 %.0f/%.0f  领域 %.0f/%.0f\n开发技能冷却 %.1fs  | 当前连击 %d / %.0f  上次 %d / %.0f\n原始 %.0f  结算 %.0f  扣血 %.0f | 命中 %d 防御 %d 免疫 %d 空挥 %d\n"),Name,A->GetHealth(),A->GetMaxHealth(),A->GetActionResource(),A->GetMaxActionResource(),A->GetCursedEnergy(),A->GetMaxCursedEnergy(),A->GetEnergy(),A->GetMaxEnergy(),F->GetFighterAbilitySystemComponent()->GetTrainingCooldownRemaining(),S.ComboHits,S.ComboDamage,S.LastComboHits,S.LastComboDamage,S.RawDamage,S.ResolvedDamage,S.HealthLost,S.Hits,S.Guards,S.Immunes,S.Whiffs);
 };
 Fighter(TEXT("P1"),GM->GetPlayerFighter(),GM->PlayerStats); Fighter(TEXT("P2"),GM->GetOpponentFighter(),GM->OpponentStats);
 Status->SetText(FText::FromString(Lines)); History->SetText(FText::FromString(TEXT("最近请求（新→旧）\n")+FString::Join(GM->InputHistory,TEXT("\n"))));
 bRefreshing=false;
}
void UTrainingPanelWidget::SettingsChanged(bool)
{
 if(bRefreshing || !Mode.IsValid()) return;
 auto S=Mode->Settings; S.bAutoRecoverHealth=AutoHeal->IsChecked(); S.bInfiniteHealth=InfiniteHealth->IsChecked(); S.bInfiniteResources=InfiniteResources->IsChecked(); S.bNoCooldown=NoCooldown->IsChecked(); S.RecoveryDelay=Delay->GetValue(); Mode->SetTrainingSettings(S);
}
void UTrainingPanelWidget::DelayChanged(float) { SettingsChanged(false); }
void UTrainingPanelWidget::ModeChanged(FString Item,ESelectInfo::Type)
{
 if(!bRefreshing && Mode.IsValid()) Mode->SetOpponentMode(Item==TEXT("固定防御") ? EOpponentMode::FixedGuard : EOpponentMode::Static);
}
void UTrainingPanelWidget::ResetClicked() { if(Mode.IsValid()) Mode->ResetTraining(); }
void UTrainingPanelWidget::CloseClicked() { if(auto* PC=Cast<AArenaPlayerController>(GetOwningPlayer())) PC->SetTrainingPanelOpen(false); }
void UTrainingPanelWidget::ProbeClicked()
{
 auto* GM=Mode.Get(); CloseClicked(); if(GM) GM->RequestTrainingProbe(GM->GetPlayerFighter());
}
FReply UTrainingPanelWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
 if(E.GetKey()==EKeys::F1 || E.GetKey()==EKeys::Escape) { CloseClicked(); return FReply::Handled(); }
 return Super::NativeOnKeyDown(G,E);
}
FString UTrainingPanelWidget::GetDisplayedState() const { return Status ? Status->GetText().ToString() : FString(); }
bool UTrainingPanelWidget::IsAIOptionDisabled() const { return AIButton && !AIButton->GetIsEnabled(); }
