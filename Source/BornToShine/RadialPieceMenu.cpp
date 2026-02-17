// Born To Shine - Two-Tier Radial Piece Selection Menu (Clean Modern Style)

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

	// --- Geometry: computed dynamically from screen height in NativePaint ---
	// These are fallback defaults; actual values are scaled to 65% of screen
	// height in NativePaint so the menu stays proportional.
	CenterHubRadius = 70.0f;
	DeadZone        = 50.0f;
	InnerRingInner  = 85.0f;
	InnerRingOuter  = 180.0f;
	OuterRingInner  = 192.0f;
	OuterRingOuter  = 320.0f;

	FadeAlpha = 0.0f;
	FadeSpeed = 6.5f;  // ~150ms to reach full (ease-out via FInterpTo)
	GlowPulseTime = 0.0f;

	SegmentIconSize = 56.0f;
	CenterIconSize  = 80.0f;

	// --- Color palette (clean modern) ---
	BgOverlayColor           = FLinearColor(0.04f, 0.06f, 0.10f, 0.55f);    // subtle dark vignette
	SegmentFillColor         = FLinearColor(0.102f, 0.137f, 0.196f, 0.90f); // #1a2332 at 90%
	SegmentHoverFillColor    = FLinearColor(0.00f, 0.737f, 0.831f, 0.95f);  // #00bcd4
	SegmentHoverGlowColor    = FLinearColor(0.00f, 0.737f, 0.831f, 0.30f);  // soft turquoise glow
	SegmentUnavailableColor  = FLinearColor(0.06f, 0.08f, 0.10f, 0.70f);
	DividerColor             = FLinearColor(0.18f, 0.22f, 0.30f, 0.50f);    // 1px lighter border
	BorderAccentColor        = FLinearColor(0.00f, 0.737f, 0.831f, 0.40f);  // turquoise accent
	CenterFillColor          = FLinearColor(0.06f, 0.08f, 0.12f, 0.95f);
	CenterBorderColor        = FLinearColor(0.00f, 0.60f, 0.70f, 0.45f);
	TextWhite                = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextDimmed               = FLinearColor(0.60f, 0.68f, 0.78f, 1.0f);
	TextUnavailable          = FLinearColor(0.25f, 0.28f, 0.32f, 1.0f);
	SubtitleColor            = FLinearColor(0.00f, 0.737f, 0.831f, 1.0f);   // #00bcd4
	CategoryTextColor        = FLinearColor(0.90f, 0.95f, 1.00f, 1.0f);
}

// ---------------------------------------------------------------------------
void URadialPieceMenu::BuildCategories()
{
	Categories.Empty();

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

	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
	if (AngleDeg < 0.0f) AngleDeg += 360.0f;

	int32 NumCats = Categories.Num();
	float CatAngle = 360.0f / NumCats;

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

	if (HighlightedCategory != PrevHighlightedCategory ||
		HighlightedPieceSlot != PrevHighlightedPieceSlot)
	{
		PlaySoundHover();
		PrevHighlightedCategory = HighlightedCategory;
		PrevHighlightedPieceSlot = HighlightedPieceSlot;
	}

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
// Paint — clean modern radial menu
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

	TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	int32 CatFontSize = FMath::Clamp(FMath::RoundToInt(17.0f * Scale), 12, 20);
	int32 PieceNameFontSize = FMath::Clamp(FMath::RoundToInt(12.0f * Scale), 9, 15);
	int32 PieceDimFontSize = FMath::Clamp(FMath::RoundToInt(10.0f * Scale), 8, 13);
	int32 CenterNameFontSize = FMath::Clamp(FMath::RoundToInt(18.0f * Scale), 13, 24);
	int32 CenterSubFontSize = FMath::Clamp(FMath::RoundToInt(12.0f * Scale), 10, 16);

	FSlateFontInfo CatFont        = FCoreStyle::GetDefaultFontStyle("Bold", CatFontSize);
	FSlateFontInfo PieceNameFont  = FCoreStyle::GetDefaultFontStyle("Regular", PieceNameFontSize);
	FSlateFontInfo PieceDimFont   = FCoreStyle::GetDefaultFontStyle("Regular", PieceDimFontSize);
	FSlateFontInfo CenterNameFont = FCoreStyle::GetDefaultFontStyle("Bold", CenterNameFontSize);
	FSlateFontInfo CenterSubFont  = FCoreStyle::GetDefaultFontStyle("Regular", CenterSubFontSize);

	// =================================================================
	// LAYER 1: Subtle dark radial gradient (not a hard circle)
	// Draw progressively fainter rings from center outward
	// =================================================================
	{
		float MaxBgR = sOuterO + 8.0f * Scale;
		int32 GradientRings = 12;
		for (int32 r = 0; r < GradientRings; r++)
		{
			float T = (float)r / GradientRings;
			float RingInner = MaxBgR * T;
			float RingOuter = MaxBgR * (T + 1.0f / GradientRings);
			float Alpha = BgOverlayColor.A * (1.0f - T * 0.5f); // fade outer rings
			FLinearColor RingColor = BgOverlayColor;
			RingColor.A = Alpha * FadeAlpha;
			DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
				RingInner, RingOuter, -90.0f, 270.0f, RingColor, 48);
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 2: Inner ring (category wedges) — solid dark navy fills
	// =================================================================
	for (int32 i = 0; i < NumCats; i++)
	{
		float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
		float GapHalf = 0.8f;
		float StartDeg = i * CatAngle - 90.0f + GapHalf;
		float EndDeg = (i + 1) * CatAngle - 90.0f - GapHalf;

		// Fill: dark navy, turquoise on hover
		FLinearColor FillCol = FMath::Lerp(SegmentFillColor, SegmentHoverFillColor, HoverT * 0.85f);
		DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
			sInnerI, sInnerO, StartDeg, EndDeg, Faded(FillCol));
	}
	LayerId++;

	// =================================================================
	// LAYER 3: Inner ring 1px dividers between wedges
	// =================================================================
	{
		FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();
		for (int32 i = 0; i < NumCats; i++)
		{
			float AngleRad = FMath::DegreesToRadians(i * CatAngle - 90.0f);
			FVector2D Radial(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
			FVector2D Inner = Center + Radial * sInnerI;
			FVector2D Outer = Center + Radial * sInnerO;
			TArray<FVector2D> Pts;
			Pts.Add(Inner); Pts.Add(Outer);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PG,
				Pts, ESlateDrawEffect::None, Faded(DividerColor), true, 1.0f);
		}
	}
	LayerId++;

	// =================================================================
	// LAYER 4: Category names — clean white text centered in each wedge
	// =================================================================
	{
		float TextR = (sInnerI + sInnerO) / 2.0f;
		for (int32 i = 0; i < NumCats; i++)
		{
			float MidAngle = FMath::DegreesToRadians((i + 0.5f) * CatAngle - 90.0f);
			FVector2D LabelCenter = Center + FVector2D(FMath::Cos(MidAngle), FMath::Sin(MidAngle)) * TextR;

			float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
			// Hovered: dark text on turquoise; unhovered: white on dark
			FLinearColor Tint = (HoverT > 0.5f)
				? FMath::Lerp(TextWhite, FLinearColor(0.05f, 0.10f, 0.15f, 1.0f), HoverT)
				: FMath::Lerp(TextDimmed, TextWhite, HoverT * 2.0f);
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
	// LAYER 5: Outer ring — piece cards in an arc
	// =================================================================
	if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
	{
		const FCategoryInfo& Cat = Categories[HighlightedCategory];
		int32 NumPieces = Cat.PieceIndices.Num();
		if (NumPieces > 0)
		{
			float CatStartDeg = HighlightedCategory * CatAngle - 90.0f;
			float PieceAngle = CatAngle / NumPieces;

			// Card dimensions
			float CardRadialDepth = (sOuterO - sOuterI);
			float CardGapDeg = 1.2f; // degrees between cards

			for (int32 p = 0; p < NumPieces; p++)
			{
				int32 GlobalIdx = Cat.PieceIndices[p];
				bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
				float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;

				float StartDeg = CatStartDeg + p * PieceAngle + CardGapDeg;
				float EndDeg = CatStartDeg + (p + 1) * PieceAngle - CardGapDeg;

				// Card fill: dark semi-transparent, turquoise border on hover
				FLinearColor CardFill;
				if (!bAvailable)
				{
					CardFill = SegmentUnavailableColor;
				}
				else
				{
					CardFill = SegmentFillColor;
					// Subtle turquoise tint on hover
					CardFill = FMath::Lerp(CardFill,
						FLinearColor(0.05f, 0.18f, 0.22f, 0.92f), HoverT * 0.6f);
				}
				DrawFilledArc(OutDrawElements, LayerId, AllottedGeometry, Center,
					sOuterI, sOuterO, StartDeg, EndDeg, Faded(CardFill));

				// Turquoise border on hover
				if (HoverT > 0.01f && bAvailable)
				{
					FLinearColor BorderCol = BorderAccentColor;
					BorderCol.A = BorderAccentColor.A * HoverT * FadeAlpha;
					DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
						sOuterO - 1.0f, StartDeg, EndDeg, BorderCol, 2.0f);
					DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
						sOuterI + 1.0f, StartDeg, EndDeg, BorderCol, 1.5f);

					// Side borders
					FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();
					float Rad1 = FMath::DegreesToRadians(StartDeg);
					float Rad2 = FMath::DegreesToRadians(EndDeg);
					for (float Rad : {Rad1, Rad2})
					{
						FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad));
						TArray<FVector2D> Pts;
						Pts.Add(Center + Dir * sOuterI);
						Pts.Add(Center + Dir * sOuterO);
						FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PG,
							Pts, ESlateDrawEffect::None, BorderCol, true, 1.5f);
					}
				}
			}
			LayerId++;

			// Piece icons — centered in each card
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
							IconTint = FLinearColor(0.25f, 0.28f, 0.32f, FadeAlpha * 0.5f);
						else
							IconTint = FMath::Lerp(
								FLinearColor(0.85f, 0.92f, 0.98f, FadeAlpha),
								FLinearColor(1.0f, 1.0f, 1.0f, FadeAlpha),
								HoverT);

						FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
							IconGeo.ToPaintGeometry(), &IconBrushes[GlobalIdx],
							ESlateDrawEffect::None, IconTint);
					}
				}
			}
			LayerId++;

			// Piece names — white text below icon
			{
				float NameR = sOuterI + CardRadialDepth * 0.18f;
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

			// Piece dimensions — turquoise text below name
			{
				float DimR = sOuterI + CardRadialDepth * 0.08f;
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
					else Tint = SubtitleColor; // #00bcd4
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
	// LAYER 6: Center hub — dark circle with turquoise border
	// =================================================================
	DrawCircleFill(OutDrawElements, LayerId, AllottedGeometry, Center,
		sHub, Faded(CenterFillColor));

	DrawArcOutline(OutDrawElements, LayerId, AllottedGeometry, Center,
		sHub, -90.0f, 270.0f, Faded(CenterBorderColor), 2.0f);
	LayerId++;

	// =================================================================
	// LAYER 7: Center hub content — selected piece info
	// =================================================================
	{
		int32 GlobalIdx = GetHighlightedIndex();
		bool bHasSelection = (GlobalIdx >= 0 && AllPieceInfos.IsValidIndex(GlobalIdx));

		if (bHasSelection)
		{
			const FPieceTypeInfo& Info = AllPieceInfos[GlobalIdx];

			// Icon
			float ScaledCenterIcon = CenterIconSize * Scale;
			if (IconBrushes.IsValidIndex(GlobalIdx) && IconBrushes[GlobalIdx].GetResourceObject())
			{
				FVector2D TexSize(ScaledCenterIcon, ScaledCenterIcon);
				FVector2D TexPos = Center - FVector2D(ScaledCenterIcon / 2.0f, ScaledCenterIcon / 2.0f + 10.0f * Scale);
				FGeometry IconGeo = AllottedGeometry.MakeChild(TexSize, FSlateLayoutTransform(TexPos));
				FLinearColor CenterIconTint = Info.bAvailable
					? FLinearColor(0.85f, 0.95f, 1.00f, FadeAlpha)
					: FLinearColor(0.30f, 0.35f, 0.40f, FadeAlpha * 0.5f);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					IconGeo.ToPaintGeometry(), &IconBrushes[GlobalIdx],
					ESlateDrawEffect::None, CenterIconTint);
			}

			// Name
			FLinearColor NameColor = Info.bAvailable ? TextWhite : TextUnavailable;
			FVector2D NameSize = FontMeasure->Measure(Info.DisplayName, CenterNameFont);
			FVector2D NamePos = Center + FVector2D(-NameSize.X / 2.0f, ScaledCenterIcon / 2.0f - 6.0f * Scale);
			FGeometry NameGeo = AllottedGeometry.MakeChild(NameSize, FSlateLayoutTransform(NamePos));
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, NameGeo.ToPaintGeometry(),
				Info.DisplayName, CenterNameFont, ESlateDrawEffect::None, Faded(NameColor));

			// Dimensions (turquoise)
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
				FSlateFontInfo LockFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Clamp(FMath::RoundToInt(11.0f * Scale), 9, 14));
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
			FSlateFontInfo PromptFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::Clamp(FMath::RoundToInt(14.0f * Scale), 11, 18));
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

	// Use fewer rings for a cleaner fill (no visible ring artifacts)
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
// DrawArcOutline
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
// DrawCircleFill
// ---------------------------------------------------------------------------
void URadialPieceMenu::DrawCircleFill(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const
{
	DrawFilledArc(OutDrawElements, LayerId, Geo, Center, 0.0f, Radius,
		-90.0f, 270.0f, Color, 48);
}
