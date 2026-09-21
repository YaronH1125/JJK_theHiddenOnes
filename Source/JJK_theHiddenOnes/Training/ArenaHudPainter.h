// 正式战斗 HUD 自绘工具层：把 1920×1080 设计坐标的图元（多边形/环/扇形/文字/线性图标）
// 翻译成 Slate 绘制调用。视觉基准 Docs/assets/hud-prototype-v7.0.html，规格见 14_HUD开发指引.md。

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"

struct FGeometry;
class FSlateWindowElementList;
struct FSlateBrush;
struct FSlateFontInfo;

/** v7.0 色板与令牌（14_HUD开发指引.md §4）；线性 0..1 色 */
struct FArenaHudPalette
{
	static const FLinearColor Bg;        // #070B14 画布底
	static const FLinearColor Fg;        // #EAF6FF 主文字
	static const FLinearColor Muted;     // rgba(150,190,255,.55)
	static const FLinearColor P1;        // #1FB4F0
	static const FLinearColor P2;        // #FF5F1F
	static const FLinearColor T0;        // #04070E
	static const FLinearColor T1;        // #0A1220
	static const FLinearColor T2;        // #18233A
	static const FLinearColor T3;        // #3C4E70
	static const FLinearColor T4;        // #7C93B4
	static const FLinearColor T5;        // #EAF6FF
	static const FLinearColor Deng;      // #3FDCFF 领域能量
	static const FLinearColor Deng2;     // #A9F0FF
	static const FLinearColor Deng3;     // #E6FBFF
	static const FLinearColor DomAct;    // #A64DE6 领域展开期紫
	static const FLinearColor Danger;    // #FF4A38
	static const FLinearColor Danger2;   // #FFD9D4
	static const FLinearColor Aim;       // #73F2F2
	static const FLinearColor Chg;       // #FFE633
	static const FLinearColor Gold;      // #FFD166 满蓄/发炮刻度金
	/** 管体五段（暗边→亮带38%→主色→暗底），生命橙 / 咒力深蓝 / 行动金（§3.1 实测值） */
	static const FLinearColor HpSeg[5];
	static const FLinearColor CurseSeg[5];
	static const FLinearColor ActionSeg[5];
	/** 虚血残影两段 */
	static const FLinearColor GhostSeg[2];
	/** 空豆面（压到只剩结构线的暗灰阶） */
	static const FLinearColor PipOffSeg[3];
};

/** 文字水平对齐 */
enum class EArenaHudAlign : uint8 { Left, Center, Right };

/**
 * 一次 NativePaint 的绘制环境。
 * 所有公开入口吃 1920×1080 设计坐标（或标注 Local 的控件局部坐标），内部完成缩放换算；
 * 自定义顶点（多边形/环/扇形）需要窗口空间坐标，由 AbsOrigin+AccScale 换算。
 */
struct FArenaHudCanvas
{
	FArenaHudCanvas(const FGeometry& InGeometry, FSlateWindowElementList& InElements, const FSlateFontInfo& InBaseFont);

	// ---- 坐标与度量 ----
	FVector2f Design(float X, float Y) const { return FVector2f(X * U, Y * U); }
	float Len(float DesignLength) const { return DesignLength * U; }
	const FVector2f& ViewSize() const { return View; }
	/** 设计坐标 → 窗口像素坐标（custom verts 用） */
	FVector2f ToWindow(const FVector2f& DesignPos) const;

	// ---- 图元 ----
	void Box(const FVector2f& LocalPos, const FVector2f& LocalSize, const FLinearColor& Color, int32 Layer) const;
	void Poly(const TArray<FVector2f>& DesignPts, const TArray<FLinearColor>& Colors, int32 Layer) const;
	void Lines(const TArray<FVector2f>& DesignPts, const FLinearColor& Color, float DesignThickness, int32 Layer, bool bClosed = false) const;
	void Disc(const FVector2f& DesignCenter, float DesignRadius, const FLinearColor& Color, int32 Layer) const;
	/** 圆环/圆弧带；角度为设计坐标（Y 向下，0°=右，顺时针为正），StartDeg 起、顺时针扫 SweepDeg */
	void Ring(const FVector2f& DesignCenter, float DesignRadius, float DesignHalfWidth, const FLinearColor& Color,
		int32 Layer, float StartDeg = 0.f, float SweepDeg = 360.f, float CapDotRadius = 0.f) const;
	/** 顶部压暗带这类"整幅纵向渐变"：两列 N 行的顶点色网格 */
	void VGradient(const FVector2f& DesignPos, const FVector2f& DesignSize, const TArray<FLinearColor>& RowColors, int32 Layer) const;

	// ---- 文字 ----
	FVector2f Measure(const FString& Text, float DesignFontSize) const;
	/** bShadow 画一遍偏移 (1,1) 的深色拷贝当投影 */
	void Text(const FString& Text, const FVector2f& DesignPos, float DesignFontSize, const FLinearColor& Color,
		int32 Layer, EArenaHudAlign Align = EArenaHudAlign::Left, bool bShadow = true) const;

private:
	void SubmitVerts(const TArray<FVector2f>& DesignPts, const TArray<FLinearColor>& Colors, const TArray<SlateIndex>& Indices, int32 Layer) const;

	const FGeometry* Geom = nullptr;
	FSlateWindowElementList* Elements = nullptr;
	FVector2f View = FVector2f::ZeroVector;   // 控件局部逻辑尺寸
	float U = 1.f;                            // 设计 1080 → 局部
	float AccScale = 1.f;                     // 局部 → 窗口像素
	FVector2f AbsOrigin = FVector2f::ZeroVector;
	mutable FSlateFontInfo Font;              // 尺寸按需覆写
	const FSlateBrush* White = nullptr;
};

/** 线性技能图标（24×24 设计空间构图，占位矢量版 v2）：Id 取 ArenaCombatHudWidget.cpp 内的图标键 */
void DrawArenaHudIcon(const FArenaHudCanvas& Canvas, FName Id, const FVector2f& DesignCenter,
	float DesignBox, const FLinearColor& Color, int32 Layer);
