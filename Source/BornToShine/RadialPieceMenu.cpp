// Born To Shine - Radial Piece Selection Menu (Rust/Fortnite quality)

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

	// --- Geometry: DOUBLED from original (was 240/90) ---
	OuterRadius    = 480.0f;
	InnerRadius    = 200.0f;
	CenterHubRadius = 170.0f;
	DeadZone       = 80.0f;

	// Animation
	FadeAlpha = 0.0f;
	FadeSpeed = 8.0f;

	// Icons — large and prominent (80+ for readability)
	SegmentIconSize = 80.0f;
	CenterIconSize  = 96.0f;

	// --- Color palette: shipped-game quality ---
	// Background: dark gray, solid enough to kill any bleed-through
	BgOverlayColor           = FLinearColor(0.02f, 0.02f, 0.03f, 0.88f);
	// Unselected segments: subtle dark
	SegmentFillColor         = FLinearColor(0.08f, 0.08f, 0.09f, 0.85f);
	// Selected/hovered: warm wood tone
	SegmentHoverFillColor    = FLinearColor(0.65f, 0.42f, 0.14f, 0.92f);
	// Glow behind hovered segment
	SegmentHoverGlowColor    = FLinearColor(0.80f, 0.55f, 0.18f, 0.30f);
	// Unavailable: very dark
	SegmentUnavailableColor  = FLinearColor(0.04f, 0.04f, 0.04f, 0.75f);
	// Thin dividers
	DividerColor             = FLinearColor(0.25f, 0.22f, 0.18f, 0.30f);
	// Gold/bronze border accent
	BorderAccentColor        = FLinearColor(0.72f, 0.55f, 0.22f, 0.50f);
	// Center hub
	CenterFillColor          = FLinearColor(0.03f, 0.03f, 0.04f, 0.95f);
	CenterBorderColor        = FLinearColor(0.60f, 0.45f, 0.18f, 0.60f);
	// Text: white
	TextWhite                = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextDimmed               = FLinearColor(0.70f, 0.68f, 0.65f, 1.0f);
	TextUnavailable          = FLinearColor(0.30f, 0.28f, 0.25f, 1.0f);
	// Subtitle: lighter gray
	SubtitleColor            = FLinearColor(0.55f, 0.52f, 0.48f, 1.0f);
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

	// Build icon brush cache — load all textures now
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
// Tick: mouse tracking, fade animation, hover interpolation
// ---------------------------------------------------------------------------
void URadialPieceMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (NumSegments == 0) return;

	// Fade in
	FadeAlpha = FMath::FInterpTo(FadeAlpha, 1.0f, InDeltaTime, FadeSpeed);

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
		// Angle: 0 = top, clockwise
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
// Paint: full production-quality rendering
// ---------------------------------------------------------------------------
int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (NumSegments == 0 || FadeAlpha < 0.001f) return LayerId;

	FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;
	float SegAngle = 360.0f / NumSegments;

	// Solid white brush — the CoreStyle "default" brush is a checkerboard
	static const FSlateColorBrush SolidBrush(FLinearColor::White);

	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// Fonts — clean modern style
	FSlateFontInfo SegNameFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	FSlateFontInfo SegSubFont  = FCoreStyle::GetDefaultFontStyle("Regular", 11);
	FSlateFontInfo CenterNameFont = FCoreStyle::GetDefaultFontStyle("Bold", 22);
	FSlateFontInfo CenterSubFont  = FCoreStyle::GetDefaultFontStyle("Regular", 14);

	// =====================================================================
	// LAYER 1: Full-screen dark semi-transparent overlay (frost effect)
	// =====================================================================
	FSlateDrawElement::MakeBox(
		OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(),
		&SolidBrush, ESlateDrawEffect::None,
		Faded(BgOverlayColor));
	LayerId++;

	// =====================================================================
	// LAYER 2: Hover glow (bloom behind hovered segment)
	// =====================================================================
	for (int32 i = 0; i < NumSegments; i++)
	{
		float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;
		if (HoverT < 0.01f) continue;

		float StartDeg = i * SegAngle - 90.0f;
		float EndDeg = (i + 1) * SegAngle - 90.0f;

		// Glow extends outward and inward from the segment
		float GlowOutR = OuterRadius + 18.0f * HoverT;
		float GlowInR = InnerRadius - 8.0f * HoverT;

		FLinearColor GlowCol = SegmentHoverGlowColor;
		GlowCol.A *= HoverT * FadeAlpha;
		DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
			GlowInR, GlowOutR, StartDeg, EndDeg, GlowCol);
	}
	LayerId++;

	// =====================================================================
	// LAYER 3: Segment fills (equal sized, no gaps between segments)
	// =====================================================================
	for (int32 i = 0; i < NumSegments; i++)
	{
		float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;
		bool bAvailable = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].bAvailable : true;

		// Thin 1-degree gap on each side of divider
		float GapHalf = 0.6f;
		float StartDeg = i * SegAngle - 90.0f + GapHalf;
		float EndDeg = (i + 1) * SegAngle - 90.0f - GapHalf;

		// Slight outward push on hover
		float EffOutR = OuterRadius + 6.0f * HoverT;

		FLinearColor Fill;
		if (!bAvailable)
		{
			Fill = SegmentUnavailableColor;
		}
		else
		{
			Fill = FMath::Lerp(SegmentFillColor, SegmentHoverFillColor, HoverT);
		}

		DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRadius, EffOutR, StartDeg, EndDeg, Faded(Fill));
	}
	LayerId++;

	// =====================================================================
	// LAYER 4: Thin divider lines between segments
	// =====================================================================
	{
		FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();
		for (int32 i = 0; i < NumSegments; i++)
		{
			float Angle = FMath::DegreesToRadians(i * SegAngle - 90.0f);
			FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			FVector2D Inner = Center + Dir * InnerRadius;
			FVector2D Outer = Center + Dir * OuterRadius;

			TArray<FVector2D> Pts;
			Pts.Add(Inner);
			Pts.Add(Outer);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PG,
				Pts, ESlateDrawEffect::None, Faded(DividerColor), true, 1.0f);
		}
	}
	LayerId++;

	// =====================================================================
	// LAYER 5: Gold/bronze accent border (outer ring — thin, no ticks)
	// =====================================================================
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		OuterRadius, -90.0f, 270.0f, Faded(BorderAccentColor), 1.5f);
	// Inner ring accent
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		InnerRadius, -90.0f, 270.0f, Faded(FLinearColor(BorderAccentColor.R, BorderAccentColor.G, BorderAccentColor.B, BorderAccentColor.A * 0.5f)), 1.0f);
	LayerId++;

	// =====================================================================
	// LAYER 6: Icons per segment — LARGE and centered (main visual)
	// =====================================================================
	{
		// Icon centered in the segment arc (content group centroid)
		float IconR = InnerRadius + (OuterRadius - InnerRadius) * 0.65f;

		for (int32 i = 0; i < NumSegments; i++)
		{
			float MidAngle = FMath::DegreesToRadians((i + 0.5f) * SegAngle - 90.0f);
			FVector2D IconCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * IconR;

			bool bAvailable = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].bAvailable : true;
			float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;

			if (IconBrushes.IsValidIndex(i) && IconBrushes[i].GetResourceObject())
			{
				// Icons scale up 15% on hover
				float ScaledSize = SegmentIconSize * (1.0f + 0.15f * HoverT);

				FVector2D TexSize(ScaledSize, ScaledSize);
				FVector2D TexPos = IconCenter - TexSize / 2.0f;

				FGeometry IconGeo = AllottedGeometry.MakeChild(TexSize, FSlateLayoutTransform(TexPos));

				FLinearColor IconTint = bAvailable
					? FLinearColor(1.0f, 1.0f, 1.0f, FadeAlpha)
					: FLinearColor(0.25f, 0.25f, 0.25f, FadeAlpha * 0.5f);

				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					IconGeo.ToPaintGeometry(), &IconBrushes[i],
					ESlateDrawEffect::None, IconTint);
			}
		}
	}
	LayerId++;

	// =====================================================================
	// LAYER 7: Piece name text per segment (small, below icon)
	// =====================================================================
	{
		float NameR = InnerRadius + (OuterRadius - InnerRadius) * 0.35f;

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

	// =====================================================================
	// LAYER 8: Size subtitle per segment (below name, even smaller)
	// =====================================================================
	{
		float SubR = InnerRadius + (OuterRadius - InnerRadius) * 0.23f;

		for (int32 i = 0; i < NumSegments; i++)
		{
			FString Sub = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].Subtitle : TEXT("");
			if (Sub.IsEmpty()) continue;

			bool bAvailable = SegmentInfos.IsValidIndex(i) ? SegmentInfos[i].bAvailable : true;
			float HoverT = SegmentHoverScales.IsValidIndex(i) ? SegmentHoverScales[i] : 0.0f;

			float MidAngle = FMath::DegreesToRadians((i + 0.5f) * SegAngle - 90.0f);
			FVector2D LabelCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * SubR;

			FVector2D TextSize = FontMeasure->Measure(Sub, SegSubFont);
			FVector2D TextPos = LabelCenter - TextSize / 2.0f;

			FLinearColor Tint;
			if (!bAvailable)
				Tint = TextUnavailable;
			else
				Tint = FMath::Lerp(SubtitleColor, FLinearColor(0.75f, 0.70f, 0.62f, 1.0f), HoverT);
			Tint.A *= FadeAlpha;

			FGeometry TextGeo = AllottedGeometry.MakeChild(TextSize, FSlateLayoutTransform(TextPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, TextGeo.ToPaintGeometry(),
				Sub, SegSubFont, ESlateDrawEffect::None, Tint);
		}
	}
	LayerId++;

	// =====================================================================
	// LAYER 9: Center hub — dark fill + border ring
	// =====================================================================
	DrawCircleFill(OutDrawElements, LayerId, AllottedGeometry, Center,
		CenterHubRadius, Faded(CenterFillColor));
	// Gold accent ring around center hub
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		CenterHubRadius, -90.0f, 270.0f, Faded(CenterBorderColor), 2.0f);
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
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					IconGeo.ToPaintGeometry(), &IconBrushes[HighlightedIndex],
					ESlateDrawEffect::None, Faded(TextWhite));
			}

			// Piece name below icon
			FVector2D NameSize = FontMeasure->Measure(Info.DisplayName, CenterNameFont);
			FVector2D NamePos = Center + FVector2D(-NameSize.X / 2.0f, CenterIconSize / 2.0f - 14.0f);
			FGeometry NameGeo = AllottedGeometry.MakeChild(NameSize, FSlateLayoutTransform(NamePos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, NameGeo.ToPaintGeometry(),
				Info.DisplayName, CenterNameFont, ESlateDrawEffect::None, Faded(TextWhite));

			// Size subtitle below name
			if (!Info.Subtitle.IsEmpty())
			{
				FVector2D SubSize = FontMeasure->Measure(Info.Subtitle, CenterSubFont);
				FVector2D SubPos = NamePos + FVector2D(NameSize.X / 2.0f - SubSize.X / 2.0f, NameSize.Y + 4.0f);
				FGeometry SubGeo = AllottedGeometry.MakeChild(SubSize, FSlateLayoutTransform(SubPos));
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, SubGeo.ToPaintGeometry(),
					Info.Subtitle, CenterSubFont, ESlateDrawEffect::None, Faded(SubtitleColor));
			}
		}
		else
		{
			// No selection: "Select Piece" prompt
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

	// More rings for larger arcs = smoother fill
	int32 NumRings = FMath::Max(16, FMath::CeilToInt(Span / 3.5f));
	float RingSpacing = Span / (float)NumRings;
	float LineWidth = RingSpacing * 1.95f; // Slight overlap to prevent gaps

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
// DrawCircleFill: solid filled circle using filled arc from 0 to full
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawCircleFill(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const
{
	DrawFilledArc(OutDrawElements, LayerId, Geo, Center, 0.0f, Radius,
		-90.0f, 270.0f, Color, 64);
}
