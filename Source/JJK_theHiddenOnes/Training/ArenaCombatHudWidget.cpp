#include "Training/ArenaCombatHudWidget.h"
#include "Training/ArenaHudPainter.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
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
	constexpr float StackW = 500.f, PortraitS = 92.f, IdentGap = 11.f;
	constexpr float HpY = 52.f, HpH = 16.f, HpSlant = 10.f, HpTip = 15.f;
	constexpr float CurseY = 68.f, CurseH = 11.f, CurseSlant = 7.f, CurseTip = 11.f;
	constexpr float PipsY = 95.f;                        // 行动豆中心行
	constexpr float ChargeCenterX = 960.f, ChargeCenterY = 934.f;
	constexpr float SkillY = 1004.f, SkillR = 35.f, UltR = 48.f, UltX = 1820.f;

	float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - (1.f - T) * (1.f - T); }
	FVector2f Vec(float X, float Y) { return FVector2f(X, Y); }
	FLinearColor WithAlpha(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, C.A * A); }

	/** 条内槽轮廓：斜切左端 + 箭头尖角右端（§3.1；P2 侧由调用方水平镜像填充方向） */
	struct FBarProfile
	{
		float L = 0.f, R = 0.f, T = 0.f, B = 0.f, MidY = 0.f;
		float SlantEnd = 0.f, TipStart = 0.f;

		float TopAt(float X) const
		{
			if (X <= SlantEnd) return FMath::Lerp(B, T, (X - L) / FMath::Max(SlantEnd - L, 0.01f));
			if (X >= TipStart) return FMath::Lerp(T, MidY, (X - TipStart) / FMath::Max(R - TipStart, 0.01f));
			return T;
		}
		float BotAt(float X) const
		{
			if (X >= TipStart) return FMath::Lerp(B, MidY, (X - TipStart) / FMath::Max(R - TipStart, 0.01f));
			return B;
		}
	};

	FBarProfile MakeProfile(float W, float H, float LeftSlant, float TipLen)
	{
		FBarProfile P;
		P.L = 1.f; P.R = W - 1.f; P.T = 1.f; P.B = H - 1.f; P.MidY = H * 0.5f;
		P.SlantEnd = FMath::Max(LeftSlant - 1.f, P.L + 0.5f);
		P.TipStart = FMath::Min(W - TipLen + 1.f, P.R - 0.5f);
		return P;
	}

	/** 填充带：把 [Xa,Xb]×[RowT0,RowT1] 与轮廓求交，按断点拆成若干四边形逐个提交 */
	void DrawFillBand(const FArenaHudCanvas& C, const FVector2f& Pos, const FBarProfile& P,
		float Xa, float Xb, float RowT0, float RowT1, const FLinearColor& ColTop, const FLinearColor& ColBot, int32 Layer)
	{
		Xa = FMath::Max(Xa, P.L - 0.5f);
		Xb = FMath::Min(Xb, P.R + 0.5f);
		if (Xb - Xa < 0.4f || RowT1 - RowT0 < 0.003f) return;
		float Break[4];
		int32 N = 0;
		Break[N++] = Xa;
		if (P.SlantEnd > Xa && P.SlantEnd < Xb) Break[N++] = P.SlantEnd;
		if (P.TipStart > Xa && P.TipStart < Xb) Break[N++] = P.TipStart;
		Break[N++] = Xb;

		const float RowTop = FMath::Lerp(P.T, P.B, RowT0);
		const float RowBot = FMath::Lerp(P.T, P.B, RowT1);
		const FLinearColor CT = FMath::Lerp(ColTop, ColBot, RowT0);
		const FLinearColor CB = FMath::Lerp(ColTop, ColBot, RowT1);

		for (int32 i = 0; i < N - 1; ++i)
		{
			const float A = Break[i], B = Break[i + 1];
			const float TopA = FMath::Max(P.TopAt(A), RowTop), TopB = FMath::Max(P.TopAt(B), RowTop);
			const float BotA = FMath::Min(P.BotAt(A), RowBot), BotB = FMath::Min(P.BotAt(B), RowBot);
			if (TopA >= BotA - 0.2f || TopB >= BotB - 0.2f) continue;
			TArray<FVector2f> Pts;
			TArray<FLinearColor> Cs;
			Pts.Add(Pos + Vec(A, TopA)); Pts.Add(Pos + Vec(B, TopB)); Pts.Add(Pos + Vec(B, BotB)); Pts.Add(Pos + Vec(A, BotA));
			Cs.Add(CT); Cs.Add(CT); Cs.Add(CB); Cs.Add(CB);
			C.Poly(Pts, Cs, Layer);
		}
	}

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
		V.StanceSwitchInterval = D->StanceSwitchInterval;
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

void UArenaCombatHudWidget::DetectFloats(const FArenaHudSideView& Prev, const FArenaHudSideView& NowView,
	AFighterCharacter* Fighter, bool bVictimIsOpponent, double Now)
{
	if (!NowView.bValid || !Prev.bValid || !IsValid(Fighter)) return;
	ATrainingGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
	if (GM == nullptr) return;

	// 接触统计按攻击方累计：本侧是被打的一方 → 看对手那一份（RecordContact 口径）
	FTrainingStats& Stats = bVictimIsOpponent ? GM->PlayerStats : GM->OpponentStats;
	FArenaHudSideAnim& A = bVictimIsOpponent ? PlayerAnim : OpponentAnim;
	if (!A.bStatsInit)
	{
		A.bStatsInit = true;
		A.PrevResolvedDamage = Stats.ResolvedDamage;
		A.PrevGuards = Stats.Guards;
		A.PrevImmunes = Stats.Immunes;
		return;
	}

	const FVector Anchor = Fighter->GetActorLocation() + FVector(0.f, 0.f, 130.f);
	const float Dmg = Stats.ResolvedDamage - A.PrevResolvedDamage;
	if (Dmg > 0.5f && !NowView.bDead)
	{
		SpawnFloat(Anchor, FString::Printf(TEXT("%.0f"), Dmg),
			Dmg >= 100.f ? FArenaHudPalette::Gold : FLinearColor::White,
			Dmg >= 100.f ? 28.f : 20.f, Now);
	}
	if (Stats.Guards > A.PrevGuards) SpawnFloat(Anchor, TEXT("防御"), FArenaHudPalette::Aim, 15.f, Now);
	if (Stats.Immunes > A.PrevImmunes) SpawnFloat(Anchor, TEXT("免疫"), FArenaHudPalette::Muted, 15.f, Now);
	A.PrevResolvedDamage = Stats.ResolvedDamage;
	A.PrevGuards = Stats.Guards;
	A.PrevImmunes = Stats.Immunes;
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

	const FArenaHudSideView PrevPlayer = PlayerView;
	const FArenaHudSideView PrevOpponent = OpponentView;
	SnapshotSide(true, GM->GetPlayerFighter(), GM, PlayerView);
	SnapshotSide(false, GM->GetOpponentFighter(), GM, OpponentView);
	UpdateSideAnim(PlayerView, PlayerAnim, Now, InDeltaTime);
	UpdateSideAnim(OpponentView, OpponentAnim, Now, InDeltaTime);

	DetectFloats(PrevOpponent, OpponentView, GM->GetOpponentFighter(), true, Now);
	DetectFloats(PrevPlayer, PlayerView, GM->GetPlayerFighter(), false, Now);

	// 闪避成功闪示（只玩家侧）
	if (World->GetTimeSeconds() - PlayerAnim.DodgeFlashTime >= 0.0 &&
		World->GetTimeSeconds() - PlayerAnim.DodgeFlashTime < InDeltaTime + 0.02 &&
		IsValid(GM->GetPlayerFighter()))
	{
		SpawnFloat(GM->GetPlayerFighter()->GetActorLocation() + FVector(0.f, 0.f, 150.f),
			TEXT("闪避"), FArenaHudPalette::Aim, 15.f, Now);
	}
}

// ---------- 绘制主入口 ----------
int32 UArenaCombatHudWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateFontInfo BaseFont(UEngine::GetMediumFont(), 12.f);
	FArenaHudCanvas C(AllottedGeometry, OutDrawElements, BaseFont);
	const double Now = LastTime;

	int32 L = LayerId;
	DrawTopBand(C, L++);
	if (PlayerView.bValid) DrawFighterSide(C, true, PlayerView, PlayerAnim, Now, L++);
	if (OpponentView.bValid) DrawFighterSide(C, false, OpponentView, OpponentAnim, Now, L++);
	DrawCenterStatus(C, PlayerView, PlayerAnim, OpponentView, Now, L++);
	if (PlayerView.bValid)
	{
		DrawSkillArea(C, PlayerView, PlayerAnim, Now, L++);
		DrawFormAndCharge(C, PlayerView, PlayerAnim, Now, L++);
		DrawCrosshairChargeRing(C, L++);
	}
	DrawFloats(C, Now, L++);
	DrawKeyHints(C, L++);
	DrawResult(C, Now, L++);
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

void UArenaCombatHudWidget::DrawPointedBar(const FArenaHudCanvas& C, const FVector2f& Pos, float W, float H,
	float LeftSlant, float TipLen, float Fill, float Ghost, const FLinearColor Seg[5], bool bRightAnchor,
	double NowT, int32 Layer, bool bLowPulse) const
{
	// 低生命呼吸：整条外圈红光（1.2s ease-in-out 呼吸，§7）
	if (bLowPulse)
	{
		const float Pulse = 0.5f - 0.5f * FMath::Cos(FMath::Fmod(NowT, 1.2) / 1.2 * 2.f * PI);
		TArray<FVector2f> Glow;
		Glow.Add(Pos + Vec(LeftSlant - 2.f, -2.f));
		Glow.Add(Pos + Vec(W - TipLen + 2.f, -2.f));
		Glow.Add(Pos + Vec(W + 4.f, H * 0.5f));
		Glow.Add(Pos + Vec(W - TipLen + 2.f, H + 2.f));
		Glow.Add(Pos + Vec(-2.f, H + 2.f));
		Glow.Add(Pos + Vec(LeftSlant - 2.f, -2.f));
		C.Lines(Glow, WithAlpha(FArenaHudPalette::Danger, 0.35f + 0.45f * Pulse), 4.f, Layer);
	}

	// 背景（外管：冷白高光 → 暗管底的竖向渐变）
	{
		TArray<FVector2f> Pts;
		Pts.Add(Pos + Vec(LeftSlant, 0.f)); Pts.Add(Pos + Vec(W - TipLen, 0.f)); Pts.Add(Pos + Vec(W, H * 0.5f));
		Pts.Add(Pos + Vec(W - TipLen, H)); Pts.Add(Pos + Vec(0.f, H));
		TArray<FLinearColor> Cs;
		const FLinearColor TopLight(0.75f, 0.83f, 0.95f, 0.40f);
		const FLinearColor BotDark(0.04f, 0.08f, 0.16f, 0.60f);
		for (const FVector2f& Pt : Pts) Cs.Add(FMath::Lerp(TopLight, BotDark, (Pt.Y - Pos.Y) / H));
		C.Poly(Pts, Cs, Layer);
	}
	// 内槽（暗底）
	const FBarProfile Inner = MakeProfile(W, H, LeftSlant, TipLen);
	{
		TArray<FVector2f> Pts;
		Pts.Add(Pos + Vec(Inner.L, Inner.T)); Pts.Add(Pos + Vec(Inner.SlantEnd, Inner.T)); Pts.Add(Pos + Vec(Inner.R, Inner.MidY));
		Pts.Add(Pos + Vec(Inner.SlantEnd, Inner.B)); Pts.Add(Pos + Vec(Inner.L, Inner.B));
		C.Poly(Pts, { FArenaHudPalette::T0, FArenaHudPalette::T0, FArenaHudPalette::T1, FArenaHudPalette::T1, FArenaHudPalette::T0 }, Layer + 1);
	}

	// 填充按锚定侧算内槽宽度（§8.3：右锚定条按内层测量）；虚血残影在填充后方延伸到 Union 区间
	const float InnerW = Inner.R - Inner.L;
	const float FillEdge = bRightAnchor ? Inner.R - InnerW * FMath::Clamp(Fill, 0.f, 1.f)
		: Inner.L + InnerW * FMath::Clamp(Fill, 0.f, 1.f);
	const float GhostEdge = bRightAnchor ? Inner.R - InnerW * FMath::Clamp(Ghost, 0.f, 1.f)
		: Inner.L + InnerW * FMath::Clamp(Ghost, 0.f, 1.f);
	// 左锚定 = [内槽左缘, max(填充缘, 虚血缘)]；右锚定 = [min(...), 内槽右缘]
	const float FillLo = bRightAnchor ? FMath::Min(FillEdge, GhostEdge) : Inner.L;
	const float FillHi = bRightAnchor ? Inner.R : FMath::Max(FillEdge, GhostEdge);

	// 虚血残影在填充后面（画整个 Union 区间，填充随后盖住重叠部分）
	if (FMath::Abs(GhostEdge - FillEdge) > 0.8f)
	{
		DrawFillBand(C, Pos, Inner, FillLo, FillHi, 0.f, 1.f,
			FArenaHudPalette::GhostSeg[0], FArenaHudPalette::GhostSeg[1], Layer + 2);
	}
	// 五段管体填充（亮带固定约 38% 高度，§3.1 实测；5 个色阶 = 4 段线性）；只覆盖填充自身区间
	const float BandLo = bRightAnchor ? FillEdge : Inner.L;
	const float BandHi = bRightAnchor ? Inner.R : FillEdge;
	constexpr float Stops[5] = { 0.f, 0.17f, 0.38f, 0.62f, 1.f };
	for (int32 i = 0; i < 4; ++i)
	{
		DrawFillBand(C, Pos, Inner, BandLo, BandHi, Stops[i], Stops[i + 1], Seg[i], Seg[i + 1], Layer + 3);
	}
	// 前沿光刃（2px 白 + 青白柔光）
	if (Fill > 0.004f)
	{
		const float EdgeX = FMath::Clamp(FillEdge, Inner.L, Inner.R);
		const float Top = Inner.TopAt(EdgeX), Bot = Inner.BotAt(EdgeX);
		C.Lines({ Pos + Vec(EdgeX, Top), Pos + Vec(EdgeX, Bot) }, FLinearColor(0.62f, 0.91f, 0.96f, 0.35f), 6.f, Layer + 4);
		C.Lines({ Pos + Vec(EdgeX, Top), Pos + Vec(EdgeX, Bot) }, FLinearColor(1.f, 1.f, 1.f, 0.9f), 2.f, Layer + 4);
	}
	// 外框 1px 冷白描边
	{
		TArray<FVector2f> Pts;
		Pts.Add(Pos + Vec(LeftSlant, 0.f)); Pts.Add(Pos + Vec(W - TipLen, 0.f)); Pts.Add(Pos + Vec(W, H * 0.5f));
		Pts.Add(Pos + Vec(W - TipLen, H)); Pts.Add(Pos + Vec(0.f, H));
		C.Lines(Pts, FLinearColor(1.f, 1.f, 1.f, 0.42f), 1.f, Layer + 5, true);
	}
}

void UArenaCombatHudWidget::DrawCurseGate(const FArenaHudCanvas& C, const FVector2f& BarPos, float W, float H,
	float ThresholdFraction, bool bRightAnchor, int32 Layer) const
{
	const float X = BarPos.X + (bRightAnchor ? W * (1.f - ThresholdFraction) : W * ThresholdFraction);
	const float Y0 = BarPos.Y - 4.f, Y1 = BarPos.Y + H + 4.f;
	// 白热描边负责对比、红色负责危险语义（§3.1 8 点门槛刻度；上下各出头 4px）
	C.Lines({ Vec(X, Y0), Vec(X, Y1) }, FLinearColor(1.f, 0.956f, 0.941f, 0.92f), 4.f, Layer);
	C.Lines({ Vec(X, Y0), Vec(X, Y1) }, FArenaHudPalette::Danger, 2.f, Layer + 1);
	C.Poly({ Vec(X - 4.f, Y1), Vec(X + 4.f, Y1), Vec(X, Y1 + 4.f) },
		{ FArenaHudPalette::Danger, FArenaHudPalette::Danger, FArenaHudPalette::Danger }, Layer + 1);
	C.Poly({ Vec(X - 4.f, Y0), Vec(X + 4.f, Y0), Vec(X, Y0 - 4.f) },
		{ FArenaHudPalette::Danger, FArenaHudPalette::Danger, FArenaHudPalette::Danger }, Layer + 1);
}

void UArenaCombatHudWidget::DrawActionPips(const FArenaHudCanvas& C, const FVector2f& BarRightEnd,
	const FArenaHudSideView& V, const FArenaHudSideAnim& A, double NowT, int32 Layer) const
{
	const float RightX = BarRightEnd.X;
	const float HalfDiag = 9.2f;   // 13×13 菱形豆的半对角（§3.3 尺寸待决项取 13）
	const int32 Count = FMath::Clamp(FMath::FloorToInt32(V.MaxAction), 1, 5);
	const float SpendPulse = FMath::Clamp(1.f - (NowT - A.PipSpendTime) / 0.45, 0.0, 1.0);

	C.Text(TEXT("行动"), Vec(RightX - StackW, PipsY - 7.f), 9.f, FArenaHudPalette::T4, Layer, EArenaHudAlign::Left);
	C.Text(FString::FromInt(A.PipCount),
		Vec(RightX - 3.f - HalfDiag - (Count - 1) * 24.f - 14.f, PipsY - 8.f), 12.f,
		A.PipCount == 0 ? FArenaHudPalette::Danger : WithAlpha(FArenaHudPalette::T5, 0.86f), Layer, EArenaHudAlign::Right);

	for (int32 i = 0; i < Count; ++i)
	{
		const FVector2f Center(RightX - 3.f - HalfDiag - (Count - 1 - i) * 24.f, PipsY);
		const bool bOn = i < A.PipCount;

		// 外框菱形：左上受光、右下背光
		TArray<FVector2f> Frame;
		DiamondPts(Center, HalfDiag, Frame);
		C.Poly(Frame, { FLinearColor(1, 1, 1, .40f), FLinearColor(.59f, .75f, 1, .12f), FLinearColor(0, 0, 0, .55f), FLinearColor(.59f, .75f, 1, .12f) }, Layer);
		// 内面：满 = 行动金管体，空 = 暗灰阶（满/空靠"有没有光"区分，§3.3）
		const float H2 = HalfDiag - 1.4f;
		TArray<FVector2f> Face;
		DiamondPts(Center, H2, Face);
		TArray<FLinearColor> FaceColors;
		if (bOn)
		{
			FaceColors = { FArenaHudPalette::ActionSeg[1], FArenaHudPalette::ActionSeg[2], FArenaHudPalette::ActionSeg[3], FArenaHudPalette::ActionSeg[1] };
		}
		else
		{
			FaceColors = { FArenaHudPalette::PipOffSeg[0], FArenaHudPalette::PipOffSeg[1], FArenaHudPalette::PipOffSeg[2], FArenaHudPalette::PipOffSeg[0] };
		}
		C.Poly(Face, FaceColors, Layer + 1);
		// 宝石高光：左上小三角
		if (bOn)
		{
			C.Poly({ Center + Vec(0.f, -H2 + 1.8f), Center + Vec(H2 - 1.8f, 0.f), Center + Vec(-H2 + 1.8f, 0.f) },
				{ FLinearColor(1, 1, 1, .85f), FLinearColor(1, 1, 1, 0.f), FLinearColor(1, 1, 1, 0.f) }, Layer + 2);
		}
		// 刚被扣掉的豆：0.45s 泄光，把"扣 2 点"读成一个事件（§3.3）
		if (!bOn && SpendPulse > 0.01f)
		{
			TArray<FLinearColor> Flash;
			for (int32 K = 0; K < 4; ++K) Flash.Add(FLinearColor(1, 1, 1, 0.55f * SpendPulse));
			C.Poly(Face, Flash, Layer + 2);
		}
	}
}

void UArenaCombatHudWidget::DrawPortrait(const FArenaHudCanvas& C, const FVector2f& Pos,
	const FArenaHudSideView& V, bool bLeft, double NowT, int32 Layer) const
{
	constexpr float S = PortraitS, Cut = 12.f;
	// 四角切角方框（切角 12px 安全区，§10.1）
	auto CutPoly = [Pos, S, Cut](float Inset, TArray<FVector2f>& Out)
	{
		const float L = Pos.X + Inset, T = Pos.Y + Inset, R = Pos.X + S - Inset, B = Pos.Y + S - Inset;
		const float K = Cut - Inset;
		Out.Reset();
		Out.Add(Vec(L + K, T)); Out.Add(Vec(R - K, T));
		Out.Add(Vec(R, T + K)); Out.Add(Vec(R, B - K));
		Out.Add(Vec(R - K, B)); Out.Add(Vec(L + K, B));
		Out.Add(Vec(L, B - K)); Out.Add(Vec(L, T + K));
	};

	// 三层框：外细线渐变 / 深色带 / 暗像底
	{
		TArray<FVector2f> Pts;
		CutPoly(0.f, Pts);
		const FLinearColor TL(1, 1, 1, .34f), Mid(.59f, .75f, 1, .14f), BR(1, 1, 1, .16f);
		TArray<FLinearColor> Cs;
		for (const FVector2f& Pt : Pts)
		{
			const float K = ((Pt.X - Pos.X) + (Pt.Y - Pos.Y)) / (2.f * S);
			Cs.Add(K < 0.5f ? FMath::Lerp(TL, Mid, K * 2.f) : FMath::Lerp(Mid, BR, (K - 0.5f) * 2.f));
		}
		C.Poly(Pts, Cs, Layer);
	}
	{
		TArray<FVector2f> Pts;
		CutPoly(1.f, Pts);
		C.Poly(Pts, { FArenaHudPalette::T1, FArenaHudPalette::T1, FArenaHudPalette::T0, FArenaHudPalette::T0, FArenaHudPalette::T0, FArenaHudPalette::T0, FArenaHudPalette::T1, FArenaHudPalette::T1 }, Layer + 1);
	}
	{
		TArray<FVector2f> Pts;
		CutPoly(3.f, Pts);
		C.Poly(Pts, { FArenaHudPalette::T2, FArenaHudPalette::T2, FArenaHudPalette::T0, FArenaHudPalette::T0, FArenaHudPalette::T0, FArenaHudPalette::T0, FArenaHudPalette::T2, FArenaHudPalette::T2 }, Layer + 2);
	}

	// 身份色边光条（P1 左缘 / P2 右缘）
	const float EdgeX = bLeft ? Pos.X + 3.f : Pos.X + S - 5.f;
	C.Box(Vec(EdgeX - (bLeft ? 3.f : -3.f), Pos.Y + 8.f), Vec(8.f, S - 16.f), WithAlpha(V.Identity, 0.16f), Layer + 2);
	C.Box(Vec(EdgeX, Pos.Y + 8.f), Vec(2.f, S - 16.f), V.Identity, Layer + 3);

	// 斜向高光（光自左上 45°）
	C.Poly({ Pos + Vec(Cut, 0.f), Pos + Vec(34.f, 0.f), Pos + Vec(6.f, 36.f) },
		{ FLinearColor(1, 1, 1, .16f), FLinearColor(1, 1, 1, .05f), FLinearColor(1, 1, 1, 0.f) }, Layer + 4);

	// 占位字符（正式头像 512×512 资产就位后移除，§10.1）
	if (V.Name.Len() > 0)
	{
		C.Text(V.Name.Left(1), Vec(Pos.X + S * 0.5f, Pos.Y + S * 0.5f - 17.f), 30.f, WithAlpha(FArenaHudPalette::Fg, 0.22f), Layer + 4, EArenaHudAlign::Center);
	}

	// P1/P2 角标
	const float TagW = 22.f;
	const float TagX = bLeft ? Pos.X + 3.f : Pos.X + S - 3.f - TagW;
	C.Box(Vec(TagX, Pos.Y + S - 18.f), Vec(TagW, 15.f), FLinearColor(0.012f, 0.024f, 0.047f, 0.9f), Layer + 5);
	C.Text(bLeft ? TEXT("P1") : TEXT("P2"), Vec(TagX + TagW * 0.5f, Pos.Y + S - 18.f + 2.f), 10.f, V.Identity, Layer + 6, EArenaHudAlign::Center);

	// 双领域受压制的头像外圈转紫呼吸（§3.7）
	if (V.bHasDomain && V.bDomainSuppressed)
	{
		const float Breath = 0.62f + 0.38f * (0.5f - 0.5f * FMath::Cos(FMath::Fmod(NowT, 1.6) / 1.6 * 2.f * PI));
		TArray<FVector2f> RingPts;
		const float Ex = 5.f;
		RingPts.Add(Pos + Vec(-Ex + Cut, -Ex));
		RingPts.Add(Pos + Vec(S + Ex - Cut, -Ex));
		RingPts.Add(Pos + Vec(S + Ex, -Ex + Cut));
		RingPts.Add(Pos + Vec(S + Ex, S + Ex - Cut));
		RingPts.Add(Pos + Vec(S + Ex - Cut, S + Ex));
		RingPts.Add(Pos + Vec(-Ex + Cut, S + Ex));
		RingPts.Add(Pos + Vec(-Ex, S + Ex - Cut));
		RingPts.Add(Pos + Vec(-Ex, -Ex + Cut));
		RingPts.Add(Pos + Vec(-Ex + Cut, -Ex));
		C.Lines(RingPts, WithAlpha(FArenaHudPalette::DomAct, Breath * 0.85f), 2.f, Layer + 5);
	}
}

void UArenaCombatHudWidget::DrawFighterSide(const FArenaHudCanvas& C, bool bLeft, const FArenaHudSideView& V,
	const FArenaHudSideAnim& A, double NowT, int32 Layer) const
{
	const float ViewW = DesignWidth(C);
	const float RightX = bLeft ? IdentX + PortraitS + IdentGap + StackW : ViewW - IdentX;   // 条右端 635 / W-32
	const float X0 = bLeft ? IdentX : RightX - (PortraitS + IdentGap + StackW);
	const float StackX = X0 + PortraitS + IdentGap;
	const float StackRight = StackX + StackW;

	DrawPortrait(C, Vec(X0, IdentY), V, bLeft, NowT, Layer);
	int32 L = Layer + 8;

	// ---- 名字行：名 / 形态标签 / 咒力状态标签 / 生命数值 ----
	const bool bRanged = V.Stance == EFighterStance::Ranged;
	const float NameY = IdentY + 1.f;
	const FVector2f NameSize = C.Measure(V.Name, 14.5f);
	if (bLeft)
	{
		C.Text(V.Name, Vec(StackX, NameY), 14.5f, FArenaHudPalette::Fg, L, EArenaHudAlign::Left);
		float Cur = StackX + NameSize.X + 9.f;
		Cur += Chip(C, Vec(Cur, NameY + 1.f), 15.f, bRanged ? TEXT("远程 / 咒力放出") : TEXT("近战 / 咒力放出"),
			9.f, FArenaHudPalette::T4, FArenaHudPalette::T3, WithAlpha(FArenaHudPalette::T2, 0.85f), L, EArenaHudAlign::Left) + 7.f;
		// 咒力状态：已满 / 回气中 / 不足（warn2 红底）
		const bool bDry = V.Curse < V.CurseMinCost;
		const bool bFull = V.Curse >= V.MaxCurse - 0.5f;
		const FString Note = bFull ? TEXT("咒力已满") : bDry ? TEXT("咒力不足 · 无法开炮")
			: FString::Printf(TEXT("回气中 +%.0f/s"), V.CurseRegenPerSec);
		Chip(C, Vec(Cur, NameY + 1.f), 15.f, Note, 9.f,
			bDry ? FArenaHudPalette::Danger2 : FArenaHudPalette::T4,
			bDry ? WithAlpha(FArenaHudPalette::Danger, 0.7f) : FArenaHudPalette::T3,
			bDry ? WithAlpha(FLinearColor(0.35f, 0.08f, 0.05f), 0.92f) : WithAlpha(FArenaHudPalette::T2, 0.85f), L, EArenaHudAlign::Left);
		// 生命数值（右对齐；受伤 0.18s 放大 1.18× 高亮）
		const float Bump = EaseOut(1.f - FMath::Clamp((NowT - A.HpBumpTime) / 0.18, 0.0, 1.0));
		const FString Suffix = FString::Printf(TEXT(" / %.0f"), V.MaxHealth);
		const FString Main = FString::Printf(TEXT("%.0f"), V.Health);
		const FVector2f SufSize = C.Measure(Suffix, 12.f);
		const float MainSize = 15.f * (1.f + 0.18f * Bump);
		C.Text(Suffix, Vec(StackRight, NameY + 3.f), 12.f, FArenaHudPalette::T4, L, EArenaHudAlign::Right);
		C.Text(Main, Vec(StackRight - SufSize.X, NameY + 1.f), MainSize,
			Bump > 0.01f ? FLinearColor(1, 1, 1, .98f) : WithAlpha(FArenaHudPalette::Fg, 0.92f), L, EArenaHudAlign::Right);
	}
	else
	{
		C.Text(V.Name, Vec(StackRight, NameY), 14.5f, FArenaHudPalette::Fg, L, EArenaHudAlign::Right);
		float Cur = StackRight - NameSize.X - 9.f;
		Cur -= Chip(C, Vec(Cur, NameY + 1.f), 15.f, bRanged ? TEXT("远程 / 咒力放出") : TEXT("近战 / 咒力放出"),
			9.f, FArenaHudPalette::T4, FArenaHudPalette::T3, WithAlpha(FArenaHudPalette::T2, 0.85f), L, EArenaHudAlign::Right) + 7.f;
		const bool bDry = V.Curse < V.CurseMinCost;
		const bool bFull = V.Curse >= V.MaxCurse - 0.5f;
		const FString Note = bFull ? TEXT("咒力已满") : bDry ? TEXT("咒力不足 · 无法开炮")
			: FString::Printf(TEXT("回气中 +%.0f/s"), V.CurseRegenPerSec);
		Cur -= Chip(C, Vec(Cur, NameY + 1.f), 15.f, Note, 9.f,
			bDry ? FArenaHudPalette::Danger2 : FArenaHudPalette::T4,
			bDry ? WithAlpha(FArenaHudPalette::Danger, 0.7f) : FArenaHudPalette::T3,
			bDry ? WithAlpha(FLinearColor(0.35f, 0.08f, 0.05f), 0.92f) : WithAlpha(FArenaHudPalette::T2, 0.85f), L, EArenaHudAlign::Right) + 7.f;
		const float Bump = EaseOut(1.f - FMath::Clamp((NowT - A.HpBumpTime) / 0.18, 0.0, 1.0));
		// 镜像布局：当前值"1000 /"在左（随受伤放大），上限" 1000"跟在其后
		const FString Main = FString::Printf(TEXT("%.0f /"), V.Health);
		const FString Tail = FString::Printf(TEXT(" %.0f"), V.MaxHealth);
		const float MainSize = 15.f * (1.f + 0.18f * Bump);
		const FVector2f MainS = C.Measure(Main, MainSize);
		C.Text(Main, Vec(StackX, NameY + 1.f), MainSize,
			Bump > 0.01f ? FLinearColor(1, 1, 1, .98f) : WithAlpha(FArenaHudPalette::Fg, 0.92f), L, EArenaHudAlign::Left);
		C.Text(Tail, Vec(StackX + MainS.X, NameY + 3.f), 12.f, FArenaHudPalette::T4, L, EArenaHudAlign::Left);
	}
	L += 2;

	// ---- 生命条 / 咒力条（零间隙贴合，两条同族尖尾形制） ----
	const bool bLowHp = V.Health / V.MaxHealth < 0.3f;
	DrawPointedBar(C, Vec(StackX, HpY), StackW, HpH, HpSlant, HpTip, A.HpFillShown, A.GhostShown, FArenaHudPalette::HpSeg, !bLeft, NowT, L, bLowHp);
	L += 7;
	DrawPointedBar(C, Vec(StackX, CurseY), StackW, CurseH, CurseSlant, CurseTip, A.CurseShown, A.CurseShown, FArenaHudPalette::CurseSeg, !bLeft, NowT, L);
	L += 7;
	// 8 点门槛刻度（决策信息：低于该值无法开炮）
	DrawCurseGate(C, Vec(StackX, CurseY), StackW, CurseH, FMath::Clamp(V.CurseMinCost / V.MaxCurse, 0.f, 1.f), !bLeft, L);
	L += 3;

	// ---- 行动点（只玩家侧；右对齐到条右端） ----
	if (bLeft)
	{
		DrawActionPips(C, Vec(StackRight, PipsY), V, A, NowT, L);
	}
}

// ---------- 顶部中央：模式徽标 + 领域状态文字（§3.7；一个字都不少） ----------
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
		C.Text(TEXT("领域展开 · 极宴流光"), Vec(CX, 52.f), 27.f, FArenaHudPalette::Gold, Layer, EArenaHudAlign::Center);
		// 饰线 + 两端菱形
		C.Lines({ Vec(CX - 230.f, 88.f), Vec(CX + 230.f, 88.f) }, WithAlpha(FArenaHudPalette::Gold, 0.5f), 1.f, Layer);
		C.Disc(Vec(CX - 233.f, 88.f), 3.5f, FArenaHudPalette::Gold, Layer);
		C.Disc(Vec(CX + 233.f, 88.f), 3.5f, FArenaHudPalette::Gold, Layer);

		const bool bSupp = Player.bDomainSuppressed;
		// 倒计时
		C.Text(FString::Printf(TEXT("%.1f"), Player.DomainRemain), Vec(CX - 160.f, 92.f), 18.f, FArenaHudPalette::Fg, Layer + 1, EArenaHudAlign::Right);
		// 发炮刻度条（300×12）
		const float BarL = CX - 150.f, BarW = 300.f;
		C.Lines({ Vec(BarL, 103.f), Vec(BarL + BarW, 103.f) }, WithAlpha(FArenaHudPalette::T4, 0.22f), 2.f, Layer);
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
			DiamondPts(Vec(Tx, 103.f), 4.5f, D);
			TArray<FLinearColor> Cs;
			for (int32 K = 0; K < 4; ++K) Cs.Add(Fill);
			C.Poly(D, Cs, Layer + 1);
			C.Lines(D, Border, 1.f, Layer + 2, true);
		}
		// 进度游标
		const float Elapsed = FMath::Clamp(1.f - Player.DomainRemain / Player.DomainDuration, 0.f, 1.f);
		C.Lines({ Vec(BarL + BarW * Elapsed, 95.f), Vec(BarL + BarW * Elapsed, 111.f) }, FArenaHudPalette::Gold, 2.f, Layer + 2);
		// 下一发 / 压制文案
		if (bSupp)
		{
			C.Text(TEXT("自动炮暂停"), Vec(CX + 160.f, 96.f), 11.f, FArenaHudPalette::Muted, Layer + 1, EArenaHudAlign::Left);
		}
		else if (Player.NextOrbIn >= 0.f)
		{
			C.Text(Player.bNextOrbSkipped ? TEXT("咒力不足 · 本发跳过") : FString::Printf(TEXT("下一发 %.1fs"), Player.NextOrbIn),
				Vec(CX + 160.f, 96.f), 11.f, Player.bNextOrbSkipped ? WithAlpha(FArenaHudPalette::Danger, 0.9f) : WithAlpha(FArenaHudPalette::Gold, 0.95f), Layer + 1, EArenaHudAlign::Left);
		}
		else
		{
			C.Text(TEXT("本轮结束"), Vec(CX + 160.f, 96.f), 11.f, FArenaHudPalette::Muted, Layer + 1, EArenaHudAlign::Left);
		}
		// 在途球数
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector2f O(CX + 252.f + i * 14.f, 103.f);
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
		C.Lines({ Vec(CX - 260.f, 127.f), Vec(CX + 260.f, 127.f) }, WithAlpha(FArenaHudPalette::DomAct, 0.5f * Breath), 2.f, Layer);
		for (int32 i = -1; i <= 1; ++i)
		{
			TArray<FVector2f> D;
			DiamondPts(Vec(CX + i * 40.f, 127.f), 4.5f, D);
			C.Lines(D, WithAlpha(FArenaHudPalette::DomAct, 0.85f * Breath), 1.5f, Layer + 1, true);
		}
		C.Lines({ Vec(CX - 260.f, 123.f), Vec(CX - 260.f, 131.f) }, WithAlpha(FArenaHudPalette::DomAct, 0.7f), 2.f, Layer);
		C.Lines({ Vec(CX + 260.f, 123.f), Vec(CX + 260.f, 131.f) }, WithAlpha(FArenaHudPalette::DomAct, 0.7f), 2.f, Layer);
		const FString Banner = TEXT("领域受压制 · 自动炮暂停");
		const FVector2f Bs = C.Measure(Banner, 11.5f);
		C.Box(Vec(CX - Bs.X * 0.5f - 11.f, 138.f), Vec(Bs.X + 22.f, 19.f), FLinearColor(0.031f, 0.055f, 0.102f, 0.8f), Layer);
		C.Text(Banner, Vec(CX, 140.5f), 11.5f, FLinearColor(0.765f, 0.8f, 0.855f, 1.f), Layer + 1, EArenaHudAlign::Center);
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
	const float SwitchDur = P.StanceSwitchInterval > 0.05f ? P.StanceSwitchInterval : 0.5f;
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
		const bool bCool = Bi == 1 && bSuperCd;

		// 外圈（受光环）
		C.Disc(Center, B.R, bUse ? FLinearColor(1.f, 0.965f, 0.69f, 0.9f)
			: bUlt ? (bReady ? WithAlpha(FArenaHudPalette::Deng3, 0.85f) : FLinearColor(0.886f, 0.965f, 1.f, 0.6f))
			: FLinearColor(0.75f, 0.83f, 0.95f, 0.55f), Layer);
		// 内盘（左上受光的暗盘）
		C.Disc(Center, B.R - 1.6f, bCool ? FLinearColor(0.07f, 0.1f, 0.16f, 0.98f) : FLinearColor(0.08f, 0.12f, 0.19f, 0.99f), Layer + 1);
		C.Disc(Center + Vec(-B.R * 0.2f, -B.R * 0.28f), (B.R - 2.f) * 0.8f, FLinearColor(0.1f, 0.15f, 0.24f, 0.9f), Layer + 2);
		C.Disc(Center + Vec(-B.R * 0.2f, -B.R * 0.3f), (B.R - 2.f) * 0.42f, FLinearColor(1.f, 1.f, 1.f, 0.05f), Layer + 3);

		// 图标（只有图标，没有技能名文字 —— §3.4 明确设计决定）
		const FLinearColor IconCol = bUse ? FArenaHudPalette::Chg
			: bUlt ? (bReady ? FArenaHudPalette::Deng3 : WithAlpha(FArenaHudPalette::Fg, 0.85f))
			: bCool ? WithAlpha(FArenaHudPalette::Fg, 0.3f) : FArenaHudPalette::Fg;
		DrawArenaHudIcon(C, B.Icon, Center, bUlt ? 46.f : 38.f, IconCol, Layer + 4);

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
			C.Box(Vec(B.X - 14.f, SkillY + B.R + 1.5f), Vec(28.f, 15.f), FLinearColor(0.012f, 0.024f, 0.047f, 0.94f), Layer + 5);
			TArray<FVector2f> Outline;
			Outline.Add(Vec(B.X - 13.5f, SkillY + B.R + 2.f)); Outline.Add(Vec(B.X + 13.5f, SkillY + B.R + 2.f));
			Outline.Add(Vec(B.X + 13.5f, SkillY + B.R + 16.f)); Outline.Add(Vec(B.X - 13.5f, SkillY + B.R + 16.f));
			C.Lines(Outline, FLinearColor(0.59f, 0.75f, 1.f, 0.3f), 1.f, Layer + 6, true);
			DrawArenaHudIcon(C, FName("mouse"), Vec(B.X, SkillY + B.R + 9.f), 16.f, FArenaHudPalette::Fg, Layer + 6);
		}
		else
		{
			const FString KeyStr = B.Key;
			const FVector2f Ks = C.Measure(KeyStr, 11.f);
			const float Kw = Ks.X + 12.f;
			C.Box(Vec(B.X - Kw * 0.5f, SkillY + B.R + 1.5f), Vec(Kw, 15.f), FLinearColor(0.012f, 0.024f, 0.047f, 0.94f), Layer + 5);
			TArray<FVector2f> Outline;
			Outline.Add(Vec(B.X - Kw * 0.5f + 0.5f, SkillY + B.R + 2.f)); Outline.Add(Vec(B.X + Kw * 0.5f - 0.5f, SkillY + B.R + 2.f));
			Outline.Add(Vec(B.X + Kw * 0.5f - 0.5f, SkillY + B.R + 16.f)); Outline.Add(Vec(B.X - Kw * 0.5f + 0.5f, SkillY + B.R + 16.f));
			C.Lines(Outline, FLinearColor(0.59f, 0.75f, 1.f, 0.3f), 1.f, Layer + 6, true);
			const FLinearColor KeyCol = (bUlt && bReady) ? FArenaHudPalette::Deng2 : FArenaHudPalette::Fg;
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
	const float Q = bCharging ? FMath::Clamp((NowT - A.ChargeStartTime - P.ChargeGate) / P.ChargeCap, 0.f, 1.f) : 0.f;
	const float QPaid = bCharging ? FMath::Clamp(P.ChargePaidQ, 0.f, 1.f) : 0.f;
	const bool bLimited = bCharging && QPaid < Q - 0.01f;
	const bool bFull = bCharging && Q >= 1.f && !bLimited;
	const float Damage = P.ChargeMinDmg + (P.ChargeMaxDmg - P.ChargeMinDmg) * QPaid;

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
	DrawArenaHudIcon(C, bRanged ? FName("blast") : FName("punch"), Center + Vec(0.f, -10.f), 32.f, WithAlpha(FArenaHudPalette::Fg, IconAlpha), Layer + 3);
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
			if (bLimited)
			{
				// 受限态读数恒定在支付上限，任何持炮时长都不出现更低的数字（§6）
				Main = FString::Printf(TEXT("强度上限 %.0f"), Damage);
				Sub = TEXT("咒力不足 · 继续持炮不会变强");
			}
			else
			{
				Main = FString::Printf(TEXT("伤害 %.0f"), Damage);
				Sub = FString::Printf(TEXT("蓄力 %.2fs · 已支付 %.0f 咒力"),
					FMath::Min((NowT - A.ChargeStartTime), P.ChargeGate + P.ChargeCap),
					FMath::Min(P.ChargeMinCost + (P.ChargeMaxCost - P.ChargeMinCost) * QPaid, P.Curse));
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

