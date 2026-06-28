// Born To Shine - HUD with crosshair for building mode

#include "BornToShineHUD.h"
#include "MoonshineCharacter_Simple.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "CanvasItem.h"
#include "RenderResource.h" // GWhiteTexture (untextured triangle fill)

namespace
{
	// Build the 10-vertex 5-point star path (alternating outer/inner radius), starting at the top.
	void BuildStarVerts(float CenterX, float CenterY, float OuterR, float InnerR, FVector2D OutVerts[10])
	{
		for (int32 i = 0; i < 10; ++i)
		{
			const float AngleRad = FMath::DegreesToRadians(-90.0f + i * 36.0f);
			const float R = (i % 2 == 0) ? OuterR : InnerR;
			OutVerts[i] = FVector2D(CenterX + R * FMath::Cos(AngleRad), CenterY + R * FMath::Sin(AngleRad));
		}
	}
}

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

	DrawHeatMeter();
}

void ABornToShineHUD::DrawHeatMeter()
{
	if (!Canvas) return;

	const AMoonshineCharacter_Simple* Player = Cast<AMoonshineCharacter_Simple>(GetOwningPawn());
	if (!Player) return;

	const int32 Stars = Player->GetSuspicionStars();
	const float Heat = Player->GetSuspicionHeat();
	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// Heat-trend detection (frame-to-frame): are we cooling down (about to lose a star)?
	if (PrevHeat >= 0.0f)
	{
		const float Delta = Heat - PrevHeat;
		if (Delta < -0.0005f)      bHeatCoolingDown = true;   // dropping -> cooling off
		else if (Delta > 0.0005f)  bHeatCoolingDown = false;  // rising -> wanted
		// steady (grace window): keep solid, treat as not cooling.
		else                       bHeatCoolingDown = false;
	}
	PrevHeat = Heat;

	// Brief brighten when a new star is earned.
	if (Stars > PrevStars)
	{
		NewStarFlashUntil = Time + NewStarFlashDuration;
	}
	PrevStars = Stars;

	// Soft "wanted" brightness pulse (sine), brightness oscillates (1-Depth)..1.0.
	const float IdlePulse = 1.0f - StarPulseDepth * (0.5f + 0.5f * FMath::Sin(Time * StarPulseSpeed));

	const FLinearColor EmptyTint(0.25f, 0.25f, 0.25f, 0.6f); // dim gray outline for unearned
	const FLinearColor GrayTint(0.35f, 0.35f, 0.35f, 0.85f);

	const float TotalWidth = 5 * StarSize + 4 * StarSpacing;
	const float StartX = (HeatMeterScreenPos.X < 0.0f)
		? (Canvas->SizeX * 0.5f - TotalWidth * 0.5f)
		: HeatMeterScreenPos.X;
	const float Y = HeatMeterScreenPos.Y;

	const int32 TopEarnedIndex = Stars - 1; // about-to-be-lost star when cooling

	for (int32 i = 0; i < 5; ++i)
	{
		const float X = StartX + i * (StarSize + StarSpacing);
		const bool bEarned = i < Stars;

		// Solid red fill; the pulse modulates OPACITY only (geometry is fixed — never scaled).
		FLinearColor FillColor = StarFillColor;
		FLinearColor OutlineColor = bEarned ? StarColor : EmptyTint;

		if (bEarned)
		{
			FillColor.A = StarFillColor.A * IdlePulse;

			// Cooling-down: flash the top (about-to-be-lost) star's fill toward gray.
			if (bShowCoolingFlash && bHeatCoolingDown && i == TopEarnedIndex)
			{
				const float CoolMix = 0.5f + 0.5f * FMath::Sin(Time * StarCoolFlashSpeed);
				FillColor = FLinearColor(
					FMath::Lerp(StarFillColor.R, GrayTint.R, CoolMix),
					FMath::Lerp(StarFillColor.G, GrayTint.G, CoolMix),
					FMath::Lerp(StarFillColor.B, GrayTint.B, CoolMix),
					StarFillColor.A);
			}

			// Freshly-gained: pop the newest star to full opacity briefly, then settle.
			if (Time < NewStarFlashUntil && i == TopEarnedIndex)
			{
				FillColor.A = StarFillColor.A;
			}
		}

		UTexture2D* Tex = bEarned ? StarFilled : StarEmpty;
		if (Tex)
		{
			DrawTexture(Tex, X, Y, StarSize, StarSize, 0.0f, 0.0f, 1.0f, 1.0f, bEarned ? FillColor : EmptyTint);
		}
		else
		{
			const float CX = X + StarSize * 0.5f;
			const float CY = Y + StarSize * 0.5f;
			DrawStarPolygon(CX, CY, StarSize * 0.5f, StarSize * 0.2f, OutlineColor, bEarned, FillColor);
		}
	}
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

void ABornToShineHUD::DrawStarPolygon(float CenterX, float CenterY, float OuterR, float InnerR,
	const FLinearColor& OutlineColor, bool bFilled, const FLinearColor& FillColor)
{
	if (!Canvas) return;

	// SOLID FILL FIRST (so the crisp outline sits on top of its edge).
	// Same 10-vertex star path, inset to 0.9x so it sits just INSIDE the outline — never outside.
	// Triangle fan from the center: center -> vertex[i] -> vertex[i+1] across all 10 perimeter verts.
	if (bFilled)
	{
		FVector2D Fill[10];
		BuildStarVerts(CenterX, CenterY, OuterR * 0.9f, InnerR * 0.9f, Fill);
		const FVector2D Center(CenterX, CenterY);
		for (int32 i = 0; i < 10; ++i)
		{
			FCanvasTriangleItem Tri(Center, Fill[i], Fill[(i + 1) % 10], GWhiteTexture);
			Tri.SetColor(FillColor);
			Canvas->DrawItem(Tri);
		}
	}

	// OUTLINE: the 10-vertex star path drawn as a closed line loop (full radius).
	FVector2D Edge[10];
	BuildStarVerts(CenterX, CenterY, OuterR, InnerR, Edge);
	for (int32 i = 0; i < 10; ++i)
	{
		const FVector2D& A = Edge[i];
		const FVector2D& B = Edge[(i + 1) % 10];
		DrawLine(A.X, A.Y, B.X, B.Y, OutlineColor, 2.0f);
	}
}
