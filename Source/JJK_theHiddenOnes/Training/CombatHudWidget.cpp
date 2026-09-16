#include "Training/CombatHudWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Training/TrainingGameMode.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/CombatTypes.h"
#include "TimerManager.h"

void UCombatHudWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 SetVisibility(ESlateVisibility::HitTestInvisible);
 auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
 auto Text=[this](const FString& S,int Size)
 {
  auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(S));
  auto Font=T->GetFont(); Font.Size=Size; T->SetFont(Font);
  T->SetColorAndOpacity(FSlateColor(FLinearColor(.95f,.97f,1.f)));
  T->SetShadowColorAndOpacity(FLinearColor(0,0,0,.9f)); T->SetShadowOffset(FVector2D(1,1)); return T;
 };
 for(int32 Side=0;Side<2;++Side)
 {
  const FLinearColor Identity=Side==0 ? FLinearColor(.08f,.7f,.95f) : FLinearColor(1.f,.38f,.12f);
  auto* Card=WidgetTree->ConstructWidget<UBorder>(); Cards.Add(Card);
  Card->SetBrushColor(FLinearColor(.014f,.022f,.038f,.88f)); Card->SetPadding(FMargin(14,10));
  auto* CardSlot=Canvas->AddChildToCanvas(Card);
  CardSlot->SetAnchors(Side==0 ? FAnchors(0,0,.44f,0) : FAnchors(.56f,0,1,0));
  CardSlot->SetOffsets(Side==0 ? FMargin(24,24,0,164) : FMargin(0,24,24,164));
  auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(Box);
  auto* Title=Text(Side==0 ? TEXT("P1  玩家") : TEXT("P2  对手"),17);
  Title->SetColorAndOpacity(FSlateColor(Identity)); Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0,0,0,5));
  for(int32 Resource=0;Resource<4;++Resource)
  {
   auto* Height=WidgetTree->ConstructWidget<USizeBox>(); Height->SetHeightOverride(19);
   Box->AddChildToVerticalBox(Height)->SetPadding(FMargin(0,2));
   auto* Overlay=WidgetTree->ConstructWidget<UOverlay>(); Height->SetContent(Overlay);
   auto* Bar=WidgetTree->ConstructWidget<UProgressBar>(); Bars.Add(Bar);
   const FLinearColor Colors[]={Identity,FLinearColor(.9f,.66f,.12f),FLinearColor(.25f,.44f,1.f),FLinearColor(.65f,.3f,.9f)};
   Bar->SetFillColorAndOpacity(Colors[Resource]);
   auto Style=Bar->GetWidgetStyle(); Style.BackgroundImage.TintColor=FSlateColor(FLinearColor(.07f,.09f,.13f));
   Style.FillImage.TintColor=FSlateColor(FLinearColor::White); Bar->SetWidgetStyle(Style);
   auto* BSlot=Overlay->AddChildToOverlay(Bar); BSlot->SetHorizontalAlignment(HAlign_Fill); BSlot->SetVerticalAlignment(VAlign_Fill);
   auto* Value=Text(TEXT(""),12); Values.Add(Value);
   auto* VSlot=Overlay->AddChildToOverlay(Value); VSlot->SetPadding(FMargin(7,0)); VSlot->SetVerticalAlignment(VAlign_Center);
  }
  auto* State=Text(TEXT(""),12); FighterStates.Add(State); Box->AddChildToVerticalBox(State)->SetPadding(FMargin(0,5,0,0));
 }
 ModeLabel=Text(TEXT(""),14); ModeLabel->SetJustification(ETextJustify::Center);
 auto* ModeSlot=Canvas->AddChildToCanvas(ModeLabel); ModeSlot->SetAnchors(FAnchors(.5f,0));
 ModeSlot->SetAlignment(FVector2D(.5f,0)); ModeSlot->SetPosition(FVector2D(0,198)); ModeSlot->SetAutoSize(true);
 auto* Help=Text(TEXT("WASD 移动   Shift 冲刺 / 后撤（移动时按住持续跑）   F 防御   左键 / Q 拳脚   F1 训练设置"),13);
 auto* HelpSlot=Canvas->AddChildToCanvas(Help); HelpSlot->SetAnchors(FAnchors(.5f,1));
 HelpSlot->SetAlignment(FVector2D(.5f,1)); HelpSlot->SetPosition(FVector2D(0,-20)); HelpSlot->SetAutoSize(true);
}

void UCombatHudWidget::NativeConstruct()
{
 Super::NativeConstruct();
 // 10 Hz also updates countdowns and brief feedback without adding ASC subscriptions.
 GetWorld()->GetTimerManager().SetTimer(RefreshTimer,this,&UCombatHudWidget::Refresh,.1f,true);
 Refresh();
}
void UCombatHudWidget::NativeDestruct()
{
 if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
 Super::NativeDestruct();
}
void UCombatHudWidget::Refresh()
{
 auto* GM=GetWorld() ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
 if(!GM || Bars.Num()!=8) return;
 DisplayedState.Reset();
 for(int32 Side=0;Side<2;++Side)
 {
  auto* F=Side==0 ? GM->GetPlayerFighter() : GM->GetOpponentFighter();
  auto* A=IsValid(F) ? F->GetFighterAttributeSet() : nullptr;
  const float Current[]={A ? A->GetHealth():0,A ? A->GetActionResource():0,A ? A->GetCursedEnergy():0,A ? A->GetEnergy():0};
  const float Max[]={A ? A->GetMaxHealth():0,A ? A->GetMaxActionResource():0,A ? A->GetMaxCursedEnergy():0,A ? A->GetMaxEnergy():0};
  const TCHAR* Names[]={TEXT("生命"),TEXT("行动"),TEXT("咒力"),TEXT("领域")};
  DisplayedState+=Side==0 ? TEXT("P1 ") : TEXT("P2 ");
  for(int32 Resource=0;Resource<4;++Resource)
  {
   const int32 I=Side*4+Resource;
   Bars[I]->SetPercent(Max[Resource]>0 ? FMath::Clamp(Current[Resource]/Max[Resource],0.f,1.f) : 0.f);
   const FString S=Resource==1 ? FString::Printf(TEXT("%s  %.1f / %.0f"),Names[Resource],Current[Resource],Max[Resource])
    : FString::Printf(TEXT("%s  %.0f / %.0f"),Names[Resource],Current[Resource],Max[Resource]);
   Values[I]->SetText(FText::FromString(S)); DisplayedState+=S+TEXT("  ");
  }
  FString State=TEXT("等待角色");
  const bool bSuccess=IsValid(F) && F->HasRecentDodgeSuccess();
  if(IsValid(F))
  {
   State=F->IsDead() ? TEXT("倒下") : bSuccess ? TEXT("闪避成功") : F->IsSprinting() ? TEXT("疾跑")
    : F->HasCombatTag(TAG_State_DodgeInvulnerable) ? TEXT("闪避") : F->IsGuarding() ? TEXT("防御")
    : F->IsAttacking() ? TEXT("攻击") : !F->CanAct() ? TEXT("恢复中") : TEXT("就绪");
   State+=F->GetStance()==EFighterStance::Melee ? TEXT("  ·  近战") : TEXT("  ·  远程（技能待接入）");
   const float Cooldown=F->GetFighterAbilitySystemComponent()->GetTrainingCooldownRemaining();
   if(Cooldown>0) State+=FString::Printf(TEXT("  ·  测试技能 %.1fs"),Cooldown);
  }
  FighterStates[Side]->SetText(FText::FromString(State));
  Cards[Side]->SetBrushColor(bSuccess ? FLinearColor(.025f,.26f,.3f,.95f) : FLinearColor(.014f,.022f,.038f,.88f));
  DisplayedState+=State+TEXT("\n");
 }
 const TCHAR* Modes[]={TEXT("静止木桩"),TEXT("固定防御"),TEXT("AI 对战")};
 FString Mode=Modes[static_cast<int32>(GM->OpponentMode)%3];
 if(GM->IsTrainingMenuOpen()) Mode+=TEXT("  ·  设置中");
 if(GM->IsMatchResolved())
 {
  const TCHAR* Results[]={TEXT(""),TEXT("玩家胜利"),TEXT("对手胜利"),TEXT("平局")};
  Mode+=TEXT("  ·  ")+FString(Results[static_cast<int32>(GM->GetMatchOutcome())%4]);
 }
 if(GM->Settings.bInfiniteHealth) Mode+=TEXT("  |  无限生命");
 if(GM->Settings.bInfiniteResources) Mode+=TEXT("  |  无限资源");
 if(GM->Settings.bNoCooldown) Mode+=TEXT("  |  无冷却");
 ModeLabel->SetText(FText::FromString(Mode)); DisplayedState+=Mode;
}
