// Born To Shine - Radial Piece Selection Menu (Sci-fi holographic style)

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

	// --- Geometry ---
	OuterRadius    = 480.0f;
	InnerRadius    = 200.0f;
	CenterHubRadius = 170.0f;
	DeadZone       = 80.0f;

	// Animation
	FadeAlpha = 0.0f;
	FadeSpeed = 8.0f;
	GlowPulseTime = 0.0f;

	// Icons — large segment icons, prominent center icon
	SegmentIconSize = 220.0f;
	CenterIconSize  = 140.0f;

	// --- Color palette: sci-fi holographic blueprint ---
	BgOverlayColor           = FLinearColor(0.01f, 0.02f, 0.05f, 0.80f);
	SegmentFillColor         = FLinearColor(0.06f, 0.08f, 0.14f, 0.80f);
	SegmentHoverFillColor    = FLinearColor(0.00f, 0.80f, 0.90f, 0.92f);
	SegmentHoverGlowColor    = FLinearColor(0.00f, 0.90f, 1.00f, 0.45f);
	SegmentUnavailableColor  = FLinearColor(0.03f, 0.04f, 0.06f, 0.70f);
	DividerColor             = FLinearColor(0.10f, 0.30f, 0.40f, 0.25f);
	BorderAccentColor        = FLinearColor(0.00f, 0.70f, 0.80f, 0.55f);
	CenterFillColor          = FLinearColor(0.01f, 0.02f, 0.04f, 0.95f);
	CenterBorderColor        = FLinearColor(0.00f, 0.60f, 0.70f, 0.55f);
	TextWhite                = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextDimmed               = FLinearColor(0.55f, 0.65f, 0.75f, 1.0f);
	TextUnavailable          = FLinearColor(0.20f, 0.25f, 0.30f, 1.0f);
	SubtitleColor            = FLinearColor(0.40f, 0.55f, 0.65f, 1.0f);
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
	GlowPulseTime = 0.0f;

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
			IconBrushes[i].ImageSize = FVector2D(SegmentIconSize, SegmentIconSize);
			IconBrushes[i].DrawAs = ESlateBrushDrawType::Image;
			IconBrushes[i].Tiling = ESlateBrushTileType::NoTile;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RadialPieceMenu: Init %d segments, current=%d"), NumSegments, CurrentIndex);
}

// ---------------------------------------------------------------------------
// Sound stubs
// ---------------------------------------------------------------------------
void URadialPieceMenu::PlaySoundOpen()
{
	UE_LOG(LogTemp, Verbose, TEXT("RadialPieceMenu: [SFX] Menu opened"));
}

void URadialPieceMenu::PlaySoundClose()
{
	UE_LOG(LogTemp, Verbose, TEXT("RadialPieceMenu: [SFX] Menu closed"));
}

void URadialPieceMenu::PlaySoundHover()
{
	UE_LOG(LogTemp, Verbose, TEXT("RadialPieceMenu: [SFX] Segment hover"));
}

// ---------------------------------------------------------------------------
// Fade helper
// ---------------------------------------------------------------------------
FLinearColor URadialPieceMenu::Faded(FLinearColor Color) const
{
	Color.A *= FadeAlpha;
	return Color;
}

// ---------------------------------------------------------------------------
// Tick: mouse tracking, fade animation, hover interpolation, pulse timer
// ---------------------------------------------------------------------------
void URadialPieceMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (NumSegments == 0) return;

	// Fade in
	FadeAlpha = FMath::FInterpTo(FadeAlpha, 1.0f, InDeltaTime, FadeSpeed);

	// Breathing glow pulse (accumulate time)
	GlowPulseTime += InDeltaTime;

	// Mouse tracking
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
		float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
		if (AngleDeg < 0.0f) AngleDeg += 360.0f;

		float SegAngle = 360.0f / NumSegments;
		HighlightedIndex = FMath::Clamp((int32)(AngleDeg / SegAngle), 0, NumSegments - 1);
	}

	// Sound on segment change
	if (HighlightedIndex != PrevHighlightedIndex)
	{
		PlaySoundHover();
		PrevHighlightedIndex = HighlightedIndex;
	}

	// Per-segment hover scale interpolation
	if (SegmentHoverScales.Num() != NumSegments)
	{
		SegmentHoverScales.Init(0.0f, NumSegments);
	}
	for (int32 i = 0; i < NumSegments; i++)
	{
		float Target = (i == HighlightedIndex) ? 1.0f : 0.0f;
		SegmentHoverScales[i] = FMath::FInterpTo(SegmentHoverScales[i], Target, InDeltaTime, 12.0f);
	}
}

// ---------------------------------------------------------------------------
// Paint: sci-fi holographic blueprint rendering
// ---------------------------------------------------------------------------
int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (NumSegments == 0 || FadeAlpha < 0.001f) return LayerId;

	FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;
	float SegAngle = 360.0f / NumSegments;

	static const FSlateColorBrush SolidBrush(FLinearColor::White);

	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	FSlateFontInfo SegNameFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	FSlateFontInfo CenterNameFont = FCoreStyle::GetDefaultFontStyle("Bold", 22);
	FSlateFontInfo CenterSubFont  = FCoreStyle::GetDefaultFontStyle("Regular", 14);

	// Breathing pulse (slow sine wave, 0.85–1.0 range)
	float Pulse = 0.85f + 0.15f * (0.5f + 0.5f * FMath::Sin(GlowPulseTime * 2.2f));

	// =====================================================================
	// LAYER 1: Menu-area dark background (circle, NOT fullscreen overlay)
	// Game world shows through everywhere outside the menu.
	// =====================================================================
	DrawCircleFill(OutDrawElements, LayerId, AllottedGeometry, Center,
		OuterRadius + 24.0f, Faded(BgOverlayColor));
	LayerId++;

	// =====================================================================
	// LAYER 2: Clean single-layer cyan glow on hovered segment
	// One soft bloom — no triple ring effect.
	// =====================================================================
	for (int32 i = 0; i < NumSegments; i++)
	{
		float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;
		if (HoverT < 0.01f) continue;

		float StartDeg = i * SegAngle - 90.0f;
		float EndDeg = (i + 1) * SegAngle - 90.0f;

		// Single soft cyan bloom behind the segment
		FLinearColor GlowCol = SegmentHoverGlowColor;
		GlowCol.A = SegmentHoverGlowColor.A * HoverT * FadeAlpha * Pulse;
		DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRadius - 2.0f * HoverT, OuterRadius + 4.0f * HoverT,
			StartDeg - 0.8f * HoverT, EndDeg + 0.8f * HoverT, GlowCol);
	}
	LayerId++;

	// =====================================================================
	// LAYER 3: Segment fills — clean flat fill, bright cyan on hover
	// =====================================================================
	for (int32 i = 0; i < NumSegments; i++)
	{
		float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;
		bool bAvailable = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].bAvailable : true;

		float GapHalf = 0.6f;
		float StartDeg = i * SegAngle - 90.0f + GapHalf;
		float EndDeg = (i + 1) * SegAngle - 90.0f - GapHalf;

		if (!bAvailable)
		{
			DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
				InnerRadius, OuterRadius, StartDeg, EndDeg, Faded(SegmentUnavailableColor));
		}
		else
		{
			// Single clean fill: lerp from dark to bright cyan
			FLinearColor FillCol = FMath::Lerp(SegmentFillColor, SegmentHoverFillColor, HoverT);
			DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
				InnerRadius, OuterRadius, StartDeg, EndDeg, Faded(FillCol));
		}
	}
	LayerId++;

	// =====================================================================
	// LAYER 4: 3D metallic beveled divider lines between segments
	// Five parallel strokes simulate highlight → body → shadow depth.
	// =====================================================================
	{
		FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();

		// Metallic bevel colors (same palette as outer ring)
		FLinearColor DivHighlight(0.45f, 0.55f, 0.60f, 0.55f);
		FLinearColor DivBevelLight(0.28f, 0.35f, 0.40f, 0.60f);
		FLinearColor DivBody(0.12f, 0.16f, 0.20f, 0.75f);
		FLinearColor DivBevelDark(0.06f, 0.08f, 0.10f, 0.65f);
		FLinearColor DivShadow(0.02f, 0.03f, 0.05f, 0.55f);

		struct FDivLayer { float Offset; FLinearColor Color; float Width; };
		const FDivLayer Layers[] = {
			{ -2.0f, DivHighlight,  1.0f },
			{ -0.8f, DivBevelLight, 1.2f },
			{  0.0f, DivBody,       1.5f },
			{  0.8f, DivBevelDark,  1.2f },
			{  2.0f, DivShadow,     1.0f },
		};

		for (int32 i = 0; i < NumSegments; i++)
		{
			float AngleRad = FMath::DegreesToRadians(i * SegAngle - 90.0f);
			FVector2D Radial(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
			FVector2D Tangent(-FMath::Sin(AngleRad), FMath::Cos(AngleRad));

			for (const FDivLayer& L : Layers)
			{
				FVector2D Off = Tangent * L.Offset;
				FVector2D Inner = Center + Radial * InnerRadius + Off;
				FVector2D Outer = Center + Radial * OuterRadius + Off;

				TArray<FVector2D> Pts;
				Pts.Add(Inner);
				Pts.Add(Outer);
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PG,
					Pts, ESlateDrawEffect::None, Faded(L.Color), true, L.Width);
			}
		}
	}
	LayerId++;

	// =====================================================================
	// LAYER 5a: 3D metallic outer ring — beveled chrome appearance
	// Multiple concentric bands simulate depth: highlight → body → shadow
	// =====================================================================
	{
		FLinearColor HighlightEdge(0.45f, 0.55f, 0.60f, 0.65f * Pulse);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			OuterRadius + 4.0f, -90.0f, 270.0f, Faded(HighlightEdge), 1.5f);

		FLinearColor BevelLight(0.28f, 0.35f, 0.40f, 0.70f);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			OuterRadius + 2.5f, -90.0f, 270.0f, Faded(BevelLight), 2.0f);

		FLinearColor RingBody(0.15f, 0.20f, 0.25f, 0.85f);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			OuterRadius, -90.0f, 270.0f, Faded(RingBody), 5.0f);

		FLinearColor BevelDark(0.06f, 0.08f, 0.10f, 0.80f);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			OuterRadius - 2.5f, -90.0f, 270.0f, Faded(BevelDark), 2.0f);

		FLinearColor ShadowEdge(0.02f, 0.03f, 0.05f, 0.70f);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			OuterRadius - 4.0f, -90.0f, 270.0f, Faded(ShadowEdge), 1.5f);
	}
	LayerId++;

	// =====================================================================
	// LAYER 5b: Turquoise energy glow on outer ring (UE5-style emission)
	// =====================================================================
	{
		FLinearColor WideBloom(0.00f, 0.70f, 0.85f, 0.06f * Pulse);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			OuterRadius, -90.0f, 270.0f, Faded(WideBloom), 20.0f);

		FLinearColor MedBloom(0.00f, 0.75f, 0.90f, 0.10f * Pulse);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			OuterRadius, -90.0f, 270.0f, Faded(MedBloom), 10.0f);

		FLinearColor AccentLine(0.00f, 0.85f, 0.95f, 0.60f * Pulse);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			OuterRadius, -90.0f, 270.0f, Faded(AccentLine), 2.0f);
	}
	LayerId++;

	// =====================================================================
	// LAYER 5c: Inner ring border — subtle metallic + cyan accent
	// =====================================================================
	{
		FLinearColor InnerRingBody(0.12f, 0.16f, 0.20f, 0.70f);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRadius, -90.0f, 270.0f, Faded(InnerRingBody), 3.0f);

		FLinearColor InnerGlow(0.00f, 0.65f, 0.80f, 0.08f * Pulse);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRadius, -90.0f, 270.0f, Faded(InnerGlow), 10.0f);

		FLinearColor InnerAccent(0.00f, 0.70f, 0.85f, 0.40f * Pulse);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRadius, -90.0f, 270.0f, Faded(InnerAccent), 1.5f);
	}
	LayerId++;

	// =====================================================================
	// LAYER 6: Large icons per segment — centered inside the segment band
	// =====================================================================
	{
		// Center icons at 55% through the band (slight outer bias for visual balance)
		float IconR = InnerRadius + (OuterRadius - InnerRadius) * 0.55f;

		for (int32 i = 0; i < NumSegments; i++)
		{
			float MidAngle = FMath::DegreesToRadians((i + 0.5f) * SegAngle - 90.0f);
			FVector2D IconCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * IconR;

			bool bAvailable = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].bAvailable : true;
			float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;

			if (IconBrushes.IsValidIndex(i) && IconBrushes[i].GetResourceObject())
			{
				float ScaledSize = SegmentIconSize * (1.0f + 0.10f * HoverT);

				FVector2D TexSize(ScaledSize, ScaledSize);
				FVector2D TexPos = IconCenter - TexSize / 2.0f;

				FGeometry IconGeo = AllottedGeometry.MakeChild(TexSize, FSlateLayoutTransform(TexPos));

				FLinearColor IconTint;
				if (!bAvailable)
					IconTint = FLinearColor(0.20f, 0.25f, 0.30f, FadeAlpha * 0.5f);
				else
					IconTint = FMath::Lerp(
						FLinearColor(0.85f, 0.90f, 0.95f, FadeAlpha),
						FLinearColor(0.70f, 1.00f, 1.00f, FadeAlpha),
						HoverT);

				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					IconGeo.ToPaintGeometry(), &IconBrushes[i],
					ESlateDrawEffect::None, IconTint);
			}
		}
	}
	LayerId++;

	// =====================================================================
	// LAYER 7: Piece name text per segment (bold name only, no subtitle)
	// =====================================================================
	{
		float NameR = InnerRadius + (OuterRadius - InnerRadius) * 0.18f;

		for (int32 i = 0; i < NumSegments; i++)
		{
			FString Name = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].DisplayName : TEXT("");
			if (Name.IsEmpty()) continue;

			bool bAvailable = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].bAvailable : true;
			float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;

			float MidAngle = FMath::DegreesToRadians((i + 0.5f) * SegAngle - 90.0f);
			FVector2D LabelCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * NameR;

			FVector2D TextSize = FontMeasure->Measure(Name, SegNameFont);
			FVector2D TextPos = LabelCenter - TextSize / 2.0f;

			FLinearColor Tint;
			if (!bAvailable)
				Tint = TextUnavailable;
			else
				Tint = FMath::Lerp(TextDimmed, TextWhite, HoverT);
			Tint.A *= FadeAlpha;

			FGeometry TextGeo = AllottedGeometry.MakeChild(TextSize, FSlateLayoutTransform(TextPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, TextGeo.ToPaintGeometry(),
				Name, SegNameFont, ESlateDrawEffect::None, Tint);
		}
	}
	LayerId++;

	// (Layer 8 — segment subtitles removed per design request)

	// =====================================================================
	// LAYER 9: Center hub — dark fill + turquoise glow ring
	// =====================================================================
	DrawCircleFill(OutDrawElements, LayerId, AllottedGeometry, Center,
		CenterHubRadius, Faded(CenterFillColor));

	FLinearColor HubGlow(0.00f, 0.65f, 0.80f, 0.10f * Pulse);
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		CenterHubRadius, -90.0f, 270.0f, Faded(HubGlow), 16.0f);

	FLinearColor HubGlowMed(0.00f, 0.70f, 0.85f, 0.16f * Pulse);
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		CenterHubRadius, -90.0f, 270.0f, Faded(HubGlowMed), 8.0f);

	FLinearColor HubAccent(0.00f, 0.75f, 0.88f, 0.50f * Pulse);
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		CenterHubRadius, -90.0f, 270.0f, Faded(HubAccent), 2.0f);
	LayerId++;

	// =====================================================================
	// LAYER 10: Center hub content — large icon + piece name + subtitle
	// =====================================================================
	{
		bool bHasSelection = (HighlightedIndex >= 0 && SegmentInfos.IsValidIndex(HighlightedIndex));

		if (bHasSelection)
		{
			const FPieceTypeInfo& Info = SegmentInfos[HighlightedIndex];

			// Large icon in center
			if (IconBrushes.IsValidIndex(HighlightedIndex) && IconBrushes[HighlightedIndex].GetResourceObject())
			{
				FVector2D TexSize(CenterIconSize, CenterIconSize);
				FVector2D TexPos = Center - FVector2D(CenterIconSize / 2.0f, CenterIconSize / 2.0f + 24.0f);

				FGeometry IconGeo = AllottedGeometry.MakeChild(TexSize, FSlateLayoutTransform(TexPos));

				FLinearColor CenterIconTint(0.80f, 0.95f, 1.00f, FadeAlpha);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					IconGeo.ToPaintGeometry(), &IconBrushes[HighlightedIndex],
					ESlateDrawEffect::None, CenterIconTint);
			}

			// Piece name (centered below icon)
			FVector2D NameSize = FontMeasure->Measure(Info.DisplayName, CenterNameFont);
			FVector2D NamePos = Center + FVector2D(-NameSize.X / 2.0f, CenterIconSize / 2.0f - 14.0f);
			FGeometry NameGeo = AllottedGeometry.MakeChild(NameSize, FSlateLayoutTransform(NamePos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, NameGeo.ToPaintGeometry(),
				Info.DisplayName, CenterNameFont, ESlateDrawEffect::None, Faded(TextWhite));
		}
		else
		{
			FString Prompt = TEXT("Select Piece");
			FSlateFontInfo PromptFont = FCoreStyle::GetDefaultFontStyle("Regular", 16);
			FVector2D PromptSize = FontMeasure->Measure(Prompt, PromptFont);
			FVector2D PromptPos = Center - PromptSize / 2.0f;
			FGeometry PromptGeo = AllottedGeometry.MakeChild(PromptSize, FSlateLayoutTransform(PromptPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, PromptGeo.ToPaintGeometry(),
				Prompt, PromptFont, ESlateDrawEffect::None, Faded(TextDimmed));
		}
	}
	LayerId++;

	return LayerId;
}

// ---------------------------------------------------------------------------
// DrawFilledArc: filled annular arc using concentric polyline rings
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawFilledArc(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float InR, float OutR,
	float StartDeg, float EndDeg, FLinearColor Color, int32 ArcSteps) const
{
	if (Color.A < 0.001f) return;

	float Span = OutR - InR;
	if (Span < 1.0f) return;

	int32 NumRings = FMath::Max(16, FMath::CeilToInt(Span / 3.5f));
	float RingSpacing = Span / (float)NumRings;
	float LineWidth = RingSpacing * 1.95f;

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
// DrawArcOutline: thin polyline circle arc
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawArcOutline(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius,
	float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const
{
	if (Color.A < 0.001f) return;

	const int32 NumSteps = 96;
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

// ---------------------------------------------------------------------------
// DrawCircleFill: solid filled circle
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawCircleFill(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const
{
	DrawFilledArc(OutDrawElements, LayerId, Geo, Center, 0.0f, Radius,
		-90.0f, 270.0f, Color, 64);
}
