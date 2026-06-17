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

	// Police-light flash color for filled stars (only when at least 1 star).
	FLinearColor FilledTint(0.85f, 0.1f, 0.1f, 1.0f); // steady red baseline
	if (Stars >= 1)
	{
		const float Rate = HeatFlashBaseRate + (Stars - 1) * HeatFlashRatePerStar;
		const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		const bool bBlue = (FMath::FloorToInt(Time * FMath::Max(Rate, 0.1f)) % 2) != 0;
		FilledTint = bBlue ? FLinearColor(0.1f, 0.25f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.1f, 0.1f, 1.0f);
	}

	const FLinearColor EmptyTint(0.15f, 0.15f, 0.15f, 0.6f);

	const float TotalWidth = 5 * StarSize + 4 * StarSpacing;
	const float StartX = (HeatMeterScreenPos.X < 0.0f)
		? (Canvas->SizeX * 0.5f - TotalWidth * 0.5f)
		: HeatMeterScreenPos.X;
	const float Y = HeatMeterScreenPos.Y;

	for (int32 i = 0; i < 5; ++i)
	{
		const float X = StartX + i * (StarSize + StarSpacing);
		const bool bFilled = i < Stars;
		const FLinearColor Tint = bFilled ? FilledTint : EmptyTint;
		UTexture2D* Tex = bFilled ? StarFilled : StarEmpty;

		if (Tex)
		{
			DrawTexture(Tex, X, Y, StarSize, StarSize, 0.0f, 0.0f, 1.0f, 1.0f, Tint);
		}
		else
		{
			const float CX = X + StarSize * 0.5f;
			const float CY = Y + StarSize * 0.5f;
			DrawStarPolygon(CX, CY, StarSize * 0.5f, StarSize * 0.2f, Tint, bFilled);
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
