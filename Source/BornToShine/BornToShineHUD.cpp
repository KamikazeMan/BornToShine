// Born To Shine - HUD with crosshair for building mode

#include "BornToShineHUD.h"
#include "MoonshineCharacter_Simple.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"

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

	// The big crosshairs replace the dot — never both at once.
	if (bShowCenterDot && !bShowCrosshair && !bShowDeleteCrosshair)
	{
		DrawCenterDot();
	}

	DrawMoney();
}

void ABornToShineHUD::DrawCenterDot()
{
	if (!Canvas) return;

	// Hide while the inventory UI is open (it shows a cursor; the dot is just noise there).
	if (const AMoonshineCharacter_Simple* Player = Cast<AMoonshineCharacter_Simple>(GetOwningPawn()))
	{
		if (Player->InventoryWidgetInstance && Player->InventoryWidgetInstance->IsInViewport())
		{
			return;
		}
	}

	const float CenterX = Canvas->SizeX * 0.5f;
	const float CenterY = Canvas->SizeY * 0.5f;
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, DotOpacity),
		CenterX - DotSize * 0.5f, CenterY - DotSize * 0.5f, DotSize, DotSize);
}

void ABornToShineHUD::DrawMoney()
{
	if (!Canvas) return;

	const AMoonshineCharacter_Simple* Player = Cast<AMoonshineCharacter_Simple>(GetOwningPawn());
	if (!Player) return;

	const FString MoneyText = FString::Printf(TEXT("$%d"), Player->GetMoney());
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;

	// Right-align with a margin in the top-right corner; the icon+text pair is anchored together.
	float TextWidth = 0.0f, TextHeight = 0.0f;
	GetTextSize(MoneyText, TextWidth, TextHeight, Font, 1.5f);
	const float TextX = Canvas->SizeX - TextWidth - 20.0f;
	const float TextY = 20.0f;
	DrawText(MoneyText, FLinearColor::White, TextX, TextY, Font, 1.5f);

	if (MoneyIcon)
	{
		const float IconSize = 28.0f;
		const float IconX = TextX - IconSize - 6.0f;
		const float IconY = TextY + (TextHeight - IconSize) * 0.5f; // vertically centered on the text
		DrawTexture(MoneyIcon, IconX, IconY, IconSize, IconSize, 0.0f, 0.0f, 1.0f, 1.0f);
	}
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
