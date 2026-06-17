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

		FLinearColor Tint;
		if (bEarned)
		{
			// Base: solid earned color with the gentle idle pulse.
			Tint = StarColor * IdlePulse;
			Tint.A = StarColor.A;

			// Cooling-down: flash the top earned star between its color and gray.
			if (bShowCoolingFlash && bHeatCoolingDown && i == TopEarnedIndex)
			{
				const float CoolMix = 0.5f + 0.5f * FMath::Sin(Time * StarCoolFlashSpeed);
				Tint = FLinearColor(
					FMath::Lerp(StarColor.R, GrayTint.R, CoolMix),
					FMath::Lerp(StarColor.G, GrayTint.G, CoolMix),
					FMath::Lerp(StarColor.B, GrayTint.B, CoolMix),
					FMath::Lerp(StarColor.A, GrayTint.A, CoolMix));
			}

			// Freshly-gained: brighten the newest star briefly, then settle.
			if (Time < NewStarFlashUntil && i == TopEarnedIndex)
			{
				Tint = FLinearColor(
					FMath::Min(1.0f, StarColor.R * 1.6f),
					FMath::Min(1.0f, StarColor.G * 1.6f),
					FMath::Min(1.0f, StarColor.B * 1.6f),
					StarColor.A);
			}
		}
		else
		{
			Tint = EmptyTint;
		}

		UTexture2D* Tex = bEarned ? StarFilled : StarEmpty;
		if (Tex)
		{
			DrawTexture(Tex, X, Y, StarSize, StarSize, 0.0f, 0.0f, 1.0f, 1.0f, Tint);
		}
		else
		{
			const float CX = X + StarSize * 0.5f;
			const float CY = Y + StarSize * 0.5f;
			DrawStarPolygon(CX, CY, StarSize * 0.5f, StarSize * 0.2f, Tint, bEarned);
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

void ABornToShineHUD::DrawStarPolygon(float CenterX, float CenterY, float OuterR, float InnerR, const FLinearColor& Color, bool bFilled)
{
	if (!Canvas) return;

	// 10 vertices: alternating outer/inner, starting at top (-90 deg).
	TArray<FVector2D> Verts;
	Verts.SetNum(10);
	for (int32 i = 0; i < 10; ++i)
	{
		const float AngleDeg = -90.0f + i * 36.0f;
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);
		const float R = (i % 2 == 0) ? OuterR : InnerR;
		Verts[i] = FVector2D(CenterX + R * FMath::Cos(AngleRad), CenterY + R * FMath::Sin(AngleRad));
	}

	if (bFilled)
	{
		// Fill by drawing thick lines from center to each outer vertex + connecting edges.
		const float FillThickness = InnerR * 1.4f;
		for (int32 i = 0; i < 10; i += 2)
		{
			DrawLine(CenterX, CenterY, Verts[i].X, Verts[i].Y, Color, FillThickness);
		}
		for (int32 i = 0; i < 10; ++i)
		{
			const FVector2D& A = Verts[i];
			const FVector2D& B = Verts[(i + 1) % 10];
			DrawLine(A.X, A.Y, B.X, B.Y, Color, 2.0f);
		}
	}
	else
	{
		for (int32 i = 0; i < 10; ++i)
		{
			const FVector2D& A = Verts[i];
			const FVector2D& B = Verts[(i + 1) % 10];
			DrawLine(A.X, A.Y, B.X, B.Y, Color, 2.0f);
		}
	}
}
