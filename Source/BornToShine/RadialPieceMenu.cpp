// RadialPieceMenu.cpp
// Born To Shine - In-Place Radial Piece Selection Menu
//
// Design: Single-ring wheel that swaps between category view and piece view.
// Click a category → wheel morphs into piece wedges for that category.
// Click center hub → back to categories. Click a piece → selects it.
// All text curves along wedge arcs. Icons sit above text in each wedge.

#include "RadialPieceMenu.h"
#include "ConstructionPhaseManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Engine/Texture2D.h"

// ============================================================================
// Constructor
// ============================================================================

URadialPieceMenu::URadialPieceMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrentView = ERadialMenuView::Main;
	ActiveCategory = -1;
	HighlightedCategory = -1;
	PrevHighlightedCategory = -1;
	HighlightedPieceSlot = -1;
	PrevHighlightedPieceSlot = -1;
	SelectedPieceSlot = -1;

	// Geometry (matches the React prototype radii)
	InnerRadius  = 50.0f;
	OuterRadius  = 155.0f;
	HubRadius    = 46.0f;
	DeadZone     = 30.0f;
	WedgeGapDeg  = 1.0f;
	IconSize     = 40.0f;

	// Animation
	FadeAlpha = 0.0f;
	FadeSpeed = 6.5f;
	GlowPulseTime = 0.0f;
	ViewTransition = 0.0f;
	ViewTransitionSpeed = 8.0f;

	// ---- Color Palette (dark sci-fi with cyan glow) ----
	DarkBg          = FLinearColor(0.024f, 0.031f, 0.063f, 1.0f);   // #060810
	DarkWedge       = FLinearColor(0.059f, 0.082f, 0.125f, 0.90f);  // #0f1520
	DarkHover       = FLinearColor(0.078f, 0.118f, 0.176f, 0.95f);  // #141e2d
	ActiveWedgeFill = FLinearColor(0.047f, 0.102f, 0.165f, 0.90f);  // #0c1a2a

	Cyan    = FLinearColor(0.0f, 0.898f, 1.0f, 1.0f);    // #00e5ff
	CyanDim = FLinearColor(0.0f, 0.898f, 1.0f, 0.27f);
	CyanMid = FLinearColor(0.0f, 0.898f, 1.0f, 0.53f);

	TextWhite       = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextDimmed      = FLinearColor(0.290f, 0.396f, 0.459f, 1.0f);  // #4a6575
	TextUnavailable = FLinearColor(0.165f, 0.243f, 0.290f, 1.0f);  // #2a3e4a

	// Per-category color accents (Foundation=orange, Floor=green, Walls=blue, Roof=gold)
	// These match the React prototype's per-section colors
	CategoryColorTable.SetNum(4);

	// Foundation - warm orange
	CategoryColorTable[0].Accent   = FLinearColor(1.0f, 0.416f, 0.259f, 1.0f);    // #ff6a42
	CategoryColorTable[0].WedgeDim = FLinearColor(1.0f, 0.416f, 0.259f, 0.08f);
	CategoryColorTable[0].WedgeLit = FLinearColor(1.0f, 0.416f, 0.259f, 0.30f);

	// Floor - green
	CategoryColorTable[1].Accent   = FLinearColor(0.239f, 0.863f, 0.518f, 1.0f);   // #3ddc84
	CategoryColorTable[1].WedgeDim = FLinearColor(0.239f, 0.863f, 0.518f, 0.08f);
	CategoryColorTable[1].WedgeLit = FLinearColor(0.239f, 0.863f, 0.518f, 0.30f);

	// Walls - blue
	CategoryColorTable[2].Accent   = FLinearColor(0.369f, 0.612f, 1.0f, 1.0f);     // #5e9cff
	CategoryColorTable[2].WedgeDim = FLinearColor(0.369f, 0.612f, 1.0f, 0.08f);
	CategoryColorTable[2].WedgeLit = FLinearColor(0.369f, 0.612f, 1.0f, 0.30f);

	// Roof - gold
	CategoryColorTable[3].Accent   = FLinearColor(0.941f, 0.784f, 0.314f, 1.0f);   // #f0c850
	CategoryColorTable[3].WedgeDim = FLinearColor(0.941f, 0.784f, 0.314f, 0.08f);
	CategoryColorTable[3].WedgeLit = FLinearColor(0.941f, 0.784f, 0.314f, 0.30f);
}

// ============================================================================
// BuildCategories — group pieces into 4 categories
// ============================================================================

void URadialPieceMenu::BuildCategories()
{
	Categories.Empty();

	FCategoryInfo Foundation;
	Foundation.Name = TEXT("FOUNDATION");
	Foundation.Icon = TEXT("\u2B1B");

	FCategoryInfo Floor;
	Floor.Name = TEXT("FLOOR");
	Floor.Icon = TEXT("\u25A6");

	FCategoryInfo Walls;
	Walls.Name = TEXT("WALLS");
	Walls.Icon = TEXT("\u25A5");

	FCategoryInfo Roof;
	Roof.Name = TEXT("ROOF");
	Roof.Icon = TEXT("\u25B3");

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

// ============================================================================
// InitMenu
// ============================================================================

void URadialPieceMenu::InitMenu(const TArray<FPieceTypeInfo>& InInfos, int32 CurrentIndex)
{
	AllPieceInfos = InInfos;
	BuildCategories();

	CurrentView = ERadialMenuView::Main;
	ActiveCategory = -1;
	HighlightedCategory = FindCategoryForPieceIndex(CurrentIndex);
	PrevHighlightedCategory = HighlightedCategory;
	HighlightedPieceSlot = -1;
	SelectedPieceSlot = -1;

	FadeAlpha = 0.0f;
	ViewTransition = 0.0f;
	GlowPulseTime = 0.0f;

	CategoryHoverScales.Init(0.0f, Categories.Num());
	PieceHoverScales.Empty();

	// Load icon textures
	IconBrushes.Empty();
	IconBrushes.SetNum(AllPieceInfos.Num());
	for (int32 i = 0; i < AllPieceInfos.Num(); i++)
	{
		UTexture2D* Tex = AllPieceInfos[i].Icon.LoadSynchronous();
		if (Tex)
		{
			IconBrushes[i].SetResourceObject(Tex);
			IconBrushes[i].ImageSize = FVector2D(IconSize, IconSize);
			IconBrushes[i].DrawAs = ESlateBrushDrawType::Image;
			IconBrushes[i].Tiling = ESlateBrushTileType::NoTile;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RadialPieceMenu: Init %d pieces in %d categories, current=%d"),
		AllPieceInfos.Num(), Categories.Num(), CurrentIndex);
}

int32 URadialPieceMenu::GetHighlightedIndex() const
{
	if (ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		const TArray<int32>& Pieces = Categories[ActiveCategory].PieceIndices;
		// SelectedPieceSlot takes priority (persists after click)
		int32 ActiveSlot = (SelectedPieceSlot >= 0) ? SelectedPieceSlot : HighlightedPieceSlot;
		if (ActiveSlot >= 0 && Pieces.IsValidIndex(ActiveSlot))
			return Pieces[ActiveSlot];
		if (Pieces.Num() > 0)
			return Pieces[0];
	}
	return -1;
}

// ============================================================================
// Sound stubs
// ============================================================================

void URadialPieceMenu::PlaySoundOpen()   { }
void URadialPieceMenu::PlaySoundClose()  { }
void URadialPieceMenu::PlaySoundHover()  { }
void URadialPieceMenu::PlaySoundSelect() { }
void URadialPieceMenu::PlaySoundBack()   { }

// ============================================================================
// Color utilities
// ============================================================================

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

// ============================================================================
// NativeOnMouseButtonDown — handle clicks for view transitions and selection
// ============================================================================

FReply URadialPieceMenu::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!InMouseEvent.GetEffectingButton().IsValid()) return FReply::Unhandled();
	if (Categories.Num() == 0) return FReply::Unhandled();

	FVector2D LocalSize = InGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;
	float MenuDiameter = LocalSize.Y * 0.65f;
	float Scale = MenuDiameter / (OuterRadius * 2.0f);

	FVector2D MouseLocal = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	float DX = MouseLocal.X - Center.X;
	float DY = MouseLocal.Y - Center.Y;
	float Dist = FMath::Sqrt(DX * DX + DY * DY);

	float sInner = InnerRadius * Scale;
	float sOuter = OuterRadius * Scale;
	float sHub   = HubRadius * Scale;

	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
	if (AngleDeg < 0.0f) AngleDeg += 360.0f;

	if (CurrentView == ERadialMenuView::Main)
	{
		// Click on a category wedge → enter sub view
		if (Dist >= sInner && Dist <= sOuter)
		{
			int32 NumCats = Categories.Num();
			float CatAngle = 360.0f / NumCats;
			int32 ClickedCat = FMath::Clamp((int32)(AngleDeg / CatAngle), 0, NumCats - 1);
			ActiveCategory = ClickedCat;
			HighlightedPieceSlot = -1;
			SelectedPieceSlot = -1;
			PieceHoverScales.Empty();
			CurrentView = ERadialMenuView::Sub;
			PlaySoundSelect();
			return FReply::Handled();
		}
	}
	else if (CurrentView == ERadialMenuView::Sub)
	{
		// Click center hub → go back to main
		if (Dist <= sHub + 5.0f * Scale)
		{
			CurrentView = ERadialMenuView::Main;
			ActiveCategory = -1;
			HighlightedPieceSlot = -1;
			SelectedPieceSlot = -1;
			PieceHoverScales.Empty();
			PlaySoundBack();
			return FReply::Handled();
		}

		// Click on a piece wedge → select it
		if (Dist >= sInner && Dist <= sOuter && ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
		{
			int32 NumPieces = Categories[ActiveCategory].PieceIndices.Num();
			if (NumPieces > 0)
			{
				float PieceAngle = 360.0f / NumPieces;
				int32 ClickedPiece = FMath::Clamp((int32)(AngleDeg / PieceAngle), 0, NumPieces - 1);
				SelectedPieceSlot = ClickedPiece;
				HighlightedPieceSlot = ClickedPiece;
				PlaySoundSelect();
				return FReply::Handled();
			}
		}
	}

	return FReply::Unhandled();
}

// ============================================================================
// NativeTick — mouse tracking + animation interpolation
// ============================================================================

void URadialPieceMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Categories.Num() == 0) return;

	// Fade in
	FadeAlpha = FMath::FInterpTo(FadeAlpha, 1.0f, InDeltaTime, FadeSpeed);
	GlowPulseTime += InDeltaTime;

	// View transition interpolation
	float TargetTransition = (CurrentView == ERadialMenuView::Sub) ? 1.0f : 0.0f;
	ViewTransition = FMath::FInterpTo(ViewTransition, TargetTransition, InDeltaTime, ViewTransitionSpeed);

	// Mouse position
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);

	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
		GEngine->GameViewport->GetViewportSize(ViewportSize);

	FVector2D Center = ViewportSize / 2.0f;
	float Scale = (ViewportSize.Y * 0.65f) / (OuterRadius * 2.0f);

	float sInner = InnerRadius * Scale;
	float sOuter = OuterRadius * Scale;
	float sDead  = DeadZone * Scale;

	float DX = MouseX - Center.X;
	float DY = MouseY - Center.Y;
	float Dist = FMath::Sqrt(DX * DX + DY * DY);
	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
	if (AngleDeg < 0.0f) AngleDeg += 360.0f;

	if (CurrentView == ERadialMenuView::Main)
	{
		// Track hovered category
		int32 NumCats = Categories.Num();
		if (Dist >= sInner && Dist <= sOuter)
		{
			float CatAngle = 360.0f / NumCats;
			HighlightedCategory = FMath::Clamp((int32)(AngleDeg / CatAngle), 0, NumCats - 1);
		}
		else if (Dist < sDead)
		{
			// Keep current
		}
		else
		{
			HighlightedCategory = -1;
		}

		// Interpolate category hover scales
		if (CategoryHoverScales.Num() != NumCats)
			CategoryHoverScales.Init(0.0f, NumCats);
		for (int32 i = 0; i < NumCats; i++)
		{
			float Target = (i == HighlightedCategory) ? 1.0f : 0.0f;
			CategoryHoverScales[i] = FMath::FInterpTo(CategoryHoverScales[i], Target, InDeltaTime, 12.0f);
		}
	}
	else if (CurrentView == ERadialMenuView::Sub && ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		// Track hovered piece
		int32 NumPieces = Categories[ActiveCategory].PieceIndices.Num();
		if (Dist >= sInner && Dist <= sOuter && NumPieces > 0)
		{
			float PieceAngle = 360.0f / NumPieces;
			HighlightedPieceSlot = FMath::Clamp((int32)(AngleDeg / PieceAngle), 0, NumPieces - 1);
		}
		else if (Dist < sDead)
		{
			// Keep current
		}
		else
		{
			HighlightedPieceSlot = -1;
		}

		// Interpolate piece hover scales
		if (PieceHoverScales.Num() != NumPieces)
			PieceHoverScales.Init(0.0f, NumPieces);
		for (int32 i = 0; i < NumPieces; i++)
		{
			// Lit if hovered OR selected (same as React prototype)
			float Target = (i == HighlightedPieceSlot || i == SelectedPieceSlot) ? 1.0f : 0.0f;
			PieceHoverScales[i] = FMath::FInterpTo(PieceHoverScales[i], Target, InDeltaTime, 12.0f);
		}
	}

	// Sound on highlight change
	if (HighlightedCategory != PrevHighlightedCategory ||
		HighlightedPieceSlot != PrevHighlightedPieceSlot)
	{
		PlaySoundHover();
		PrevHighlightedCategory = HighlightedCategory;
		PrevHighlightedPieceSlot = HighlightedPieceSlot;
	}
}

// ============================================================================
// NativePaint — main entry point, delegates to sub-functions
// ============================================================================

int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (Categories.Num() == 0 || FadeAlpha < 0.001f) return LayerId;

	FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;
	float Scale = (LocalSize.Y * 0.65f) / (OuterRadius * 2.0f);

	// Decorative outer rings + tick marks
	LayerId = PaintRingDecorations(OutDrawElements, LayerId, AllottedGeometry, Center, Scale);

	// Main view or sub view (cross-faded via ViewTransition)
	if (ViewTransition < 0.99f)
		LayerId = PaintMainView(OutDrawElements, LayerId, AllottedGeometry, Center, Scale);
	if (ViewTransition > 0.01f)
		LayerId = PaintSubView(OutDrawElements, LayerId, AllottedGeometry, Center, Scale);

	// Center hub (always visible)
	LayerId = PaintCenterHub(OutDrawElements, LayerId, AllottedGeometry, Center, Scale);

	return LayerId;
}

// ============================================================================
// PaintRingDecorations — outer ring pulses + tick marks
// ============================================================================

int32 URadialPieceMenu::PaintRingDecorations(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Scale) const
{
	float sOuter = OuterRadius * Scale;
	float Pulse = 0.85f + 0.15f * FMath::Sin(GlowPulseTime * 1.8f);

	// Three concentric pulsing rings
	DrawArcOutline(Out, LayerId, Geo, Center, sOuter + 18.0f * Scale, -90.0f, 270.0f,
		WithAlpha(Cyan, 0.05f * Pulse * FadeAlpha), 0.5f);
	DrawArcOutline(Out, LayerId, Geo, Center, sOuter + 10.0f * Scale, -90.0f, 270.0f,
		WithAlpha(Cyan, 0.08f * Pulse * FadeAlpha), 0.8f);
	DrawArcOutline(Out, LayerId, Geo, Center, sOuter + 3.0f * Scale, -90.0f, 270.0f,
		WithAlpha(Cyan, 0.12f * Pulse * FadeAlpha), 1.0f);

	// 72 tick marks (every 5°)
	for (int32 t = 0; t < 72; t++)
	{
		float AngDeg = t * 5.0f - 90.0f;
		float AngRad = FMath::DegreesToRadians(AngDeg);
		FVector2D Dir(FMath::Cos(AngRad), FMath::Sin(AngRad));

		bool bMajor = (t % 4 == 0);
		float TickStart = sOuter + 4.0f * Scale;
		float TickEnd   = sOuter + (bMajor ? 14.0f : 8.0f) * Scale;
		float Alpha     = bMajor ? 0.3f : 0.12f;
		float Width     = bMajor ? 1.5f : 0.5f;

		DrawLine(Out, LayerId, Geo,
			Center + Dir * TickStart, Center + Dir * TickEnd,
			WithAlpha(Cyan, Alpha * Pulse * FadeAlpha), Width);
	}

	return LayerId + 1;
}

// ============================================================================
// PaintMainView — 4 category wedges
// ============================================================================

int32 URadialPieceMenu::PaintMainView(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Scale) const
{
	int32 NumCats = Categories.Num();
	float CatAngle = 360.0f / NumCats;
	float GapHalf = WedgeGapDeg / 2.0f;
	float Pulse = 0.85f + 0.15f * FMath::Sin(GlowPulseTime * 1.8f);

	float sInner = InnerRadius * Scale;
	float sOuter = OuterRadius * Scale;

	// Fade out main view as sub view fades in
	float MainAlpha = 1.0f - ViewTransition;

	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	int32 CatFontSize = FMath::Clamp(FMath::RoundToInt(11.0f * Scale), 8, 16);
	FSlateFontInfo CatFont = FCoreStyle::GetDefaultFontStyle("Bold", CatFontSize);

	for (int32 i = 0; i < NumCats; i++)
	{
		float StartDeg = i * CatAngle - 90.0f + GapHalf;
		float EndDeg   = (i + 1) * CatAngle - 90.0f - GapHalf;
		float MidDeg   = (StartDeg + EndDeg) / 2.0f;

		float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
		FLinearColor AccentCol = CategoryColorTable.IsValidIndex(i) ? CategoryColorTable[i].Accent : Cyan;

		// Wedge fill
		FLinearColor FillCol = FMath::Lerp(DarkWedge, ActiveWedgeFill, HoverT);
		FillCol.A *= FadeAlpha * MainAlpha;
		DrawFilledArc(Out, LayerId, Geo, Center, sInner, sOuter, StartDeg, EndDeg, FillCol);

		// Glow border — outer arc
		float GlowI = FMath::Lerp(0.25f, 1.0f, HoverT);
		DrawArcOutline(Out, LayerId, Geo, Center, sOuter, StartDeg, EndDeg,
			WithAlpha(Cyan, GlowI * 0.5f * Pulse * FadeAlpha * MainAlpha), 2.5f * Scale * HoverT + 1.0f * Scale);
		DrawArcOutline(Out, LayerId, Geo, Center, sOuter, StartDeg, EndDeg,
			WithAlpha(Cyan, GlowI * Pulse * FadeAlpha * MainAlpha), FMath::Lerp(0.8f, 1.5f, HoverT));

		// Inner arc
		DrawArcOutline(Out, LayerId, Geo, Center, sInner, StartDeg, EndDeg,
			WithAlpha(Cyan, GlowI * 0.8f * Pulse * FadeAlpha * MainAlpha), 1.0f * Scale * HoverT + 0.5f * Scale);

		// Divider lines at wedge edges
		for (float Deg : {StartDeg, EndDeg})
		{
			float Rad = FMath::DegreesToRadians(Deg);
			FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad));
			DrawLine(Out, LayerId, Geo, Center + Dir * sInner, Center + Dir * sOuter,
				WithAlpha(Cyan, GlowI * 0.7f * Pulse * FadeAlpha * MainAlpha), 0.8f);
		}

		// Icon at 38% of the radius band
		float IconR = sInner + (sOuter - sInner) * 0.35f;
		float ScaledIconSz = IconSize * Scale * 0.7f;
		float IconAngleRad = FMath::DegreesToRadians(MidDeg);
		FVector2D IconCenter = Center + FVector2D(FMath::Cos(IconAngleRad), FMath::Sin(IconAngleRad)) * IconR;

		// Draw unicode icon fallback as text
		if (!Categories[i].Icon.IsEmpty())
		{
			int32 IconFontSz = FMath::Clamp(FMath::RoundToInt(22.0f * Scale), 14, 30);
			FSlateFontInfo IconFont = FCoreStyle::GetDefaultFontStyle("Regular", IconFontSz);
			FLinearColor IconTint = FMath::Lerp(TextDimmed, AccentCol, HoverT);
			IconTint.A *= FadeAlpha * MainAlpha;

			FVector2D ISize = FontMeasure->Measure(Categories[i].Icon, IconFont);
			FVector2D IPos = IconCenter - ISize / 2.0f;
			FGeometry IGeo = Geo.MakeChild(ISize, FSlateLayoutTransform(IPos));
			FSlateDrawElement::MakeText(Out, LayerId, IGeo.ToPaintGeometry(),
				Categories[i].Icon, IconFont, ESlateDrawEffect::None, IconTint);
		}

		// Curved category name at 65% of radius band
		float TextR = sInner + (sOuter - sInner) * 0.68f;
		FLinearColor NameCol = FMath::Lerp(TextDimmed, Cyan, HoverT);
		NameCol.A *= FadeAlpha * MainAlpha;
		DrawCurvedText(Out, LayerId, Geo, Center, TextR, MidDeg,
			Categories[i].Name, CatFont, NameCol, 4.5f);
	}

	return LayerId + 1;
}

// ============================================================================
// PaintSubView — piece wedges for ActiveCategory
// ============================================================================

int32 URadialPieceMenu::PaintSubView(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Scale) const
{
	if (ActiveCategory < 0 || !Categories.IsValidIndex(ActiveCategory)) return LayerId;

	const FCategoryInfo& Cat = Categories[ActiveCategory];
	int32 NumPieces = Cat.PieceIndices.Num();
	if (NumPieces == 0) return LayerId;

	float PieceAngle = 360.0f / NumPieces;
	float GapHalf = WedgeGapDeg / 2.0f;
	float Pulse = 0.85f + 0.15f * FMath::Sin(GlowPulseTime * 1.8f);

	float sInner = InnerRadius * Scale;
	float sOuter = OuterRadius * Scale;
	float SubAlpha = ViewTransition;

	// Get category accent color
	FLinearColor AccentCol = CategoryColorTable.IsValidIndex(ActiveCategory)
		? CategoryColorTable[ActiveCategory].Accent : Cyan;
	FLinearColor WedgeDimCol = CategoryColorTable.IsValidIndex(ActiveCategory)
		? CategoryColorTable[ActiveCategory].WedgeDim : WithAlpha(Cyan, 0.08f);
	FLinearColor WedgeLitCol = CategoryColorTable.IsValidIndex(ActiveCategory)
		? CategoryColorTable[ActiveCategory].WedgeLit : WithAlpha(Cyan, 0.30f);

	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	int32 PieceNameSize = FMath::Clamp(FMath::RoundToInt(9.5f * Scale), 7, 14);
	int32 PieceDimSize  = FMath::Clamp(FMath::RoundToInt(7.5f * Scale), 6, 11);
	FSlateFontInfo PieceNameFont = FCoreStyle::GetDefaultFontStyle("Bold", PieceNameSize);
	FSlateFontInfo PieceDimFont  = FCoreStyle::GetDefaultFontStyle("Regular", PieceDimSize);

	for (int32 p = 0; p < NumPieces; p++)
	{
		int32 GlobalIdx = Cat.PieceIndices[p];
		bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
		float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;
		bool bLit = (p == HighlightedPieceSlot || p == SelectedPieceSlot);

		float StartDeg = p * PieceAngle - 90.0f + GapHalf;
		float EndDeg   = (p + 1) * PieceAngle - 90.0f - GapHalf;
		float MidDeg   = (StartDeg + EndDeg) / 2.0f;

		// Wedge fill — colored by category accent
		FLinearColor FillCol = bLit
			? FMath::Lerp(WedgeDimCol, WedgeLitCol, HoverT)
			: WedgeDimCol;
		if (!bAvailable) FillCol = WithAlpha(FillCol, FillCol.A * 0.4f);
		FillCol.A *= FadeAlpha * SubAlpha;
		DrawFilledArc(Out, LayerId, Geo, Center, sInner, sOuter, StartDeg, EndDeg, FillCol);

		// Glow border — outer arc
		FLinearColor GlowCol = bAvailable ? AccentCol : TextUnavailable;
		float GlowI = bLit ? 1.0f : (bAvailable ? 0.3f : 0.08f);
		GlowI = FMath::Lerp(GlowI * 0.5f, GlowI, HoverT + (bLit ? 0.5f : 0.0f));

		// Blur layer
		DrawArcOutline(Out, LayerId, Geo, Center, sOuter, StartDeg, EndDeg,
			WithAlpha(GlowCol, GlowI * 0.5f * Pulse * FadeAlpha * SubAlpha),
			FMath::Lerp(1.0f * Scale, 2.5f * Scale, HoverT));
		// Crisp layer
		DrawArcOutline(Out, LayerId, Geo, Center, sOuter, StartDeg, EndDeg,
			WithAlpha(GlowCol, GlowI * Pulse * FadeAlpha * SubAlpha),
			FMath::Lerp(0.6f, 1.5f, HoverT));

		// Inner arc
		DrawArcOutline(Out, LayerId, Geo, Center, sInner, StartDeg, EndDeg,
			WithAlpha(GlowCol, GlowI * 0.6f * Pulse * FadeAlpha * SubAlpha),
			FMath::Lerp(0.5f, 1.0f, HoverT));

		// Divider lines
		for (float Deg : {StartDeg, EndDeg})
		{
			float Rad = FMath::DegreesToRadians(Deg);
			FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad));
			DrawLine(Out, LayerId, Geo, Center + Dir * sInner, Center + Dir * sOuter,
				WithAlpha(GlowCol, GlowI * 0.5f * Pulse * FadeAlpha * SubAlpha), 0.6f);
		}

		// --- Icon at 38% radius ---
		float IconR = sInner + (sOuter - sInner) * 0.35f;
		float ScaledIcon = IconSize * Scale * 0.55f;
		if (IconBrushes.IsValidIndex(GlobalIdx) && IconBrushes[GlobalIdx].GetResourceObject())
		{
			float DrawSz = ScaledIcon * (1.0f + 0.06f * HoverT);
			FLinearColor IconTint = bAvailable
				? FMath::Lerp(FLinearColor(0.6f, 0.75f, 0.85f, 1.0f), FLinearColor::White, HoverT)
				: FLinearColor(0.2f, 0.25f, 0.3f, 0.4f);
			IconTint.A *= FadeAlpha * SubAlpha;
			DrawIconAtAngle(Out, LayerId, Geo, Center, IconR, MidDeg, IconBrushes[GlobalIdx], DrawSz, IconTint);
		}

		// --- Curved piece name at 62% radius ---
		FString PieceName = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].DisplayName : TEXT("");
		if (!PieceName.IsEmpty())
		{
			float NameR = sInner + (sOuter - sInner) * 0.65f;
			FLinearColor NameTint = bAvailable
				? FMath::Lerp(FLinearColor(0.557f, 0.667f, 0.733f, 1.0f), TextWhite, HoverT)
				: TextUnavailable;
			NameTint.A *= FadeAlpha * SubAlpha;

			// Adjust char spacing based on piece count (more pieces = tighter text)
			float CharSpacing = (NumPieces <= 4) ? 4.0f : (NumPieces <= 6 ? 3.2f : 2.5f);
			DrawCurvedText(Out, LayerId, Geo, Center, NameR, MidDeg,
				PieceName, PieceNameFont, NameTint, CharSpacing);
		}

		// --- Curved subtitle/dimensions at 78% radius ---
		FString Dims = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].Subtitle : TEXT("");
		if (!Dims.IsEmpty())
		{
			float DimR = sInner + (sOuter - sInner) * 0.82f;
			FLinearColor DimTint = bAvailable
				? WithAlpha(AccentCol, FMath::Lerp(0.4f, 0.8f, HoverT))
				: TextUnavailable;
			DimTint.A *= FadeAlpha * SubAlpha;

			float CharSpacing = (NumPieces <= 4) ? 3.5f : (NumPieces <= 6 ? 2.8f : 2.2f);
			DrawCurvedText(Out, LayerId, Geo, Center, DimR, MidDeg,
				Dims, PieceDimFont, DimTint, CharSpacing);
		}
	}

	return LayerId + 1;
}

// ============================================================================
// PaintCenterHub
// ============================================================================

int32 URadialPieceMenu::PaintCenterHub(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Scale) const
{
	float sHub = HubRadius * Scale;
	float Pulse = 0.85f + 0.15f * FMath::Sin(GlowPulseTime * 1.8f);
	float CenterPulse = 0.3f + 0.4f * (0.5f + 0.5f * FMath::Sin(GlowPulseTime * 1.05f));

	bool bInSub = (CurrentView == ERadialMenuView::Sub && ActiveCategory >= 0);

	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// Outer glow ring (pulsing)
	FLinearColor GlowRingCol = bInSub && CategoryColorTable.IsValidIndex(ActiveCategory)
		? CategoryColorTable[ActiveCategory].Accent : Cyan;
	DrawArcOutline(Out, LayerId, Geo, Center, sHub + 4.0f * Scale, -90.0f, 270.0f,
		WithAlpha(GlowRingCol, CenterPulse * Pulse * FadeAlpha), 2.0f * Scale);

	// Hub dark fill
	DrawCircleFill(Out, LayerId, Geo, Center, sHub, Faded(DarkBg));

	// Hub border
	float BorderAlpha = bInSub ? 0.6f : 0.25f;
	DrawArcOutline(Out, LayerId, Geo, Center, sHub, -90.0f, 270.0f,
		WithAlpha(GlowRingCol, BorderAlpha * Pulse * FadeAlpha), 1.5f);

	// Inner pulse ring
	float PulseR = sHub + 4.0f * Scale * (0.5f + 0.5f * FMath::Sin(GlowPulseTime * 0.785f));
	DrawArcOutline(Out, LayerId, Geo, Center, PulseR, -90.0f, 270.0f,
		WithAlpha(Cyan, 0.15f * FadeAlpha), 1.0f);

	// Content
	if (bInSub && Categories.IsValidIndex(ActiveCategory))
	{
		const FCategoryInfo& Cat = Categories[ActiveCategory];
		FLinearColor AccentCol = CategoryColorTable.IsValidIndex(ActiveCategory)
			? CategoryColorTable[ActiveCategory].Accent : Cyan;

		// Category icon
		if (!Cat.Icon.IsEmpty())
		{
			int32 IconFontSz = FMath::Clamp(FMath::RoundToInt(20.0f * Scale), 14, 28);
			FSlateFontInfo IconFont = FCoreStyle::GetDefaultFontStyle("Regular", IconFontSz);
			FVector2D ISize = FontMeasure->Measure(Cat.Icon, IconFont);
			FVector2D IPos = Center - FVector2D(ISize.X / 2.0f, ISize.Y / 2.0f + 10.0f * Scale);
			FGeometry IGeo = Geo.MakeChild(ISize, FSlateLayoutTransform(IPos));
			FSlateDrawElement::MakeText(Out, LayerId, IGeo.ToPaintGeometry(),
				Cat.Icon, IconFont, ESlateDrawEffect::None, Faded(AccentCol));
		}

		// Category name
		int32 NameFontSz = FMath::Clamp(FMath::RoundToInt(10.0f * Scale), 7, 14);
		FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", NameFontSz);
		FVector2D NSize = FontMeasure->Measure(Cat.Name, NameFont);
		FVector2D NPos = Center + FVector2D(-NSize.X / 2.0f, 4.0f * Scale);
		FGeometry NGeo = Geo.MakeChild(NSize, FSlateLayoutTransform(NPos));
		FSlateDrawElement::MakeText(Out, LayerId, NGeo.ToPaintGeometry(),
			Cat.Name, NameFont, ESlateDrawEffect::None, Faded(TextWhite));

		// "◂ BACK" text
		int32 BackFontSz = FMath::Clamp(FMath::RoundToInt(7.0f * Scale), 5, 10);
		FSlateFontInfo BackFont = FCoreStyle::GetDefaultFontStyle("Regular", BackFontSz);
		FString BackText = FString::Printf(TEXT("%c BACK"), 0x25C2); // ◂
		FVector2D BSize = FontMeasure->Measure(BackText, BackFont);
		FVector2D BPos = Center + FVector2D(-BSize.X / 2.0f, 16.0f * Scale);
		FGeometry BGeo = Geo.MakeChild(BSize, FSlateLayoutTransform(BPos));
		FSlateDrawElement::MakeText(Out, LayerId, BGeo.ToPaintGeometry(),
			BackText, BackFont, ESlateDrawEffect::None, WithAlpha(AccentCol, 0.6f * FadeAlpha));
	}
	else
	{
		// Main view: crosshair + BUILD text
		float GapR = 8.0f * Scale;
		float ArmR = 20.0f * Scale;
		FLinearColor CrossCol = WithAlpha(Cyan, 0.4f * Pulse * FadeAlpha);

		DrawLine(Out, LayerId, Geo, Center - FVector2D(ArmR, 0), Center - FVector2D(GapR, 0), CrossCol, 1.0f);
		DrawLine(Out, LayerId, Geo, Center + FVector2D(GapR, 0), Center + FVector2D(ArmR, 0), CrossCol, 1.0f);
		DrawLine(Out, LayerId, Geo, Center - FVector2D(0, ArmR), Center - FVector2D(0, GapR), CrossCol, 1.0f);
		DrawLine(Out, LayerId, Geo, Center + FVector2D(0, GapR), Center + FVector2D(0, ArmR), CrossCol, 1.0f);

		int32 BuildFontSz = FMath::Clamp(FMath::RoundToInt(12.0f * Scale), 9, 16);
		FSlateFontInfo BuildFont = FCoreStyle::GetDefaultFontStyle("Bold", BuildFontSz);
		FString BuildText = TEXT("BUILD");
		FVector2D BSize = FontMeasure->Measure(BuildText, BuildFont);
		FVector2D BPos = Center - FVector2D(BSize.X / 2.0f, BSize.Y / 2.0f + 3.0f * Scale);
		FGeometry BGeo = Geo.MakeChild(BSize, FSlateLayoutTransform(BPos));
		FSlateDrawElement::MakeText(Out, LayerId, BGeo.ToPaintGeometry(),
			BuildText, BuildFont, ESlateDrawEffect::None, Faded(Cyan));

		int32 SubFontSz = FMath::Clamp(FMath::RoundToInt(8.0f * Scale), 6, 11);
		FSlateFontInfo SubFont = FCoreStyle::GetDefaultFontStyle("Regular", SubFontSz);
		FString SubText = TEXT("select category");
		FVector2D SSize = FontMeasure->Measure(SubText, SubFont);
		FVector2D SPos = Center + FVector2D(-SSize.X / 2.0f, 8.0f * Scale);
		FGeometry SGeo = Geo.MakeChild(SSize, FSlateLayoutTransform(SPos));
		FSlateDrawElement::MakeText(Out, LayerId, SGeo.ToPaintGeometry(),
			SubText, SubFont, ESlateDrawEffect::None, WithAlpha(Cyan, 0.35f * FadeAlpha));
	}

	return LayerId + 1;
}

// ============================================================================
// DrawCurvedText — each character placed and rotated along an arc
// ============================================================================

void URadialPieceMenu::DrawCurvedText(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius,
	float MidAngleDeg, const FString& Text, const FSlateFontInfo& Font,
	FLinearColor Color, float CharSpacingDeg) const
{
	if (Text.IsEmpty() || Color.A < 0.001f) return;

	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	int32 Len = Text.Len();

	// Total angular span of the text
	float TotalSpanDeg = (Len - 1) * CharSpacingDeg;
	float StartAngleDeg = MidAngleDeg - TotalSpanDeg / 2.0f;

	for (int32 c = 0; c < Len; c++)
	{
		FString CharStr = Text.Mid(c, 1);
		float CharAngleDeg = StartAngleDeg + c * CharSpacingDeg;
		float CharAngleRad = FMath::DegreesToRadians(CharAngleDeg);

		// Position on arc
		FVector2D CharPos = Center + FVector2D(FMath::Cos(CharAngleRad), FMath::Sin(CharAngleRad)) * Radius;

		// Measure character
		FVector2D CharSize = FontMeasure->Measure(CharStr, Font);

		// The rotation angle: tangent to the circle at this point
		float RotationDeg = CharAngleDeg + 90.0f;
		float RotationRad = FMath::DegreesToRadians(RotationDeg);

		FVector2D Pivot = CharSize / 2.0f;

		// Build transform: offset so character center is at CharPos, then rotate
		// Manually compose: translate(-pivot), rotate, translate(charPos)
		FQuat2D Rot(RotationRad);
		FVector2D RotatedPivot = Rot.IsIdentity() ? -Pivot : TransformPoint(Rot, -Pivot);
		FSlateRenderTransform FinalTransform(Rot, RotatedPivot + CharPos);

		FGeometry CharGeo = Geo.MakeChild(CharSize,
			FSlateLayoutTransform(1.0f),
			FinalTransform,
			FVector2D(0.0f, 0.0f));

		FSlateDrawElement::MakeText(Out, LayerId, CharGeo.ToPaintGeometry(),
			CharStr, Font, ESlateDrawEffect::None, Color);
	}
}

// ============================================================================
// DrawIconAtAngle — texture at a polar coordinate
// ============================================================================

void URadialPieceMenu::DrawIconAtAngle(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius, float AngleDeg,
	const FSlateBrush& Brush, float DrawSize, FLinearColor Tint) const
{
	if (Tint.A < 0.001f) return;

	float AngleRad = FMath::DegreesToRadians(AngleDeg);
	FVector2D Pos = Center + FVector2D(FMath::Cos(AngleRad), FMath::Sin(AngleRad)) * Radius;
	FVector2D Size(DrawSize, DrawSize);
	FVector2D TopLeft = Pos - Size / 2.0f;

	FGeometry IconGeo = Geo.MakeChild(Size, FSlateLayoutTransform(TopLeft));
	FSlateDrawElement::MakeBox(Out, LayerId, IconGeo.ToPaintGeometry(),
		&Brush, ESlateDrawEffect::None, Tint);
}

// ============================================================================
// DrawFilledArc — annular sector via concentric ring strokes
// ============================================================================

void URadialPieceMenu::DrawFilledArc(FSlateWindowElementList& Out, int32 LayerId,
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
		FSlateDrawElement::MakeLines(Out, LayerId, PG,
			Points, ESlateDrawEffect::None, Color, false, LineWidth);
	}
}

// ============================================================================
// DrawArcOutline
// ============================================================================

void URadialPieceMenu::DrawArcOutline(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius,
	float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const
{
	if (Color.A < 0.001f) return;
	const int32 Steps = 64;
	TArray<FVector2D> Points;
	Points.Reserve(Steps + 1);
	for (int32 i = 0; i <= Steps; i++)
	{
		float T = (float)i / Steps;
		float Angle = FMath::DegreesToRadians(FMath::Lerp(StartDeg, EndDeg, T));
		Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}
	FSlateDrawElement::MakeLines(Out, LayerId, Geo.ToPaintGeometry(),
		Points, ESlateDrawEffect::None, Color, true, Thickness);
}

// ============================================================================
// DrawCircleFill
// ============================================================================

void URadialPieceMenu::DrawCircleFill(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const
{
	DrawFilledArc(Out, LayerId, Geo, Center, 0.0f, Radius, -90.0f, 270.0f, Color, 48);
}

// ============================================================================
// DrawLine
// ============================================================================

void URadialPieceMenu::DrawLine(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D A, FVector2D B, FLinearColor Color, float Thickness) const
{
	if (Color.A < 0.001f) return;
	TArray<FVector2D> Points;
	Points.Add(A);
	Points.Add(B);
	FSlateDrawElement::MakeLines(Out, LayerId, Geo.ToPaintGeometry(),
		Points, ESlateDrawEffect::None, Color, true, Thickness);
}
