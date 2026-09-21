#include "Training/ArenaHudPainter.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/RenderingCommon.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

// ---------- 色板 ----------
const FLinearColor FArenaHudPalette::Bg = FLinearColor::FromSRGBColor(FColor(7, 11, 20));
const FLinearColor FArenaHudPalette::Fg = FLinearColor::FromSRGBColor(FColor(234, 246, 255));
const FLinearColor FArenaHudPalette::Muted(0.588f, 0.745f, 1.f, 0.55f);
const FLinearColor FArenaHudPalette::P1 = FLinearColor::FromSRGBColor(FColor(31, 180, 240));
const FLinearColor FArenaHudPalette::P2 = FLinearColor::FromSRGBColor(FColor(255, 95, 31));
const FLinearColor FArenaHudPalette::T0 = FLinearColor::FromSRGBColor(FColor(4, 7, 14));
const FLinearColor FArenaHudPalette::T1 = FLinearColor::FromSRGBColor(FColor(10, 18, 32));
const FLinearColor FArenaHudPalette::T2 = FLinearColor::FromSRGBColor(FColor(24, 35, 58));
const FLinearColor FArenaHudPalette::T3 = FLinearColor::FromSRGBColor(FColor(60, 78, 112));
const FLinearColor FArenaHudPalette::T4 = FLinearColor::FromSRGBColor(FColor(124, 147, 180));
const FLinearColor FArenaHudPalette::T5 = FLinearColor::FromSRGBColor(FColor(234, 246, 255));
const FLinearColor FArenaHudPalette::Deng = FLinearColor::FromSRGBColor(FColor(63, 220, 255));
const FLinearColor FArenaHudPalette::Deng2 = FLinearColor::FromSRGBColor(FColor(169, 240, 255));
const FLinearColor FArenaHudPalette::Deng3 = FLinearColor::FromSRGBColor(FColor(230, 251, 255));
const FLinearColor FArenaHudPalette::DomAct = FLinearColor::FromSRGBColor(FColor(166, 77, 230));
const FLinearColor FArenaHudPalette::Danger = FLinearColor::FromSRGBColor(FColor(255, 74, 56));
const FLinearColor FArenaHudPalette::Danger2 = FLinearColor::FromSRGBColor(FColor(255, 217, 212));
const FLinearColor FArenaHudPalette::Aim = FLinearColor::FromSRGBColor(FColor(115, 242, 242));
const FLinearColor FArenaHudPalette::Chg = FLinearColor::FromSRGBColor(FColor(255, 230, 51));
const FLinearColor FArenaHudPalette::Gold = FLinearColor::FromSRGBColor(FColor(255, 209, 102));
// 生命五段：#743203 / #CC6210 / #FFDA9E(38%) / #FF9A33 / #B84A0B
const FLinearColor FArenaHudPalette::HpSeg[5] = {
	FLinearColor::FromSRGBColor(FColor(116, 50, 3)), FLinearColor::FromSRGBColor(FColor(204, 98, 16)),
	FLinearColor::FromSRGBColor(FColor(255, 218, 158)), FLinearColor::FromSRGBColor(FColor(255, 154, 51)),
	FLinearColor::FromSRGBColor(FColor(184, 74, 11)) };
// 咒力五段：#0A1E63 / #1640B0 / #8FBCFF(38%) / #2258EE / #102B80
const FLinearColor FArenaHudPalette::CurseSeg[5] = {
	FLinearColor::FromSRGBColor(FColor(10, 30, 99)), FLinearColor::FromSRGBColor(FColor(22, 64, 176)),
	FLinearColor::FromSRGBColor(FColor(143, 188, 255)), FLinearColor::FromSRGBColor(FColor(34, 88, 238)),
	FLinearColor::FromSRGBColor(FColor(16, 43, 128)) };
// 行动五段：#5E4207 / #A8760F / #FFE0A0(38%) / #C08A14 / #6E4E08
const FLinearColor FArenaHudPalette::ActionSeg[5] = {
	FLinearColor::FromSRGBColor(FColor(94, 66, 7)), FLinearColor::FromSRGBColor(FColor(168, 118, 15)),
	FLinearColor::FromSRGBColor(FColor(255, 224, 160)), FLinearColor::FromSRGBColor(FColor(192, 138, 20)),
	FLinearColor::FromSRGBColor(FColor(110, 78, 8)) };
// 虚血残影（比填充亮一档的红橙）
const FLinearColor FArenaHudPalette::GhostSeg[2] = {
	FLinearColor::FromSRGBColor(FColor(255, 142, 116)), FLinearColor::FromSRGBColor(FColor(150, 44, 26)) };
// 空豆面：#1A2130 / #141A27 / #0D121C
const FLinearColor FArenaHudPalette::PipOffSeg[3] = {
	FLinearColor::FromSRGBColor(FColor(26, 33, 48)), FLinearColor::FromSRGBColor(FColor(20, 26, 39)),
	FLinearColor::FromSRGBColor(FColor(13, 18, 28)) };

// ---------- 画布 ----------
FArenaHudCanvas::FArenaHudCanvas(const FGeometry& InGeometry, FSlateWindowElementList& InElements, const FSlateFontInfo& InBaseFont)
	: Geom(&InGeometry)
	, Elements(&InElements)
	, View(InGeometry.GetLocalSize())
	, U(View.Y > 1.f ? View.Y / 1080.f : 1.f)
	, AccScale(FMath::Max(InGeometry.Scale, 0.01f))
	, AbsOrigin(FVector2f(InGeometry.AbsolutePosition))
	, Font(InBaseFont)
	, White(FCoreStyle::Get().GetBrush("WhiteBrush"))
{
}

FVector2f FArenaHudCanvas::ToWindow(const FVector2f& DesignPos) const
{
	return AbsOrigin + DesignPos * (U * AccScale);
}

void FArenaHudCanvas::Box(const FVector2f& LocalPos, const FVector2f& LocalSize, const FLinearColor& Color, int32 Layer) const
{
	FSlateDrawElement::MakeBox(*Elements, Layer,
		Geom->ToPaintGeometry(FVector2f(LocalSize), FSlateLayoutTransform(LocalPos)), White,
		ESlateDrawEffect::None, Color);
}

void FArenaHudCanvas::SubmitVerts(const TArray<FVector2f>& DesignPts, const TArray<FLinearColor>& Colors, const TArray<SlateIndex>& Indices, int32 Layer) const
{
	const int32 Num = DesignPts.Num();
	if (Num < 3 || Colors.Num() < Num) return;

	const FSlateResourceHandle Handle = White->GetRenderingResource();
	TArray<FSlateVertex> Verts;
	Verts.Reserve(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		FSlateVertex V;
		V.Position = ToWindow(DesignPts[i]);
		V.TexCoords[0] = V.TexCoords[1] = 0.5f;   // 白纹理中心，避开边缘采样
		V.TexCoords[2] = V.TexCoords[3] = 1.f;
		V.MaterialTexCoords = FVector2f::ZeroVector;
		// Slate 顶点着色器按 sRGB 解释 FColor；ToFColor(true) 保持线性色不变色
		V.Color = Colors[i].ToFColor(true);
		V.SecondaryColor = FColor::Transparent;
		V.PixelSize[0] = V.PixelSize[1] = 0;
		Verts.Add(V);
	}
	FSlateDrawElement::MakeCustomVerts(*Elements, Layer, Handle, Verts, Indices, nullptr, 0, 0);
}

void FArenaHudCanvas::Poly(const TArray<FVector2f>& DesignPts, const TArray<FLinearColor>& Colors, int32 Layer) const
{
	const int32 Num = DesignPts.Num();
	if (Num < 3) return;
	TArray<SlateIndex> Indices;
	Indices.Reserve((Num - 2) * 3);
	for (int32 i = 2; i < Num; ++i)
	{
		Indices.Add(0); Indices.Add(i - 1); Indices.Add(i);
	}
	SubmitVerts(DesignPts, Colors, Indices, Layer);
}

void FArenaHudCanvas::Lines(const TArray<FVector2f>& DesignPts, const FLinearColor& Color, float DesignThickness, int32 Layer, bool bClosed) const
{
	if (DesignPts.Num() < 2) return;
	TArray<FVector2f> Local;
	Local.Reserve(DesignPts.Num() + 1);
	for (const FVector2f& P : DesignPts) Local.Add(P * U);
	if (bClosed) Local.Add(DesignPts[0] * U);
	FSlateDrawElement::MakeLines(*Elements, Layer, Geom->ToPaintGeometry(), MoveTemp(Local),
		ESlateDrawEffect::None, Color, true, DesignThickness * U);
}

void FArenaHudCanvas::Disc(const FVector2f& DesignCenter, float DesignRadius, const FLinearColor& Color, int32 Layer) const
{
	constexpr int32 Segments = 24;
	TArray<FVector2f> Pts;
	TArray<FLinearColor> Colors;
	Pts.Reserve(Segments + 2); Colors.Reserve(Segments + 2);
	Pts.Add(DesignCenter); Colors.Add(Color);
	for (int32 i = 0; i <= Segments; ++i)
	{
		const float A = (2.f * PI * i) / Segments;
		Pts.Add(DesignCenter + FVector2f(FMath::Cos(A), FMath::Sin(A)) * DesignRadius);
		Colors.Add(Color);
	}
	Poly(Pts, Colors, Layer);
}

void FArenaHudCanvas::Ring(const FVector2f& DesignCenter, float DesignRadius, float DesignHalfWidth, const FLinearColor& Color,
	int32 Layer, float StartDeg, float SweepDeg, float CapDotRadius) const
{
	SweepDeg = FMath::Clamp(SweepDeg, 0.f, 360.f);
	if (SweepDeg <= 0.05f || DesignHalfWidth <= 0.f) return;   // 零长度弧必须整体跳过（§8.4 零长度端点坑）
	const int32 Segments = FMath::Clamp(FMath::CeilToInt(FMath::Abs(SweepDeg) / 6.f), 2, 60);
	const float DegToRad = PI / 180.f;
	TArray<FVector2f> Pts;
	TArray<FLinearColor> Colors;
	Pts.Reserve((Segments + 1) * 2); Colors.Reserve((Segments + 1) * 2);
	for (int32 i = 0; i <= Segments; ++i)
	{
		const float A = (StartDeg + SweepDeg * i / Segments) * DegToRad;
		const FVector2f DirA(FMath::Cos(A), FMath::Sin(A));
		Pts.Add(DesignCenter + DirA * (DesignRadius + DesignHalfWidth));   // 偶数位 = 外圈
		Pts.Add(DesignCenter + DirA * (DesignRadius - DesignHalfWidth));   // 奇数位 = 内圈
		Colors.Add(Color); Colors.Add(Color);
	}
	TArray<SlateIndex> Indices;
	Indices.Reserve(Segments * 6);
	for (int32 i = 0; i < Segments; ++i)
	{
		const int32 A = i * 2, B = i * 2 + 1, C = i * 2 + 2, D = i * 2 + 3;
		Indices.Add(A); Indices.Add(B); Indices.Add(C);
		Indices.Add(B); Indices.Add(D); Indices.Add(C);
	}
	SubmitVerts(Pts, Colors, Indices, Layer);
	if (CapDotRadius > 0.f)
	{
		const float ARad = StartDeg * DegToRad;
		const float BRad = (StartDeg + SweepDeg) * DegToRad;
		Disc(DesignCenter + FVector2f(FMath::Cos(ARad), FMath::Sin(ARad)) * DesignRadius, CapDotRadius, Color, Layer);
		Disc(DesignCenter + FVector2f(FMath::Cos(BRad), FMath::Sin(BRad)) * DesignRadius, CapDotRadius, Color, Layer);
	}
}

void FArenaHudCanvas::VGradient(const FVector2f& DesignPos, const FVector2f& DesignSize, const TArray<FLinearColor>& RowColors, int32 Layer) const
{
	const int32 Rows = RowColors.Num();
	if (Rows < 2) return;
	TArray<FVector2f> Pts;
	TArray<FLinearColor> Colors;
	Pts.Reserve(Rows * 2); Colors.Reserve(Rows * 2);
	for (int32 i = 0; i < Rows; ++i)
	{
		const float Y = DesignPos.Y + DesignSize.Y * i / (Rows - 1);
		Pts.Add(FVector2f(DesignPos.X, Y));
		Pts.Add(FVector2f(DesignPos.X + DesignSize.X, Y));
		Colors.Add(RowColors[i]); Colors.Add(RowColors[i]);
	}
	TArray<SlateIndex> Indices;
	Indices.Reserve((Rows - 1) * 6);
	for (int32 i = 0; i < Rows - 1; ++i)
	{
		const int32 A = i * 2, B = i * 2 + 1, C = i * 2 + 2, D = i * 2 + 3;
		Indices.Add(A); Indices.Add(B); Indices.Add(C);
		Indices.Add(B); Indices.Add(D); Indices.Add(C);
	}
	SubmitVerts(Pts, Colors, Indices, Layer);
}

FVector2f FArenaHudCanvas::Measure(const FString& Text, float DesignFontSize) const
{
	if (Text.IsEmpty()) return FVector2f::ZeroVector;
	FSlateFontInfo F = Font;
	F.Size = FMath::Max(4, FMath::RoundToInt(DesignFontSize));
	const FVector2f Size(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(FStringView(Text), F));
	return FVector2f(Size.X / U, Size.Y / U);
}

void FArenaHudCanvas::Text(const FString& Text, const FVector2f& DesignPos, float DesignFontSize, const FLinearColor& Color,
	int32 Layer, EArenaHudAlign Align, bool bShadow) const
{
	if (Text.IsEmpty()) return;
	FSlateFontInfo F = Font;
	F.Size = FMath::Max(4, FMath::RoundToInt(DesignFontSize));
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2f Pixel(Measure->Measure(FStringView(Text), F));	const FVector2f DesignSize(Pixel.X / U, Pixel.Y / U);

	FVector2f Pos = DesignPos;
	if (Align == EArenaHudAlign::Center) Pos.X -= DesignSize.X * 0.5f;
	else if (Align == EArenaHudAlign::Right) Pos.X -= DesignSize.X;
	// MakeText 以变换原点为文字块左上角（与 STextBlock 一致），DesignPos 即文字顶点

	if (bShadow)
	{
		const FLinearColor Shadow(0.f, 0.f, 0.f, 0.85f * Color.A);
		FSlateDrawElement::MakeText(*Elements, Layer,
			Geom->ToPaintGeometry(Pixel, FSlateLayoutTransform(Pos * U + FVector2f(Len(1), Len(1)))), Text, F,
			ESlateDrawEffect::None, Shadow);
	}
	FSlateDrawElement::MakeText(*Elements, Layer,
		Geom->ToPaintGeometry(Pixel, FSlateLayoutTransform(Pos * U)), Text, F, ESlateDrawEffect::None, Color);
}

// ---------- 线性图标（24×24 构图；占位矢量版，与 hud-prototype-v7.0 的 SVG 同源） ----------
namespace
{
	struct FIconPen
	{
		const FArenaHudCanvas* C;
		FVector2f Origin;   // 24 空间的 (0,0) 在设计坐标的位置
		float S;            // 缩放（设计盒 / 24）
		FLinearColor Col;
		int32 Layer;

		FVector2f Map(float X, float Y) const { return Origin + FVector2f(X, Y) * S; }
		void Line(float X1, float Y1, float X2, float Y2, float Alpha = 1.f) const
		{
			TArray<FVector2f> P; P.Add(Map(X1, Y1)); P.Add(Map(X2, Y2));
			C->Lines(P, FLinearColor(Col.R, Col.G, Col.B, Col.A * Alpha), 2.1f, Layer);
		}
		void Path(std::initializer_list<FVector2f> Pts, float Alpha = 1.f) const
		{
			TArray<FVector2f> P;
			for (const FVector2f& Pt : Pts) P.Add(Origin + Pt * S);
			C->Lines(P, FLinearColor(Col.R, Col.G, Col.B, Col.A * Alpha), 2.1f, Layer);
		}
		void Poly(std::initializer_list<FVector2f> Pts, float Alpha) const
		{
			TArray<FVector2f> P; TArray<FLinearColor> Cs;
			for (const FVector2f& Pt : Pts) { P.Add(Origin + Pt * S); Cs.Add(FLinearColor(Col.R, Col.G, Col.B, Col.A * Alpha)); }
			C->Poly(P, Cs, Layer);
		}
		void Dot(float X, float Y, float R, float Alpha = 1.f) const
		{
			C->Disc(Map(X, Y), R * S, FLinearColor(Col.R, Col.G, Col.B, Col.A * Alpha), Layer);
		}
		void Circle(float X, float Y, float R, float Alpha = 1.f) const
		{
			TArray<FVector2f> P;
			for (int32 i = 0; i <= 14; ++i)
			{
				const float A = 2.f * PI * i / 14.f;
				P.Add(Map(X + R * FMath::Cos(A), Y + R * FMath::Sin(A)));
			}
			C->Lines(P, FLinearColor(Col.R, Col.G, Col.B, Col.A * Alpha), 2.f, Layer);
		}
	};

	void DrawBase(FIconPen& Pen) { Pen.Line(7.f, 19.4f, 17.f, 19.4f, 0.38f); }
}

void DrawArenaHudIcon(const FArenaHudCanvas& Canvas, FName Id, const FVector2f& DesignCenter,
	float DesignBox, const FLinearColor& Color, int32 Layer)
{
	// 24 空间按 17 等分映射；各图标实际内容重心不同，逐个校正使视觉居中
	FVector2f ContentCenter(12.f, 12.f);
	if (Id == "punch")       ContentCenter = FVector2f(10.7f, 14.0f);
	else if (Id == "kick")   ContentCenter = FVector2f(12.4f, 11.6f);
	else if (Id == "swap")   ContentCenter = FVector2f(12.0f, 12.4f);
	else if (Id == "blast")  ContentCenter = FVector2f(12.0f, 9.8f);
	else if (Id == "sblast") ContentCenter = FVector2f(12.0f, 9.4f);
	else if (Id == "domain") ContentCenter = FVector2f(12.2f, 11.8f);
	const float S = DesignBox / 17.f;
	FIconPen Pen{ &Canvas, DesignCenter - ContentCenter * S, S, Color, Layer };

	if (Id == "punch")
	{
		DrawBase(Pen);
		// 拳峰：亮面填充 + 描边 + 拳面分隔线（不要实心块）
		Pen.Poly({ FVector2f(4.8f, 10.8f), FVector2f(12.2f, 10.8f), FVector2f(12.2f, 17.2f), FVector2f(4.8f, 17.2f) }, 0.16f);
		Pen.Path({ FVector2f(4.8f, 10.8f), FVector2f(12.2f, 10.8f), FVector2f(12.2f, 17.2f), FVector2f(4.8f, 17.2f), FVector2f(4.8f, 10.8f) });
		Pen.Line(7.4f, 10.8f, 7.4f, 17.2f, 0.8f);
		Pen.Line(9.8f, 10.8f, 9.8f, 17.2f, 0.8f);
		// 三段连打节奏：右侧三枚箭头纵向均匀分布
		Pen.Path({ FVector2f(14.4f, 8.6f), FVector2f(16.6f, 10.2f), FVector2f(14.4f, 11.8f) });
		Pen.Path({ FVector2f(14.4f, 12.f), FVector2f(16.6f, 13.6f), FVector2f(14.4f, 15.2f) });
		Pen.Path({ FVector2f(14.4f, 15.4f), FVector2f(16.6f, 17.f), FVector2f(14.4f, 18.6f) });
	}
	else if (Id == "kick")
	{
		DrawBase(Pen);
		// 方形腰带扣：顶部居中，内芯亮点
		Pen.Poly({ FVector2f(6.8f, 3.8f), FVector2f(13.2f, 3.8f), FVector2f(13.2f, 10.f), FVector2f(6.8f, 10.f) }, 0.08f);
		Pen.Path({ FVector2f(6.8f, 3.8f), FVector2f(13.2f, 3.8f), FVector2f(13.2f, 10.f), FVector2f(6.8f, 10.f), FVector2f(6.8f, 3.8f) });
		Pen.Poly({ FVector2f(8.9f, 5.9f), FVector2f(11.1f, 5.9f), FVector2f(11.1f, 7.9f), FVector2f(8.9f, 7.9f) }, 0.45f);
		// 腿弧：从扣底摆向右下 + 命中记号
		Pen.Path({ FVector2f(9.9f, 10.f), FVector2f(11.2f, 14.6f), FVector2f(16.6f, 18.2f) });
		Pen.Path({ FVector2f(15.4f, 19.1f), FVector2f(18.9f, 17.5f) }, 0.75f);
		Pen.Path({ FVector2f(14.8f, 15.8f), FVector2f(17.5f, 14.8f) }, 0.75f);
	}
	else if (Id == "swap")
	{
		// 飞机头块面 + 横杠 + 上下双向箭头（构图居中）
		Pen.Poly({ FVector2f(9.7f, 8.8f), FVector2f(9.7f, 6.4f), FVector2f(10.8f, 4.2f), FVector2f(13.3f, 4.2f), FVector2f(14.4f, 6.4f), FVector2f(14.4f, 8.8f) }, 0.30f);
		Pen.Path({ FVector2f(9.7f, 8.8f), FVector2f(9.7f, 6.4f), FVector2f(10.8f, 4.2f), FVector2f(13.3f, 4.2f), FVector2f(14.4f, 6.4f), FVector2f(14.4f, 8.8f) });
		Pen.Line(8.3f, 9.9f, 15.7f, 9.9f);
		Pen.Path({ FVector2f(5.f, 13.9f), FVector2f(18.4f, 13.9f) });
		Pen.Path({ FVector2f(15.9f, 11.7f), FVector2f(18.4f, 13.9f), FVector2f(15.9f, 16.1f) });
		Pen.Path({ FVector2f(19.f, 18.7f), FVector2f(5.6f, 18.7f) });
		Pen.Path({ FVector2f(8.1f, 16.5f), FVector2f(5.6f, 18.7f), FVector2f(8.1f, 20.7f) });
	}
	else if (Id == "blast")
	{
		DrawBase(Pen);
		Pen.Line(9.6f, 14.4f, 9.6f, 10.f);
		Pen.Line(14.4f, 14.4f, 14.4f, 10.f);
		Pen.Path({ FVector2f(9.6f, 10.f), FVector2f(10.4f, 8.3f), FVector2f(12.f, 7.9f), FVector2f(13.6f, 8.3f), FVector2f(14.4f, 10.f) });
		Pen.Circle(12.f, 6.9f, 2.f);
		Pen.Dot(12.f, 6.9f, 0.9f, 0.85f);
		Pen.Line(12.f, 4.1f, 12.f, 2.5f, 0.85f);
		Pen.Line(9.1f, 4.9f, 8.f, 3.6f, 0.85f);
		Pen.Line(14.9f, 4.9f, 16.f, 3.6f, 0.85f);
		Pen.Line(6.6f, 16.6f, 17.4f, 16.6f, 0.45f);
	}
	else if (Id == "sblast")
	{
		DrawBase(Pen);
		Pen.Line(8.8f, 15.2f, 8.8f, 10.2f);
		Pen.Line(15.2f, 15.2f, 15.2f, 10.2f);
		Pen.Path({ FVector2f(8.8f, 10.2f), FVector2f(9.9f, 8.f), FVector2f(12.f, 7.5f), FVector2f(14.1f, 8.f), FVector2f(15.2f, 10.2f) });
		Pen.Circle(12.f, 6.1f, 2.6f);
		Pen.Dot(12.f, 6.1f, 1.1f, 0.85f);
		Pen.Line(12.f, 2.7f, 12.f, 1.f, 0.85f);
		Pen.Line(8.f, 3.7f, 6.7f, 2.3f, 0.85f);
		Pen.Line(16.f, 3.7f, 17.3f, 2.3f, 0.85f);
		Pen.Line(5.8f, 17.6f, 18.2f, 17.6f, 0.45f);
	}
	else if (Id == "domain")
	{
		Pen.Poly({ FVector2f(12.4f, 10.4f), FVector2f(13.9f, 11.9f), FVector2f(12.4f, 13.4f), FVector2f(10.9f, 11.9f) }, 0.42f);
		Pen.Circle(12.4f, 10.4f, 2.2f);
		Pen.Path({ FVector2f(11.2f, 20.4f), FVector2f(11.7f, 16.8f), FVector2f(12.3f, 13.1f) });
		Pen.Path({ FVector2f(12.4f, 8.3f), FVector2f(11.9f, 3.1f) });
		Pen.Path({ FVector2f(14.3f, 9.1f), FVector2f(18.4f, 6.5f) });
		Pen.Path({ FVector2f(14.8f, 11.4f), FVector2f(19.2f, 13.7f) });
		Pen.Path({ FVector2f(11.f, 12.f), FVector2f(8.3f, 15.6f) });
		Pen.Path({ FVector2f(10.6f, 9.6f), FVector2f(6.4f, 7.4f) });
		Pen.Path({ FVector2f(13.2f, 12.2f), FVector2f(14.3f, 16.8f) });
		Pen.Dot(11.8f, 2.5f, 0.85f);
		Pen.Dot(19.f, 6.1f, 0.85f);
		Pen.Dot(7.9f, 15.8f, 0.85f);
		Pen.Dot(6.f, 7.2f, 0.85f);
		Pen.Dot(14.8f, 17.6f, 0.7f, 0.7f);
		Pen.Path({ FVector2f(3.6f, 21.f), FVector2f(6.2f, 19.9f), FVector2f(9.2f, 19.3f), FVector2f(12.8f, 19.3f), FVector2f(16.4f, 19.6f), FVector2f(20.4f, 20.5f) }, 0.5f);
	}
	else if (Id == "mouse")
	{
		Pen.Poly({ FVector2f(12.f, 2.9f), FVector2f(8.6f, 3.8f), FVector2f(6.4f, 6.6f), FVector2f(6.4f, 8.4f), FVector2f(12.f, 8.4f) }, 0.55f);
		Pen.Path({ FVector2f(12.f, 2.6f), FVector2f(16.f, 3.6f), FVector2f(18.3f, 6.4f), FVector2f(18.3f, 15.4f), FVector2f(15.4f, 19.4f), FVector2f(12.f, 21.4f), FVector2f(8.6f, 19.4f), FVector2f(5.7f, 15.4f), FVector2f(5.7f, 6.4f), FVector2f(8.f, 3.6f), FVector2f(12.f, 2.6f) });
		Pen.Line(12.f, 2.6f, 12.f, 8.7f, 0.55f);
		Pen.Line(5.7f, 8.7f, 18.3f, 8.7f, 0.55f);
	}
}
