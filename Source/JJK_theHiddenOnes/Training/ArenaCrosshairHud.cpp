// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/ArenaCrosshairHud.h"

#include "Training/ArenaPlayerController.h"
#include "Training/FighterCharacter.h"
#include "Engine/Canvas.h"

void AArenaCrosshairHud::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr) return;
	auto* PC = Cast<AArenaPlayerController>(PlayerOwner);
	const auto* Fighter = PC ? PC->GetPlayerFighter() : nullptr;
	if (Fighter == nullptr) return;
	DrawCrosshair(*Fighter);
}

void AArenaCrosshairHud::DrawCrosshair(const AFighterCharacter& Fighter)
{
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;

	const float ChargeAlpha = Fighter.GetBlastChargeAlpha();
	const bool bCharging = Fighter.IsBlastCharging();
	const bool bAiming = Fighter.IsAimingEffective();
	const bool bDomain = Fighter.IsDomainActive();

	// JJKDemo 口径：蓄力时向心收拢，瞄准收半，平时最松；领域加粗加长
	const float Gap = bCharging ? FMath::Lerp(12.0f, 4.0f, ChargeAlpha) : (bAiming ? 8.0f : 14.0f);
	const float LineLength = bDomain ? 16.0f : 10.0f;
	const float Thickness = bDomain ? 3.0f : 2.0f;

	FLinearColor CrosshairColor(0.8f, 0.9f, 1.0f, 0.95f);
	if (bDomain)
	{
		CrosshairColor = FLinearColor(1.0f, 0.4f, 0.2f, 0.95f);
	}
	else if (bCharging)
	{
		CrosshairColor = FLinearColor(1.0f, 0.9f, 0.2f, 0.95f);
	}
	else if (bAiming)
	{
		CrosshairColor = FLinearColor(0.45f, 0.95f, 0.95f, 0.95f);
	}

	const auto DrawCrossLine = [&](float X, float Y, float Width, float Height)
	{
		DrawRect(CrosshairColor, X, Y, Width, Height);
	};

	DrawCrossLine(CenterX - Thickness * 0.5f, CenterY - Gap - LineLength, Thickness, LineLength);
	DrawCrossLine(CenterX - Thickness * 0.5f, CenterY + Gap, Thickness, LineLength);
	DrawCrossLine(CenterX - Gap - LineLength, CenterY - Thickness * 0.5f, LineLength, Thickness);
	DrawCrossLine(CenterX + Gap, CenterY - Thickness * 0.5f, LineLength, Thickness);
	DrawRect(CrosshairColor, CenterX - 1.5f, CenterY - 1.5f, 3.0f, 3.0f);
}
