// Born To Shine - Two-Tier Radial Piece Selection Menu (Cyan Glow Sci-fi)
//
// Rebuilt to match the React prototype: dark background (#0a0e17),
// cyan glow (#00e5ff) on all edges/splits/borders, two-tier layout
// (inner categories + outer piece cards), center hub with crosshair,
// connector lines, tick marks, smooth animations.

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

	// --- Geometry: reference pixels, scaled to 65% of screen height in NativePaint ---
	CenterHubRadius = 70.0f;
	InnerRingInner  = 70.0f;   // flush with hub
	InnerRingOuter  = 155.0f;
	OuterRingInner  = 175.0f;
	OuterRingOuter  = 290.0f;
	DeadZone        = 50.0f;

	FadeAlpha = 0.0f;
	FadeSpeed = 6.5f;
	GlowPulseTime = 0.0f;

	SegmentIconSize = 56.0f;
	CenterIconSize  = 80.0f;

	WedgeGapDeg = 0.6f;

	// --- Color palette: matched to React prototype ---

	// Background
	DarkBg         = FLinearColor(0.039f, 0.055f, 0.090f, 1.0f);   // #0a0e17
	BgOverlayColor = FLinearColor(0.039f, 0.055f, 0.090f, 0.60f);  // dark vignette

	// Cyan glow family — all based on #00e5ff
	Cyan     = FLinearColor(0.0f, 0.898f, 1.0f, 1.0f);    // #00e5ff full
	CyanDim  = FLinearColor(0.0f, 0.898f, 1.0f, 0.27f);   // ~27% alpha
	CyanMid  = FLinearColor(0.0f, 0.898f, 1.0f, 0.53f);   // ~53% alpha
	CyanGlow = FLinearColor(0.0f, 0.898f, 1.0f, 0.80f);   // ~80% alpha

	// Wedge / card fills
	DarkWedge       = FLinearColor(0.059f, 0.082f, 0.125f, 0.90f); // #0f1520
	DarkHover       = FLinearColor(0.078f, 0.118f, 0.176f, 0.95f); // #141e2d
	DarkCard        = FLinearColor(0.051f, 0.071f, 0.098f, 0.90f); // #0d1219
	ActiveWedgeFill = FLinearColor(0.047f, 0.102f, 0.165f, 0.90f); // #0c1a2a

	// Text
	TextWhite       = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextDimmed      = FLinearColor(0.290f, 0.396f, 0.459f, 1.0f);  // #4a6575
	TextUnavailable = FLinearColor(0.165f, 0.243f, 0.290f, 1.0f);  // #2a3e4a
	SubtitleColor   = Cyan;

	// Legacy aliases (point to new palette for any code that still references them)
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
	Foundation.Name = TEXT("Foundation");
	Foundation.Icon = TEXT("\u2302"); // Unicode house

	FCategoryInfo Floor;
	Floor.Name = TEXT("Floor");
	Floor.Icon = TEXT("\u25A6"); // Squared with horizontal fill

	FCategoryInfo Walls;
	Walls.Name = TEXT("Walls");
	Walls.Icon = TEXT("\u25EB"); // Square with right half black

	FCategoryInfo Roof;
	Roof.Name = TEXT("Roof");
	Roof.Icon = TEXT("\u25B3"); // White up-pointing triangle

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
	{
		if (Categories[c].PieceIndices.Contains(PieceIndex))
			return c;
	}
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
		{
			return Pieces[HighlightedPieceSlot];
		}
		if (Pieces.Num() > 0)
		{
			return Pieces[0];
		}
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

// ---------------------------------------------------------------------------
// Tick: mouse tracking for both rings + animation interpolation
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

	// Scale geometry to 65% of screen height
	float Scale = (ViewportSize.Y * 0.65f) / (OuterRingOuter * 2.0f);
	float ScaledInnerI  = InnerRingInner * Scale;
	float ScaledInnerO  = InnerRingOuter * Scale;
	float ScaledOuterI  = OuterRingInner * Scale;
	float ScaledOuterO  = OuterRingOuter * Scale;
	float ScaledDead    = DeadZone * Scale;

	float DX = MouseX - Center.X;
	float DY = MouseY - Center.Y;
	float Dist = FMath::Sqrt(DX * DX + DY * DY);

	// Angle from top (12 o'clock) going clockwise
	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
	if (AngleDeg < 0.0f) AngleDeg += 360.0f;

	int32 NumCats = Categories.Num();
	float CatAngle = 360.0f / NumCats;

	// --- Inner ring: category selection ---
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
	// --- Outer ring: piece selection within active category ---
	else if (Dist >= ScaledOuterI && Dist <= ScaledOuterO + 20.0f * Scale && HighlightedCategory >= 0)
	{
		const TArray<int32>& Pieces = Categories[HighlightedCategory].PieceIndices;
		int32 NumPieces = Pieces.Num();
		if (NumPieces > 0)
		{
			float CatStartDeg = HighlightedCategory * CatAngle;
			float PieceAngle = CatAngle / NumPieces;

			float RelAngle = AngleDeg - CatStartDeg;
			if (RelAngle < 0.0f) RelAngle += 360.0f;
			if (RelAngle > CatAngle) RelAngle -= 360.0f;

			if (RelAngle >= 0.0f && RelAngle <= CatAngle)
			{
				int32 PieceSlot = FMath::Clamp((int32)(RelAngle / PieceAngle), 0, NumPieces - 1);
				HighlightedPieceSlot = PieceSlot;
			}
		}
	}
	else if (Dist < ScaledDead)
	{
		// Dead zone: keep current selection
	}

	// Play hover sound on change
	if (HighlightedCategory != PrevHighlightedCategory ||
		HighlightedPieceSlot != PrevHighlightedPieceSlot)
	{
		PlaySoundHover();
		PrevHighlightedCategory = HighlightedCategory;
		PrevHighlightedPieceSlot = HighlightedPieceSlot;
	}

	// --- Interpolate hover scales for smooth glow transitions ---
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
// Paint — Cyan Glow Sci-fi Radial Menu (matched to React prototype)
// ---------------------------------------------------------------------------
int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 NumCats = Categories.Num();
	if (NumCats == 0 || FadeAlpha < 0.001f) return LayerId;

	FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;

	// Scale all radii to 65% of screen height
	float MenuDiameter = LocalSize.Y * 0.65f;
	float Scale = MenuDiameter / (OuterRingOuter * 2.0f);

	float sHub     = CenterHubRadius * Scale;
	float sInnerI  = InnerRingInner * Scale;
	float sInnerO  = InnerRingOuter * Scale;
	float sOuterI  = OuterRingInner * Scale;
	float sOuterO  = OuterRingOuter * Scale;

	float CatAngle = 360.0f / NumCats;
	float GapHalf = WedgeGapDeg / 2.0f;

	// Glow pulse: slow sine 0.7..1.0
	float Pulse = 0.85f + 0.15f * FMath::Sin(GlowPulseTime * 1.8f);

	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	int32 CatFontSize        = FMath::Clamp(FMath::RoundToInt(14.0f * Scale), 10, 18);
	int32 CatIconFontSize    = FMath::Clamp(FMath::RoundToInt(20.0f * Scale), 14, 26);
	int32 PieceNameFontSize  = FMath::Clamp(FMath::RoundToInt(11.0f * Scale), 8, 14);
	int32 PieceDimFontSize   = FMath::Clamp(FMath::RoundToInt(9.0f * Scale), 7, 12);
	int32 CenterNameFontSize = FMath::Clamp(FMath::RoundToInt(16.0f * Scale), 12, 22);
	int32 CenterSubFontSize  = FMath::Clamp(FMath::RoundToInt(11.0f * Scale), 9, 15);

	FSlateFontInfo CatFont        = FCoreStyle::GetDefaultFontStyle("Bold", CatFontSize);
	FSlateFontInfo CatIconFont    = FCoreStyle::GetDefaultFontStyle("Regular", CatIconFontSize);
	FSlateFontInfo PieceNameFont  = FCoreStyle::GetDefaultFontStyle("Regular", PieceNameFontSize);
	FSlateFontInfo PieceDimFont   = FCoreStyle::GetDefaultFontStyle("Regular", PieceDimFontSize);
	FSlateFontInfo CenterNameFont = FCoreStyle::GetDefaultFontStyle("Bold", CenterNameFontSize);
	FSlateFontInfo CenterSubFont  = FCoreStyle::GetDefaultFontStyle("Regular", CenterSubFontSize);

	// No background disc — game world stays fully visible behind the menu

	// =================================================================
	// LAYER 2: Inner ring — category wedges with glow
	// =================================================================
	for (int32 i = 0; i < NumCats; i++)
	{
		float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
		float StartDeg = i * CatAngle - 90.0f + GapHalf;
		float EndDeg   = (i + 1) * CatAngle - 90.0f - GapHalf;
		bool bActive = (i == HighlightedCategory);

		// Wedge fill: dark navy, brighter on hover/active
		FLinearColor FillCol = bActive
			? FMath::Lerp(DarkWedge, ActiveWedgeFill, HoverT)
			: DarkWedge;
		DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
			sInnerI, sInnerO, StartDeg, EndDeg, Faded(FillCol));

		// Glow layer underneath: soft cyan bloom for hovered wedge
		if (HoverT > 0.01f)
		{
			FLinearColor GlowCol = WithAlpha(CyanDim, CyanDim.A * HoverT * Pulse * FadeAlpha);
			// Draw slightly expanded arc as "bloom"
			DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
				sInnerI - 2.0f * Scale, sInnerO + 2.0f * Scale,
				StartDeg - 0.3f, EndDeg + 0.3f, GlowCol, 32);
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 3: Inner ring cyan borders — dividers + inner/outer arcs
	// =================================================================
	{
		// Outer arc of inner ring (full circle)
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			sInnerO, -90.0f, 270.0f, Faded(WithAlpha(CyanDim, CyanDim.A * Pulse)), 1.5f);

		// Inner arc of inner ring (full circle — hub border)
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			sInnerI, -90.0f, 270.0f, Faded(WithAlpha(CyanDim, CyanDim.A * 0.7f * Pulse)), 1.0f);

		// Divider lines between category wedges (cyan)
		for (int32 i = 0; i < NumCats; i++)
		{
			float AngleRad = FMath::DegreesToRadians(i * CatAngle - 90.0f);
			FVector2D Radial(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
			FVector2D Inner = Center + Radial * sInnerI;
			FVector2D Outer = Center + Radial * sInnerO;
			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				Inner, Outer, Faded(WithAlpha(CyanDim, CyanDim.A * Pulse)), 1.0f);
		}

		// Tick marks on outer edge of inner ring (small radial ticks at each wedge boundary)
		for (int32 i = 0; i < NumCats; i++)
		{
			float AngleRad = FMath::DegreesToRadians(i * CatAngle - 90.0f);
			FVector2D Radial(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
			FVector2D TickInner = Center + Radial * (sInnerO - 4.0f * Scale);
			FVector2D TickOuter = Center + Radial * (sInnerO + 4.0f * Scale);
			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				TickInner, TickOuter, Faded(WithAlpha(CyanMid, CyanMid.A * Pulse)), 2.0f);
		}

		// Active category gets a bright cyan outer arc highlight
		if (HighlightedCategory >= 0)
		{
			float HoverT = CategoryHoverScales.IsValidIndex(HighlightedCategory) ? CategoryHoverScales[HighlightedCategory] : 0.0f;
			float StartDeg = HighlightedCategory * CatAngle - 90.0f + GapHalf;
			float EndDeg   = (HighlightedCategory + 1) * CatAngle - 90.0f - GapHalf;
			FLinearColor ArcGlow = WithAlpha(CyanGlow, CyanGlow.A * HoverT * Pulse * FadeAlpha);
			DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
				sInnerO, StartDeg, EndDeg, ArcGlow, 2.5f);
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 4: Category names + icons centered in each wedge
	// =================================================================
	{
		float TextR = (sInnerI + sInnerO) / 2.0f + 5.0f * Scale;
		float IconR = (sInnerI + sInnerO) / 2.0f - 12.0f * Scale;

		for (int32 i = 0; i < NumCats; i++)
		{
			float MidAngleDeg = (i + 0.5f) * CatAngle - 90.0f;
			float MidAngleRad = FMath::DegreesToRadians(MidAngleDeg);
			FVector2D Dir(FMath::Cos(MidAngleRad), FMath::Sin(MidAngleRad));

			float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
			bool bActive = (i == HighlightedCategory);

			// Category icon (Unicode character)
			if (!Categories[i].Icon.IsEmpty())
			{
				FVector2D IconCenter = Center + Dir * IconR;
				FLinearColor IconTint = bActive
					? FMath::Lerp(CyanDim, Cyan, HoverT)
					: CyanDim;
				IconTint.A *= FadeAlpha;

				FVector2D IconSize = FontMeasure->Measure(Categories[i].Icon, CatIconFont);
				FVector2D IconPos = IconCenter - IconSize / 2.0f;
				FGeometry IconGeo = AllottedGeometry.MakeChild(IconSize, FSlateLayoutTransform(IconPos));
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, IconGeo.ToPaintGeometry(),
					Categories[i].Icon, CatIconFont, ESlateDrawEffect::None, IconTint);
			}

			// Category name
			FVector2D LabelCenter = Center + Dir * TextR;
			FLinearColor TextTint = bActive
				? FMath::Lerp(TextDimmed, TextWhite, HoverT)
				: TextDimmed;
			TextTint.A *= FadeAlpha;

			FVector2D TextSize = FontMeasure->Measure(Categories[i].Name, CatFont);
			FVector2D TextPos = LabelCenter - TextSize / 2.0f;
			FGeometry TextGeo = AllottedGeometry.MakeChild(TextSize, FSlateLayoutTransform(TextPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, TextGeo.ToPaintGeometry(),
				Categories[i].Name, CatFont, ESlateDrawEffect::None, TextTint);
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 5: Connector lines from active category to outer ring
	// =================================================================
	if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
	{
		float CatStartDeg = HighlightedCategory * CatAngle - 90.0f;
		float CatEndDeg   = CatStartDeg + CatAngle;
		float CatMidDeg   = (CatStartDeg + CatEndDeg) / 2.0f;

		// Two connector lines from the edges of the active category wedge
		// extending outward to the outer ring boundaries
		float StartRad = FMath::DegreesToRadians(CatStartDeg + GapHalf);
		float EndRad   = FMath::DegreesToRadians(CatEndDeg - GapHalf);

		FVector2D StartDir(FMath::Cos(StartRad), FMath::Sin(StartRad));
		FVector2D EndDir(FMath::Cos(EndRad), FMath::Sin(EndRad));

		FVector2D ConnStart1 = Center + StartDir * sInnerO;
		FVector2D ConnEnd1   = Center + StartDir * sOuterI;
		FVector2D ConnStart2 = Center + EndDir * sInnerO;
		FVector2D ConnEnd2   = Center + EndDir * sOuterI;

		float HoverT = CategoryHoverScales.IsValidIndex(HighlightedCategory) ? CategoryHoverScales[HighlightedCategory] : 0.0f;
		FLinearColor ConnColor = WithAlpha(CyanDim, CyanDim.A * HoverT * Pulse * FadeAlpha);
		DrawLine(OutDrawElements, LayerId, AllottedGeometry, ConnStart1, ConnEnd1, ConnColor, 1.0f);
		DrawLine(OutDrawElements, LayerId, AllottedGeometry, ConnStart2, ConnEnd2, ConnColor, 1.0f);

		// Dashed center connector (dimmer)
		float MidRad = FMath::DegreesToRadians(CatMidDeg);
		FVector2D MidDir(FMath::Cos(MidRad), FMath::Sin(MidRad));
		FVector2D MidStart = Center + MidDir * sInnerO;
		FVector2D MidEnd   = Center + MidDir * sOuterI;
		FLinearColor MidConnColor = WithAlpha(CyanDim, CyanDim.A * 0.4f * HoverT * Pulse * FadeAlpha);
		DrawLine(OutDrawElements, LayerId, AllottedGeometry, MidStart, MidEnd, MidConnColor, 0.5f);
	}
	LayerId++;

	// =================================================================
	// LAYER 6: Outer ring — piece cards with cyan glow borders
	// =================================================================
	if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
	{
		const FCategoryInfo& Cat = Categories[HighlightedCategory];
		int32 NumPieces = Cat.PieceIndices.Num();
		if (NumPieces > 0)
		{
			float CatStartDeg = HighlightedCategory * CatAngle - 90.0f;
			float PieceAngle = CatAngle / NumPieces;
			float CardGapDeg = WedgeGapDeg;

			// --- Card fills ---
			for (int32 p = 0; p < NumPieces; p++)
			{
				int32 GlobalIdx = Cat.PieceIndices[p];
				bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
				float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;

				float StartDeg = CatStartDeg + p * PieceAngle + CardGapDeg;
				float EndDeg   = CatStartDeg + (p + 1) * PieceAngle - CardGapDeg;

				// Card fill: dark, slightly brighter on hover
				FLinearColor CardFill;
				if (!bAvailable)
				{
					CardFill = WithAlpha(DarkCard, DarkCard.A * 0.5f);
				}
				else
				{
					CardFill = FMath::Lerp(DarkCard, DarkHover, HoverT * 0.7f);
				}
				DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
					sOuterI, sOuterO, StartDeg, EndDeg, Faded(CardFill));

				// Glow bloom underneath hovered card
				if (HoverT > 0.01f && bAvailable)
				{
					FLinearColor BloomCol = WithAlpha(Cyan, 0.08f * HoverT * Pulse * FadeAlpha);
					DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
						sOuterI - 1.0f * Scale, sOuterO + 1.0f * Scale,
						StartDeg - 0.2f, EndDeg + 0.2f, BloomCol, 24);
				}
			}
			LayerId++;

			// --- Cyan borders on each card ---
			for (int32 p = 0; p < NumPieces; p++)
			{
				int32 GlobalIdx = Cat.PieceIndices[p];
				bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
				float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;

				float StartDeg = CatStartDeg + p * PieceAngle + CardGapDeg;
				float EndDeg   = CatStartDeg + (p + 1) * PieceAngle - CardGapDeg;

				// Border intensity: dim default, bright on hover
				float BorderAlpha = bAvailable
					? FMath::Lerp(0.18f, 0.85f, HoverT) * Pulse
					: 0.08f;
				FLinearColor BorderCol = WithAlpha(Cyan, BorderAlpha * FadeAlpha);

				// Outer arc border
				DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
					sOuterO - 0.5f, StartDeg, EndDeg, BorderCol, FMath::Lerp(1.0f, 2.5f, HoverT));

				// Inner arc border
				DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
					sOuterI + 0.5f, StartDeg, EndDeg, BorderCol, FMath::Lerp(1.0f, 2.0f, HoverT));

				// Side borders (radial lines)
				float Rad1 = FMath::DegreesToRadians(StartDeg);
				float Rad2 = FMath::DegreesToRadians(EndDeg);
				FVector2D Dir1(FMath::Cos(Rad1), FMath::Sin(Rad1));
				FVector2D Dir2(FMath::Cos(Rad2), FMath::Sin(Rad2));
				DrawLine(OutDrawElements, LayerId, AllottedGeometry,
					Center + Dir1 * sOuterI, Center + Dir1 * sOuterO,
					BorderCol, FMath::Lerp(0.5f, 1.5f, HoverT));
				DrawLine(OutDrawElements, LayerId, AllottedGeometry,
					Center + Dir2 * sOuterI, Center + Dir2 * sOuterO,
					BorderCol, FMath::Lerp(0.5f, 1.5f, HoverT));
			}
			LayerId++;

			// --- Piece icons centered in each card ---
			{
				float IconR = (sOuterI + sOuterO) / 2.0f - 5.0f * Scale;
				float ScaledIconSize = SegmentIconSize * Scale;

				for (int32 p = 0; p < NumPieces; p++)
				{
					int32 GlobalIdx = Cat.PieceIndices[p];
					float MidAngle = FMath::DegreesToRadians(CatStartDeg + (p + 0.5f) * PieceAngle);
					FVector2D IconCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * IconR;

					bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
					float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;

					if (IconBrushes.IsValidIndex(GlobalIdx) && IconBrushes[GlobalIdx].GetResourceObject())
					{
						float DrawSize = ScaledIconSize * (1.0f + 0.08f * HoverT);
						FVector2D TexSize(DrawSize, DrawSize);
						FVector2D TexPos = IconCenter - TexSize / 2.0f;
						FGeometry IconGeo = AllottedGeometry.MakeChild(TexSize, FSlateLayoutTransform(TexPos));

						FLinearColor IconTint;
						if (!bAvailable)
							IconTint = FLinearColor(0.20f, 0.25f, 0.30f, FadeAlpha * 0.4f);
						else
							IconTint = FMath::Lerp(
								FLinearColor(0.70f, 0.82f, 0.90f, FadeAlpha),
								FLinearColor(1.0f, 1.0f, 1.0f, FadeAlpha),
								HoverT);

						FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
							IconGeo.ToPaintGeometry(), &IconBrushes[GlobalIdx],
							ESlateDrawEffect::None, IconTint);
					}
				}
			}
			LayerId++;

			// --- Piece names below icons ---
			{
				float NameR = sOuterI + (sOuterO - sOuterI) * 0.20f;
				for (int32 p = 0; p < NumPieces; p++)
				{
					int32 GlobalIdx = Cat.PieceIndices[p];
					FString Name = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].DisplayName : TEXT("");
					if (Name.IsEmpty()) continue;

					bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
					float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;

					float MidAngle = FMath::DegreesToRadians(CatStartDeg + (p + 0.5f) * PieceAngle);
					FVector2D LabelCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * NameR;

					FLinearColor Tint;
					if (!bAvailable) Tint = TextUnavailable;
					else Tint = FMath::Lerp(TextDimmed, TextWhite, HoverT);
					Tint.A *= FadeAlpha;

					FVector2D TextSize = FontMeasure->Measure(Name, PieceNameFont);
					FVector2D TextPos = LabelCenter - TextSize / 2.0f;
					FGeometry TextGeo = AllottedGeometry.MakeChild(TextSize, FSlateLayoutTransform(TextPos));
					FSlateDrawElement::MakeText(OutDrawElements, LayerId, TextGeo.ToPaintGeometry(),
						Name, PieceNameFont, ESlateDrawEffect::None, Tint);
				}
			}
			LayerId++;

			// --- Piece dimensions (cyan subtitle) below name ---
			{
				float DimR = sOuterI + (sOuterO - sOuterI) * 0.08f;
				for (int32 p = 0; p < NumPieces; p++)
				{
					int32 GlobalIdx = Cat.PieceIndices[p];
					FString Dims = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].Subtitle : TEXT("");
					if (Dims.IsEmpty()) continue;

					bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;

					float MidAngle = FMath::DegreesToRadians(CatStartDeg + (p + 0.5f) * PieceAngle);
					FVector2D LabelCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * DimR;

					FLinearColor Tint;
					if (!bAvailable) Tint = TextUnavailable;
					else Tint = WithAlpha(Cyan, 0.8f);
					Tint.A *= FadeAlpha;

					FVector2D TextSize = FontMeasure->Measure(Dims, PieceDimFont);
					FVector2D TextPos = LabelCenter - TextSize / 2.0f;
					FGeometry TextGeo = AllottedGeometry.MakeChild(TextSize, FSlateLayoutTransform(TextPos));
					FSlateDrawElement::MakeText(OutDrawElements, LayerId, TextGeo.ToPaintGeometry(),
						Dims, PieceDimFont, ESlateDrawEffect::None, Tint);
				}
			}
			LayerId++;
		}
	}

	// =================================================================
	// LAYER 7: Outer ring border (full circle, dim cyan)
	// =================================================================
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		sOuterO, -90.0f, 270.0f, Faded(WithAlpha(CyanDim, CyanDim.A * 0.5f * Pulse)), 1.0f);
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		sOuterI, -90.0f, 270.0f, Faded(WithAlpha(CyanDim, CyanDim.A * 0.4f * Pulse)), 1.0f);
	LayerId++;

	// =================================================================
	// LAYER 8: Center hub — dark circle with cyan border + crosshair/content
	// =================================================================
	{
		// Hub fill
		DrawCircleFill(OutDrawElements, LayerId, AllottedGeometry, Center,
			sHub, Faded(DarkBg));

		// Hub border (bright cyan, pulsing)
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			sHub, -90.0f, 270.0f, Faded(WithAlpha(CyanMid, CyanMid.A * Pulse)), 2.0f);

		// Subtle outer glow ring around hub
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			sHub + 2.0f, -90.0f, 270.0f, Faded(WithAlpha(CyanDim, CyanDim.A * 0.3f * Pulse)), 4.0f);
	}
	LayerId++;

	// =================================================================
	// LAYER 9: Center hub content — crosshair when idle, piece info when active
	// =================================================================
	{
		int32 GlobalIdx = GetHighlightedIndex();
		bool bHasSelection = (GlobalIdx >= 0 && AllPieceInfos.IsValidIndex(GlobalIdx));

		if (bHasSelection)
		{
			const FPieceTypeInfo& Info = AllPieceInfos[GlobalIdx];

			// Icon
			float ScaledCenterIcon = CenterIconSize * Scale * 0.75f;
			if (IconBrushes.IsValidIndex(GlobalIdx) && IconBrushes[GlobalIdx].GetResourceObject())
			{
				FVector2D TexSize(ScaledCenterIcon, ScaledCenterIcon);
				FVector2D TexPos = Center - FVector2D(ScaledCenterIcon / 2.0f, ScaledCenterIcon / 2.0f + 12.0f * Scale);
				FGeometry IconGeo = AllottedGeometry.MakeChild(TexSize, FSlateLayoutTransform(TexPos));
				FLinearColor CenterIconTint = Info.bAvailable
					? FLinearColor(0.85f, 0.95f, 1.00f, FadeAlpha)
					: FLinearColor(0.25f, 0.30f, 0.35f, FadeAlpha * 0.4f);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					IconGeo.ToPaintGeometry(), &IconBrushes[GlobalIdx],
					ESlateDrawEffect::None, CenterIconTint);
			}

			// Name
			FLinearColor NameColor = Info.bAvailable ? TextWhite : TextUnavailable;
			FVector2D NameSize = FontMeasure->Measure(Info.DisplayName, CenterNameFont);
			FVector2D NamePos = Center + FVector2D(-NameSize.X / 2.0f, ScaledCenterIcon / 2.0f - 8.0f * Scale);
			FGeometry NameGeo = AllottedGeometry.MakeChild(NameSize, FSlateLayoutTransform(NamePos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, NameGeo.ToPaintGeometry(),
				Info.DisplayName, CenterNameFont, ESlateDrawEffect::None, Faded(NameColor));

			// Subtitle (cyan)
			if (!Info.Subtitle.IsEmpty())
			{
				FLinearColor SubColor = Info.bAvailable ? Cyan : TextUnavailable;
				FVector2D SubSize = FontMeasure->Measure(Info.Subtitle, CenterSubFont);
				FVector2D SubPos = NamePos + FVector2D((NameSize.X - SubSize.X) / 2.0f, NameSize.Y + 2.0f);
				FGeometry SubGeo = AllottedGeometry.MakeChild(SubSize, FSlateLayoutTransform(SubPos));
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, SubGeo.ToPaintGeometry(),
					Info.Subtitle, CenterSubFont, ESlateDrawEffect::None, Faded(SubColor));
			}

			// Lock message (orange)
			if (!Info.bAvailable)
			{
				FString LockMsg = TEXT("LOCKED");
				if (AConstructionPhaseManager::Instance)
				{
					FString PrereqMsg = AConstructionPhaseManager::Instance->GetPrerequisiteMessage(Info.PieceType);
					if (!PrereqMsg.IsEmpty()) LockMsg = PrereqMsg;
				}
				FSlateFontInfo LockFont = FCoreStyle::GetDefaultFontStyle("Bold",
					FMath::Clamp(FMath::RoundToInt(10.0f * Scale), 8, 13));
				FLinearColor LockColor(1.0f, 0.6f, 0.1f, 1.0f);
				FVector2D LockSize = FontMeasure->Measure(LockMsg, LockFont);
				float SubOffset = Info.Subtitle.IsEmpty() ? 0.0f
					: FontMeasure->Measure(Info.Subtitle, CenterSubFont).Y + 4.0f;
				FVector2D LockPos = NamePos + FVector2D(
					(NameSize.X - LockSize.X) / 2.0f, NameSize.Y + SubOffset + 4.0f);
				FGeometry LockGeo = AllottedGeometry.MakeChild(LockSize, FSlateLayoutTransform(LockPos));
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, LockGeo.ToPaintGeometry(),
					LockMsg, LockFont, ESlateDrawEffect::None, Faded(LockColor));
			}
		}
		else
		{
			// No selection — draw crosshair in center hub
			float CrossLen = sHub * 0.4f;
			FLinearColor CrossColor = Faded(WithAlpha(CyanDim, CyanDim.A * Pulse));

			// Horizontal line
			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				Center - FVector2D(CrossLen, 0.0f),
				Center + FVector2D(CrossLen, 0.0f),
				CrossColor, 1.0f);
			// Vertical line
			DrawLine(OutDrawElements, LayerId, AllottedGeometry,
				Center - FVector2D(0.0f, CrossLen),
				Center + FVector2D(0.0f, CrossLen),
				CrossColor, 1.0f);

			// Small circle at center
			DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
				4.0f * Scale, -90.0f, 270.0f, CrossColor, 1.0f);

			// "Select" text
			FString Prompt = TEXT("SELECT");
			FSlateFontInfo PromptFont = FCoreStyle::GetDefaultFontStyle("Bold",
				FMath::Clamp(FMath::RoundToInt(12.0f * Scale), 9, 16));
			FVector2D PromptSize = FontMeasure->Measure(Prompt, PromptFont);
			FVector2D PromptPos = Center + FVector2D(-PromptSize.X / 2.0f, CrossLen + 4.0f * Scale);
			FGeometry PromptGeo = AllottedGeometry.MakeChild(PromptSize, FSlateLayoutTransform(PromptPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, PromptGeo.ToPaintGeometry(),
				Prompt, PromptFont, ESlateDrawEffect::None, Faded(TextDimmed));
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
// DrawCircleFill — filled circle as a full-sweep filled arc
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
