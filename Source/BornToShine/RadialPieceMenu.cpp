// Born To Shine - Radial Piece Selection Menu

#include "RadialPieceMenu.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

URadialPieceMenu::URadialPieceMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HighlightedIndex = -1;
	NumSegments = 0;
	OuterRadius = 220.0f;
	InnerRadius = 70.0f;
	DeadZone = 40.0f;
	SegmentGapDeg = 2.0f;
}

void URadialPieceMenu::InitMenu(const TArray<FString>& InNames, int32 CurrentIndex)
{
	SegmentNames = InNames;
	NumSegments = SegmentNames.Num();
	HighlightedIndex = CurrentIndex;
}

// ---------------------------------------------------------------------------
// Mouse tracking
// ---------------------------------------------------------------------------
void URadialPieceMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (NumSegments == 0) return;

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);

	// Viewport center
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	FVector2D Center = ViewportSize / 2.0f;

	float DX = MouseX - Center.X;
	float DY = MouseY - Center.Y;
	float Dist = FMath::Sqrt(DX * DX + DY * DY);

	if (Dist < DeadZone)
	{
		return; // keep current highlight
	}

	// Angle: 0° = top, clockwise
	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY)); // top=0, CW
	if (AngleDeg < 0.0f) AngleDeg += 360.0f;

	float SegAngle = 360.0f / NumSegments;
	HighlightedIndex = FMath::Clamp((int32)(AngleDeg / SegAngle), 0, NumSegments - 1);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (NumSegments == 0) return LayerId;

	FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;
	float SegAngle = 360.0f / NumSegments;

	// Colors
	FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.45f);
	FLinearColor SegNormal(0.12f, 0.12f, 0.15f, 0.85f);
	FLinearColor SegHighlight(0.15f, 0.65f, 0.25f, 0.9f);
	FLinearColor DividerColor(0.35f, 0.35f, 0.40f, 1.0f);
	FLinearColor OutlineColor(0.3f, 0.3f, 0.35f, 0.8f);
	FLinearColor CenterFill(0.06f, 0.06f, 0.08f, 0.92f);
	FLinearColor TextNormal(0.78f, 0.78f, 0.78f, 1.0f);
	FLinearColor TextHighlight(1.0f, 1.0f, 1.0f, 1.0f);

	// 1. Full-screen dark overlay
	FSlateDrawElement::MakeBox(
		OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetDefaultBrush(),
		ESlateDrawEffect::None, OverlayColor);
	LayerId++;

	// 2. Filled segments
	for (int32 i = 0; i < NumSegments; i++)
	{
		float StartDeg = i * SegAngle - 90.0f + SegmentGapDeg / 2.0f;
		float EndDeg = (i + 1) * SegAngle - 90.0f - SegmentGapDeg / 2.0f;
		FLinearColor Fill = (i == HighlightedIndex) ? SegHighlight : SegNormal;
		DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRadius, OuterRadius, StartDeg, EndDeg, Fill);
	}
	LayerId++;

	// 3. Divider lines
	FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();
	for (int32 i = 0; i < NumSegments; i++)
	{
		float Angle = FMath::DegreesToRadians(i * SegAngle - 90.0f);
		FVector2D Inner = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * InnerRadius;
		FVector2D Outer = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * OuterRadius;
		TArray<FVector2D> Pts;
		Pts.Add(Inner);
		Pts.Add(Outer);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PG,
			Pts, ESlateDrawEffect::None, DividerColor, true, 1.5f);
	}
	LayerId++;

	// 4. Circle outlines
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		OuterRadius, -90.0f, 270.0f, OutlineColor, 2.0f);
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		InnerRadius, -90.0f, 270.0f, OutlineColor, 1.5f);
	LayerId++;

	// 5. Inner circle fill (dark center)
	DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
		0.0f, InnerRadius - 2.0f, -90.0f, 270.0f, CenterFill);
	LayerId++;

	// 6. Text labels on each segment
	FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	for (int32 i = 0; i < NumSegments; i++)
	{
		float MidAngle = FMath::DegreesToRadians((i + 0.5f) * SegAngle - 90.0f);
		float LabelR = (InnerRadius + OuterRadius) / 2.0f;
		FVector2D LabelCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * LabelR;

		FString Label = SegmentNames.IsValidIndex(i) ? SegmentNames[i] : TEXT("?");
		FVector2D TextSize = FontMeasure->Measure(Label, LabelFont);
		FVector2D TextPos = LabelCenter - TextSize / 2.0f;

		FLinearColor Tint = (i == HighlightedIndex) ? TextHighlight : TextNormal;

		FGeometry TextGeo = AllottedGeometry.MakeChild(TextSize, FSlateLayoutTransform(TextPos));
		FSlateDrawElement::MakeText(OutDrawElements, LayerId, TextGeo.ToPaintGeometry(),
			Label, LabelFont, ESlateDrawEffect::None, Tint);
	}
	LayerId++;

	// 7. Center text (highlighted piece name)
	{
		FSlateFontInfo CenterFont = FCoreStyle::GetDefaultFontStyle("Bold", 16);
		FString CenterLabel = (HighlightedIndex >= 0 && SegmentNames.IsValidIndex(HighlightedIndex))
			? SegmentNames[HighlightedIndex]
			: TEXT("Select Piece");

		FVector2D CTextSize = FontMeasure->Measure(CenterLabel, CenterFont);
		FVector2D CTextPos = Center - CTextSize / 2.0f;
		FGeometry CTextGeo = AllottedGeometry.MakeChild(CTextSize, FSlateLayoutTransform(CTextPos));
		FSlateDrawElement::MakeText(OutDrawElements, LayerId, CTextGeo.ToPaintGeometry(),
			CenterLabel, CenterFont, ESlateDrawEffect::None, TextHighlight);
	}
	LayerId++;

	return LayerId;
}

// ---------------------------------------------------------------------------
// Draw a filled annular arc using concentric polylines
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawFilledArc(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float InR, float OutR,
	float StartDeg, float EndDeg, FLinearColor Color) const
{
	const int32 ArcSteps = 32;
	const int32 NumRings = 20;
	float Span = OutR - InR;
	if (Span < 1.0f) return;

	float RingSpacing = Span / (float)NumRings;
	float LineWidth = RingSpacing * 1.8f; // overlap to avoid gaps

	FPaintGeometry PG = Geo.ToPaintGeometry();

	for (int32 r = 0; r < NumRings; r++)
	{
		float Radius = InR + RingSpacing * (r + 0.5f);

		TArray<FVector2D> Points;
		Points.Reserve(ArcSteps + 1);

		for (int32 s = 0; s <= ArcSteps; s++)
		{
			float T = (float)s / ArcSteps;
			float Angle = FMath::DegreesToRadians(FMath::Lerp(StartDeg, EndDeg, T));
			Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}

		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PG,
			Points, ESlateDrawEffect::None, Color, false, LineWidth);
	}
}

// ---------------------------------------------------------------------------
// Draw an arc outline (thin polyline)
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawArcOutline(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius,
	float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const
{
	const int32 NumSteps = 64;
	TArray<FVector2D> Points;
	Points.Reserve(NumSteps + 1);

	for (int32 i = 0; i <= NumSteps; i++)
	{
		float T = (float)i / NumSteps;
		float Angle = FMath::DegreesToRadians(FMath::Lerp(StartDeg, EndDeg, T));
		Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}

	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geo.ToPaintGeometry(),
		Points, ESlateDrawEffect::None, Color, true, Thickness);
}
