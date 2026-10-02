#include "Training/ArenaCombatHudWidget.h"
#include "Training/ArenaHudPainter.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Training/ArenaPlayerController.h"
#include "Training/CombatFeedbackComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Training/AttackDefinition.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Training/TrainingGameMode.h"

namespace
{
	// ---- 几何常量（1920×1080 设计坐标，14_HUD开发指引.md §3） ----
	constexpr float IdentX = 32.f, IdentY = 28.f;       // 身份块
	constexpr float StackW = 570.f, PortraitS = 92.f, IdentGap = 11.f;
	constexpr float HpY = 91.f, HpH = 16.f, HpSlant = 10.f, HpTip = 12.f;
	constexpr float CurseY = 107.f, CurseH = 12.f, CurseSlant = 7.f, CurseTip = 11.f;
	constexpr float PipsY = 95.f;                        // 行动豆中心行
	constexpr float ChargeCenterX = 960.f, ChargeCenterY = 934.f;
	constexpr float SkillY = 1004.f, SkillR = 35.f, UltR = 48.f, UltX = 1820.f;

	float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - (1.f - T) * (1.f - T); }
	FVector2f Vec(float X, float Y) { return FVector2f(X, Y); }
	FLinearColor WithAlpha(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, C.A * A); }

	/** 菱形（旋转 45° 方块）四角点 */
	void DiamondPts(const FVector2f& Center, float HalfDiag, TArray<FVector2f>& Out)
	{
		Out.Reset();
		Out.Add(Center + Vec(0.f, -HalfDiag));
		Out.Add(Center + Vec(HalfDiag, 0.f));
		Out.Add(Center + Vec(0.f, HalfDiag));
		Out.Add(Center + Vec(-HalfDiag, 0.f));
	}
}

// ---------- 基础 ----------
UArenaCombatHudWidget::UArenaCombatHudWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Constructor references are tracked by the cooker; no runtime filesystem dependency.
	for (const TCHAR* Name : { TEXT("scroll"), TEXT("portrait"), TEXT("punch"), TEXT("heavy"),
		TEXT("kick"), TEXT("hkick"), TEXT("swap"), TEXT("blast"), TEXT("sblast"),
		TEXT("aim"), TEXT("domain"), TEXT("mouse") })
	{
		const FString Path = FString::Printf(TEXT("/Game/UI/HUD/V9/T_%s.T_%s"), Name, Name);
		ConstructorHelpers::FObjectFinder<UTexture2D> Asset(*Path);
		if (Asset.Succeeded()) HudTextures.Add(FName(Name), Asset.Object);
	}
}

void UArenaCombatHudWidget::DrawIcon(const FArenaHudCanvas& C, FName Id, const FVector2f& Center,
	float Size, const FLinearColor& Color, int32 Layer) const
{
	if (const TObjectPtr<UTexture2D>* Texture = HudTextures.Find(Id))
		C.Image(Texture->Get(), Center - Vec(Size * .5f, Size * .5f), Vec(Size, Size), Color, Layer);
	else DrawArenaHudIcon(C, Id, Center, Size, Color, Layer);
}

void UArenaCombatHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (WidgetTree)
	{
		// 空根画布：只提供全视口几何，所有内容走 NativePaint 自绘
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
	}
}

void UArenaCombatHudWidget::SetHudVisible(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

// ---------- 采集 ----------
void UArenaCombatHudWidget::SnapshotSide(bool bPlayer, AFighterCharacter* F, ATrainingGameMode* GM, FArenaHudSideView& V)
{
	V = FArenaHudSideView();
	if (!IsValid(F) || GM == nullptr) return;
	V.bValid = true;
	V.Identity = bPlayer ? FArenaHudPalette::P1 : FArenaHudPalette::P2;

	if (const UFighterDefinition* D = F->GetDefinition())
	{
		V.Name = D->DisplayName.IsEmpty() ? (bPlayer ? TEXT("P1") : TEXT("P2")) : D->DisplayName.ToString();
	}
	else
	{
		V.Name = bPlayer ? TEXT("P1") : TEXT("P2");
	}

	if (const UFighterAttributeSet* A = F->GetFighterAttributeSet())
	{
		V.Health = A->GetHealth();           V.MaxHealth = FMath::Max(A->GetMaxHealth(), 1.f);
		V.Curse = A->GetCursedEnergy();      V.MaxCurse = FMath::Max(A->GetMaxCursedEnergy(), 1.f);
		V.Action = A->GetActionResource();   V.MaxAction = FMath::Max(A->GetMaxActionResource(), 1.f);
		V.Energy = A->GetEnergy();           V.MaxEnergy = FMath::Max(A->GetMaxEnergy(), 1.f);
	}

	V.Stance = F->GetStance();
	V.bAttacking = F->IsAttacking();
	V.bGuarding = F->IsGuarding();
	V.bDodgeInvuln = F->HasCombatTag(TAG_State_DodgeInvulnerable);
	V.bSprinting = F->IsSprinting();
	V.bDead = F->IsDead();
	V.bAiming = F->IsAimingEffective();
	V.bChargingNow = F->IsBlastCharging();
	V.bSuperCdNow = F->HasCombatTag(TAG_State_SuperBlastCooldown);
	V.bCastingNow = F->HasCombatTag(TAG_State_DomainCasting);
	V.bDomainActive = F->IsDomainActive();
	V.ChargePaidQ = F->GetBlastChargeAlpha();
 V.bInfiniteResources = GM->Settings.bInfiniteResources;
	// 移动炮叠加 RangedBlastCharging；定点超级炮没有该标签（ChargedBlastAbility ApplyChargeStateTags）
	V.bSuperBlastCharging = V.bChargingNow && !F->HasCombatTag(TAG_State_RangedBlastCharging);

	if (const UFighterDefinition* D = F->GetDefinition())
	{
		V.CurseRegenPerSec = D->ResourceFlow.CurseRegenPerSecond;
		V.CurseMinCost = D->MobileBlast.MinCost;
		if (V.bSuperBlastCharging)
		{
			V.ChargeGate = D->SuperBlast.MinChargeTime;
			V.ChargeCap = FMath::Max(D->SuperBlast.CapTime - D->SuperBlast.MinChargeTime, 0.1f);
			V.ChargeMinCost = D->SuperBlast.MinCost;  V.ChargeMaxCost = D->SuperBlast.MaxCost;
			V.ChargeMinDmg = D->SuperBlast.MinDamage; V.ChargeMaxDmg = D->SuperBlast.MaxDamage;
		}
		else
		{
			V.ChargeGate = 0.f;
			V.ChargeCap = FMath::Max(D->MobileBlast.CapTime, 0.1f);
			V.ChargeMinCost = D->MobileBlast.MinCost;  V.ChargeMaxCost = D->MobileBlast.MaxCost;
			V.ChargeMinDmg = D->MobileBlast.MinDamage; V.ChargeMaxDmg = D->MobileBlast.MaxDamage;
		}
		V.SuperCdDuration = FMath::Max(D->SuperBlast.Cooldown, 0.5f);
		V.StanceSwitchInterval = D->StanceSwitchDuration;
  V.SuperMinCost = D->SuperBlast.MinCost;
		V.DomainCastTime = FMath::Max(D->DomainConfig.CastTime, 0.1f);
		V.DomainDuration = FMath::Max(D->DomainConfig.Duration, 0.5f);
		for (int32 i = 0; i < 3; ++i)
		{
			if (D->ComboSegments.IsValidIndex(i) && D->ComboSegments[i] != nullptr) V.MeleeDamage[i] = D->ComboSegments[i]->Damage;
		}
		if (D->HeavyPunchDefinition != nullptr) V.HeavyPunchDamage = D->HeavyPunchDefinition->Damage;
		if (D->KickDefinition != nullptr) V.KickDamage = D->KickDefinition->Damage;
		if (D->HeavyKickDefinition != nullptr) V.HeavyKickDamage = D->HeavyKickDefinition->Damage;
	}

	const FDomainStatusView DV = GM->GetDomainStatusFor(F);
	V.bHasDomain = DV.bActive;
	V.bDomainSuppressed = DV.bSuppressed;
	V.DomainRemain = DV.RemainingSeconds;
	V.NextOrbIn = DV.NextOrbIn;
	V.OrbsInFlight = DV.OrbsInFlight;
	V.DomainTickIndex = DV.TickIndex;
	V.bNextOrbSkipped = DV.bNextOrbSkipped;
}

void UArenaCombatHudWidget::UpdateSideAnim(const FArenaHudSideView& V, FArenaHudSideAnim& A, double Now, double DeltaSeconds)
{
	if (!V.bValid) return;

	const float HpTarget = FMath::Clamp(V.Health / V.MaxHealth, 0.f, 1.f);
	if (!FMath::IsNearlyEqual(HpTarget, A.HpFillTarget, 0.0005f))
	{
		if (HpTarget < A.HpFillTarget) { A.bHpBump = true; A.HpBumpTime = Now; }
		A.HpFillFrom = A.HpFillShown;
		A.HpFillTarget = HpTarget;
		A.GhostFrom = A.GhostShown;
		A.HpChangeTime = Now;
	}
	// 即时 120ms ease-out（§7）
	A.HpFillShown = FMath::Lerp(A.HpFillFrom, A.HpFillTarget, EaseOut((Now - A.HpChangeTime) / 0.12));
	// 虚血：停 0.6s 后 350ms ease-out 回落
	A.GhostShown = FMath::Lerp(A.GhostFrom, A.HpFillTarget, EaseOut((Now - A.HpChangeTime - 0.6) / 0.35));

	// 咒力 140ms linear 近似
	A.CurseShown = FMath::FInterpTo(A.CurseShown, FMath::Clamp(V.Curse / V.MaxCurse, 0.f, 1.f), FMath::Max(DeltaSeconds, 0.001), 24.f);
	A.CurseTarget = FMath::Clamp(V.Curse / V.MaxCurse, 0.f, 1.f);

	const int32 Pip = FMath::Clamp(FMath::FloorToInt32(V.Action + 0.001f), 0, FMath::FloorToInt32(V.MaxAction));
	if (Pip < A.PipCount) A.PipSpendTime = Now;
	A.PipCount = Pip;

	// 超级炮冷却：标签跳变即启动一次计时（时长取 Definition；发射/已付费中断才启动，见 §6）
	if (V.bSuperCdNow && !A.bSuperCdSeen) { A.bSuperCdSeen = true; A.SuperCdStartTime = Now; }
	else if (!V.bSuperCdNow) A.bSuperCdSeen = false;

	// 蓄力起点观察（时间强度 q 与已支付 qPaid 分开）
	if (V.bChargingNow && !A.bChargingSeen) { A.bChargingSeen = true; A.ChargeStartTime = Now; }
	else if (!V.bChargingNow) A.bChargingSeen = false;

	// 形态切换观察（E 收束弧）
	if (!A.bStanceInit) { A.bStanceInit = true; A.Stance = V.Stance; }
	else if (A.Stance != V.Stance) { A.Stance = V.Stance; A.StanceSwitchTime = Now; }

	// 结印起点观察
	if (V.bCastingNow && !A.bCastingSeen) { A.bCastingSeen = true; A.CastStartTime = Now; }
	else if (!V.bCastingNow) A.bCastingSeen = false;

	// 闪避成功沿
	const bool bDodge = V.bDodgeInvuln;
	if (bDodge && !A.bDodgePrev) A.DodgeFlashTime = Now;
	A.bDodgePrev = bDodge;
}

void UArenaCombatHudWidget::SpawnFloat(const FVector& WorldPos, const FString& Text, const FLinearColor& Color, float Size, double Now)
{
	FArenaHudFloatItem Item;
	Item.WorldPos = WorldPos + FVector(FMath::FRandRange(-30.f, 30.f), 0.f, FMath::FRandRange(-6.f, 18.f));
	Item.Text = Text;
	Item.Color = Color;
	Item.FontSize = Size;
	Item.BornTime = Now;
	Floats.Add(Item);
	while (Floats.Num() > 24) Floats.RemoveAt(0);
}

void UArenaCombatHudWidget::ClearFeedback(bool bRoundReset)
{
 Floats.Reset();
 if (!bRoundReset) return; // Preserve actual cooldown/continuous state through menu and focus changes.
 PlayerAnim = FArenaHudSideAnim(); OpponentAnim = FArenaHudSideAnim();
 auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
 if (!GM) return;
 SnapshotSide(true, GM->GetPlayerFighter(), GM, PlayerView);
 SnapshotSide(false, GM->GetOpponentFighter(), GM, OpponentView);
 auto Seed = [](const FArenaHudSideView& V, FArenaHudSideAnim& A)
 {
  A.HpFillShown = A.HpFillFrom = A.HpFillTarget = A.GhostShown = A.GhostFrom = FMath::Clamp(V.Health / V.MaxHealth, 0.f, 1.f);
  A.CurseShown = A.CurseTarget = FMath::Clamp(V.Curse / V.MaxCurse, 0.f, 1.f);
 };
 Seed(PlayerView, PlayerAnim); Seed(OpponentView, OpponentAnim);
}

void UArenaCombatHudWidget::ConsumeContact(const FCombatContactFeedback& E)
{
 auto* Target = E.Target.Get();
 auto* Source = E.Source.Get();
 if (!IsValid(Target) || !IsValid(Source) || E.bLethal || Target->IsDead()) return;
 if (E.Result != ECombatFeedbackResult::Hit && E.Result != ECombatFeedbackResult::Guard && E.Result != ECombatFeedbackResult::Immune) return;
 const double Now = GetWorld()->GetTimeSeconds();
 const double Window = E.Tier == ECombatFeedbackTier::DomainOrb ? .35 : .10;
 // One bounded prompt per target/result; domain balls update the same short prompt without extending its lifetime.
 for (auto& Item : Floats) if (Item.Target.Get() == Target && Item.Result == E.Result && Now - Item.BornTime < Window)
 {
  Item.ActualDamage += FMath::Max(E.ActualDamage, 0.f);
  if (E.Result == ECombatFeedbackResult::Guard)
   Item.Text = Item.ActualDamage > .01f ? FString::Printf(TEXT("防御 · %.0f"), Item.ActualDamage) : TEXT("防御");
  else if (E.Result == ECombatFeedbackResult::Hit && Item.ActualDamage > .01f)
   Item.Text = FString::Printf(TEXT("%.0f"), Item.ActualDamage);
  ++MergedPromptCount;
  return;
 }
 FString Text;
 const auto Color = E.Result == ECombatFeedbackResult::Guard ? FArenaHudPalette::Aim
  : E.Result == ECombatFeedbackResult::Immune ? FArenaHudPalette::Muted : E.ActualDamage >= 100.f ? FArenaHudPalette::Gold : FLinearColor::White;
 if (E.Result == ECombatFeedbackResult::Guard) Text = E.ActualDamage > .01f ? FString::Printf(TEXT("防御 · %.0f"), E.ActualDamage) : TEXT("防御");
 else if (E.Result == ECombatFeedbackResult::Immune) Text = TEXT("免疫");
 else Text = E.ActualDamage > .01f ? FString::Printf(TEXT("%.0f"), E.ActualDamage) : TEXT("命中");
 SpawnFloat(E.Location + FVector(0, 0, 35), Text, Color, E.Result == ECombatFeedbackResult::Hit ? 20.f : 15.f, Now);
 Floats.Last().Target = Target; Floats.Last().Source = Source; Floats.Last().Result = E.Result; Floats.Last().ActualDamage = E.ActualDamage;
 while (Floats.Num() > 4) Floats.RemoveAt(0);
 ++ContactPromptCount;
}

void UArenaCombatHudWidget::ConsumeAction(const FCombatActionFeedback& E)
{
 auto* PC = Cast<AArenaPlayerController>(GetOwningPlayer());
 auto* GM = PC ? PC->GetTrainingGameMode() : nullptr;
 if (!GM || (E.Tier != ECombatFeedbackTier::MobileBlast && E.Tier != ECombatFeedbackTier::SuperBlast)) return;
 auto* Source = E.Source.Get();
 FArenaHudSideAnim* A = Source == GM->GetPlayerFighter() ? &PlayerAnim : Source == GM->GetOpponentFighter() ? &OpponentAnim : nullptr;
 if (!A) return;
 if (E.Stage == ECombatActionStage::Start && E.SessionId > A->ChargeSession)
 {
  A->ChargeSession = E.SessionId; A->ChargeStartTime = GetWorld()->GetTimeSeconds(); A->bChargingSeen = true;
 }
 // PaidQ/Full/Fire are read from the live ability. Never manufacture state from an old Full or End callback.
}

void UArenaCombatHudWidget::ConsumeLifecycle(const FCombatLifecycleFeedback& E)
{
 if (E.Reason == ECombatFeedbackEnd::Death || E.Reason == ECombatFeedbackEnd::Reset
  || E.Reason == ECombatFeedbackEnd::Destroyed) Floats.Reset();
 else if (E.AttackInstanceId == 0) Floats.RemoveAll([&E](const FArenaHudFloatItem& Item) { return Item.Source == E.Source; });
}

float UArenaCombatHudWidget::ChargeTimeQ(const FArenaHudSideView& V, const FArenaHudSideAnim& A, double Now) const
{
 return V.bChargingNow ? FMath::Clamp(static_cast<float>((Now - A.ChargeStartTime - V.ChargeGate) / V.ChargeCap), 0.f, 1.f) : 0.f;
}
bool UArenaCombatHudWidget::IsChargeLimited(const FArenaHudSideView& V, const FArenaHudSideAnim& A, double Now) const
{
 // Charge payment updates at 20Hz. A transient gap between elapsed time and PaidQ is not resource exhaustion.
 return V.bChargingNow && !V.bInfiniteResources && V.Curse <= .01f && V.ChargePaidQ < .999f
  && V.ChargePaidQ < ChargeTimeQ(V, A, Now) - .01f;
}

TArray<FString> UArenaCombatHudWidget::GetFeedbackTexts() const
{
 TArray<FString> Texts;
 for (const auto& Item : Floats) Texts.Add(Item.Text);
 return Texts;
}
FString UArenaCombatHudWidget::GetFeedbackState() const
{
 const auto& P = PlayerView;
 const bool bFull = P.bChargingNow && P.ChargePaidQ >= .999f;
 const bool bLimited = IsChargeLimited(P, PlayerAnim, LastTime);
 const bool bReady = P.bChargingNow && (!P.bSuperBlastCharging || LastTime - PlayerAnim.ChargeStartTime >= P.ChargeGate);
 return FString::Printf(TEXT("{\"player_health\":%.4f,\"opponent_health\":%.4f,\"player_hp_fill\":%.4f,\"opponent_hp_fill\":%.4f,\"player_ghost\":%.4f,\"opponent_ghost\":%.4f,\"paid_q\":%.4f,\"time_q\":%.4f,\"charging\":%s,\"full\":%s,\"limited\":%s,\"gate_ready\":%s,\"paid_cost\":%.4f,\"damage_preview\":%.4f,\"super_cooldown\":%s,\"cooldown_remaining\":%.4f,\"domain\":%s,\"domain_suppressed\":%s,\"next_orb_skipped\":%s,\"floats\":%d}"),
  P.Health, OpponentView.Health, PlayerAnim.HpFillShown, OpponentAnim.HpFillShown, PlayerAnim.GhostShown, OpponentAnim.GhostShown,
  P.ChargePaidQ, ChargeTimeQ(P, PlayerAnim, LastTime), P.bChargingNow ? TEXT("true") : TEXT("false"), bFull ? TEXT("true") : TEXT("false"), bLimited ? TEXT("true") : TEXT("false"), bReady ? TEXT("true") : TEXT("false"),
  P.bChargingNow ? P.ChargeMinCost + (P.ChargeMaxCost - P.ChargeMinCost) * P.ChargePaidQ : 0.f,
  P.bChargingNow ? P.ChargeMinDmg + (P.ChargeMaxDmg - P.ChargeMinDmg) * P.ChargePaidQ : 0.f,
  P.bSuperCdNow ? TEXT("true") : TEXT("false"), P.bSuperCdNow ? FMath::Max(0., P.SuperCdDuration - (LastTime - PlayerAnim.SuperCdStartTime)) : 0.,
  P.bHasDomain ? TEXT("true") : TEXT("false"), P.bDomainSuppressed ? TEXT("true") : TEXT("false"), P.bNextOrbSkipped ? TEXT("true") : TEXT("false"), Floats.Num());
}

void UArenaCombatHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	CachedGeometry = MyGeometry;

	UWorld* World = GetWorld();
	ATrainingGameMode* GM = World ? World->GetAuthGameMode<ATrainingGameMode>() : nullptr;
	if (GM == nullptr) return;

	const double Now = World->GetTimeSeconds();
	LastTime = Now;

	SnapshotSide(true, GM->GetPlayerFighter(), GM, PlayerView);
	SnapshotSide(false, GM->GetOpponentFighter(), GM, OpponentView);
	UpdateSideAnim(PlayerView, PlayerAnim, Now, InDeltaTime);
	UpdateSideAnim(OpponentView, OpponentAnim, Now, InDeltaTime);

 Floats.RemoveAll([Now](const FArenaHudFloatItem& Item) { return Now - Item.BornTime >= .9 || !Item.Target.IsValid(); });
 const auto* PC = Cast<AArenaPlayerController>(GetOwningPlayer());
 if (!PC || !PC->CanPlayCombatFeedback() || !GM->GetPlayerFighter()
  || !GM->GetPlayerFighter()->GetCombatFeedback()->IsChannelEnabled(ECombatFeedbackChannel::HUD)) Floats.Reset();
}

// ---------- 绘制主入口 ----------
int32 UArenaCombatHudWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateFontInfo BaseFont(UEngine::GetMediumFont(), 12.f);
	FArenaHudCanvas C(AllottedGeometry, OutDrawElements, BaseFont);
	const double Now = LastTime;

	int32 L = LayerId;
	DrawTopBand(C, L); L += 32;
	if (PlayerView.bValid) DrawFighterSide(C, true, PlayerView, PlayerAnim, Now, L);
	L += 32;
	if (OpponentView.bValid) DrawFighterSide(C, false, OpponentView, OpponentAnim, Now, L);
	L += 32;
	DrawCenterStatus(C, PlayerView, PlayerAnim, OpponentView, Now, L); L += 32;
	if (PlayerView.bValid)
	{
		DrawSkillArea(C, PlayerView, PlayerAnim, Now, L); L += 32;
		DrawFormAndCharge(C, PlayerView, PlayerAnim, Now, L); L += 32;
		DrawCrosshairChargeRing(C, L); L += 32;
	}
	DrawFloats(C, Now, L); L += 32;
	DrawKeyHints(C, L); L += 32;
	DrawResult(C, Now, L); L += 32;
	return L;
}

float UArenaCombatHudWidget::DesignWidth(const FArenaHudCanvas& C) const
{
	return C.ViewSize().X / FMath::Max(C.Len(1.f), 0.01f);
}

void UArenaCombatHudWidget::DrawTopBand(const FArenaHudCanvas& C, int32 Layer) const
{
	// 顶部压暗带：上深下亮（原型 #topband 四段纵向渐变）
	TArray<FLinearColor> Rows;
	Rows.Add(FLinearColor(0.004f, 0.012f, 0.027f, 0.99f));
	Rows.Add(FLinearColor(0.02f, 0.039f, 0.078f, 0.72f));
	Rows.Add(FLinearColor(0.047f, 0.078f, 0.149f, 0.30f));
	Rows.Add(FLinearColor(0.012f, 0.024f, 0.047f, 0.f));
	C.VGradient(Vec(0.f, 0.f), Vec(DesignWidth(C), 230.f), Rows, Layer);
}

float UArenaCombatHudWidget::Chip(const FArenaHudCanvas& C, const FVector2f& Pos, float Height, const FString& Text,
	float FontSize, const FLinearColor& TextColor, const FLinearColor& BorderColor, const FLinearColor& BgColor,
	int32 Layer, EArenaHudAlign Anchor) const
{
	const FVector2f Size = C.Measure(Text, FontSize);
	const float PadX = 6.f;
	const float W = Size.X + PadX * 2.f;
	const float X = (Anchor == EArenaHudAlign::Right ? Pos.X - W : Pos.X);
	C.Box(Vec(X, Pos.Y), Vec(W, Height), BgColor, Layer);
	TArray<FVector2f> Outline;
	Outline.Add(Vec(X + 0.5f, Pos.Y + 0.5f));
	Outline.Add(Vec(X + W - 0.5f, Pos.Y + 0.5f));
	Outline.Add(Vec(X + W - 0.5f, Pos.Y + Height - 0.5f));
	Outline.Add(Vec(X + 0.5f, Pos.Y + Height - 0.5f));
	C.Lines(Outline, BorderColor, 1.f, Layer + 1, true);
	C.Text(Text, Vec(X + W * 0.5f, Pos.Y + (Height - Size.Y) * 0.42f), FontSize, TextColor, Layer + 1, EArenaHudAlign::Center);
	return W;
}

void UArenaCombatHudWidget::DrawPointedBar(const FArenaHudCanvas& C, const FVector2f& Pos,
	float W, float H, float LeftSlant, float TipLen, float FillFraction, float GhostFraction,
	const FLinearColor Seg[5], bool bRightAnchor, double NowT, int32 Layer, bool bLowPulse) const
{
	auto Point = [&](float X, float Y) { return Pos + Vec(bRightAnchor ? W - X : X, Y); };
	const FLinearColor Rail = FLinearColor::FromSRGBColor(FColor(210, 188, 120));
	C.Poly({ Point(LeftSlant, 0), Point(W, 0), Point(W - TipLen, H), Point(0, H) },
		{ Rail, Rail, Rail * .65f, Rail * .65f }, Layer);
	// Thin, clipped scan strips preserve the parallelogram and mirror the complete P2 bar.
	const float Fill = FMath::Clamp(FillFraction, 0.f, 1.f) * W;
	const float Ghost = FMath::Clamp(GhostFraction, 0.f, 1.f) * W;
	const float Stops[5] = {0.f, .22f, .49f, .70f, 1.f};
	auto ColorAt = [&](float T) {
		for (int32 I = 0; I < 4; ++I)
			if (T <= Stops[I + 1]) return FMath::Lerp(Seg[I], Seg[I + 1], (T - Stops[I]) / (Stops[I + 1] - Stops[I]));
		return Seg[4];
	};
	for (float Y = 2.f; Y < H - 2.f; Y += .5f)
	{
		const float Y1 = FMath::Min(Y + .5f, H - 2.f);
		const float X0 = LeftSlant * (1.f - Y / H) + 1.f, X1 = W - TipLen * Y / H - 1.f;
		const float X2 = LeftSlant * (1.f - Y1 / H) + 1.f, X3 = W - TipLen * Y1 / H - 1.f;
		auto Strip = [&](float End, FLinearColor A, FLinearColor B, int32 L) {
			if (End <= FMath::Max(X0, X2)) return;
			C.Poly({ Point(X0,Y), Point(FMath::Min(X1,End),Y), Point(FMath::Min(X3,End),Y1), Point(X2,Y1) }, {A,A,B,B}, L);
		};
		const FLinearColor Empty = FLinearColor::FromSRGBColor(FColor(34,42,39));
		Strip(W, Empty, Empty, Layer + 1);
		Strip(Ghost, FArenaHudPalette::GhostSeg[1], FArenaHudPalette::GhostSeg[1], Layer + 2);
		Strip(Fill, ColorAt((Y-2.f)/(H-4.f)), ColorAt((Y1-2.f)/(H-4.f)), Layer + 3);
	}
}

void UArenaCombatHudWidget::DrawCurseGate(const FArenaHudCanvas& C, const FVector2f& Pos,
	float W, float H, float ThresholdFraction, bool bRightAnchor, int32 Layer) const
{
	const float X = Pos.X + W * (bRightAnchor ? 1.f - ThresholdFraction : ThresholdFraction);
	C.Lines({Vec(X,Pos.Y+3.f),Vec(X,Pos.Y+H-3.f)}, FLinearColor::FromSRGBColor(FColor(220,211,184)), 2.f, Layer);
}

void UArenaCombatHudWidget::DrawActionPips(const FArenaHudCanvas& C, const FVector2f& Start,
	const FArenaHudSideView& V, const FArenaHudSideAnim& A, double NowT, int32 Layer) const
{
	C.Text(TEXT("行动"), Start, 9.f, FArenaHudPalette::Fg, Layer);
	const int32 Count = FMath::Clamp(FMath::FloorToInt32(V.MaxAction), 1, 5);
	for (int32 I = 0; I < Count; ++I)
	{
		const FVector2f P = Start + Vec(28.f + I * 31.f, 2.f);
		const bool bOn = I < A.PipCount;
		const FLinearColor Top = FLinearColor::FromSRGBColor(bOn ? FColor(249,207,99) : FColor(54,59,44));
		const FLinearColor Bottom = FLinearColor::FromSRGBColor(bOn ? FColor(211,148,44) : FColor(34,40,30));
		C.Poly({P+Vec(2,0),P+Vec(29,0),P+Vec(27,7),P+Vec(0,7)}, {Top,Top,Bottom,Bottom}, Layer+1);
	}
}

void UArenaCombatHudWidget::DrawPortrait(const FArenaHudCanvas& C, const FVector2f& Pos,
	const FArenaHudSideView& V, bool bLeft, double NowT, int32 Layer) const
{
	if (const auto* Texture = HudTextures.Find(TEXT("portrait")))
		C.Image(Texture->Get(), Pos, Vec(92,100), FLinearColor::White, Layer, !bLeft);
	if (V.bDomainSuppressed)
		C.Ring(Pos+Vec(46,50),54,1.5f,WithAlpha(FArenaHudPalette::DomAct,.75f),Layer+2);
}

void UArenaCombatHudWidget::DrawFighterSide(const FArenaHudCanvas& C, bool bLeft,
	const FArenaHudSideView& V, const FArenaHudSideAnim& A, double NowT, int32 Layer) const
{
	const float ViewW = DesignWidth(C);
	const float StackX = bLeft ? 270.f : ViewW - 840.f;
	if (const auto* Texture = HudTextures.Find(TEXT("scroll")))
		C.Image(Texture->Get(), Vec(bLeft ? 90.f : ViewW - 870.f,32),Vec(780,155),FLinearColor::White,Layer,!bLeft);
	DrawPortrait(C,Vec(bLeft ? 177.f : ViewW - 269.f,62),V,bLeft,NowT,Layer+1);
	C.Text(V.Name,Vec(bLeft ? StackX+24.f : StackX+StackW-24.f,62),26.f,FArenaHudPalette::Fg,Layer+3,
		bLeft ? EArenaHudAlign::Left : EArenaHudAlign::Right);
	const float Hp = V.Health / V.MaxHealth;
	auto S = [](const TCHAR* Hex) { return FLinearColor::FromSRGBColor(FColor::FromHex(Hex)); };
	const FLinearColor Green[5] = {S(TEXT("366c4d")),S(TEXT("5eaa6c")),S(TEXT("88c18a")),S(TEXT("579760")),S(TEXT("3e7150"))};
	const FLinearColor Yellow[5] = {S(TEXT("806b28")),S(TEXT("d0bf48")),S(TEXT("e0d16b")),S(TEXT("c2ad40")),S(TEXT("7e6d29"))};
	const FLinearColor Red[5] = {S(TEXT("74251e")),S(TEXT("c14d36")),S(TEXT("e27654")),S(TEXT("af3c29")),S(TEXT("69251c"))};
	const FLinearColor Blue[5] = {S(TEXT("25547a")),S(TEXT("5aace1")),S(TEXT("8dceef")),S(TEXT("4c99cf")),S(TEXT("234d78"))};
	DrawPointedBar(C,Vec(StackX,HpY),StackW,HpH,10,12,A.HpFillShown,A.GhostShown,Hp<=.3f?Red:Hp<=.5f?Yellow:Green,!bLeft,NowT,Layer+4);
	const FVector2f CursePos(StackX+(bLeft?-10.f:10.f),CurseY);
	DrawPointedBar(C,CursePos,StackW,CurseH,7,11,A.CurseShown,A.CurseShown,Blue,!bLeft,NowT,Layer+9);
	DrawCurseGate(C,CursePos,StackW,CurseH,V.CurseMinCost/V.MaxCurse,!bLeft,Layer+13);
	if (bLeft) DrawActionPips(C,Vec(StackX,123),V,A,NowT,Layer+14);
}

void UArenaCombatHudWidget::DrawCenterStatus(const FArenaHudCanvas& C, const FArenaHudSideView& Player,
	const FArenaHudSideAnim& PAnim, const FArenaHudSideView& Opponent, double NowT, int32 Layer) const
{
	const float CX = DesignWidth(C) * 0.5f;

	// ---- 可展开 chip（能量满且领域未开） ----
	const bool bArmed = Player.bValid && Player.Energy >= Player.MaxEnergy - 0.5f && !Player.bHasDomain && !Player.bCastingNow;
	if (bArmed)
	{
		const float Pulse = 0.5f - 0.5f * FMath::Cos(FMath::Fmod(NowT, 1.4) / 1.4 * 2.f * PI);
		const float W = 150.f;
		C.Box(Vec(CX - W * 0.5f, 70.f), Vec(W, 26.f), FLinearColor(0.024f, 0.133f, 0.18f, 0.92f), Layer);
		TArray<FVector2f> Outline;
		Outline.Add(Vec(CX - W * 0.5f + 0.5f, 70.5f)); Outline.Add(Vec(CX + W * 0.5f - 0.5f, 70.5f));
		Outline.Add(Vec(CX + W * 0.5f - 0.5f, 95.5f)); Outline.Add(Vec(CX - W * 0.5f + 0.5f, 95.5f));
		C.Lines(Outline, WithAlpha(FArenaHudPalette::Deng, 0.42f + 0.3f * Pulse), 1.f, Layer + 1, true);
		// 键帽 R
		C.Box(Vec(CX - W * 0.5f + 10.f, 75.f), Vec(16.f, 16.f), FLinearColor(0.08f, 0.12f, 0.2f, 0.9f), Layer + 1);
		C.Text(TEXT("R"), Vec(CX - W * 0.5f + 18.f, 77.5f), 10.f, FArenaHudPalette::T5, Layer + 2, EArenaHudAlign::Center);
		C.Text(TEXT("可展开 · 极宴流光"), Vec(CX + 6.f, 77.5f), 11.f, FArenaHudPalette::Deng2, Layer + 2, EArenaHudAlign::Left);
	}

	// ---- 结印中：1s 径向进度环 + 刻度依次点亮 + 可被打断的细线警示 ----
	if (Player.bValid && Player.bCastingNow)
	{
		const float P = FMath::Clamp((NowT - PAnim.CastStartTime) / Player.DomainCastTime, 0.f, 1.f);
		const FVector2f Center(CX, 84.f);
		C.Ring(Center, 20.f, 2.5f, WithAlpha(FArenaHudPalette::Deng, 0.22f), Layer);
		C.Ring(Center, 20.f, 2.5f, FArenaHudPalette::Deng, Layer + 1, -90.f, 360.f * P);
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = (-90.f + 360.f * i / 12.f) * PI / 180.f;
			const FVector2f Dir(FMath::Cos(A), FMath::Sin(A));
			C.Lines({ Center + Dir * 26.f, Center + Dir * 31.f },
				i / 12.f < P ? FArenaHudPalette::Deng : WithAlpha(FArenaHudPalette::T4, 0.4f), 1.5f, Layer + 1);
		}
		// 可被打断警示：两侧细线（危险色，呼吸）
		const float Warn = 0.25f + 0.45f * (0.5f - 0.5f * FMath::Cos(FMath::Fmod(NowT, 0.8) / 0.8 * 2.f * PI));
		C.Lines({ Center + Vec(-34.f, 0.f), Center + Vec(-22.f, 0.f) }, WithAlpha(FArenaHudPalette::Danger, Warn), 1.5f, Layer + 1);
		C.Lines({ Center + Vec(22.f, 0.f), Center + Vec(34.f, 0.f) }, WithAlpha(FArenaHudPalette::Danger, Warn), 1.5f, Layer + 1);
		C.Text(TEXT("结印中 · 极宴流光"), Vec(CX, 116.f), 11.f, FArenaHudPalette::Deng2, Layer + 1, EArenaHudAlign::Center);
	}

	// ---- 展开中：大字结界名 + 倒计时 + 三个发炮刻度 + 在途球数 ----
	if (Player.bValid && Player.bHasDomain)
	{
		C.Text(TEXT("领域展开 · 极宴流光"), Vec(CX, 182.0f), 27.f, FArenaHudPalette::Gold, Layer, EArenaHudAlign::Center);
		// 饰线 + 两端菱形
		C.Lines({ Vec(CX - 230.f, 218.0f), Vec(CX + 230.f, 218.0f) }, WithAlpha(FArenaHudPalette::Gold, 0.5f), 1.f, Layer);
		C.Disc(Vec(CX - 233.f, 218.0f), 3.5f, FArenaHudPalette::Gold, Layer);
		C.Disc(Vec(CX + 233.f, 218.0f), 3.5f, FArenaHudPalette::Gold, Layer);

		const bool bSupp = Player.bDomainSuppressed;
		// 倒计时
		C.Text(FString::Printf(TEXT("%.1f"), Player.DomainRemain), Vec(CX - 160.f, 222.0f), 18.f, FArenaHudPalette::Fg, Layer + 1, EArenaHudAlign::Right);
		// 发炮刻度条（300×12）
		const float BarL = CX - 150.f, BarW = 300.f;
		C.Lines({ Vec(BarL, 233.0f), Vec(BarL + BarW, 233.0f) }, WithAlpha(FArenaHudPalette::T4, 0.22f), 2.f, Layer);
		for (int32 i = 0; i < 3; ++i)
		{
			const float Tx = BarL + BarW * (i == 0 ? 0.05f : i == 1 ? 0.383f : 0.717f);
			const bool bDone = Player.DomainTickIndex > i;
			const bool bNext = Player.DomainTickIndex == i;
			FLinearColor Fill = bDone ? FArenaHudPalette::Gold : FLinearColor(0.024f, 0.039f, 0.07f, 0.92f);
			FLinearColor Border = bDone ? FArenaHudPalette::Gold
				: (bNext && Player.bNextOrbSkipped) ? WithAlpha(FArenaHudPalette::Danger, 0.85f)
				: WithAlpha(FArenaHudPalette::Gold, 0.5f);
			if (bNext && !bSupp && !Player.bNextOrbSkipped)
			{
				const float Pulse = 0.5f - 0.5f * FMath::Cos(FMath::Fmod(NowT, 0.5) / 0.5 * 2.f * PI);
				Border = WithAlpha(FLinearColor::White, 0.4f + 0.6f * Pulse);
			}
			// 菱形刻度（9px 旋转 45°）
			TArray<FVector2f> D;
			DiamondPts(Vec(Tx, 233.0f), 4.5f, D);
			TArray<FLinearColor> Cs;
			for (int32 K = 0; K < 4; ++K) Cs.Add(Fill);
			C.Poly(D, Cs, Layer + 1);
			C.Lines(D, Border, 1.f, Layer + 2, true);
		}
		// 进度游标
		const float Elapsed = FMath::Clamp(1.f - Player.DomainRemain / Player.DomainDuration, 0.f, 1.f);
		C.Lines({ Vec(BarL + BarW * Elapsed, 225.0f), Vec(BarL + BarW * Elapsed, 241.0f) }, FArenaHudPalette::Gold, 2.f, Layer + 2);
		// 下一发 / 压制文案
		if (bSupp)
		{
			C.Text(TEXT("自动炮暂停"), Vec(CX + 160.f, 226.0f), 11.f, FArenaHudPalette::Muted, Layer + 1, EArenaHudAlign::Left);
		}
		else if (Player.NextOrbIn >= 0.f)
		{
			C.Text(Player.bNextOrbSkipped ? TEXT("咒力不足 · 本发跳过") : FString::Printf(TEXT("下一发 %.1fs"), Player.NextOrbIn),
				Vec(CX + 160.f, 226.0f), 11.f, Player.bNextOrbSkipped ? WithAlpha(FArenaHudPalette::Danger, 0.9f) : WithAlpha(FArenaHudPalette::Gold, 0.95f), Layer + 1, EArenaHudAlign::Left);
		}
		else
		{
			C.Text(TEXT("本轮结束"), Vec(CX + 160.f, 226.0f), 11.f, FArenaHudPalette::Muted, Layer + 1, EArenaHudAlign::Left);
		}
		// 在途球数
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector2f O(CX + 252.f + i * 14.f, 233.f);
			if (i < Player.OrbsInFlight)
			{
				C.Disc(O, 4.f, FArenaHudPalette::DomAct, Layer + 1);
				C.Disc(O, 1.6f, FArenaHudPalette::Deng3, Layer + 2);
			}
			C.Ring(O, 4.f, 0.6f, WithAlpha(FArenaHudPalette::Gold, 0.55f), Layer + 1);
		}
	}

	// ---- 双领域压制：链锁横杠 + 横幅 ----
	const bool bSuppressed = (Player.bValid && Player.bDomainSuppressed) || (Opponent.bValid && Opponent.bDomainSuppressed);
	if (bSuppressed)
	{
		const float Breath = 0.6f + 0.4f * (0.5f - 0.5f * FMath::Cos(FMath::Fmod(NowT, 1.6) / 1.6 * 2.f * PI));
		C.Lines({ Vec(CX - 260.f, 257.0f), Vec(CX + 260.f, 257.0f) }, WithAlpha(FArenaHudPalette::DomAct, 0.5f * Breath), 2.f, Layer);
		for (int32 i = -1; i <= 1; ++i)
		{
			TArray<FVector2f> D;
			DiamondPts(Vec(CX + i * 40.f, 257.0f), 4.5f, D);
			C.Lines(D, WithAlpha(FArenaHudPalette::DomAct, 0.85f * Breath), 1.5f, Layer + 1, true);
		}
		C.Lines({ Vec(CX - 260.f, 253.0f), Vec(CX - 260.f, 261.0f) }, WithAlpha(FArenaHudPalette::DomAct, 0.7f), 2.f, Layer);
		C.Lines({ Vec(CX + 260.f, 253.0f), Vec(CX + 260.f, 261.0f) }, WithAlpha(FArenaHudPalette::DomAct, 0.7f), 2.f, Layer);
		const FString Banner = TEXT("领域受压制 · 自动炮暂停");
		const FVector2f Bs = C.Measure(Banner, 11.5f);
		C.Box(Vec(CX - Bs.X * 0.5f - 11.f, 268.0f), Vec(Bs.X + 22.f, 19.0f), FLinearColor(0.031f, 0.055f, 0.102f, 0.8f), Layer);
		C.Text(Banner, Vec(CX, 270.5f), 11.5f, FLinearColor(0.765f, 0.8f, 0.855f, 1.f), Layer + 1, EArenaHudAlign::Center);
	}

	// ---- 模式徽标（顶部中央 y=168：当前模式 + 训练开关） ----
	ATrainingGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
	if (GM == nullptr) return;
	const TCHAR* ModeNames[] = { TEXT("静止木桩"), TEXT("固定防御"), TEXT("AI 对战") };
	TArray<FString> Chips;
	TArray<bool> Lit;
	Chips.Add(ModeNames[static_cast<int32>(GM->OpponentMode) % 3]);
	Lit.Add(true);
	if (GM->Settings.bInfiniteHealth) { Chips.Add(TEXT("无限生命")); Lit.Add(true); }
	if (GM->Settings.bInfiniteResources) { Chips.Add(TEXT("无限资源")); Lit.Add(true); }
	if (GM->Settings.bNoCooldown) { Chips.Add(TEXT("无冷却")); Lit.Add(true); }
	if (GM->IsTrainingMenuOpen()) { Chips.Add(TEXT("设置中")); Lit.Add(true); }

	constexpr float Gap = 8.f, ChipH = 18.f;
	float Total = 0.f;
	TArray<float> Widths;
	for (const FString& S : Chips)
	{
		const float W = C.Measure(S, 10.f).X + 16.f;
		Widths.Add(W);
		Total += W;
	}
	Total += Gap * (Chips.Num() - 1);
	float X = CX - Total * 0.5f;
	for (int32 i = 0; i < Chips.Num(); ++i)
	{
		const bool bOn = Lit[i];
		C.Box(Vec(X, 168.f), Vec(Widths[i], ChipH), bOn ? FLinearColor(0.45f, 0.95f, 0.95f, 0.10f) : WithAlpha(FArenaHudPalette::T2, 0.9f), Layer);
		TArray<FVector2f> Outline;
		Outline.Add(Vec(X + 0.5f, 168.5f)); Outline.Add(Vec(X + Widths[i] - 0.5f, 168.5f));
		Outline.Add(Vec(X + Widths[i] - 0.5f, 168.f + ChipH - 0.5f)); Outline.Add(Vec(X + 0.5f, 168.f + ChipH - 0.5f));
		C.Lines(Outline, bOn ? WithAlpha(FArenaHudPalette::Aim, 0.62f) : FArenaHudPalette::T3, 1.f, Layer + 1, true);
		C.Text(Chips[i], Vec(X + Widths[i] * 0.5f, 171.f), 10.f, bOn ? FLinearColor(0.624f, 0.973f, 0.973f) : FArenaHudPalette::T4, Layer + 1, EArenaHudAlign::Center);
		X += Widths[i] + Gap;
	}
}

// ---------- 右下技能区（等距四钮，只有键位标识；§3.4） ----------
void UArenaCombatHudWidget::DrawSkillArea(const FArenaHudCanvas& C, const FArenaHudSideView& P,
	const FArenaHudSideAnim& A, double NowT, int32 Layer) const
{
	const bool bRanged = P.Stance == EFighterStance::Ranged;

	const bool bSuperCd = P.bSuperCdNow;
	const float CdFrac = bSuperCd ? 1.f - FMath::Clamp((NowT - A.SuperCdStartTime) / P.SuperCdDuration, 0.f, 1.f) : 0.f;
	const float SwitchDur = FMath::Max(P.StanceSwitchInterval, .01f);
	const float SwitchT = (NowT - A.StanceSwitchTime) / SwitchDur;
	const bool bSwitching = SwitchT >= 0.f && SwitchT < 1.f;
	const bool bLmbUse = P.bChargingNow && !P.bSuperBlastCharging;
	const bool bQUse = P.bSuperBlastCharging;
	const float EnergyFrac = FMath::Clamp(P.Energy / P.MaxEnergy, 0.f, 1.f);
	const bool bReady = EnergyFrac >= 0.999f;

	struct FBtn { float X; float R; FName Icon; const TCHAR* Key; bool bMouseKey; };
	const FBtn Btns[4] = {
		{ 1460.f, SkillR, bRanged ? FName("blast") : FName("punch"), TEXT(""), true },
		{ 1580.f, SkillR, bRanged ? FName("sblast") : FName("kick"), TEXT("Q"), false },
		{ 1700.f, SkillR, FName("swap"), TEXT("E"), false },
		{ UltX, UltR, FName("domain"), TEXT("R"), false },
	};

	for (int32 Bi = 0; Bi < 4; ++Bi)
	{
		const FBtn& B = Btns[Bi];
		const FVector2f Center(B.X, SkillY);
		const bool bUlt = Bi == 3;
		const bool bUse = (Bi == 0 && bLmbUse) || (Bi == 1 && bQUse);
		const bool bCool = Bi == 1 && bRanged && bSuperCd;
  if (Bi == 1 && bRanged && !P.bChargingNow)
   C.Text(bCool ? TEXT("熔断") : P.Curse < P.SuperMinCost && !P.bInfiniteResources ? TEXT("咒力不足") : TEXT(""),
    Center + Vec(0, -B.R - 18.f), 11.f, bCool ? FArenaHudPalette::Muted : FArenaHudPalette::Danger, Layer + 7, EArenaHudAlign::Center);

		// v9: white emblems over a quiet transparent disk; state rings stay legible.
		if (Bi > 0) C.Ring(Center, B.R, .5f, WithAlpha(FArenaHudPalette::Fg,.18f),Layer);
		if (bCool || bUse) C.Disc(Center,B.R,FLinearColor(0,0,0,bUse?.10f:.25f),Layer+1);

		// 图标（只有图标，没有技能名文字 —— §3.4 明确设计决定）
		const FLinearColor IconCol = bUse ? FArenaHudPalette::Chg
			: bUlt ? (bReady ? FArenaHudPalette::Deng3 : WithAlpha(FArenaHudPalette::Fg, 0.85f))
			: bCool ? WithAlpha(FArenaHudPalette::Fg, 0.3f) : FArenaHudPalette::Fg;
		DrawIcon(C, B.Icon, Center, bUlt ? 76.f : 62.f, IconCol, Layer + 4);

		// 超级炮冷却：自顶部顺时针的暗色扇形扫掠（不写秒数，§9）
		if (bCool && CdFrac > 0.004f)
		{
			TArray<FVector2f> Pie;
			TArray<FLinearColor> Cs;
			Pie.Add(Center);
			Cs.Add(FLinearColor(0, 0, 0, 0.78f));
			constexpr int32 Seg = 26;
			for (int32 i = 0; i <= Seg; ++i)
			{
				const float Ang = (-90.f + 360.f * CdFrac * i / Seg) * PI / 180.f;
				Pie.Add(Center + FVector2f(FMath::Cos(Ang), FMath::Sin(Ang)) * (B.R - 1.5f));
				Cs.Add(FLinearColor(0, 0, 0, 0.78f));
			}
			C.Poly(Pie, Cs, Layer + 5);
		}
		// E 再切间隔：收束细弧（500ms linear，§7）
		if (Bi == 2 && bSwitching)
		{
			C.Ring(Center, B.R - 4.f, 1.f, WithAlpha(FArenaHudPalette::Aim, 0.9f), Layer + 5, -90.f, 360.f * (1.f - SwitchT), 2.f);
		}
		// R 大招同心充能环 = 领域能量表（§3.7）
		if (bUlt)
		{
			C.Ring(Center, B.R - 2.5f, 1.75f, FLinearColor(0.67f, 0.8f, 1.f, 0.26f), Layer + 5);
			if (EnergyFrac > 0.004f)
			{
				C.Ring(Center, B.R - 2.5f, 1.75f, bReady ? FArenaHudPalette::Deng3 : FArenaHudPalette::Deng, Layer + 6, -90.f, 360.f * EnergyFrac, 1.75f);
			}
			// 满能量外溢：常驻亮环 + 双层外扩光环 + 6 颗粒子（向外扩散，不是旋转扫掠 §9）
			if (bReady)
			{
				const float Breath = 0.5f - 0.5f * FMath::Cos(FMath::Fmod(NowT, 1.6) / 1.6 * 2.f * PI);
				C.Ring(Center, B.R + 7.f, 5.f, WithAlpha(FArenaHudPalette::Deng, 0.3f + 0.22f * Breath), Layer - 1);
				for (int32 K = 0; K < 2; ++K)
				{
					const float T = FMath::Fmod(NowT + K * 1.1, 2.2) / 2.2;
					C.Ring(Center, B.R + 2.f + 26.f * T, 1.2f, WithAlpha(K == 0 ? FArenaHudPalette::Deng : FArenaHudPalette::Deng2, 0.62f * (1.f - T)), Layer + 7);
				}
				for (int32 K = 0; K < 6; ++K)
				{
					const float T = FMath::Fmod(NowT + K * 0.37, 2.2) / 2.2;
					const float Ang = K * 60.f * PI / 180.f;
					const FVector2f Dir(FMath::Cos(Ang), FMath::Sin(Ang));
					C.Disc(Center + Dir * (B.R + 3.f + 12.f * T), 2.f, WithAlpha(FArenaHudPalette::Deng2, 0.95f - 0.4f * T), Layer + 7);
				}
			}
		}
		// 键位标识：压在按钮下缘外（bottom:-9px，高 15px）
		if (B.bMouseKey)
		{
			C.Box(Vec(B.X - 14.f, SkillY + B.R + 1.5f), Vec(28.f, 15.f), FLinearColor(.91f,.90f,.82f,1.f), Layer + 5);
			TArray<FVector2f> Outline;
			Outline.Add(Vec(B.X - 13.5f, SkillY + B.R + 2.f)); Outline.Add(Vec(B.X + 13.5f, SkillY + B.R + 2.f));
			Outline.Add(Vec(B.X + 13.5f, SkillY + B.R + 16.f)); Outline.Add(Vec(B.X - 13.5f, SkillY + B.R + 16.f));
			C.Lines(Outline, FLinearColor(0.59f, 0.75f, 1.f, 0.3f), 1.f, Layer + 6, true);
			DrawIcon(C, FName("mouse"), Vec(B.X, SkillY + B.R + 9.f), 16.f, FArenaHudPalette::T0, Layer + 6);
		}
		else
		{
			const FString KeyStr = B.Key;
			const FVector2f Ks = C.Measure(KeyStr, 11.f);
			const float Kw = Ks.X + 12.f;
			C.Box(Vec(B.X - Kw * 0.5f, SkillY + B.R + 1.5f), Vec(Kw, 15.f), FLinearColor(.91f,.90f,.82f,1.f), Layer + 5);
			TArray<FVector2f> Outline;
			Outline.Add(Vec(B.X - Kw * 0.5f + 0.5f, SkillY + B.R + 2.f)); Outline.Add(Vec(B.X + Kw * 0.5f - 0.5f, SkillY + B.R + 2.f));
			Outline.Add(Vec(B.X + Kw * 0.5f - 0.5f, SkillY + B.R + 16.f)); Outline.Add(Vec(B.X - Kw * 0.5f + 0.5f, SkillY + B.R + 16.f));
			C.Lines(Outline, FLinearColor(0.59f, 0.75f, 1.f, 0.3f), 1.f, Layer + 6, true);
			const FLinearColor KeyCol = FArenaHudPalette::T0;
			C.Text(KeyStr, Vec(B.X, SkillY + B.R + 3.5f), 11.f, KeyCol, Layer + 6, EArenaHudAlign::Center);
		}
	}
}

// ---------- 下中：蓄力读数条 + 蓄力环 + 形态圆盘（§3.5） ----------
void UArenaCombatHudWidget::DrawFormAndCharge(const FArenaHudCanvas& C, const FArenaHudSideView& P,
	const FArenaHudSideAnim& A, double NowT, int32 Layer) const
{
	const FVector2f Center(ChargeCenterX, ChargeCenterY);
	const bool bRanged = P.Stance == EFighterStance::Ranged;
	const bool bCharging = P.bChargingNow;

	// 蓄力时间强度 q 与已支付 qPaid（§6：伤害由已支付咒力封顶，不是由时长单独决定）
	const float Q = ChargeTimeQ(P, A, NowT);
	const float QPaid = bCharging ? FMath::Clamp(P.ChargePaidQ, 0.f, 1.f) : 0.f;
	const bool bLimited = IsChargeLimited(P, A, NowT);
	const bool bFull = bCharging && QPaid >= .999f;
	const float Damage = P.ChargeMinDmg + (P.ChargeMaxDmg - P.ChargeMinDmg) * QPaid;

	if (!bCharging)
	{
		C.Disc(Center,40.f,FLinearColor(0,0,0,.18f),Layer);
		DrawIcon(C,bRanged?FName("blast"):FName("punch"),Center,52.f,FArenaHudPalette::Fg,Layer+1);
		C.Text(bRanged?TEXT("远程"):TEXT("近战"),Center+Vec(0,35),12.f,FArenaHudPalette::Fg,Layer+2,EArenaHudAlign::Center);
  if (bRanged && P.Curse < P.CurseMinCost && !P.bInfiniteResources)
   C.Text(TEXT("咒力不足"),Center+Vec(0,57),11.f,FArenaHudPalette::Danger,Layer+2,EArenaHudAlign::Center);
		return;
	}

	// ---- 蓄力环（同心外环，绕在形态圆盘外圈） ----
	C.Ring(Center, 122.f, 17.f, WithAlpha(FArenaHudPalette::P1, 0.10f), Layer);
	C.Ring(Center, 124.f, 12.f, FLinearColor(0, 0, 0, 0.62f), Layer + 1);
	C.Ring(Center, 124.f, 9.f, FLinearColor(0.59f, 0.75f, 1.f, 0.24f), Layer + 1);
	C.Ring(Center, 138.f, 0.6f, FLinearColor(0.67f, 0.8f, 1.f, 0.40f), Layer + 1);
	C.Ring(Center, 96.f, 0.5f, FLinearColor(0.59f, 0.75f, 1.f, 0.08f), Layer + 1);
	// 60 格细刻度：按时间进度点亮
	for (int32 i = 0; i < 60; ++i)
	{
		const float AngDeg = 135.f + 270.f * i / 59.f;
		const float A2 = AngDeg * PI / 180.f;
		const FVector2f Dir(FMath::Cos(A2), FMath::Sin(A2));
		C.Lines({ Center + Dir * 110.f, Center + Dir * 118.f },
			bCharging && i / 59.f <= Q ? WithAlpha(FArenaHudPalette::Chg, 0.95f) : FLinearColor(0.67f, 0.8f, 1.f, 0.36f), 1.5f, Layer + 2);
	}
	// 已支付段（黄；满蓄转金）+ 受限段（红橙虚线语意的实底近似，§6）
	if (bCharging && QPaid > 0.004f)
	{
		C.Ring(Center, 124.f, 9.f, bFull ? FArenaHudPalette::Gold : FArenaHudPalette::Chg, Layer + 3, 135.f, 270.f * QPaid, 4.5f);
	}
	if (bLimited && Q - QPaid > 0.004f)
	{
		C.Ring(Center, 124.f, 9.f, WithAlpha(FArenaHudPalette::Danger, 0.85f), Layer + 3, 135.f + 270.f * QPaid, 270.f * (Q - QPaid));
	}
	if (bCharging)
	{
		const float A2 = (135.f + 270.f * Q) * PI / 180.f;
		C.Disc(Center + FVector2f(FMath::Cos(A2), FMath::Sin(A2)) * 124.f, 3.4f, FLinearColor::White, Layer + 4);
	}

	// ---- 形态圆盘（与蓄力环同心 116×116） ----
	C.Disc(Center, 64.f, FLinearColor(0, 0, 0, 0.5f), Layer);
	C.Disc(Center, 58.f, FLinearColor(0.024f, 0.039f, 0.07f, 0.97f), Layer + 1);
	C.Disc(Center, 50.f, WithAlpha(bRanged ? FArenaHudPalette::Aim : FArenaHudPalette::P1, 0.10f), Layer + 1);
	C.Ring(Center, 58.f, 0.7f, FLinearColor(0.59f, 0.75f, 1.f, 0.30f), Layer + 2);
	C.Ring(Center, 49.f, 0.7f, FArenaHudPalette::T3, Layer + 2);
	// 形态图标 + 名（切换 120ms 淡入，§7）
	const float FadeIn = FMath::Clamp((NowT - A.StanceSwitchTime) / 0.12, 0.0, 1.0);
	const float IconAlpha = A.StanceSwitchTime > 0.0 ? 0.3f + 0.7f * FadeIn : 1.f;
	DrawIcon(C, bRanged ? FName("blast") : FName("punch"), Center + Vec(0.f, -10.f), 52.f, WithAlpha(FArenaHudPalette::Fg, IconAlpha), Layer + 3);
	C.Text(bRanged ? TEXT("远程") : TEXT("近战"), Center + Vec(0.f, 10.f), 12.5f, WithAlpha(FArenaHudPalette::Fg, IconAlpha), Layer + 3, EArenaHudAlign::Center);

	// ---- 蓄力读数条（340×34，环上方；左缘状态色起笔） ----
	const FVector2f Ro(790.f, 714.f);
	const FVector2f RoS(340.f, 34.f);
	{
		TArray<FLinearColor> Rows;
		Rows.Add(FLinearColor(0.125f, 0.188f, 0.298f, 0.96f));
		Rows.Add(FLinearColor(0.035f, 0.055f, 0.102f, 0.94f));
		C.VGradient(Ro, RoS, Rows, Layer);
		TArray<FVector2f> Outline;
		Outline.Add(Ro + Vec(0.5f, 0.5f)); Outline.Add(Ro + Vec(RoS.X - 0.5f, 0.5f));
		Outline.Add(Ro + Vec(RoS.X - 0.5f, RoS.Y - 0.5f)); Outline.Add(Ro + Vec(0.5f, RoS.Y - 0.5f));
		C.Lines(Outline, FArenaHudPalette::T3, 1.f, Layer + 1, true);
		// 左缘状态色
		const FLinearColor EdgeCol = bFull ? FArenaHudPalette::Gold : bLimited ? FArenaHudPalette::Danger : FArenaHudPalette::Chg;
		C.Box(Ro, Vec(3.f, RoS.Y), EdgeCol, Layer + 1);
		C.Box(Ro + Vec(-3.f, 0.f), Vec(9.f, RoS.Y), WithAlpha(EdgeCol, 0.18f), Layer);

		FString Main, Sub;
		if (bCharging)
		{
   if (P.bSuperBlastCharging && NowT - A.ChargeStartTime < P.ChargeGate)
   {
    Main = TEXT("蓄力未就绪");
    Sub = FString::Printf(TEXT("至少 %.1fs · 已支付 %.0f 咒力"), P.ChargeGate,
     P.ChargeMinCost + (P.ChargeMaxCost - P.ChargeMinCost) * QPaid);
   }
			else if (bLimited)
			{
				// 受限态读数恒定在支付上限，任何持炮时长都不出现更低的数字（§6）
				Main = FString::Printf(TEXT("强度上限 %.0f"), Damage);
				Sub = TEXT("咒力不足 · 继续持炮不会变强");
			}
			else if (bFull)
   {
    Main = FString::Printf(TEXT("满蓄 %.0f"), Damage);
    Sub = TEXT("松开才发射");
   }
   else
			{
				Main = FString::Printf(TEXT("伤害 %.0f"), Damage);
				Sub = FString::Printf(TEXT("蓄力 %.2fs · 已支付 %.0f 咒力"),
					FMath::Min((NowT - A.ChargeStartTime), P.ChargeGate + P.ChargeCap),
					P.ChargeMinCost + (P.ChargeMaxCost - P.ChargeMinCost) * QPaid);
			}
		}
		else if (bRanged)
		{
			Main = TEXT("待机");
			Sub = FString::Printf(TEXT("左键蓄力炮 %.0f→%.0f · Q 超级炮 150→220"), P.ChargeMinDmg, P.ChargeMaxDmg);
		}
		else
		{
			Main = TEXT("近战");
			Sub = FString::Printf(TEXT("拳击 %.0f / %.0f / %.0f · 重拳 %.0f · 腿击 %.0f · 重踢 %.0f"),
				P.MeleeDamage[0], P.MeleeDamage[1], P.MeleeDamage[2], P.HeavyPunchDamage, P.KickDamage, P.HeavyKickDamage);
		}
		const FVector2f MainS = C.Measure(Main, 15.f);
		const FVector2f SubS = C.Measure(Sub, 10.f);
		const float PairW = MainS.X + 9.f + SubS.X;
		const float PairX = Ro.X + RoS.X * 0.5f - PairW * 0.5f;
		C.Text(Main, Vec(PairX + MainS.X * 0.5f, Ro.Y + (RoS.Y - MainS.Y) * 0.4f), 15.f, EdgeCol, Layer + 2, EArenaHudAlign::Center);
		C.Text(Sub, Vec(PairX + MainS.X + 9.f, Ro.Y + (RoS.Y - SubS.Y) * 0.45f), 10.f, FArenaHudPalette::T4, Layer + 2, EArenaHudAlign::Left);
	}
}

// ---------- 准星蓄力环（四线准星由 AArenaCrosshairHud 负责，此处只补进度环 §3.6） ----------
void UArenaCombatHudWidget::DrawCrosshairChargeRing(const FArenaHudCanvas& C, int32 Layer) const
{
	if (PlayerView.Stance != EFighterStance::Ranged || !PlayerView.bChargingNow) return;
	const FVector2f Center(DesignWidth(C) * 0.5f, C.ViewSize().Y / FMath::Max(C.Len(1.f), 0.01f) * 0.5f);
	const float Q = FMath::Clamp(PlayerView.ChargePaidQ, 0.f, 1.f);
	C.Ring(Center, 86.f, 1.f, FLinearColor(0.706f, 0.831f, 1.f, 0.30f), Layer);
	if (Q > 0.004f)
	{
		C.Ring(Center, 86.f, 1.f, FArenaHudPalette::Chg, Layer + 1, -90.f, 360.f * Q, 1.f);
	}
}

// ---------- 飘字层（§7 伤害数字 900ms ease-out 上飘淡出） ----------
void UArenaCombatHudWidget::DrawFloats(const FArenaHudCanvas& C, double NowT, int32 Layer) const
{
	if (Floats.Num() == 0) return;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController<APlayerController>() : nullptr;
	if (PC == nullptr) return;
	const float PxToDesign = 1.f / FMath::Max(C.Len(1.f) * FMath::Max(CachedGeometry.Scale, 0.01f), 0.01f);

	for (const FArenaHudFloatItem& Item : Floats)
	{
		const float Age = NowT - Item.BornTime;
		if (Age < 0.f || Age > 0.9f) continue;
		FVector2D ScreenPx;
		if (!PC->ProjectWorldLocationToScreen(Item.WorldPos, ScreenPx, false)) continue;
		const FVector2f Design(ScreenPx.X * PxToDesign, ScreenPx.Y * PxToDesign);
		const float Rise = FMath::Lerp(10.f, -44.f, EaseOut(Age / 0.9f));
		const float Alpha = Age < 0.2f ? Age / 0.2f : 1.f - (Age - 0.2f) / 0.7f;
		C.Text(Item.Text, Design + Vec(0.f, Rise), Item.FontSize, WithAlpha(Item.Color, Alpha), Layer, EArenaHudAlign::Center);
	}
}

// ---------- 按键提示行（底部一行，低调 §3.8） ----------
void UArenaCombatHudWidget::DrawKeyHints(const FArenaHudCanvas& C, int32 Layer) const
{
	struct FHint { const TCHAR* Key; const TCHAR* Label; };
	const FHint Hints[] = {
		{ TEXT("WASD"), TEXT("移动") }, { TEXT("Shift"), TEXT("冲刺/后撤") }, { TEXT("F"), TEXT("防御") },
		{ TEXT("左键"), TEXT("攻击/蓄力") }, { TEXT("右键"), TEXT("瞄准") }, { TEXT("E"), TEXT("切形态") },
		{ TEXT("Q"), TEXT("超级炮") }, { TEXT("R"), TEXT("领域") }, { TEXT("F1"), TEXT("训练设置") },
	};
	constexpr int32 HintNum = 9;
	constexpr float Gap = 18.f, H = 28.f;
	const float ViewH = C.ViewSize().Y / FMath::Max(C.Len(1.f), 0.01f);
	const float Y = ViewH - 16.f - H;

	float W = 28.f;
	TArray<float> ItemW;
	for (const FHint& It : Hints)
	{
		const float Kw = C.Measure(It.Key, 10.f).X + 10.f;
		const float Lw = C.Measure(It.Label, 11.f).X;
		ItemW.Add(Kw + 5.f + Lw);
		W += Kw + 5.f + Lw + Gap;
	}
	W -= Gap - 14.f;
	TArray<FLinearColor> Rows;
	Rows.Add(FLinearColor(0.078f, 0.118f, 0.196f, 0.78f));
	Rows.Add(FLinearColor(0.027f, 0.043f, 0.078f, 0.72f));
	C.VGradient(Vec(32.f, Y), Vec(W, H), Rows, Layer);
	{
		TArray<FVector2f> Outline;
		Outline.Add(Vec(32.5f, Y + 0.5f)); Outline.Add(Vec(32.f + W - 0.5f, Y + 0.5f));
		Outline.Add(Vec(32.f + W - 0.5f, Y + H - 0.5f)); Outline.Add(Vec(32.5f, Y + H - 0.5f));
		C.Lines(Outline, FArenaHudPalette::T3, 1.f, Layer + 1, true);
	}
	float X = 32.f + 14.f;
	for (int32 i = 0; i < HintNum; ++i)
	{
		const FHint& It = Hints[i];
		const float Kw = C.Measure(It.Key, 10.f).X + 10.f;
		C.Box(Vec(X, Y + 6.f), Vec(Kw, 16.f), FLinearColor(0.14f, 0.2f, 0.3f, 0.85f), Layer + 1);
		C.Text(It.Key, Vec(X + Kw * 0.5f, Y + 8.f), 10.f, FArenaHudPalette::T5, Layer + 2, EArenaHudAlign::Center);
		C.Text(It.Label, Vec(X + Kw + 5.f, Y + 7.5f), 11.f, FArenaHudPalette::T4, Layer + 1, EArenaHudAlign::Left);
		X += ItemW[i] + Gap;
	}
}

// ---------- 结果层（全屏覆盖 §3.8） ----------
void UArenaCombatHudWidget::DrawResult(const FArenaHudCanvas& C, double NowT, int32 Layer) const
{
	ATrainingGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
	if (GM == nullptr || !GM->IsMatchResolved()) return;

	const float W = DesignWidth(C);
	const float H = C.ViewSize().Y / FMath::Max(C.Len(1.f), 0.01f);
	C.Box(Vec(0.f, 0.f), Vec(W, H), FLinearColor(0.02f, 0.039f, 0.086f, 0.70f), Layer);

	const TCHAR* Titles[] = { TEXT(""), TEXT("玩家胜利"), TEXT("对手胜利"), TEXT("平局") };
	const FString Title = Titles[static_cast<int32>(GM->GetMatchOutcome()) % 4];
	C.Text(Title, Vec(W * 0.5f, H * 0.40f), 46.f, FArenaHudPalette::Aim, Layer + 1, EArenaHudAlign::Center);
	C.Lines({ Vec(W * 0.5f - 280.f, H * 0.40f + 62.f), Vec(W * 0.5f + 280.f, H * 0.40f + 62.f) },
		FLinearColor(0.59f, 0.75f, 1.f, 0.5f), 1.f, Layer + 1);

	const FTrainingStats& Ps = GM->PlayerStats;
	const FTrainingStats& Os = GM->OpponentStats;
	const FString Stats = FString::Printf(TEXT("命中 %d · 最大连段 %d · 造成 %.0f 伤害        对手 命中 %d · 造成 %.0f 伤害"),
		Ps.Hits, FMath::Max(Ps.LastComboHits, Ps.ComboHits), Ps.ResolvedDamage, Os.Hits, Os.ResolvedDamage);
	C.Text(Stats, Vec(W * 0.5f, H * 0.40f + 82.f), 13.f, FLinearColor(0.949f, 0.969f, 1.f, 0.82f), Layer + 1, EArenaHudAlign::Center);
	C.Text(TEXT("[空格] 重新开始　[F1] 训练设置"), Vec(W * 0.5f, H * 0.40f + 116.f), 11.f, FArenaHudPalette::Muted, Layer + 1, EArenaHudAlign::Center);
}

