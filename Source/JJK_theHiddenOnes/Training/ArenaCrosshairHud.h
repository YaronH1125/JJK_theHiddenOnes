// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArenaCrosshairHud.generated.h"

class AFighterCharacter;

/** 准星 HUD：移植自 JJKDemo 的 Canvas 四线+中心点画法，随蓄力/瞄准/领域变色收放。 */
UCLASS()
class AArenaCrosshairHud : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCrosshair(const AFighterCharacter& Fighter);
};
