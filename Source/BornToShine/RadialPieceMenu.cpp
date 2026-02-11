// Born To Shine - Polished Radial Piece Selection Menu

#include "RadialPieceMenu.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Engine/Texture2D.h"

URadialPieceMenu::URadialPieceMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HighlightedIndex = -1;
	PrevHighlightedIndex = -1;
	NumSegments = 0;

	// Geometry
	OuterRadius = 240.0f;
	InnerRadius = 90.0f;
	DeadZone = 45.0f;
	SegmentGapDeg = 1.5f;
	HoverGlowExtend = 14.0f;
	BorderWidth = 5.0f;

	// Animation
	FadeAlpha = 0.0f;
	FadeSpeed = 6.0f;

	// Icons
	IconDisplaySize = 40.0f;

	// --- Color palette: warm construction / wood-tone theme ---
	BgOverlayColor         = FLinearColor(0.0f,  0.0f,  0.0f,  0.60f);
	SegmentFillColor       = FLinearColor(0.10f, 0.08f, 0.06f, 0.90f);
	SegmentHoverColor      = FLinearColor(0.72f, 0.52f, 0.28f, 0.95f);
	SegmentGlowColor       = FLinearColor(0.82f, 0.62f, 0.32f, 0.35f);
	SegmentUnavailableColor= FLinearColor(0.06f, 0.05f, 0.04f, 0.80f);
	DividerColor           = FLinearColor(0.28f, 0.22f, 0.14f, 0.45f);
	OuterOutlineColor      = FLinearColor(0.62f, 0.45f, 0.25f, 0.65f);
	InnerOutlineColor      = FLinearColor(0.40f, 0.30f, 0.18f, 0.55f);
	CenterFillColor        = FLinearColor(0.04f, 0.03f, 0.02f, 0.96f);
	BorderRingColor        = FLinearColor(0.52f, 0.36f, 0.16f, 0.75f);
	TextNormalColor        = FLinearColor(0.78f, 0.74f, 0.68f, 1.0f);
	TextHighlightColor     = FLinearColor(1.0f,  0.96f, 0.88f, 1.0f);
	TextUnavailableColor   = FLinearColor(0.35f, 0.32f, 0.28f, 1.0f);
	SubtitleNormalColor    = FLinearColor(0.55f, 0.50f, 0.44f, 1.0f);
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void URadialPieceMenu::InitMenu(const TArray<FPieceTypeInfo>& InInfos, int32 CurrentIndex)
{
	SegmentInfos = InInfos;
	NumSegments = SegmentInfos.Num();
	HighlightedIndex = CurrentIndex;
	PrevHighlightedIndex = CurrentIndex;
	FadeAlpha = 0.0f;

	// Reset per-segment hover scales
	SegmentHoverScales.Init(0.0f, NumSegments);
	if (CurrentIndex >= 0 && CurrentIndex < NumSegments)
	{
		SegmentHoverScales[CurrentIndex] = 1.0f;
	}

	// Build icon brush cache
	IconBrushes.Empty();
	IconBrushes.SetNum(NumSegments);
	for (int32 i = 0; i < NumSegments; i++)
	{
		UTexture2D* Tex = SegmentInfos[i].Icon.LoadSynchronous();
		if (Tex)
		{
			IconBrushes[i].SetResourceObject(Tex);
			IconBrushes[i].ImageSize = FVector2D(IconDisplaySize, IconDisplaySize);
			IconBrushes[i].DrawAs = ESlateBrushDrawType::Image;
			IconBrushes[i].Tiling = ESlateBrushTileType::NoTile;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RadialPieceMenu: Init %d segments, current=%d"), NumSegments, CurrentIndex);
}

// ---------------------------------------------------------------------------
// Sound effect stubs
// ---------------------------------------------------------------------------
void URadialPieceMenu::PlaySoundOpen()
{
	UE_LOG(LogTemp, Verbose, TEXT("RadialPieceMenu: [SFX] Menu opened"));
	// TODO: Play open sound — call UGameplayStatics::PlaySound2D here
}

void URadialPieceMenu::PlaySoundClose()
{
	UE_LOG(LogTemp, Verbose, TEXT("RadialPieceMenu: [SFX] Menu closed"));
	// TODO: Play close sound
}

void URadialPieceMenu::PlaySoundHover()
{
	UE_LOG(LogTemp, Verbose, TEXT("RadialPieceMenu: [SFX] Segment hover"));
	// TODO: Play hover tick sound
}

// ---------------------------------------------------------------------------
// Fade helper: multiply color alpha by FadeAlpha
// ---------------------------------------------------------------------------
FLinearColor URadialPieceMenu::Faded(FLinearColor Color) const
{
	Color.A *= FadeAlpha;
	return Color;
}

// ---------------------------------------------------------------------------
// Tick: mouse tracking, fade animation, hover interpolation
// ---------------------------------------------------------------------------
void URadialPieceMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (NumSegments == 0) return;

	// --- Fade in ---
	FadeAlpha = FMath::FInterpTo(FadeAlpha, 1.0f, InDeltaTime, FadeSpeed);

	// --- Mouse tracking ---
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);

	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	FVector2D Center = ViewportSize / 2.0f;

	float DX = MouseX - Center.X;
	float DY = MouseY - Center.Y;
	float Dist = FMath::Sqrt(DX * DX + DY * DY);

	if (Dist >= DeadZone)
	{
		// Angle: 0 = top, clockwise
		float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
		if (AngleDeg < 0.0f) AngleDeg += 360.0f;

		float SegAngle = 360.0f / NumSegments;
		HighlightedIndex = FMath::Clamp((int32)(AngleDeg / SegAngle), 0, NumSegments - 1);
	}

	// --- Sound on segment change ---
	if (HighlightedIndex != PrevHighlightedIndex)
	{
		PlaySoundHover();
		PrevHighlightedIndex = HighlightedIndex;
	}

	// --- Per-segment hover scale interpolation ---
	if (SegmentHoverScales.Num() != NumSegments)
	{
		SegmentHoverScales.Init(0.0f, NumSegments);
	}
	for (int32 i = 0; i < NumSegments; i++)
	{
		float Target = (i == HighlightedIndex) ? 1.0f : 0.0f;
		SegmentHoverScales[i] = FMath::FInterpTo(SegmentHoverScales[i], Target, InDeltaTime, 10.0f);
	}
}

// ---------------------------------------------------------------------------
// Paint: full polished rendering
// ---------------------------------------------------------------------------
int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (NumSegments == 0 || FadeAlpha < 0.001f) return LayerId;

	FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;
	float SegAngle = 360.0f / NumSegments;

	const FSlateBrush* DefaultBrush = FCoreStyle::Get().GetDefaultBrush();

	// =====================================================================
	// Layer 1: Full-screen dark overlay
	// =====================================================================
	FSlateDrawElement::MakeBox(
		OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(),
		DefaultBrush, ESlateDrawEffect::None,
		Faded(BgOverlayColor));
	LayerId++;

	// =====================================================================
	// Layer 2: Decorative border ring (outer)
	// =====================================================================
	DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
		OuterRadius, OuterRadius + BorderWidth,
		-90.0f, 270.0f, Faded(BorderRingColor));
	LayerId++;

	// =====================================================================
	// Layer 3: Hover glow (drawn BEHIND the hovered segment for bloom effect)
	// =====================================================================
	for (int32 i = 0; i < NumSegments; i++)
	{
		float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;
		if (HoverT < 0.01f) continue;

		float StartDeg = i * SegAngle - 90.0f + SegmentGapDeg / 2.0f;
		float EndDeg = (i + 1) * SegAngle - 90.0f - SegmentGapDeg / 2.0f;

		float GlowOutR = OuterRadius + HoverGlowExtend * HoverT;
		float GlowInR = InnerRadius - 4.0f * HoverT;

		FLinearColor GlowCol = SegmentGlowColor;
		GlowCol.A *= HoverT * FadeAlpha;
		DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
			GlowInR, GlowOutR, StartDeg, EndDeg, GlowCol);
	}
	LayerId++;

	// =====================================================================
	// Layer 4: Segment fills
	// =====================================================================
	for (int32 i = 0; i < NumSegments; i++)
	{
		float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;
		bool bAvailable = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].bAvailable : true;

		float StartDeg = i * SegAngle - 90.0f + SegmentGapDeg / 2.0f;
		float EndDeg = (i + 1) * SegAngle - 90.0f - SegmentGapDeg / 2.0f;

		// Extend outer radius on hover for pop effect
		float EffOutR = OuterRadius + HoverGlowExtend * 0.5f * HoverT;

		FLinearColor Fill;
		if (!bAvailable)
		{
			Fill = SegmentUnavailableColor;
		}
		else
		{
			Fill = FMath::Lerp(SegmentFillColor, SegmentHoverColor, HoverT);
		}

		DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRadius, EffOutR, StartDeg, EndDeg, Faded(Fill));
	}
	LayerId++;

	// =====================================================================
	// Layer 5: Divider lines between segments
	// =====================================================================
	{
		FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();
		for (int32 i = 0; i < NumSegments; i++)
		{
			float Angle = FMath::DegreesToRadians(i * SegAngle - 90.0f);
			FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			FVector2D Inner = Center + Dir * InnerRadius;
			FVector2D Outer = Center + Dir * (OuterRadius + 1.0f);

			TArray<FVector2D> Pts;
			Pts.Add(Inner);
			Pts.Add(Outer);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PG,
				Pts, ESlateDrawEffect::None, Faded(DividerColor), true, 1.0f);
		}
	}
	LayerId++;

	// =====================================================================
	// Layer 6: Circle outlines (outer + inner)
	// =====================================================================
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		OuterRadius, -90.0f, 270.0f, Faded(OuterOutlineColor), 2.0f);
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		InnerRadius, -90.0f, 270.0f, Faded(InnerOutlineColor), 1.5f);
	LayerId++;

	// =====================================================================
	// Layer 7: Center hub fill
	// =====================================================================
	DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
		0.0f, InnerRadius - 2.0f, -90.0f, 270.0f, Faded(CenterFillColor));
	// Subtle warm-tint ring just inside the inner radius
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		InnerRadius - 3.0f, -90.0f, 270.0f, Faded(FLinearColor(0.72f, 0.52f, 0.28f, 0.20f)), 2.0f);
	LayerId++;

	// =====================================================================
	// Layer 8: Icons per segment
	// =====================================================================
	{
		float IconR = InnerRadius + (OuterRadius - InnerRadius) * 0.55f;

		for (int32 i = 0; i < NumSegments; i++)
		{
			float MidAngle = FMath::DegreesToRadians((i + 0.5f) * SegAngle - 90.0f);
			FVector2D IconCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * IconR;

			bool bAvailable = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].bAvailable : true;

			if (IconBrushes.IsValidIndex(i) && IconBrushes[i].GetResourceObject())
			{
				float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;
				float ScaledSize = IconDisplaySize * (1.0f + 0.12f * HoverT);

				FVector2D TexSize(ScaledSize, ScaledSize);
				FVector2D TexPos = IconCenter - TexSize / 2.0f;

				FGeometry IconGeo = AllottedGeometry.MakeChild(TexSize, FSlateLayoutTransform(TexPos));

				FLinearColor IconTint = bAvailable
					? FLinearColor(1.0f, 1.0f, 1.0f, FadeAlpha)
					: FLinearColor(0.3f, 0.3f, 0.3f, FadeAlpha * 0.6f);

				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					IconGeo.ToPaintGeometry(), &IconBrushes[i],
					ESlateDrawEffect::None, IconTint);
			}
		}
	}
	LayerId++;

	// =====================================================================
	// Layer 9: Subtitle text per segment (below icon)
	// =====================================================================
	{
		FSlateFontInfo SubFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
		TSharedRef<FSlateFontMeasure> FontMeasure =
			FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		float SubR = InnerRadius + (OuterRadius - InnerRadius) * 0.25f;

		for (int32 i = 0; i < NumSegments; i++)
		{
			FString Sub = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].Subtitle : TEXT("");
			if (Sub.IsEmpty()) continue;

			bool bAvailable = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].bAvailable : true;

			float MidAngle = FMath::DegreesToRadians((i + 0.5f) * SegAngle - 90.0f);
			FVector2D LabelCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * SubR;

			FVector2D TextSize = FontMeasure->Measure(Sub, SubFont);
			FVector2D TextPos = LabelCenter - TextSize / 2.0f;

			FLinearColor Tint = bAvailable ? SubtitleNormalColor : TextUnavailableColor;
			Tint.A *= FadeAlpha;

			FGeometry TextGeo = AllottedGeometry.MakeChild(TextSize, FSlateLayoutTransform(TextPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, TextGeo.ToPaintGeometry(),
				Sub, SubFont, ESlateDrawEffect::None, Tint);
		}
	}
	LayerId++;

	// =====================================================================
	// Layer 10: Center hub text (hovered piece name + subtitle)
	// =====================================================================
	{
		TSharedRef<FSlateFontMeasure> FontMeasure =
			FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		bool bHasSelection = (HighlightedIndex >= 0 && SegmentInfos.IsValidIndex(HighlightedIndex));

		// Main piece name
		FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", bHasSelection ? 18 : 14);
		FString NameLabel = bHasSelection
			? SegmentInfos[HighlightedIndex].DisplayName
			: TEXT("Select Piece");

		FVector2D NameSize = FontMeasure->Measure(NameLabel, NameFont);
		FVector2D NamePos = Center - FVector2D(NameSize.X / 2.0f, NameSize.Y + 2.0f);

		FGeometry NameGeo = AllottedGeometry.MakeChild(NameSize, FSlateLayoutTransform(NamePos));
		FSlateDrawElement::MakeText(OutDrawElements, LayerId, NameGeo.ToPaintGeometry(),
			NameLabel, NameFont, ESlateDrawEffect::None, Faded(TextHighlightColor));

		// Subtitle under the name
		if (bHasSelection && !SegmentInfos[HighlightedIndex].Subtitle.IsEmpty())
		{
			FSlateFontInfo SubFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
			FString SubLabel = SegmentInfos[HighlightedIndex].Subtitle;

			FVector2D SubSize = FontMeasure->Measure(SubLabel, SubFont);
			FVector2D SubPos = Center + FVector2D(-SubSize.X / 2.0f, 4.0f);

			FGeometry SubGeo = AllottedGeometry.MakeChild(SubSize, FSlateLayoutTransform(SubPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, SubGeo.ToPaintGeometry(),
				SubLabel, SubFont, ESlateDrawEffect::None, Faded(SubtitleNormalColor));
		}
	}
	LayerId++;

	return LayerId;
}

// ---------------------------------------------------------------------------
// Draw a filled annular arc using concentric polyline rings
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawFilledArc(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float InR, float OutR,
	float StartDeg, float EndDeg, FLinearColor Color) const
{
	if (Color.A < 0.001f) return;

	const int32 ArcSteps = 36;
	const int32 NumRings = 24;
	float Span = OutR - InR;
	if (Span < 1.0f) return;

	float RingSpacing = Span / (float)NumRings;
	float LineWidth = RingSpacing * 1.9f;

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
	if (Color.A < 0.001f) return;

	const int32 NumSteps = 72;
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
