// Born To Shine - HUD with crosshair for building mode

#include "BornToShineHUD.h"
#include "MoonshineCharacter_Simple.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"

ABornToShineHUD::ABornToShineHUD()
{
	bShowCrosshair = false;
	CrosshairSize = 20.0f;
	CrosshairThickness = 2.0f;
	CrosshairColor = FLinearColor::White;

	bShowDeleteCrosshair = false;
	DeleteCrosshairSize = 24.0f;
	DeleteCrosshairThickness = 2.0f;
	DeleteCrosshairColor = FLinearColor::Green;
}

void ABornToShineHUD::DrawHUD()
{
	Super::DrawHUD();

	if (bShowCrosshair)
	{
		DrawCrosshair();
	}

	if (bShowDeleteCrosshair)
	{
		DrawDeleteCrosshair();
	}

	DrawMoney();
}

void ABornToShineHUD::DrawMoney()
{
	if (!Canvas) return;

	const AMoonshineCharacter_Simple* Player = Cast<AMoonshineCharacter_Simple>(GetOwningPawn());
	if (!Player) return;

	const FString MoneyText = FString::Printf(TEXT("$%d"), Player->GetMoney());
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;

	// Right-align with a margin in the top-right corner.
	float TextWidth = 0.0f, TextHeight = 0.0f;
	GetTextSize(MoneyText, TextWidth, TextHeight, Font, 1.5f);
	DrawText(MoneyText, FLinearColor::White, Canvas->SizeX - TextWidth - 20.0f, 20.0f, Font, 1.5f);
}

void ABornToShineHUD::DrawCrosshair()
{
	if (!Canvas) return;

	// Get center of screen
	float CenterX = Canvas->SizeX * 0.5f;
	float CenterY = Canvas->SizeY * 0.5f;

	// Draw horizontal line
	DrawLine(
		CenterX - CrosshairSize, CenterY,
		CenterX + CrosshairSize, CenterY,
		CrosshairColor,
		CrosshairThickness
	);

	// Draw vertical line
	DrawLine(
		CenterX, CenterY - CrosshairSize,
		CenterX, CenterY + CrosshairSize,
		CrosshairColor,
		CrosshairThickness
	);

	// Optional: Draw a small center dot
	DrawRect(
		CrosshairColor,
		CenterX - 2, CenterY - 2,
		4, 4
	);
}

void ABornToShineHUD::DrawDeleteCrosshair()
{
	if (!Canvas) return;

	float CenterX = Canvas->SizeX * 0.5f;
	float CenterY = Canvas->SizeY * 0.5f;
	float S = DeleteCrosshairSize;
	float Gap = 6.0f; // gap in the center so it doesn't obscure the target

	// Horizontal lines (left and right of center gap)
	DrawLine(CenterX - S, CenterY, CenterX - Gap, CenterY,
		DeleteCrosshairColor, DeleteCrosshairThickness);
	DrawLine(CenterX + Gap, CenterY, CenterX + S, CenterY,
		DeleteCrosshairColor, DeleteCrosshairThickness);

	// Vertical lines (above and below center gap)
	DrawLine(CenterX, CenterY - S, CenterX, CenterY - Gap,
		DeleteCrosshairColor, DeleteCrosshairThickness);
	DrawLine(CenterX, CenterY + Gap, CenterX, CenterY + S,
		DeleteCrosshairColor, DeleteCrosshairThickness);

	// Center dot
	DrawRect(DeleteCrosshairColor, CenterX - 2, CenterY - 2, 4, 4);
}
