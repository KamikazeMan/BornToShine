// Born To Shine - Two-Tier Radial Piece Selection Menu (Cyan Glow Sci-fi)
//
// Exact match to the React SVG prototype:
//   - Transparent background (see through to game world)
//   - Inner ring: 4 category wedges with dual-layer cyan glow borders
//   - Outer ring: RECTANGULAR piece cards at polar coordinates
//   - 72 tick marks around category ring outer edge
//   - Connector lines from active category to piece cards
//   - Center hub: crosshair with gaps when idle, category info when active
//   - All glows: #00e5ff cyan with bloom pulse

#include "RadialPieceMenu.h"
#include "ConstructionPhaseManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Engine/Texture2D.h"

URadialPieceMenu::URadialPieceMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HighlightedCategory = -1;
	PrevHighlightedCategory = -1;
	HighlightedPieceSlot = -1;
	PrevHighlightedPieceSlot = -1;

	// Geometry: matches React prototype exactly
	CenterHubRadius = 70.0f;   // INNER_RADIUS
	InnerRingInner  = 70.0f;   // flush with hub
	InnerRingOuter  = 155.0f;  // CATEGORY_RING_OUTER
	OuterRingInner  = 175.0f;  // PIECE_RING_INNER
	OuterRingOuter  = 290.0f;  // PIECE_RING_OUTER
	DeadZone        = 50.0f;

	FadeAlpha = 0.0f;
	FadeSpeed = 6.5f;
	GlowPulseTime = 0.0f;

	SegmentIconSize = 48.0f;
	CenterIconSize  = 60.0f;

	WedgeGapDeg = 0.6f;

	// Color palette: exact React prototype values
	DarkBg         = FLinearColor(0.039f, 0.055f, 0.090f, 1.0f);   // #0a0e17
	BgOverlayColor = FLinearColor(0.039f, 0.055f, 0.090f, 0.60f);

	Cyan     = FLinearColor(0.0f, 0.898f, 1.0f, 1.0f);    // #00e5ff
	CyanDim  = FLinearColor(0.0f, 0.898f, 1.0f, 0.27f);   // #00e5ff44
	CyanMid  = FLinearColor(0.0f, 0.898f, 1.0f, 0.53f);   // #00e5ff88
	CyanGlow = FLinearColor(0.0f, 0.898f, 1.0f, 0.80f);   // #00e5ffcc

	DarkWedge       = FLinearColor(0.059f, 0.082f, 0.125f, 0.90f); // #0f1520
	DarkHover       = FLinearColor(0.078f, 0.118f, 0.176f, 0.95f); // #141e2d
	DarkCard        = FLinearColor(0.051f, 0.071f, 0.098f, 0.90f); // #0d1219
	ActiveWedgeFill = FLinearColor(0.047f, 0.102f, 0.165f, 0.90f); // #0c1a2a

	TextWhite       = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextDimmed      = FLinearColor(0.290f, 0.396f, 0.459f, 1.0f);  // #4a6575
	TextUnavailable = FLinearColor(0.165f, 0.243f, 0.290f, 1.0f);  // #2a3e4a
	SubtitleColor   = Cyan;

	SegmentFillColor        = DarkWedge;
	SegmentHoverFillColor   = ActiveWedgeFill;
	SegmentUnavailableColor = FLinearColor(0.06f, 0.08f, 0.10f, 0.70f);
	CenterFillColor         = DarkBg;
	CenterBorderColor       = CyanMid;
	BorderAccentColor       = Cyan;
}

// ---------------------------------------------------------------------------
void URadialPieceMenu::BuildCategories()
{
	Categories.Empty();

	FCategoryInfo Foundation;
	Foundation.Name = TEXT("FOUNDATION");
	Foundation.Icon = TEXT("\u2B1B"); // Black square

	FCategoryInfo Floor;
	Floor.Name = TEXT("FLOOR");
	Floor.Icon = TEXT("\u25A6"); // Squared with fill

	FCategoryInfo Walls;
	Walls.Name = TEXT("WALLS");
	Walls.Icon = TEXT("\u25A5"); // Square with vertical fill

	FCategoryInfo Roof;
	Roof.Name = TEXT("ROOF");
	Roof.Icon = TEXT("\u25B3"); // Triangle

	for (int32 i = 0; i < AllPieceInfos.Num(); i++)
	{
		EPieceType PT = AllPieceInfos[i].PieceType;
		switch (PT)
		{
		case EPieceType::Foundation:
			Foundation.PieceIndices.Add(i); break;
		case EPieceType::RimBoard:
		case EPieceType::FloorJoist:
		case EPieceType::Plywood:
			Floor.PieceIndices.Add(i); break;
		case EPieceType::WallStud:
		case EPieceType::WallPlate:
		case EPieceType::CornerPost:
		case EPieceType::TopPlate:
		case EPieceType::DoubleTopPlate:
		case EPieceType::DoorFrame:
		case EPieceType::WindowFrame:
		case EPieceType::Header:
			Walls.PieceIndices.Add(i); break;
		case EPieceType::RidgePost:
		case EPieceType::RidgeBoard:
		case EPieceType::Rafter:
		case EPieceType::FasciaBoard:
			Roof.PieceIndices.Add(i); break;
		default:
			Walls.PieceIndices.Add(i); break;
		}
	}

	if (Foundation.PieceIndices.Num() > 0) Categories.Add(Foundation);
	if (Floor.PieceIndices.Num() > 0)      Categories.Add(Floor);
	if (Walls.PieceIndices.Num() > 0)      Categories.Add(Walls);
	if (Roof.PieceIndices.Num() > 0)       Categories.Add(Roof);
}

int32 URadialPieceMenu::FindCategoryForPieceIndex(int32 PieceIndex) const
{
	for (int32 c = 0; c < Categories.Num(); c++)
		if (Categories[c].PieceIndices.Contains(PieceIndex))
			return c;
	return -1;
}

// ---------------------------------------------------------------------------
void URadialPieceMenu::InitMenu(const TArray<FPieceTypeInfo>& InInfos, int32 CurrentIndex)
{
	AllPieceInfos = InInfos;
	BuildCategories();

	HighlightedCategory = FindCategoryForPieceIndex(CurrentIndex);
	PrevHighlightedCategory = HighlightedCategory;
	HighlightedPieceSlot = -1;

	if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
	{
		const TArray<int32>& Pieces = Categories[HighlightedCategory].PieceIndices;
		for (int32 i = 0; i < Pieces.Num(); i++)
		{
			if (Pieces[i] == CurrentIndex)
			{
				HighlightedPieceSlot = i;
				break;
			}
		}
	}
	PrevHighlightedPieceSlot = HighlightedPieceSlot;

	FadeAlpha = 0.0f;
	GlowPulseTime = 0.0f;

	CategoryHoverScales.Init(0.0f, Categories.Num());
	if (HighlightedCategory >= 0 && HighlightedCategory < CategoryHoverScales.Num())
		CategoryHoverScales[HighlightedCategory] = 1.0f;
	PieceHoverScales.Empty();

	IconBrushes.Empty();
	IconBrushes.SetNum(AllPieceInfos.Num());
	for (int32 i = 0; i < AllPieceInfos.Num(); i++)
	{
		UTexture2D* Tex = AllPieceInfos[i].Icon.LoadSynchronous();
		if (Tex)
		{
			IconBrushes[i].SetResourceObject(Tex);
			IconBrushes[i].ImageSize = FVector2D(SegmentIconSize, SegmentIconSize);
			IconBrushes[i].DrawAs = ESlateBrushDrawType::Image;
			IconBrushes[i].Tiling = ESlateBrushTileType::NoTile;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RadialPieceMenu: Init %d pieces in %d categories, current=%d (cat=%d slot=%d)"),
		AllPieceInfos.Num(), Categories.Num(), CurrentIndex, HighlightedCategory, HighlightedPieceSlot);
}

int32 URadialPieceMenu::GetHighlightedIndex() const
{
	if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
	{
		const TArray<int32>& Pieces = Categories[HighlightedCategory].PieceIndices;
		if (HighlightedPieceSlot >= 0 && Pieces.IsValidIndex(HighlightedPieceSlot))
			return Pieces[HighlightedPieceSlot];
		if (Pieces.Num() > 0)
			return Pieces[0];
	}
	return -1;
}

// ---------------------------------------------------------------------------
void URadialPieceMenu::PlaySoundOpen()  { }
void URadialPieceMenu::PlaySoundClose() { }
void URadialPieceMenu::PlaySoundHover() { }

FLinearColor URadialPieceMenu::Faded(FLinearColor Color) const
{
	Color.A *= FadeAlpha;
	return Color;
}

FLinearColor URadialPieceMenu::WithAlpha(FLinearColor Color, float Alpha) const
{
	Color.A = Alpha;
	return Color;
}

float URadialPieceMenu::GetPieceAngleDeg(int32 PieceIndex, int32 NumPieces, float CatMidDeg, float CatSweepDeg) const
{
	if (NumPieces <= 1) return CatMidDeg;
	// Spread cards across 90% of the category sweep for a clear arc
	float TotalSpread = CatSweepDeg * 0.90f;
	return CatMidDeg - TotalSpread / 2.0f + ((float)PieceIndex / (NumPieces - 1)) * TotalSpread;
}

// ---------------------------------------------------------------------------
// Tick: mouse tracking for both rings
// ---------------------------------------------------------------------------
void URadialPieceMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Categories.Num() == 0) return;

	FadeAlpha = FMath::FInterpTo(FadeAlpha, 1.0f, InDeltaTime, FadeSpeed);
	GlowPulseTime += InDeltaTime;

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);

	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	FVector2D Center = ViewportSize / 2.0f;

	float Scale = (ViewportSize.Y * 0.65f) / (OuterRingOuter * 2.0f);
	float ScaledInnerI  = InnerRingInner * Scale;
	float ScaledInnerO  = InnerRingOuter * Scale;
	float ScaledOuterI  = OuterRingInner * Scale;
	float ScaledOuterO  = OuterRingOuter * Scale;
	float ScaledDead    = DeadZone * Scale;

	float DX = MouseX - Center.X;
	float DY = MouseY - Center.Y;
	float Dist = FMath::Sqrt(DX * DX + DY * DY);

	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
	if (AngleDeg < 0.0f) AngleDeg += 360.0f;

	int32 NumCats = Categories.Num();
	float CatAngle = 360.0f / NumCats;

	// Inner ring: category selection
	if (Dist >= ScaledInnerI && Dist < ScaledInnerO)
	{
		int32 NewCat = FMath::Clamp((int32)(AngleDeg / CatAngle), 0, NumCats - 1);
		if (NewCat != HighlightedCategory)
		{
			HighlightedCategory = NewCat;
			HighlightedPieceSlot = -1;
			PieceHoverScales.Empty();
		}
	}
	// Outer ring: piece card detection (closest card by angle)
	else if (Dist >= ScaledOuterI - 20.0f * Scale && Dist <= ScaledOuterO + 30.0f * Scale && HighlightedCategory >= 0)
	{
		const TArray<int32>& Pieces = Categories[HighlightedCategory].PieceIndices;
		int32 NumPieces = Pieces.Num();
		if (NumPieces > 0)
		{
			float CatMidDeg = (HighlightedCategory + 0.5f) * CatAngle;
			float BestAngDist = 999.0f;
			int32 BestSlot = -1;
			for (int32 p = 0; p < NumPieces; p++)
			{
				float PieceDeg = GetPieceAngleDeg(p, NumPieces, CatMidDeg, CatAngle);
				float AngDist = FMath::Abs(AngleDeg - PieceDeg);
				if (AngDist > 180.0f) AngDist = 360.0f - AngDist;
				if (AngDist < BestAngDist)
				{
					BestAngDist = AngDist;
					BestSlot = p;
				}
			}
			if (BestSlot >= 0 && BestAngDist < CatAngle * 0.6f)
				HighlightedPieceSlot = BestSlot;
		}
	}
	else if (Dist < ScaledDead)
	{
		// Dead zone: keep selection
	}

	if (HighlightedCategory != PrevHighlightedCategory ||
		HighlightedPieceSlot != PrevHighlightedPieceSlot)
	{
		PlaySoundHover();
		PrevHighlightedCategory = HighlightedCategory;
		PrevHighlightedPieceSlot = HighlightedPieceSlot;
	}

	// Interpolate hover scales
	if (CategoryHoverScales.Num() != NumCats)
		CategoryHoverScales.Init(0.0f, NumCats);
	for (int32 i = 0; i < NumCats; i++)
	{
		float Target = (i == HighlightedCategory) ? 1.0f : 0.0f;
		CategoryHoverScales[i] = FMath::FInterpTo(CategoryHoverScales[i], Target, InDeltaTime, 12.0f);
	}

	if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
	{
		int32 NumP = Categories[HighlightedCategory].PieceIndices.Num();
		if (PieceHoverScales.Num() != NumP)
			PieceHoverScales.Init(0.0f, NumP);
		for (int32 i = 0; i < NumP; i++)
		{
			float Target = (i == HighlightedPieceSlot) ? 1.0f : 0.0f;
			PieceHoverScales[i] = FMath::FInterpTo(PieceHoverScales[i], Target, InDeltaTime, 12.0f);
		}
	}
}

// ---------------------------------------------------------------------------
// Paint — Exact match to React SVG prototype
// ---------------------------------------------------------------------------
int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 NumCats = Categories.Num();
	if (NumCats == 0 || FadeAlpha < 0.001f) return LayerId;

	FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;

	float MenuDiameter = LocalSize.Y * 0.65f;
	float Scale = MenuDiameter / (OuterRingOuter * 2.0f);

	float sHub     = CenterHubRadius * Scale;
	float sInnerI  = InnerRingInner * Scale;
	float sInnerO  = InnerRingOuter * Scale;
	float sOuterI  = OuterRingInner * Scale;
	float sOuterO  = OuterRingOuter * Scale;

	float CatAngle = 360.0f / NumCats;
	float GapHalf = WedgeGapDeg / 2.0f;

	// Pulse: slow sine for glow animations
	float Pulse = 0.85f + 0.15f * FMath::Sin(GlowPulseTime * 1.8f);
	// Center hub pulse (separate, slower)
	float CenterPulse = 0.3f + 0.4f * (0.5f + 0.5f * FMath::Sin(GlowPulseTime * 1.05f));

	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	int32 CatFontSize        = FMath::Clamp(FMath::RoundToInt(13.0f * Scale), 9, 17);
	int32 CatIconFontSize    = FMath::Clamp(FMath::RoundToInt(20.0f * Scale), 14, 26);
	int32 PieceNameFontSize  = FMath::Clamp(FMath::RoundToInt(10.0f * Scale), 7, 13);
	int32 PieceDimFontSize   = FMath::Clamp(FMath::RoundToInt(8.0f * Scale), 6, 11);
	int32 CenterNameFontSize = FMath::Clamp(FMath::RoundToInt(14.0f * Scale), 10, 20);
	int32 CenterSubFontSize  = FMath::Clamp(FMath::RoundToInt(10.0f * Scale), 8, 14);
	int32 CenterIconFontSize = FMath::Clamp(FMath::RoundToInt(26.0f * Scale), 18, 34);

	FSlateFontInfo CatFont        = FCoreStyle::GetDefaultFontStyle("Bold", CatFontSize);
	FSlateFontInfo CatIconFont    = FCoreStyle::GetDefaultFontStyle("Regular", CatIconFontSize);
	FSlateFontInfo PieceNameFont  = FCoreStyle::GetDefaultFontStyle("Bold", PieceNameFontSize);
	FSlateFontInfo PieceDimFont   = FCoreStyle::GetDefaultFontStyle("Regular", PieceDimFontSize);
	FSlateFontInfo CenterNameFont = FCoreStyle::GetDefaultFontStyle("Bold", CenterNameFontSize);
	FSlateFontInfo CenterSubFont  = FCoreStyle::GetDefaultFontStyle("Regular", CenterSubFontSize);
	FSlateFontInfo CenterIconFont = FCoreStyle::GetDefaultFontStyle("Regular", CenterIconFontSize);
	FSlateFontInfo BuildFont      = FCoreStyle::GetDefaultFontStyle("Bold",
		FMath::Clamp(FMath::RoundToInt(12.0f * Scale), 9, 16));
	FSlateFontInfo BuildSubFont   = FCoreStyle::GetDefaultFontStyle("Regular",
		FMath::Clamp(FMath::RoundToInt(8.0f * Scale), 6, 11));

	// LAYER 1: No background disc — game world is fully visible behind menu.
	// Only wedge fills, piece cards, glow lines, and center hub are opaque.
	LayerId++;

	// =================================================================
	// LAYER 2: Category wedges with dual-layer cyan glow borders
	// =================================================================
	for (int32 i = 0; i < NumCats; i++)
	{
		float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
		float StartDeg = i * CatAngle - 90.0f + GapHalf;
		float EndDeg   = (i + 1) * CatAngle - 90.0f - GapHalf;
		bool bActive = (i == HighlightedCategory);

		// glowIntensity: active=1, hovered=0.6*HoverT, default=0.25
		float GlowI = bActive ? FMath::Lerp(0.25f, 1.0f, HoverT) : 0.25f;

		// Wedge fill
		FLinearColor FillCol = bActive
			? FMath::Lerp(DarkWedge, ActiveWedgeFill, HoverT)
			: FMath::Lerp(DarkWedge, DarkHover, HoverT * 0.5f);
		DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
			sInnerI, sInnerO, StartDeg, EndDeg, Faded(FillCol));

		// Active wedge inner glow overlay (CYAN at ~3%)
		if (bActive && HoverT > 0.01f)
		{
			FLinearColor InnerGlow = WithAlpha(Cyan, 0.03f * HoverT * FadeAlpha);
			DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
				sInnerI, sInnerO, StartDeg, EndDeg, InnerGlow);
		}

		// --- Outer arc: blur layer (thick, dim) + crisp layer (thin, bright) ---
		float OuterBlurW = bActive ? 2.5f * Scale : 1.5f * Scale;
		float OuterCrispW = bActive ? 1.5f : 0.8f;
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			sInnerO, StartDeg, EndDeg,
			WithAlpha(Cyan, GlowI * Pulse * FadeAlpha), OuterBlurW);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			sInnerO, StartDeg, EndDeg,
			WithAlpha(Cyan, GlowI * 1.2f * Pulse * FadeAlpha), OuterCrispW);

		// --- Inner arc: blur + crisp ---
		float InnerBlurW = bActive ? 2.0f * Scale : 1.0f * Scale;
		float InnerCrispW = bActive ? 1.0f : 0.5f;
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			sInnerI, StartDeg, EndDeg,
			WithAlpha(Cyan, GlowI * 0.8f * Pulse * FadeAlpha), InnerBlurW);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			sInnerI, StartDeg, EndDeg,
			WithAlpha(Cyan, GlowI * Pulse * FadeAlpha), InnerCrispW);

		// --- Split lines (dividers): blur + crisp ---
		float SplitStartRad = FMath::DegreesToRadians(StartDeg);
		float SplitEndRad   = FMath::DegreesToRadians(EndDeg);
		for (float Rad : {SplitStartRad, SplitEndRad})
		{
			FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad));
			FVector2D Inner = Center + Dir * sInnerI;
			FVector2D Outer = Center + Dir * sInnerO;
			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				Inner, Outer, WithAlpha(Cyan, GlowI * 0.7f * Pulse * FadeAlpha), 1.0f * Scale);
			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				Inner, Outer, WithAlpha(Cyan, GlowI * Pulse * FadeAlpha), 0.5f);
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 3: 72 tick marks around category ring outer edge
	// React: every 5°, major every 20° (i%4==0)
	// Major: from sInnerO+4 to +14, strokeWidth 1.5, opacity 0.3
	// Minor: from sInnerO+4 to +8,  strokeWidth 0.5, opacity 0.12
	// =================================================================
	{
		for (int32 t = 0; t < 72; t++)
		{
			float AngleDeg = t * 5.0f - 90.0f;
			float AngleRad = FMath::DegreesToRadians(AngleDeg);
			FVector2D Dir(FMath::Cos(AngleRad), FMath::Sin(AngleRad));

			bool bMajor = (t % 4 == 0);
			float TickStart = sInnerO + 4.0f * Scale;
			float TickEnd   = sInnerO + (bMajor ? 14.0f : 8.0f) * Scale;
			float Alpha     = bMajor ? 0.3f : 0.12f;
			float Width     = bMajor ? 1.5f : 0.5f;

			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				Center + Dir * TickStart, Center + Dir * TickEnd,
				WithAlpha(Cyan, Alpha * Pulse * FadeAlpha), Width);
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 4: Category labels (name + icon centered in each wedge)
	// React: name at midAngle y-8, icon at y+14
	// =================================================================
	{
		float LabelR = (sInnerI + sInnerO) / 2.0f;
		for (int32 i = 0; i < NumCats; i++)
		{
			float MidAngleDeg = (i + 0.5f) * CatAngle - 90.0f;
			float MidAngleRad = FMath::DegreesToRadians(MidAngleDeg);
			FVector2D Dir(FMath::Cos(MidAngleRad), FMath::Sin(MidAngleRad));
			FVector2D LabelCenter = Center + Dir * LabelR;

			float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
			bool bActive = (i == HighlightedCategory);

			// Category name (uppercase, above center of wedge)
			FLinearColor NameTint;
			if (bActive)
				NameTint = FMath::Lerp(TextDimmed, Cyan, HoverT);
			else
				NameTint = FMath::Lerp(TextDimmed, FLinearColor(0.557f, 0.847f, 0.910f, 1.0f), HoverT); // #8ed8e8
			NameTint.A *= FadeAlpha;

			FVector2D NameSize = FontMeasure->Measure(Categories[i].Name, CatFont);
			FVector2D NamePos = LabelCenter - FVector2D(NameSize.X / 2.0f, NameSize.Y + 2.0f * Scale);
			FGeometry NameGeo = AllottedGeometry.MakeChild(NameSize, FSlateLayoutTransform(NamePos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, NameGeo.ToPaintGeometry(),
				Categories[i].Name, CatFont, ESlateDrawEffect::None, NameTint);

			// Category icon (below name)
			if (!Categories[i].Icon.IsEmpty())
			{
				FLinearColor IconTint;
				if (bActive)
					IconTint = FMath::Lerp(FLinearColor(0.165f, 0.243f, 0.290f, 1.0f), Cyan, HoverT);
				else
					IconTint = FMath::Lerp(FLinearColor(0.165f, 0.243f, 0.290f, 1.0f),
						FLinearColor(0.369f, 0.722f, 0.784f, 1.0f), HoverT); // #5eb8c8
				IconTint.A *= FadeAlpha;

				FVector2D IconSize = FontMeasure->Measure(Categories[i].Icon, CatIconFont);
				FVector2D IconPos = LabelCenter + FVector2D(-IconSize.X / 2.0f, 4.0f * Scale);
				FGeometry IconGeo = AllottedGeometry.MakeChild(IconSize, FSlateLayoutTransform(IconPos));
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, IconGeo.ToPaintGeometry(),
					Categories[i].Icon, CatIconFont, ESlateDrawEffect::None, IconTint);
			}
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 5: Active category — outer ring tracks + connector lines
	// =================================================================
	if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
	{
		const FCategoryInfo& Cat = Categories[HighlightedCategory];
		int32 NumPieces = Cat.PieceIndices.Num();
		float CatMidDeg = (HighlightedCategory + 0.5f) * CatAngle;
		float HoverT = CategoryHoverScales.IsValidIndex(HighlightedCategory) ? CategoryHoverScales[HighlightedCategory] : 0.0f;

		if (NumPieces > 0)
		{
			// Inner ring track (PIECE_RING_INNER - 5)
			DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
				sOuterI - 5.0f * Scale, -90.0f, 270.0f,
				WithAlpha(Cyan, 0.2f * HoverT * Pulse * FadeAlpha), 1.0f);

			// Outer ring track (PIECE_RING_OUTER + 5)
			DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
				sOuterO + 5.0f * Scale, -90.0f, 270.0f,
				WithAlpha(Cyan, 0.1f * HoverT * Pulse * FadeAlpha), 0.5f);

			// Connector lines from category outer edge to each piece card
			for (int32 p = 0; p < NumPieces; p++)
			{
				float PieceDeg = GetPieceAngleDeg(p, NumPieces, CatMidDeg, CatAngle);
				float PieceRad = FMath::DegreesToRadians(PieceDeg - 90.0f);
				FVector2D Dir(FMath::Cos(PieceRad), FMath::Sin(PieceRad));

				FVector2D ConnInner = Center + Dir * (sInnerO + 2.0f * Scale);
				FVector2D ConnOuter = Center + Dir * (sOuterI - 8.0f * Scale);

				DrawLine(OutDrawElements, LayerId, AllottedGeometry,
					ConnInner, ConnOuter,
					WithAlpha(Cyan, 0.2f * HoverT * Pulse * FadeAlpha), 0.8f);
			}
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 6 + 7: Piece cards — rectangular cards fanned in an ARC
	// Each card is positioned at its own angle along the outer ring,
	// centered on the selected category wedge's angular range.
	// Dual-layer cyan glow border matching the wedge edge style.
	// =================================================================
	if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
	{
		const FCategoryInfo& Cat = Categories[HighlightedCategory];
		int32 NumPieces = Cat.PieceIndices.Num();
		if (NumPieces > 0)
		{
			float CatMidDeg = (HighlightedCategory + 0.5f) * CatAngle;
			float CardR = (sOuterI + sOuterO) / 2.0f;
			float CardW = 86.0f * Scale;
			float CardH = 76.0f * Scale;

			// Spread cards evenly across the category sweep
			// Each card sits at a distinct angle along the arc
			auto GetCardAngle = [&](int32 Idx) -> float
			{
				if (NumPieces <= 1) return CatMidDeg;
				// Use 90% of the category sweep for spacing
				float TotalSpread = CatAngle * 0.90f;
				float StartAngle = CatMidDeg - TotalSpread / 2.0f;
				return StartAngle + ((float)Idx / (NumPieces - 1)) * TotalSpread;
			};

			for (int32 p = 0; p < NumPieces; p++)
			{
				int32 GlobalIdx = Cat.PieceIndices[p];
				bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
				float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;
				bool bHovered = (p == HighlightedPieceSlot);

				// Position this card at its arc angle
				float PieceDeg = GetCardAngle(p);
				float PieceRad = FMath::DegreesToRadians(PieceDeg - 90.0f);
				FVector2D Dir(FMath::Cos(PieceRad), FMath::Sin(PieceRad));
				FVector2D CardCenter = Center + Dir * CardR;

				float DrawW = CardW * (1.0f + 0.08f * HoverT);
				float DrawH = CardH * (1.0f + 0.08f * HoverT);
				FVector2D TL = CardCenter - FVector2D(DrawW / 2.0f, DrawH / 2.0f);
				FVector2D TR(TL.X + DrawW, TL.Y);
				FVector2D BL(TL.X, TL.Y + DrawH);
				FVector2D BR(TL.X + DrawW, TL.Y + DrawH);

				// --- Hover glow bloom BEHIND the card (drawn first) ---
				if (HoverT > 0.01f && bAvailable)
				{
					float GlowExpand = 6.0f * Scale;
					DrawFilledRect(OutDrawElements, LayerId, AllottedGeometry,
						TL - FVector2D(GlowExpand, GlowExpand),
						DrawW + GlowExpand * 2, DrawH + GlowExpand * 2,
						WithAlpha(Cyan, 0.08f * HoverT * Pulse * FadeAlpha));
				}

				// --- Card dark fill (#0D1219) ---
				FLinearColor FillCol = bAvailable
					? FMath::Lerp(DarkCard, FLinearColor(0.051f, 0.118f, 0.176f, 0.95f), HoverT * 0.5f)
					: WithAlpha(DarkCard, DarkCard.A * 0.5f);
				FillCol.A = FMath::Max(FillCol.A, 0.92f); // Ensure solid fill
				DrawFilledRect(OutDrawElements, LayerId, AllottedGeometry,
					TL, DrawW, DrawH, Faded(FillCol));

				// --- Dual-layer cyan glow border (matching wedge edge style) ---
				float GlowI = bHovered ? 1.0f : (bAvailable ? 0.4f : 0.08f);

				// BLUR layer (thick, dim) — gives the soft glow
				float BlurW = bHovered ? 3.0f * Scale : 2.0f * Scale;
				FLinearColor BlurCol = WithAlpha(Cyan, GlowI * 0.5f * Pulse * FadeAlpha);
				DrawLine(OutDrawElements, LayerId, AllottedGeometry, TL, TR, BlurCol, BlurW);
				DrawLine(OutDrawElements, LayerId, AllottedGeometry, BR, BL, BlurCol, BlurW);
				DrawLine(OutDrawElements, LayerId, AllottedGeometry, TL, BL, BlurCol, BlurW);
				DrawLine(OutDrawElements, LayerId, AllottedGeometry, TR, BR, BlurCol, BlurW);

				// CRISP layer (thin, bright) — the sharp border
				float CrispW = bHovered ? 1.5f : 1.0f;
				FLinearColor CrispCol = WithAlpha(Cyan, GlowI * Pulse * FadeAlpha);
				DrawLine(OutDrawElements, LayerId, AllottedGeometry, TL, TR, CrispCol, CrispW);
				DrawLine(OutDrawElements, LayerId, AllottedGeometry, BR, BL, CrispCol, CrispW);
				DrawLine(OutDrawElements, LayerId, AllottedGeometry, TL, BL, CrispCol, CrispW);
				DrawLine(OutDrawElements, LayerId, AllottedGeometry, TR, BR, CrispCol, CrispW);
			}
			LayerId++;

			// --- Card content: icons, names, dimensions ---
			for (int32 p = 0; p < NumPieces; p++)
			{
				int32 GlobalIdx = Cat.PieceIndices[p];
				bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
				float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;

				float PieceDeg = GetCardAngle(p);
				float PieceRad = FMath::DegreesToRadians(PieceDeg - 90.0f);
				FVector2D CardCenter = Center + FVector2D(FMath::Cos(PieceRad), FMath::Sin(PieceRad)) * CardR;

				// Icon (texture thumbnail)
				float ScaledIcon = SegmentIconSize * Scale * 0.60f;
				if (IconBrushes.IsValidIndex(GlobalIdx) && IconBrushes[GlobalIdx].GetResourceObject())
				{
					float DrawSize = ScaledIcon * (1.0f + 0.06f * HoverT);
					FVector2D TexSize(DrawSize, DrawSize);
					FVector2D TexPos = CardCenter - FVector2D(DrawSize / 2.0f, DrawSize / 2.0f + 8.0f * Scale);
					FGeometry IconGeo = AllottedGeometry.MakeChild(TexSize, FSlateLayoutTransform(TexPos));
					FLinearColor IconTint = bAvailable
						? FMath::Lerp(FLinearColor(0.60f, 0.75f, 0.85f, FadeAlpha),
							FLinearColor(1.0f, 1.0f, 1.0f, FadeAlpha), HoverT)
						: FLinearColor(0.20f, 0.25f, 0.30f, FadeAlpha * 0.4f);
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
						IconGeo.ToPaintGeometry(), &IconBrushes[GlobalIdx],
						ESlateDrawEffect::None, IconTint);
				}

				// Piece name
				FString Name = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].DisplayName : TEXT("");
				if (!Name.IsEmpty())
				{
					FLinearColor NameTint = bAvailable
						? FMath::Lerp(FLinearColor(0.557f, 0.667f, 0.733f, 1.0f), TextWhite, HoverT)
						: TextUnavailable;
					NameTint.A *= FadeAlpha;
					FVector2D NameSize = FontMeasure->Measure(Name, PieceNameFont);
					FVector2D NamePos = CardCenter + FVector2D(-NameSize.X / 2.0f, 6.0f * Scale);
					FGeometry NameGeo = AllottedGeometry.MakeChild(NameSize, FSlateLayoutTransform(NamePos));
					FSlateDrawElement::MakeText(OutDrawElements, LayerId, NameGeo.ToPaintGeometry(),
						Name, PieceNameFont, ESlateDrawEffect::None, NameTint);
				}

				// Dimension text (cyan)
				FString Dims = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].Subtitle : TEXT("");
				if (!Dims.IsEmpty())
				{
					FLinearColor DimTint = bAvailable
						? WithAlpha(Cyan, FMath::Lerp(0.5f, 0.9f, HoverT))
						: TextUnavailable;
					DimTint.A *= FadeAlpha;
					FVector2D DimSize = FontMeasure->Measure(Dims, PieceDimFont);
					FVector2D DimPos = CardCenter + FVector2D(-DimSize.X / 2.0f, 20.0f * Scale);
					FGeometry DimGeo = AllottedGeometry.MakeChild(DimSize, FSlateLayoutTransform(DimPos));
					FSlateDrawElement::MakeText(OutDrawElements, LayerId, DimGeo.ToPaintGeometry(),
						Dims, PieceDimFont, ESlateDrawEffect::None, DimTint);
				}
			}
			LayerId++;
		}
	}

	// =================================================================
	// LAYER 8: Center hub
	// =================================================================
	{
		float HubR = sHub - 6.0f * Scale; // React: INNER_RADIUS - 6

		// Outer glow ring (pulsing)
		float GlowR = sHub - 4.0f * Scale; // React: INNER_RADIUS - 4
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			GlowR, -90.0f, 270.0f,
			WithAlpha(Cyan, CenterPulse * Pulse * FadeAlpha), 2.0f * Scale);

		// Hub dark fill
		DrawCircleFill(OutDrawElements, LayerId, AllottedGeometry, Center,
			HubR, Faded(DarkBg));

		// Hub border
		bool bCatActive = (HighlightedCategory >= 0);
		float BorderOpacity = bCatActive ? 0.6f : 0.25f;
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			HubR, -90.0f, 270.0f,
			WithAlpha(Cyan, BorderOpacity * Pulse * FadeAlpha), 1.5f);

		// Inner pulse ring
		float PulseR = HubR + (4.0f * Scale * (0.5f + 0.5f * FMath::Sin(GlowPulseTime * 0.785f)));
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			PulseR, -90.0f, 270.0f,
			WithAlpha(Cyan, 0.15f * FadeAlpha), 1.0f);
	}
	LayerId++;

	// =================================================================
	// LAYER 9: Center content — crosshair or category info
	// =================================================================
	{
		bool bCatActive = (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory));

		if (bCatActive)
		{
			const FCategoryInfo& ActiveCat = Categories[HighlightedCategory];
			int32 NumPieces = ActiveCat.PieceIndices.Num();

			// Category icon (large, cyan, at y-16)
			if (!ActiveCat.Icon.IsEmpty())
			{
				FVector2D IconSize = FontMeasure->Measure(ActiveCat.Icon, CenterIconFont);
				FVector2D IconPos = Center - FVector2D(IconSize.X / 2.0f, IconSize.Y / 2.0f + 16.0f * Scale);
				FGeometry IconGeo = AllottedGeometry.MakeChild(IconSize, FSlateLayoutTransform(IconPos));
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, IconGeo.ToPaintGeometry(),
					ActiveCat.Icon, CenterIconFont, ESlateDrawEffect::None, Faded(Cyan));
			}

			// Category name (white, uppercase, at y+8)
			FVector2D NameSize = FontMeasure->Measure(ActiveCat.Name, CenterNameFont);
			FVector2D NamePos = Center + FVector2D(-NameSize.X / 2.0f, 8.0f * Scale - NameSize.Y / 2.0f);
			FGeometry NameGeo = AllottedGeometry.MakeChild(NameSize, FSlateLayoutTransform(NamePos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, NameGeo.ToPaintGeometry(),
				ActiveCat.Name, CenterNameFont, ESlateDrawEffect::None, Faded(TextWhite));

			// Piece count (cyan, dim, at y+26)
			FString CountText = FString::Printf(TEXT("%d piece%s"), NumPieces, NumPieces != 1 ? TEXT("s") : TEXT(""));
			FVector2D CountSize = FontMeasure->Measure(CountText, CenterSubFont);
			FVector2D CountPos = Center + FVector2D(-CountSize.X / 2.0f, 26.0f * Scale - CountSize.Y / 2.0f);
			FGeometry CountGeo = AllottedGeometry.MakeChild(CountSize, FSlateLayoutTransform(CountPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, CountGeo.ToPaintGeometry(),
				CountText, CenterSubFont, ESlateDrawEffect::None,
				WithAlpha(Cyan, 0.6f * FadeAlpha));
		}
		else
		{
			// Crosshair with gaps (React: lines from ±8 to ±20, gap in center)
			float GapR = 8.0f * Scale;
			float ArmR = 20.0f * Scale;
			FLinearColor CrossCol = WithAlpha(Cyan, 0.4f * Pulse * FadeAlpha);

			// Horizontal arms
			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				Center - FVector2D(ArmR, 0.0f), Center - FVector2D(GapR, 0.0f), CrossCol, 1.0f);
			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				Center + FVector2D(GapR, 0.0f), Center + FVector2D(ArmR, 0.0f), CrossCol, 1.0f);
			// Vertical arms
			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				Center - FVector2D(0.0f, ArmR), Center - FVector2D(0.0f, GapR), CrossCol, 1.0f);
			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				Center + FVector2D(0.0f, GapR), Center + FVector2D(0.0f, ArmR), CrossCol, 1.0f);

			// "BUILD" text (at y-3)
			FString BuildText = TEXT("BUILD");
			FVector2D BuildSize = FontMeasure->Measure(BuildText, BuildFont);
			FVector2D BuildPos = Center + FVector2D(-BuildSize.X / 2.0f, -3.0f * Scale - BuildSize.Y / 2.0f);
			FGeometry BuildGeo = AllottedGeometry.MakeChild(BuildSize, FSlateLayoutTransform(BuildPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, BuildGeo.ToPaintGeometry(),
				BuildText, BuildFont, ESlateDrawEffect::None, Faded(Cyan));

			// "select category" (at y+14)
			FString SubText = TEXT("select category");
			FVector2D SubSize = FontMeasure->Measure(SubText, BuildSubFont);
			FVector2D SubPos = Center + FVector2D(-SubSize.X / 2.0f, 14.0f * Scale - SubSize.Y / 2.0f);
			FGeometry SubGeo = AllottedGeometry.MakeChild(SubSize, FSlateLayoutTransform(SubPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, SubGeo.ToPaintGeometry(),
				SubText, BuildSubFont, ESlateDrawEffect::None,
				WithAlpha(Cyan, 0.35f * FadeAlpha));
		}
	}
	LayerId++;

	return LayerId;
}

// ---------------------------------------------------------------------------
// DrawFilledArc — fill an annular sector with concentric ring strokes
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawFilledArc(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float InR, float OutR,
	float StartDeg, float EndDeg, FLinearColor Color, int32 ArcSteps) const
{
	if (Color.A < 0.001f) return;
	float Span = OutR - InR;
	if (Span < 1.0f) return;

	int32 NumRings = FMath::Max(8, FMath::CeilToInt(Span / 4.0f));
	float RingSpacing = Span / (float)NumRings;
	float LineWidth = RingSpacing * 2.1f;
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
// DrawArcOutline — single arc stroke at given radius
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawArcOutline(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius,
	float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const
{
	if (Color.A < 0.001f) return;
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

// ---------------------------------------------------------------------------
// DrawCircleFill — filled circle
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawCircleFill(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const
{
	DrawFilledArc(OutDrawElements, LayerId, Geo, Center, 0.0f, Radius,
		-90.0f, 270.0f, Color, 48);
}

// ---------------------------------------------------------------------------
// DrawLine — simple two-point line
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawLine(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D A, FVector2D B, FLinearColor Color, float Thickness) const
{
	if (Color.A < 0.001f) return;
	TArray<FVector2D> Points;
	Points.Add(A);
	Points.Add(B);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geo.ToPaintGeometry(),
		Points, ESlateDrawEffect::None, Color, true, Thickness);
}

// ---------------------------------------------------------------------------
// DrawFilledRect — filled rectangle using horizontal line strokes
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawFilledRect(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D TopLeft, float Width, float Height, FLinearColor Color) const
{
	if (Color.A < 0.001f || Width < 1.0f || Height < 1.0f) return;

	// Dense horizontal line fill with generous overlap to ensure solid coverage
	int32 NumLines = FMath::Max(4, FMath::CeilToInt(Height / 2.0f));
	float LineSpacing = Height / (float)NumLines;
	float LineWidth = LineSpacing * 2.8f; // Heavy overlap ensures no gaps
	FPaintGeometry PG = Geo.ToPaintGeometry();

	for (int32 r = 0; r < NumLines; r++)
	{
		float Y = TopLeft.Y + LineSpacing * (r + 0.5f);
		TArray<FVector2D> Points;
		Points.Add(FVector2D(TopLeft.X, Y));
		Points.Add(FVector2D(TopLeft.X + Width, Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PG,
			Points, ESlateDrawEffect::None, Color, true, LineWidth);
	}
}
