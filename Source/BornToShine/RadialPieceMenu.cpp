// Born To Shine - Two-Tier Radial Piece Selection Menu

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

	// --- Geometry: two concentric rings ---
	CenterHubRadius = 100.0f;
	DeadZone        = 60.0f;
	InnerRingInner  = 120.0f;   // Category ring
	InnerRingOuter  = 260.0f;
	OuterRingInner  = 275.0f;   // Piece ring (gap between rings)
	OuterRingOuter  = 480.0f;

	FadeAlpha = 0.0f;
	FadeSpeed = 8.0f;
	GlowPulseTime = 0.0f;

	SegmentIconSize = 100.0f;
	CenterIconSize  = 120.0f;

	// --- Color palette ---
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
	CategoryTextColor        = FLinearColor(0.80f, 0.90f, 1.00f, 1.0f);
}

// ---------------------------------------------------------------------------
void URadialPieceMenu::BuildCategories()
{
	Categories.Empty();

	// Four fixed categories
	FCategoryInfo Foundation; Foundation.Name = TEXT("Foundation");
	FCategoryInfo Floor;      Floor.Name      = TEXT("Floor");
	FCategoryInfo Walls;      Walls.Name      = TEXT("Walls");
	FCategoryInfo Roof;       Roof.Name       = TEXT("Roof");

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
			// Unknown piece type — put in walls as fallback
			Walls.PieceIndices.Add(i); break;
		}
	}

	// Only add categories that have at least one piece
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

	// Pre-highlight the category containing the current piece
	HighlightedCategory = FindCategoryForPieceIndex(CurrentIndex);
	PrevHighlightedCategory = HighlightedCategory;

	// Pre-highlight the piece within its category
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

	// Build icon brushes for all pieces
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
	// Return the global piece index
	if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
	{
		const TArray<int32>& Pieces = Categories[HighlightedCategory].PieceIndices;
		if (HighlightedPieceSlot >= 0 && Pieces.IsValidIndex(HighlightedPieceSlot))
		{
			return Pieces[HighlightedPieceSlot];
		}
		// If hovering category but no specific piece, return first piece in category
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

// ---------------------------------------------------------------------------
// Tick: mouse tracking for both rings
// ---------------------------------------------------------------------------
void URadialPieceMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Categories.Num() == 0) return;

	FadeAlpha = FMath::FInterpTo(FadeAlpha, 1.0f, InDeltaTime, FadeSpeed);
	GlowPulseTime += InDeltaTime;

	// Mouse tracking
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);

	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	FVector2D Center = ViewportSize / 2.0f;

	float DX = MouseX - Center.X;
	float DY = MouseY - Center.Y;
	float Dist = FMath::Sqrt(DX * DX + DY * DY);

	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
	if (AngleDeg < 0.0f) AngleDeg += 360.0f;

	int32 NumCats = Categories.Num();
	float CatAngle = 360.0f / NumCats;

	// Determine which ring the cursor is in
	if (Dist >= InnerRingInner && Dist < InnerRingOuter)
	{
		// Inner ring: category selection
		int32 NewCat = FMath::Clamp((int32)(AngleDeg / CatAngle), 0, NumCats - 1);
		if (NewCat != HighlightedCategory)
		{
			HighlightedCategory = NewCat;
			HighlightedPieceSlot = -1; // Reset piece selection when category changes
			// Reset piece hover scales
			PieceHoverScales.Empty();
		}
	}
	else if (Dist >= OuterRingInner && Dist <= OuterRingOuter + 20.0f && HighlightedCategory >= 0)
	{
		// Outer ring: piece selection within the active category
		const TArray<int32>& Pieces = Categories[HighlightedCategory].PieceIndices;
		int32 NumPieces = Pieces.Num();
		if (NumPieces > 0)
		{
			// The outer ring pieces are spread across the SAME angular span as their category
			float CatStartDeg = HighlightedCategory * CatAngle;
			float CatEndDeg = CatStartDeg + CatAngle;
			float PieceAngle = CatAngle / NumPieces;

			// Map the mouse angle into the category span
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
	else if (Dist < DeadZone)
	{
		// Dead zone: keep current selection
	}

	// Sound on change
	if (HighlightedCategory != PrevHighlightedCategory ||
		HighlightedPieceSlot != PrevHighlightedPieceSlot)
	{
		PlaySoundHover();
		PrevHighlightedCategory = HighlightedCategory;
		PrevHighlightedPieceSlot = HighlightedPieceSlot;
	}

	// Category hover interpolation
	if (CategoryHoverScales.Num() != NumCats)
		CategoryHoverScales.Init(0.0f, NumCats);
	for (int32 i = 0; i < NumCats; i++)
	{
		float Target = (i == HighlightedCategory) ? 1.0f : 0.0f;
		CategoryHoverScales[i] = FMath::FInterpTo(CategoryHoverScales[i], Target, InDeltaTime, 12.0f);
	}

	// Piece hover interpolation
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
// Paint
// ---------------------------------------------------------------------------
int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 NumCats = Categories.Num();
	if (NumCats == 0 || FadeAlpha < 0.001f) return LayerId;

	FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;
	float CatAngle = 360.0f / NumCats;

	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	FSlateFontInfo CatFont       = FCoreStyle::GetDefaultFontStyle("Bold", 16);
	FSlateFontInfo PieceNameFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	FSlateFontInfo CenterNameFont = FCoreStyle::GetDefaultFontStyle("Bold", 22);
	FSlateFontInfo CenterSubFont  = FCoreStyle::GetDefaultFontStyle("Regular", 14);

	float Pulse = 0.85f + 0.15f * (0.5f + 0.5f * FMath::Sin(GlowPulseTime * 2.2f));

	// =================================================================
	// LAYER 1: Background circle
	// =================================================================
	DrawCircleFill(OutDrawElements, LayerId, AllottedGeometry, Center,
		OuterRingOuter + 24.0f, Faded(BgOverlayColor));
	LayerId++;

	// =================================================================
	// LAYER 2: Inner ring (categories) — fills + glow
	// =================================================================
	for (int32 i = 0; i < NumCats; i++)
	{
		float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
		float GapHalf = 1.0f;
		float StartDeg = i * CatAngle - 90.0f + GapHalf;
		float EndDeg = (i + 1) * CatAngle - 90.0f - GapHalf;

		// Glow on hover
		if (HoverT > 0.01f)
		{
			FLinearColor GlowCol = SegmentHoverGlowColor;
			GlowCol.A = SegmentHoverGlowColor.A * HoverT * FadeAlpha * Pulse;
			DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
				InnerRingInner - 2.0f * HoverT, InnerRingOuter + 4.0f * HoverT,
				StartDeg - 0.5f * HoverT, EndDeg + 0.5f * HoverT, GlowCol);
		}

		// Fill
		FLinearColor FillCol = FMath::Lerp(SegmentFillColor, SegmentHoverFillColor, HoverT * 0.6f);
		DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRingInner, InnerRingOuter, StartDeg, EndDeg, Faded(FillCol));
	}
	LayerId++;

	// =================================================================
	// LAYER 3: Inner ring dividers
	// =================================================================
	{
		FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();
		FLinearColor DivBody(0.12f, 0.16f, 0.20f, 0.75f);
		for (int32 i = 0; i < NumCats; i++)
		{
			float AngleRad = FMath::DegreesToRadians(i * CatAngle - 90.0f);
			FVector2D Radial(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
			FVector2D Inner = Center + Radial * InnerRingInner;
			FVector2D Outer = Center + Radial * InnerRingOuter;
			TArray<FVector2D> Pts;
			Pts.Add(Inner); Pts.Add(Outer);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PG,
				Pts, ESlateDrawEffect::None, Faded(DivBody), true, 2.0f);
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 4: Inner ring borders
	// =================================================================
	{
		FLinearColor RingBody(0.15f, 0.20f, 0.25f, 0.85f);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRingOuter, -90.0f, 270.0f, Faded(RingBody), 3.0f);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRingInner, -90.0f, 270.0f, Faded(RingBody), 2.0f);

		FLinearColor AccentLine(0.00f, 0.85f, 0.95f, 0.40f * Pulse);
		DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
			InnerRingOuter, -90.0f, 270.0f, Faded(AccentLine), 1.5f);
	}
	LayerId++;

	// =================================================================
	// LAYER 5: Category names in inner ring
	// =================================================================
	{
		float TextR = (InnerRingInner + InnerRingOuter) / 2.0f;
		for (int32 i = 0; i < NumCats; i++)
		{
			float MidAngle = FMath::DegreesToRadians((i + 0.5f) * CatAngle - 90.0f);
			FVector2D LabelCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * TextR;

			float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
			FLinearColor Tint = FMath::Lerp(TextDimmed, TextWhite, HoverT);
			Tint.A *= FadeAlpha;

			FVector2D TextSize = FontMeasure->Measure(Categories[i].Name, CatFont);
			FVector2D TextPos = LabelCenter - TextSize / 2.0f;
			FGeometry TextGeo = AllottedGeometry.MakeChild(TextSize, FSlateLayoutTransform(TextPos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, TextGeo.ToPaintGeometry(),
				Categories[i].Name, CatFont, ESlateDrawEffect::None, Tint);
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 6: Outer ring (pieces in highlighted category)
	// =================================================================
	if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
	{
		const FCategoryInfo& Cat = Categories[HighlightedCategory];
		int32 NumPieces = Cat.PieceIndices.Num();
		if (NumPieces > 0)
		{
			float CatStartDeg = HighlightedCategory * CatAngle - 90.0f;
			float PieceAngle = CatAngle / NumPieces;

			// Piece fills
			for (int32 p = 0; p < NumPieces; p++)
			{
				int32 GlobalIdx = Cat.PieceIndices[p];
				bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
				float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;

				float GapHalf = 0.6f;
				float StartDeg = CatStartDeg + p * PieceAngle + GapHalf;
				float EndDeg = CatStartDeg + (p + 1) * PieceAngle - GapHalf;

				// Glow
				if (HoverT > 0.01f)
				{
					FLinearColor GlowCol = SegmentHoverGlowColor;
					GlowCol.A = SegmentHoverGlowColor.A * HoverT * FadeAlpha * Pulse;
					DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
						OuterRingInner - 2.0f * HoverT, OuterRingOuter + 4.0f * HoverT,
						StartDeg - 0.5f * HoverT, EndDeg + 0.5f * HoverT, GlowCol);
				}

				if (!bAvailable)
				{
					DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
						OuterRingInner, OuterRingOuter, StartDeg, EndDeg, Faded(SegmentUnavailableColor));
				}
				else
				{
					FLinearColor FillCol = FMath::Lerp(SegmentFillColor, SegmentHoverFillColor, HoverT);
					DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
						OuterRingInner, OuterRingOuter, StartDeg, EndDeg, Faded(FillCol));
				}
			}
			LayerId++;

			// Piece dividers
			{
				FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();
				FLinearColor DivBody(0.12f, 0.16f, 0.20f, 0.75f);
				for (int32 p = 0; p <= NumPieces; p++)
				{
					float AngleRad = FMath::DegreesToRadians(CatStartDeg + p * PieceAngle);
					FVector2D Radial(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
					FVector2D Inner = Center + Radial * OuterRingInner;
					FVector2D Outer = Center + Radial * OuterRingOuter;
					TArray<FVector2D> Pts;
					Pts.Add(Inner); Pts.Add(Outer);
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PG,
						Pts, ESlateDrawEffect::None, Faded(DivBody), true, 1.5f);
				}
			}
			LayerId++;

			// Piece icons
			{
				float IconR = OuterRingInner + (OuterRingOuter - OuterRingInner) * 0.55f;
				for (int32 p = 0; p < NumPieces; p++)
				{
					int32 GlobalIdx = Cat.PieceIndices[p];
					float MidAngle = FMath::DegreesToRadians(CatStartDeg + (p + 0.5f) * PieceAngle);
					FVector2D IconCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * IconR;

					bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
					float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;

					if (IconBrushes.IsValidIndex(GlobalIdx) && IconBrushes[GlobalIdx].GetResourceObject())
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
							IconGeo.ToPaintGeometry(), &IconBrushes[GlobalIdx],
							ESlateDrawEffect::None, IconTint);
					}
				}
			}
			LayerId++;

			// Piece names (below icons, near inner edge)
			{
				float NameR = OuterRingInner + (OuterRingOuter - OuterRingInner) * 0.20f;
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

			// Outer ring border
			{
				FLinearColor RingBody(0.15f, 0.20f, 0.25f, 0.85f);
				DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
					OuterRingOuter, CatStartDeg, CatStartDeg + CatAngle, Faded(RingBody), 3.0f);
				DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
					OuterRingInner, CatStartDeg, CatStartDeg + CatAngle, Faded(RingBody), 2.0f);

				FLinearColor AccentLine(0.00f, 0.85f, 0.95f, 0.50f * Pulse);
				DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
					OuterRingOuter, CatStartDeg, CatStartDeg + CatAngle, Faded(AccentLine), 1.5f);
			}
			LayerId++;
		}
	}

	// =================================================================
	// LAYER 7: Center hub
	// =================================================================
	DrawCircleFill(OutDrawElements, LayerId, AllottedGeometry, Center,
		CenterHubRadius, Faded(CenterFillColor));

	FLinearColor HubAccent(0.00f, 0.75f, 0.88f, 0.50f * Pulse);
	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		CenterHubRadius, -90.0f, 270.0f, Faded(HubAccent), 2.0f);
	LayerId++;

	// =================================================================
	// LAYER 8: Center hub content
	// =================================================================
	{
		int32 GlobalIdx = GetHighlightedIndex();
		bool bHasSelection = (GlobalIdx >= 0 && AllPieceInfos.IsValidIndex(GlobalIdx));

		if (bHasSelection)
		{
			const FPieceTypeInfo& Info = AllPieceInfos[GlobalIdx];

			// Large icon
			if (IconBrushes.IsValidIndex(GlobalIdx) && IconBrushes[GlobalIdx].GetResourceObject())
			{
				FVector2D TexSize(CenterIconSize, CenterIconSize);
				FVector2D TexPos = Center - FVector2D(CenterIconSize / 2.0f, CenterIconSize / 2.0f + 20.0f);
				FGeometry IconGeo = AllottedGeometry.MakeChild(TexSize, FSlateLayoutTransform(TexPos));
				FLinearColor CenterIconTint = Info.bAvailable
					? FLinearColor(0.80f, 0.95f, 1.00f, FadeAlpha)
					: FLinearColor(0.30f, 0.35f, 0.40f, FadeAlpha * 0.5f);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					IconGeo.ToPaintGeometry(), &IconBrushes[GlobalIdx],
					ESlateDrawEffect::None, CenterIconTint);
			}

			// Piece name
			FLinearColor NameColor = Info.bAvailable ? TextWhite : TextUnavailable;
			FVector2D NameSize = FontMeasure->Measure(Info.DisplayName, CenterNameFont);
			FVector2D NamePos = Center + FVector2D(-NameSize.X / 2.0f, CenterIconSize / 2.0f - 12.0f);
			FGeometry NameGeo = AllottedGeometry.MakeChild(NameSize, FSlateLayoutTransform(NamePos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, NameGeo.ToPaintGeometry(),
				Info.DisplayName, CenterNameFont, ESlateDrawEffect::None, Faded(NameColor));

			// Subtitle
			if (!Info.Subtitle.IsEmpty())
			{
				FLinearColor SubColor = Info.bAvailable ? SubtitleColor : TextUnavailable;
				FVector2D SubSize = FontMeasure->Measure(Info.Subtitle, CenterSubFont);
				FVector2D SubPos = NamePos + FVector2D((NameSize.X - SubSize.X) / 2.0f, NameSize.Y + 2.0f);
				FGeometry SubGeo = AllottedGeometry.MakeChild(SubSize, FSlateLayoutTransform(SubPos));
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, SubGeo.ToPaintGeometry(),
					Info.Subtitle, CenterSubFont, ESlateDrawEffect::None, Faded(SubColor));
			}

			// Lock message
			if (!Info.bAvailable)
			{
				FString LockMsg = TEXT("LOCKED");
				if (AConstructionPhaseManager::Instance)
				{
					FString PrereqMsg = AConstructionPhaseManager::Instance->GetPrerequisiteMessage(Info.PieceType);
					if (!PrereqMsg.IsEmpty()) LockMsg = PrereqMsg;
				}
				FSlateFontInfo LockFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
				FLinearColor LockColor(1.0f, 0.6f, 0.1f, 1.0f);
				FVector2D LockSize = FontMeasure->Measure(LockMsg, LockFont);
				float SubOffset = Info.Subtitle.IsEmpty() ? 0.0f : FontMeasure->Measure(Info.Subtitle, CenterSubFont).Y + 4.0f;
				FVector2D LockPos = NamePos + FVector2D((NameSize.X - LockSize.X) / 2.0f, NameSize.Y + SubOffset + 4.0f);
				FGeometry LockGeo = AllottedGeometry.MakeChild(LockSize, FSlateLayoutTransform(LockPos));
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, LockGeo.ToPaintGeometry(),
					LockMsg, LockFont, ESlateDrawEffect::None, Faded(LockColor));
			}
		}
		else
		{
			FString Prompt = TEXT("Select Category");
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
// DrawFilledArc
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
// DrawArcOutline
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
// DrawCircleFill
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawCircleFill(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const
{
	DrawFilledArc(OutDrawElements, LayerId, Geo, Center, 0.0f, Radius,
		-90.0f, 270.0f, Color, 64);
}
