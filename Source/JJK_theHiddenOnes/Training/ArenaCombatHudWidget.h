// 正式战斗 HUD（14_HUD开发指引.md，视觉基准 hud-prototype-v9.0.html）。
// 与调试层 UCombatHudWidget 并存：本类只读战斗状态、不拥有任何战斗数据；
// 连续量（虚血回落/蓄力环/倒计时/冷却扇形）在 Tick/Paint 按帧更新，不走 10Hz 定时器（§5.4）。

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Training/CombatTypes.h"
#include "Training/CombatFeedbackTypes.h"
#include "Training/ArenaHudPainter.h"
#include "ArenaCombatHudWidget.generated.h"

class ATrainingGameMode;
class AFighterCharacter;
class UTexture2D;
struct FArenaHudCanvas;
struct FDomainStatusView;

/** 单侧连续量的帧插值与事件观察状态（非反射数据，仅本控件持有） */
struct FArenaHudSideAnim
{
	// 生命填充/虚血（§7：即时 120ms ease-out；虚血 0.6s 延迟 + 350ms ease-out 回落）
	float HpFillShown = 1.f, HpFillFrom = 1.f, HpFillTarget = 1.f;
	float GhostShown = 1.f, GhostFrom = 1.f, GhostTarget = 1.f;
	double HpChangeTime = -100.0;
	float CurseShown = 1.f, CurseTarget = 1.f;
	bool bHpBump = false;
	double HpBumpTime = -100.0;
	// 行动豆（只玩家侧画；P2 同结构复用）
	int32 PipCount = 5;
	double PipSpendTime = -100.0;
	// 超级炮冷却观察：冷却时长以 Definition 为准，起始时刻由标签跳变推断
	bool bSuperCdSeen = false;
	double SuperCdStartTime = 0.0;
	// 蓄力观察：时间强度 q 与已支付 qPaid 分开，才能表达"受限"红橙段
	bool bChargingSeen = false;
	double ChargeStartTime = 0.0;
	// 形态切换（E）收束弧
	EFighterStance Stance = EFighterStance::Melee;
	bool bStanceInit = false;
	double StanceSwitchTime = -100.0;
	// 结印观察
	bool bCastingSeen = false;
	double CastStartTime = 0.0;
	// 闪避成功闪示
	double DodgeFlashTime = -100.0;
	bool bDodgePrev = false;
 int64 ChargeSession = 0;
};

/** 飘字条目：世界位置锚定 + 上飘淡出（§7 伤害数字 900ms ease-out） */
USTRUCT()
struct FArenaHudFloatItem
{
	GENERATED_BODY()

	UPROPERTY() FVector WorldPos = FVector::ZeroVector;
	UPROPERTY() FString Text;
	UPROPERTY() FLinearColor Color = FLinearColor::White;
	UPROPERTY() float FontSize = 20.f;
	UPROPERTY() double BornTime = 0.0;
 TWeakObjectPtr<AFighterCharacter> Target;
 TWeakObjectPtr<AFighterCharacter> Source;
 ECombatFeedbackResult Result = ECombatFeedbackResult::None;
 float ActualDamage = 0.f;
};

/** 单侧战斗状态快照：NativeTick 采集，NativePaint 只读 */
struct FArenaHudSideView
{
	bool bValid = false;
	FString Name;
	FLinearColor Identity = FLinearColor::White;

	float Health = 1.f, MaxHealth = 1.f;
	float Curse = 0.f, MaxCurse = 1.f;
	float Action = 0.f, MaxAction = 5.f;
	float Energy = 0.f, MaxEnergy = 1.f;

	EFighterStance Stance = EFighterStance::Melee;
	bool bAttacking = false, bGuarding = false, bDodgeInvuln = false, bSprinting = false;
	bool bDead = false, bAiming = false, bChargingNow = false, bSuperCdNow = false;
	bool bCastingNow = false, bDomainActive = false;
	float ChargePaidQ = 0.f;
	bool bSuperBlastCharging = false;

	// 数值文案取自 Definition（调试参数，随配置走）
	float CurseRegenPerSec = 6.f;
	float CurseMinCost = 8.f;
	float ChargeCap = 1.2f, ChargeGate = 0.f, ChargeMinCost = 8.f, ChargeMaxCost = 24.f, ChargeMinDmg = 30.f, ChargeMaxDmg = 90.f;
	float SuperCdDuration = 10.f, StanceSwitchInterval = 0.5f, DomainCastTime = 1.f, DomainDuration = 6.f;
 float SuperMinCost = 50.f;
 bool bInfiniteResources = false;
	float MeleeDamage[3] = { 35.f, 40.f, 55.f };
	float HeavyPunchDamage = 75.f, KickDamage = 45.f, HeavyKickDamage = 95.f;

	// 领域会话只读视图（ATrainingGameMode::GetDomainStatusFor）
	bool bHasDomain = false, bDomainSuppressed = false, bNextOrbSkipped = false;
	float DomainRemain = 0.f, NextOrbIn = -1.f;
	int32 OrbsInFlight = 0, DomainTickIndex = 0;
};

/**
 * 正式战斗 HUD：P1/P2 身份资源块 + 顶部领域状态文字 + 技能区 + 形态圆盘/蓄力环 +
 * 飘字层 + 结果层。几何/颜色/动效规格全部来自 14_HUD开发指引.md。
 */
UCLASS()
class UArenaCombatHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UArenaCombatHudWidget(const FObjectInitializer& ObjectInitializer);
	/** 正式层显隐（默认 HitTestInvisible；JJKCombatHud 切换） */
	UFUNCTION(BlueprintCallable, Category = "Training|UI")
	void SetHudVisible(bool bVisible);
 void ConsumeContact(const FCombatContactFeedback& Event);
 void ConsumeAction(const FCombatActionFeedback& Event);
 void ConsumeLifecycle(const FCombatLifecycleFeedback& Event);
 void ClearFeedback(bool bRoundReset = false);
 UFUNCTION(BlueprintPure, Category="Feedback|HUD") FString GetFeedbackState() const;
 UFUNCTION(BlueprintPure, Category="Feedback|HUD") TArray<FString> GetFeedbackTexts() const;
 UPROPERTY(BlueprintReadOnly, Category="Feedback|HUD") int32 ContactPromptCount = 0;
 UPROPERTY(BlueprintReadOnly, Category="Feedback|HUD") int32 MergedPromptCount = 0;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UPROPERTY() TMap<FName, TObjectPtr<UTexture2D>> HudTextures;
	void DrawIcon(const FArenaHudCanvas& C, FName Id, const FVector2f& Center, float Size, const FLinearColor& Color, int32 Layer) const;
	// ---------- 采集 ----------
	void SnapshotSide(bool bPlayer, AFighterCharacter* Fighter, ATrainingGameMode* GM, FArenaHudSideView& OutView);
	void UpdateSideAnim(const FArenaHudSideView& View, FArenaHudSideAnim& Anim, double Now, double DeltaSeconds);
	void SpawnFloat(const FVector& WorldPos, const FString& Text, const FLinearColor& Color, float Size, double Now);
 float ChargeTimeQ(const FArenaHudSideView& View, const FArenaHudSideAnim& Anim, double Now) const;
 bool IsChargeLimited(const FArenaHudSideView& View, const FArenaHudSideAnim& Anim, double Now) const;

	// ---------- 绘制 ----------
	float DesignWidth(const FArenaHudCanvas& C) const;
	void DrawTopBand(const FArenaHudCanvas& C, int32 Layer) const;
	void DrawFighterSide(const FArenaHudCanvas& C, bool bLeft, const FArenaHudSideView& View, const FArenaHudSideAnim& Anim, double NowT, int32 Layer) const;
	void DrawPortrait(const FArenaHudCanvas& C, const FVector2f& Pos, const FArenaHudSideView& View, bool bLeft, double NowT, int32 Layer) const;
	void DrawPointedBar(const FArenaHudCanvas& C, const FVector2f& Pos, float Width, float Height, float LeftSlant, float TipLength,
		float FillFraction, float GhostFraction, const FLinearColor Seg[5], bool bRightAnchor, double NowT, int32 Layer, bool bLowPulse = false) const;
	void DrawCurseGate(const FArenaHudCanvas& C, const FVector2f& BarPos, float Width, float Height, float ThresholdFraction, bool bRightAnchor, int32 Layer) const;
	void DrawActionPips(const FArenaHudCanvas& C, const FVector2f& BarRightEnd, const FArenaHudSideView& View, const FArenaHudSideAnim& Anim, double NowT, int32 Layer) const;
	void DrawCenterStatus(const FArenaHudCanvas& C, const FArenaHudSideView& Player, const FArenaHudSideAnim& PlayerAnim,
		const FArenaHudSideView& Opponent, double NowT, int32 Layer) const;
	void DrawSkillArea(const FArenaHudCanvas& C, const FArenaHudSideView& Player, const FArenaHudSideAnim& Anim, double NowT, int32 Layer) const;
	void DrawFormAndCharge(const FArenaHudCanvas& C, const FArenaHudSideView& Player, const FArenaHudSideAnim& Anim, double NowT, int32 Layer) const;
	void DrawCrosshairChargeRing(const FArenaHudCanvas& C, int32 Layer) const;
	void DrawFloats(const FArenaHudCanvas& C, double NowT, int32 Layer) const;
	void DrawKeyHints(const FArenaHudCanvas& C, int32 Layer) const;
	void DrawResult(const FArenaHudCanvas& C, double NowT, int32 Layer) const;

	// ---------- 小件 ----------
	/** 切角信息条；Pos 为左上（Anchor=Left）或右上（Anchor=Right），返回占用宽度（设计像素） */
	float Chip(const FArenaHudCanvas& C, const FVector2f& Pos, float Height, const FString& Text, float FontSize,
		const FLinearColor& TextColor, const FLinearColor& BorderColor, const FLinearColor& BgColor, int32 Layer,
		EArenaHudAlign Anchor = EArenaHudAlign::Left) const;

	// ---------- 状态 ----------
	UPROPERTY() TArray<FArenaHudFloatItem> Floats;
	FArenaHudSideView PlayerView, OpponentView;
	FArenaHudSideAnim PlayerAnim, OpponentAnim;
	FGeometry CachedGeometry;                    // NativeTick 缓存，供飘字投影换算
	double LastTime = 0.0;
	float CachedU = 1.f;
};
