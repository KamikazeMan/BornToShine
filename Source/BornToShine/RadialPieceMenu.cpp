// RadialPieceMenu.cpp
// Born To Shine - In-Place Radial Piece Selection Menu

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
	CurrentView = ERadialMenuView::Main;
	ActiveCategory = -1;
	HighlightedCategory = -1;
	PrevHighlightedCategory = -1;
	HighlightedPieceSlot = -1;
	PrevHighlightedPieceSlot = -1;
	SelectedPieceSlot = -1;

	InnerRadius  = 50.0f;
	OuterRadius  = 155.0f;
	HubRadius    = 46.0f;
	DeadZone     = 30.0f;
	WedgeGapDeg  = 1.0f;
	IconSize     = 40.0f;

	FadeAlpha = 0.0f;
	FadeSpeed = 6.5f;
	GlowPulseTime = 0.0f;
	ViewTransition = 0.0f;
	ViewTransitionSpeed = 8.0f;

	DarkBg          = FLinearColor(0.024f, 0.031f, 0.063f, 1.0f);
	DarkWedge       = FLinearColor(0.059f, 0.082f, 0.125f, 0.90f);
	DarkHover       = FLinearColor(0.078f, 0.118f, 0.176f, 0.95f);
	ActiveWedgeFill = FLinearColor(0.047f, 0.102f, 0.165f, 0.90f);

	Cyan    = FLinearColor(0.0f, 0.898f, 1.0f, 1.0f);
	CyanDim = FLinearColor(0.0f, 0.898f, 1.0f, 0.27f);
	CyanMid = FLinearColor(0.0f, 0.898f, 1.0f, 0.53f);

	TextWhite       = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextDimmed      = FLinearColor(0.290f, 0.396f, 0.459f, 1.0f);
	TextUnavailable = FLinearColor(0.165f, 0.243f, 0.290f, 1.0f);

	CategoryColorTable.SetNum(4);
	CategoryColorTable[0].Accent   = FLinearColor(1.0f, 0.416f, 0.259f, 1.0f);
	CategoryColorTable[0].WedgeDim = FLinearColor(1.0f, 0.416f, 0.259f, 0.08f);
	CategoryColorTable[0].WedgeLit = FLinearColor(1.0f, 0.416f, 0.259f, 0.30f);

	CategoryColorTable[1].Accent   = FLinearColor(0.239f, 0.863f, 0.518f, 1.0f);
	CategoryColorTable[1].WedgeDim = FLinearColor(0.239f, 0.863f, 0.518f, 0.08f);
	CategoryColorTable[1].WedgeLit = FLinearColor(0.239f, 0.863f, 0.518f, 0.30f);

	CategoryColorTable[2].Accent   = FLinearColor(0.369f, 0.612f, 1.0f, 1.0f);
	CategoryColorTable[2].WedgeDim = FLinearColor(0.369f, 0.612f, 1.0f, 0.08f);
	CategoryColorTable[2].WedgeLit = FLinearColor(0.369f, 0.612f, 1.0f, 0.30f);

	CategoryColorTable[3].Accent   = FLinearColor(0.941f, 0.784f, 0.314f, 1.0f);
	CategoryColorTable[3].WedgeDim = FLinearColor(0.941f, 0.784f, 0.314f, 0.08f);
	CategoryColorTable[3].WedgeLit = FLinearColor(0.941f, 0.784f, 0.314f, 0.30f);
}

void URadialPieceMenu::BuildCategories()
{
	Categories.Empty();

	FCategoryInfo Foundation;
	Foundation.Name = TEXT("FOUNDATION");
	Foundation.Icon = TEXT("FND");

	FCategoryInfo Floor;
	Floor.Name = TEXT("FLOOR");
	Floor.Icon = TEXT("FLR");

	FCategoryInfo Walls;
	Walls.Name = TEXT("WALLS");
	Walls.Icon = TEXT("WLL");

	FCategoryInfo Roof;
	Roof.Name = TEXT("ROOF");
	Roof.Icon = TEXT("ROF");

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

	UE_LOG(LogTemp, Log, TEXT("RadialPieceMenu: Init %d pieces, %d categories, current=%d"),
		AllPieceInfos.Num(), Categories.Num(), CurrentIndex);
}

int32 URadialPieceMenu::GetHighlightedIndex() const
{
	if (ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		const TArray<int32>& Pieces = Categories[ActiveCategory].PieceIndices;
		int32 Slot = (SelectedPieceSlot >= 0) ? SelectedPieceSlot : HighlightedPieceSlot;
		if (Slot >= 0 && Pieces.IsValidIndex(Slot))
			return Pieces[Slot];
		if (Pieces.Num() > 0)
			return Pieces[0];
	}
	return -1;
}

void URadialPieceMenu::PlaySoundOpen()   { }
void URadialPieceMenu::PlaySoundClose()  { }
void URadialPieceMenu::PlaySoundHover()  { }
void URadialPieceMenu::PlaySoundSelect() { }
void URadialPieceMenu::PlaySoundBack()   { }

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

FReply URadialPieceMenu::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!InMouseEvent.GetEffectingButton().IsValid()) return FReply::Unhandled();
	if (Categories.Num() == 0) return FReply::Unhandled();

	FVector2D LocalSize = InGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;
	float Scale = (LocalSize.Y * 0.65f) / (OuterRadius * 2.0f);

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

void URadialPieceMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Categories.Num() == 0) return;

	FadeAlpha = FMath::FInterpTo(FadeAlpha, 1.0f, InDeltaTime, FadeSpeed);
	GlowPulseTime += InDeltaTime;

	float TargetTransition = (CurrentView == ERadialMenuView::Sub) ? 1.0f : 0.0f;
	ViewTransition = FMath::FInterpTo(ViewTransition, TargetTransition, InDeltaTime, ViewTransitionSpeed);

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
		int32 NumCats = Categories.Num();
		if (Dist >= sInner && Dist <= sOuter)
		{
			float CatAngle = 360.0f / NumCats;
			HighlightedCategory = FMath::Clamp((int32)(AngleDeg / CatAngle), 0, NumCats - 1);
		}
		else if (Dist >= sDead)
		{
			HighlightedCategory = -1;
		}

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
		int32 NumPieces = Categories[ActiveCategory].PieceIndices.Num();
		if (Dist >= sInner && Dist <= sOuter && NumPieces > 0)
		{
			float PieceAngle = 360.0f / NumPieces;
			HighlightedPieceSlot = FMath::Clamp((int32)(AngleDeg / PieceAngle), 0, NumPieces - 1);
		}
		else if (Dist >= sDead)
		{
			HighlightedPieceSlot = -1;
		}

		if (PieceHoverScales.Num() != NumPieces)
			PieceHoverScales.Init(0.0f, NumPieces);
		for (int32 i = 0; i < NumPieces; i++)
		{
			float Target = (i == HighlightedPieceSlot || i == SelectedPieceSlot) ? 1.0f : 0.0f;
			PieceHoverScales[i] = FMath::FInterpTo(PieceHoverScales[i], Target, InDeltaTime, 12.0f);
		}
	}

	if (HighlightedCategory != PrevHighlightedCategory ||
		HighlightedPieceSlot != PrevHighlightedPieceSlot)
	{
		PlaySoundHover();
		PrevHighlightedCategory = HighlightedCategory;
		PrevHighlightedPieceSlot = HighlightedPieceSlot;
	}
}

int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (Categories.Num() == 0 || FadeAlpha < 0.001f) return LayerId;

	FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	FVector2D Center = LocalSize / 2.0f;
	float Scale = (LocalSize.Y * 0.65f) / (OuterRadius * 2.0f);

	LayerId = PaintRingDecorations(OutDrawElements, LayerId, AllottedGeometry, Center, Scale);

	if (ViewTransition < 0.99f)
		LayerId = PaintMainView(OutDrawElements, LayerId, AllottedGeometry, Center, Scale);
	if (ViewTransition > 0.01f)
		LayerId = PaintSubView(OutDrawElements, LayerId, AllottedGeometry, Center, Scale);

	LayerId = PaintCenterHub(OutDrawElements, LayerId, AllottedGeometry, Center, Scale);

	return LayerId;
}

int32 URadialPieceMenu::PaintRingDecorations(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Scale) const
{
	float sOuter = OuterRadius * Scale;
	float Pulse = 0.85f + 0.15f * FMath::Sin(GlowPulseTime * 1.8f);

	DrawArcOutline(Out, LayerId, Geo, Center, sOuter + 18.0f * Scale, -90.0f, 270.0f,
		WithAlpha(Cyan, 0.05f * Pulse * FadeAlpha), 0.5f);
	DrawArcOutline(Out, LayerId, Geo, Center, sOuter + 10.0f * Scale, -90.0f, 270.0f,
		WithAlpha(Cyan, 0.08f * Pulse * FadeAlpha), 0.8f);
	DrawArcOutline(Out, LayerId, Geo, Center, sOuter + 3.0f * Scale, -90.0f, 270.0f,
		WithAlpha(Cyan, 0.12f * Pulse * FadeAlpha), 1.0f);

	for (int32 t = 0; t < 72; t++)
	{
		float AngDeg = t * 5.0f - 90.0f;
		float AngRad = FMath::DegreesToRadians(AngDeg);
		FVector2D Dir(FMath::Cos(AngRad), FMath::Sin(AngRad));
		bool bMajor = (t % 4 == 0);
		float TickStart = sOuter + 4.0f * Scale;
		float TickEnd   = sOuter + (bMajor ? 14.0f : 8.0f) * Scale;
		DrawLine(Out, LayerId, Geo,
			Center + Dir * TickStart, Center + Dir * TickEnd,
			WithAlpha(Cyan, (bMajor ? 0.3f : 0.12f) * Pulse * FadeAlpha),
			bMajor ? 1.5f : 0.5f);
	}

	return LayerId + 1;
}

int32 URadialPieceMenu::PaintMainView(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Scale) const
{
	int32 NumCats = Categories.Num();
	float CatAngle = 360.0f / NumCats;
	float GapHalf = WedgeGapDeg / 2.0f;
	float Pulse = 0.85f + 0.15f * FMath::Sin(GlowPulseTime * 1.8f);
	float sInner = InnerRadius * Scale;
	float sOuter = OuterRadius * Scale;
	float MainAlpha = 1.0f - ViewTransition;

	int32 NameFontSz = FMath::Clamp(FMath::RoundToInt(12.0f * Scale), 8, 18);
	int32 IconFontSz = FMath::Clamp(FMath::RoundToInt(10.0f * Scale), 7, 14);
	FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", NameFontSz);
	FSlateFontInfo IconFont = FCoreStyle::GetDefaultFontStyle("Regular", IconFontSz);

	for (int32 i = 0; i < NumCats; i++)
	{
		float StartDeg = i * CatAngle - 90.0f + GapHalf;
		float EndDeg   = (i + 1) * CatAngle - 90.0f - GapHalf;
		float MidDeg   = (StartDeg + EndDeg) / 2.0f;
		float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
		FLinearColor AccentCol = CategoryColorTable.IsValidIndex(i) ? CategoryColorTable[i].Accent : Cyan;

		FLinearColor FillCol = FMath::Lerp(DarkWedge, ActiveWedgeFill, HoverT);
		FillCol.A *= FadeAlpha * MainAlpha;
		DrawFilledArc(Out, LayerId, Geo, Center, sInner, sOuter, StartDeg, EndDeg, FillCol);

		float GlowI = FMath::Lerp(0.25f, 1.0f, HoverT);

		DrawArcOutline(Out, LayerId, Geo, Center, sOuter, StartDeg, EndDeg,
			WithAlpha(Cyan, GlowI * 0.5f * Pulse * FadeAlpha * MainAlpha),
			2.5f * Scale * HoverT + 1.0f * Scale);
		DrawArcOutline(Out, LayerId, Geo, Center, sOuter, StartDeg, EndDeg,
			WithAlpha(Cyan, GlowI * Pulse * FadeAlpha * MainAlpha),
			FMath::Lerp(0.8f, 1.5f, HoverT));

		DrawArcOutline(Out, LayerId, Geo, Center, sInner, StartDeg, EndDeg,
			WithAlpha(Cyan, GlowI * 0.8f * Pulse * FadeAlpha * MainAlpha),
			1.0f * Scale * HoverT + 0.5f * Scale);

		for (float Deg : {StartDeg, EndDeg})
		{
			float Rad = FMath::DegreesToRadians(Deg);
			FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad));
			DrawLine(Out, LayerId, Geo, Center + Dir * sInner, Center + Dir * sOuter,
				WithAlpha(Cyan, GlowI * 0.7f * Pulse * FadeAlpha * MainAlpha), 0.8f);
		}

		float IconR = sInner + (sOuter - sInner) * 0.38f;
		FLinearColor IconTint = FMath::Lerp(TextDimmed, AccentCol, HoverT);
		IconTint.A *= FadeAlpha * MainAlpha;
		DrawTextAtAngle(Out, LayerId, Geo, Center, IconR, MidDeg,
			Categories[i].Icon, IconFont, IconTint);

		float NameR = sInner + (sOuter - sInner) * 0.68f;
		FLinearColor NameCol = FMath::Lerp(TextDimmed, Cyan, HoverT);
		NameCol.A *= FadeAlpha * MainAlpha;
		DrawTextAtAngle(Out, LayerId, Geo, Center, NameR, MidDeg,
			Categories[i].Name, NameFont, NameCol);
	}

	return LayerId + 1;
}

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

	FLinearColor AccentCol = CategoryColorTable.IsValidIndex(ActiveCategory)
		? CategoryColorTable[ActiveCategory].Accent : Cyan;
	FLinearColor WedgeDimCol = CategoryColorTable.IsValidIndex(ActiveCategory)
		? CategoryColorTable[ActiveCategory].WedgeDim : WithAlpha(Cyan, 0.08f);
	FLinearColor WedgeLitCol = CategoryColorTable.IsValidIndex(ActiveCategory)
		? CategoryColorTable[ActiveCategory].WedgeLit : WithAlpha(Cyan, 0.30f);

	int32 PieceNameSz = FMath::Clamp(FMath::RoundToInt(10.0f * Scale), 7, 15);
	int32 PieceDimSz  = FMath::Clamp(FMath::RoundToInt(8.0f * Scale), 6, 12);
	FSlateFontInfo PieceNameFont = FCoreStyle::GetDefaultFontStyle("Bold", PieceNameSz);
	FSlateFontInfo PieceDimFont  = FCoreStyle::GetDefaultFontStyle("Regular", PieceDimSz);

	for (int32 p = 0; p < NumPieces; p++)
	{
		int32 GlobalIdx = Cat.PieceIndices[p];
		bool bAvailable = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].bAvailable : true;
		float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;
		bool bLit = (p == HighlightedPieceSlot || p == SelectedPieceSlot);

		float StartDeg = p * PieceAngle - 90.0f + GapHalf;
		float EndDeg   = (p + 1) * PieceAngle - 90.0f - GapHalf;
		float MidDeg   = (StartDeg + EndDeg) / 2.0f;

		FLinearColor FillCol = bLit
			? FMath::Lerp(WedgeDimCol, WedgeLitCol, HoverT)
			: WedgeDimCol;
		if (!bAvailable) FillCol = WithAlpha(FillCol, FillCol.A * 0.4f);
		FillCol.A *= FadeAlpha * SubAlpha;
		DrawFilledArc(Out, LayerId, Geo, Center, sInner, sOuter, StartDeg, EndDeg, FillCol);

		FLinearColor GlowCol = bAvailable ? AccentCol : TextUnavailable;
		float GlowI = bLit ? 1.0f : (bAvailable ? 0.3f : 0.08f);
		GlowI = FMath::Lerp(GlowI * 0.5f, GlowI, HoverT + (bLit ? 0.5f : 0.0f));

		DrawArcOutline(Out, LayerId, Geo, Center, sOuter, StartDeg, EndDeg,
			WithAlpha(GlowCol, GlowI * 0.5f * Pulse * FadeAlpha * SubAlpha),
			FMath::Lerp(1.0f * Scale, 2.5f * Scale, HoverT));
		DrawArcOutline(Out, LayerId, Geo, Center, sOuter, StartDeg, EndDeg,
			WithAlpha(GlowCol, GlowI * Pulse * FadeAlpha * SubAlpha),
			FMath::Lerp(0.6f, 1.5f, HoverT));
		DrawArcOutline(Out, LayerId, Geo, Center, sInner, StartDeg, EndDeg,
			WithAlpha(GlowCol, GlowI * 0.6f * Pulse * FadeAlpha * SubAlpha),
			FMath::Lerp(0.5f, 1.0f, HoverT));

		for (float Deg : {StartDeg, EndDeg})
		{
			float Rad = FMath::DegreesToRadians(Deg);
			FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad));
			DrawLine(Out, LayerId, Geo, Center + Dir * sInner, Center + Dir * sOuter,
				WithAlpha(GlowCol, GlowI * 0.5f * Pulse * FadeAlpha * SubAlpha), 0.6f);
		}

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

		FString PieceName = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].DisplayName : TEXT("");
		if (!PieceName.IsEmpty())
		{
			float NameR = sInner + (sOuter - sInner) * 0.62f;
			FLinearColor NameTint = bAvailable
				? FMath::Lerp(FLinearColor(0.557f, 0.667f, 0.733f, 1.0f), TextWhite, HoverT)
				: TextUnavailable;
			NameTint.A *= FadeAlpha * SubAlpha;
			DrawTextAtAngle(Out, LayerId, Geo, Center, NameR, MidDeg,
				PieceName, PieceNameFont, NameTint);
		}

		FString Dims = AllPieceInfos.IsValidIndex(GlobalIdx) ? AllPieceInfos[GlobalIdx].Subtitle : TEXT("");
		if (!Dims.IsEmpty())
		{
			float DimR = sInner + (sOuter - sInner) * 0.82f;
			FLinearColor DimTint = bAvailable
				? WithAlpha(AccentCol, FMath::Lerp(0.4f, 0.8f, HoverT))
				: TextUnavailable;
			DimTint.A *= FadeAlpha * SubAlpha;
			DrawTextAtAngle(Out, LayerId, Geo, Center, DimR, MidDeg,
				Dims, PieceDimFont, DimTint);
		}
	}

	return LayerId + 1;
}

int32 URadialPieceMenu::PaintCenterHub(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Scale) const
{
	float sHub = HubRadius * Scale;
	float Pulse = 0.85f + 0.15f * FMath::Sin(GlowPulseTime * 1.8f);
	float CenterPulse = 0.3f + 0.4f * (0.5f + 0.5f * FMath::Sin(GlowPulseTime * 1.05f));
	bool bInSub = (CurrentView == ERadialMenuView::Sub && ActiveCategory >= 0);

	TSharedRef<FSlateFontMeasure> FM =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	FLinearColor GlowRingCol = bInSub && CategoryColorTable.IsValidIndex(ActiveCategory)
		? CategoryColorTable[ActiveCategory].Accent : Cyan;

	DrawArcOutline(Out, LayerId, Geo, Center, sHub + 4.0f * Scale, -90.0f, 270.0f,
		WithAlpha(GlowRingCol, CenterPulse * Pulse * FadeAlpha), 2.0f * Scale);

	DrawCircleFill(Out, LayerId, Geo, Center, sHub, Faded(DarkBg));

	DrawArcOutline(Out, LayerId, Geo, Center, sHub, -90.0f, 270.0f,
		WithAlpha(GlowRingCol, (bInSub ? 0.6f : 0.25f) * Pulse * FadeAlpha), 1.5f);

	float PulseR = sHub + 4.0f * Scale * (0.5f + 0.5f * FMath::Sin(GlowPulseTime * 0.785f));
	DrawArcOutline(Out, LayerId, Geo, Center, PulseR, -90.0f, 270.0f,
		WithAlpha(Cyan, 0.15f * FadeAlpha), 1.0f);

	if (bInSub && Categories.IsValidIndex(ActiveCategory))
	{
		const FCategoryInfo& Cat = Categories[ActiveCategory];
		FLinearColor AccCol = CategoryColorTable.IsValidIndex(ActiveCategory)
			? CategoryColorTable[ActiveCategory].Accent : Cyan;
		int32 NumPieces = Cat.PieceIndices.Num();

		{
			int32 FSz = FMath::Clamp(FMath::RoundToInt(14.0f * Scale), 10, 20);
			FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Bold", FSz);
			FVector2D Sz = FM->Measure(Cat.Icon, F);
			FVector2D Pos = Center - FVector2D(Sz.X / 2.0f, Sz.Y / 2.0f + 14.0f * Scale);
			FGeometry G = Geo.MakeChild(Sz, FSlateLayoutTransform(Pos));
			FSlateDrawElement::MakeText(Out, LayerId, G.ToPaintGeometry(),
				Cat.Icon, F, ESlateDrawEffect::None, Faded(AccCol));
		}

		{
			int32 FSz = FMath::Clamp(FMath::RoundToInt(11.0f * Scale), 8, 16);
			FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Bold", FSz);
			FVector2D Sz = FM->Measure(Cat.Name, F);
			FVector2D Pos = Center + FVector2D(-Sz.X / 2.0f, 2.0f * Scale);
			FGeometry G = Geo.MakeChild(Sz, FSlateLayoutTransform(Pos));
			FSlateDrawElement::MakeText(Out, LayerId, G.ToPaintGeometry(),
				Cat.Name, F, ESlateDrawEffect::None, Faded(TextWhite));
		}

		{
			FString CountText = FString::Printf(TEXT("%d piece%s"), NumPieces, NumPieces != 1 ? TEXT("s") : TEXT(""));
			int32 FSz = FMath::Clamp(FMath::RoundToInt(8.0f * Scale), 6, 11);
			FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Regular", FSz);
			FVector2D Sz = FM->Measure(CountText, F);
			FVector2D Pos = Center + FVector2D(-Sz.X / 2.0f, 15.0f * Scale);
			FGeometry G = Geo.MakeChild(Sz, FSlateLayoutTransform(Pos));
			FSlateDrawElement::MakeText(Out, LayerId, G.ToPaintGeometry(),
				CountText, F, ESlateDrawEffect::None, WithAlpha(AccCol, 0.5f * FadeAlpha));
		}

		{
			FString BackText = TEXT("< BACK");
			int32 FSz = FMath::Clamp(FMath::RoundToInt(7.0f * Scale), 5, 10);
			FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Regular", FSz);
			FVector2D Sz = FM->Measure(BackText, F);
			FVector2D Pos = Center + FVector2D(-Sz.X / 2.0f, 25.0f * Scale);
			FGeometry G = Geo.MakeChild(Sz, FSlateLayoutTransform(Pos));
			FSlateDrawElement::MakeText(Out, LayerId, G.ToPaintGeometry(),
				BackText, F, ESlateDrawEffect::None, WithAlpha(AccCol, 0.45f * FadeAlpha));
		}
	}
	else
	{
		float GapR = 8.0f * Scale;
		float ArmR = 20.0f * Scale;
		FLinearColor CrossCol = WithAlpha(Cyan, 0.4f * Pulse * FadeAlpha);

		DrawLine(Out, LayerId, Geo, Center - FVector2D(ArmR, 0), Center - FVector2D(GapR, 0), CrossCol, 1.0f);
		DrawLine(Out, LayerId, Geo, Center + FVector2D(GapR, 0), Center + FVector2D(ArmR, 0), CrossCol, 1.0f);
		DrawLine(Out, LayerId, Geo, Center - FVector2D(0, ArmR), Center - FVector2D(0, GapR), CrossCol, 1.0f);
		DrawLine(Out, LayerId, Geo, Center + FVector2D(0, GapR), Center + FVector2D(0, ArmR), CrossCol, 1.0f);

		{
			int32 FSz = FMath::Clamp(FMath::RoundToInt(13.0f * Scale), 9, 18);
			FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Bold", FSz);
			FString T = TEXT("BUILD");
			FVector2D Sz = FM->Measure(T, F);
			FVector2D Pos = Center - FVector2D(Sz.X / 2.0f, Sz.Y / 2.0f + 4.0f * Scale);
			FGeometry G = Geo.MakeChild(Sz, FSlateLayoutTransform(Pos));
			FSlateDrawElement::MakeText(Out, LayerId, G.ToPaintGeometry(),
				T, F, ESlateDrawEffect::None, Faded(Cyan));
		}

		{
			int32 FSz = FMath::Clamp(FMath::RoundToInt(8.0f * Scale), 6, 11);
			FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Regular", FSz);
			FString T = TEXT("select category");
			FVector2D Sz = FM->Measure(T, F);
			FVector2D Pos = Center + FVector2D(-Sz.X / 2.0f, 9.0f * Scale);
			FGeometry G = Geo.MakeChild(Sz, FSlateLayoutTransform(Pos));
			FSlateDrawElement::MakeText(Out, LayerId, G.ToPaintGeometry(),
				T, F, ESlateDrawEffect::None, WithAlpha(Cyan, 0.35f * FadeAlpha));
		}
	}

	return LayerId + 1;
}

void URadialPieceMenu::DrawTextAtAngle(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius, float AngleDeg,
	const FString& Text, const FSlateFontInfo& Font, FLinearColor Color) const
{
	if (Text.IsEmpty() || Color.A < 0.001f) return;

	TSharedRef<FSlateFontMeasure> FM =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	float AngleRad = FMath::DegreesToRadians(AngleDeg);
	FVector2D Pos = Center + FVector2D(FMath::Cos(AngleRad), FMath::Sin(AngleRad)) * Radius;

	FVector2D TextSize = FM->Measure(Text, Font);
	FVector2D TopLeft = Pos - TextSize / 2.0f;

	FGeometry TextGeo = Geo.MakeChild(TextSize, FSlateLayoutTransform(TopLeft));
	FSlateDrawElement::MakeText(Out, LayerId, TextGeo.ToPaintGeometry(),
		Text, Font, ESlateDrawEffect::None, Color);
}

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

void URadialPieceMenu::DrawCircleFill(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const
{
	DrawFilledArc(Out, LayerId, Geo, Center, 0.0f, Radius, -90.0f, 270.0f, Color, 48);
}

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
