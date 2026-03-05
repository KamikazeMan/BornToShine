// RadialPieceMenu.cpp - Born To Shine
// Clean rewrite matching the React prototype screenshots exactly.
// NO tick marks. NO pulsing outer rings. NO old radial wheel elements.
// Dark colored wedge fills, thin border lines, category text, icons.
#include "RadialPieceMenu.h"
#include "ConstructionPhaseManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Engine/Texture2D.h"
// Helper: degrees to XY position
FVector2D URadialPieceMenu::PolarToCart(FVector2D Center, float Radius, float Deg) const
{
	float Rad = FMath::DegreesToRadians(Deg - 90.0f); // -90 so 0° = top
	return Center + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * Radius;
}
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
	InnerRadius = 60.0f;
	OuterRadius = 170.0f;
	HubRadius   = 55.0f;
	DeadZone    = 40.0f;
	WedgeGapDeg = 1.5f;
	IconSize    = 40.0f;
	FadeAlpha = 0.0f;
	FadeSpeed = 8.0f;
	GlowPulseTime = 0.0f;
	ViewTransition = 0.0f;
	ViewTransitionSpeed = 10.0f;
	// Category styles matching the React prototype colors
	CatStyles.SetNum(4);
	// Framing - warm red/orange
	CatStyles[0].DarkFill  = FLinearColor(0.12f, 0.04f, 0.04f, 0.92f);
	CatStyles[0].LitFill   = FLinearColor(0.20f, 0.08f, 0.06f, 0.95f);
	CatStyles[0].Accent    = FLinearColor(1.0f, 0.42f, 0.26f, 1.0f);
	CatStyles[0].TextColor = FLinearColor(1.0f, 0.42f, 0.26f, 1.0f);
	// Roofing - green
	CatStyles[1].DarkFill  = FLinearColor(0.03f, 0.10f, 0.06f, 0.92f);
	CatStyles[1].LitFill   = FLinearColor(0.06f, 0.18f, 0.10f, 0.95f);
	CatStyles[1].Accent    = FLinearColor(0.24f, 0.86f, 0.52f, 1.0f);
	CatStyles[1].TextColor = FLinearColor(0.24f, 0.86f, 0.52f, 1.0f);
	// Sheathing - blue
	CatStyles[2].DarkFill  = FLinearColor(0.04f, 0.05f, 0.12f, 0.92f);
	CatStyles[2].LitFill   = FLinearColor(0.06f, 0.08f, 0.20f, 0.95f);
	CatStyles[2].Accent    = FLinearColor(0.37f, 0.61f, 1.0f, 1.0f);
	CatStyles[2].TextColor = FLinearColor(0.37f, 0.61f, 1.0f, 1.0f);
	// Utilities - gold/yellow
	CatStyles[3].DarkFill  = FLinearColor(0.10f, 0.08f, 0.03f, 0.92f);
	CatStyles[3].LitFill   = FLinearColor(0.18f, 0.14f, 0.04f, 0.95f);
	CatStyles[3].Accent    = FLinearColor(0.94f, 0.78f, 0.31f, 1.0f);
	CatStyles[3].TextColor = FLinearColor(0.94f, 0.78f, 0.31f, 1.0f);
	// Shared colors
	HubBg       = FLinearColor(0.02f, 0.03f, 0.06f, 0.95f);
	HubBorder   = FLinearColor(0.15f, 0.20f, 0.30f, 0.6f);
	TextWhite   = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);
	TextDim     = FLinearColor(0.4f, 0.45f, 0.5f, 1.0f);
	DividerColor = FLinearColor(0.2f, 0.25f, 0.3f, 0.5f);
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
}
// ============================================================================
void URadialPieceMenu::BuildCategories()
{
	Categories.Empty();
	FCategoryInfo Cat0; Cat0.Name = TEXT("FRAMING");    Cat0.Icon = TEXT("F");
	FCategoryInfo Cat1; Cat1.Name = TEXT("ROOFING");    Cat1.Icon = TEXT("R");
	FCategoryInfo Cat2; Cat2.Name = TEXT("SHEATHING");  Cat2.Icon = TEXT("S");
	FCategoryInfo Cat3; Cat3.Name = TEXT("UTILITIES");  Cat3.Icon = TEXT("U");
	for (int32 i = 0; i < AllPieceInfos.Num(); i++)
	{
		EPieceType PT = AllPieceInfos[i].PieceType;
		switch (PT)
		{
		case EPieceType::Foundation:
		case EPieceType::WallStud:
		case EPieceType::WallPlate:
		case EPieceType::CornerPost:
		case EPieceType::TopPlate:
		case EPieceType::DoubleTopPlate:
		case EPieceType::DoorFrame:
		case EPieceType::WindowFrame:
		case EPieceType::Header:
			Cat0.PieceIndices.Add(i); break;
		case EPieceType::RidgePost:
		case EPieceType::RidgeBoard:
		case EPieceType::Rafter:
		case EPieceType::FasciaBoard:
			Cat1.PieceIndices.Add(i); break;
		case EPieceType::RimBoard:
		case EPieceType::FloorJoist:
		case EPieceType::Plywood:
			Cat2.PieceIndices.Add(i); break;
		default:
			Cat3.PieceIndices.Add(i); break;
		}
	}
	if (Cat0.PieceIndices.Num() > 0) Categories.Add(Cat0);
	if (Cat1.PieceIndices.Num() > 0) Categories.Add(Cat1);
	if (Cat2.PieceIndices.Num() > 0) Categories.Add(Cat2);
	if (Cat3.PieceIndices.Num() > 0) Categories.Add(Cat3);
}
int32 URadialPieceMenu::FindCategoryForPieceIndex(int32 PieceIndex) const
{
	for (int32 c = 0; c < Categories.Num(); c++)
		if (Categories[c].PieceIndices.Contains(PieceIndex))
			return c;
	return -1;
}
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
	UE_LOG(LogTemp, Log, TEXT("RadialPieceMenu: Init %d pieces, %d categories"),
		AllPieceInfos.Num(), Categories.Num());
}
int32 URadialPieceMenu::GetHighlightedIndex() const
{
	if (ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		const TArray<int32>& P = Categories[ActiveCategory].PieceIndices;
		int32 S = (SelectedPieceSlot >= 0) ? SelectedPieceSlot : HighlightedPieceSlot;
		if (S >= 0 && P.IsValidIndex(S)) return P[S];
		if (P.Num() > 0) return P[0];
	}
	return -1;
}
// ============================================================================
// INPUT
// ============================================================================
FReply URadialPieceMenu::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Categories.Num() == 0) return FReply::Unhandled();
	FVector2D Size = InGeometry.GetLocalSize();
	FVector2D Center = Size / 2.0f;
	float Scale = (Size.Y * 0.55f) / (OuterRadius * 2.0f);
	FVector2D Mouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	float DX = Mouse.X - Center.X;
	float DY = Mouse.Y - Center.Y;
	float Dist = FMath::Sqrt(DX * DX + DY * DY);
	float sI = InnerRadius * Scale;
	float sO = OuterRadius * Scale;
	float sH = HubRadius * Scale;
	// Angle from top, clockwise
	float Ang = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
	if (Ang < 0.0f) Ang += 360.0f;
	UE_LOG(LogTemp, Log, TEXT("RadialMenu Click: Dist=%.0f Inner=%.0f Outer=%.0f Hub=%.0f Ang=%.0f View=%d"),
		Dist, sI, sO, sH, Ang, (int32)CurrentView);
	if (CurrentView == ERadialMenuView::Main)
	{
		if (Dist >= sI && Dist <= sO)
		{
			int32 N = Categories.Num();
			int32 Clicked = FMath::Clamp((int32)(Ang / (360.0f / N)), 0, N - 1);
			ActiveCategory = Clicked;
			HighlightedPieceSlot = -1;
			SelectedPieceSlot = -1;
			PieceHoverScales.Empty();
			CurrentView = ERadialMenuView::Sub;
			UE_LOG(LogTemp, Log, TEXT("RadialMenu: Entered category %d: %s"), Clicked, *Categories[Clicked].Name);
			return FReply::Handled();
		}
	}
	else
	{
		// Center hub = back
		if (Dist <= sH + 8.0f * Scale)
		{
			CurrentView = ERadialMenuView::Main;
			ActiveCategory = -1;
			HighlightedPieceSlot = -1;
			SelectedPieceSlot = -1;
			PieceHoverScales.Empty();
			UE_LOG(LogTemp, Log, TEXT("RadialMenu: Back to main"));
			return FReply::Handled();
		}
		// Piece wedge
		if (Dist >= sI && Dist <= sO && ActiveCategory >= 0)
		{
			int32 N = Categories[ActiveCategory].PieceIndices.Num();
			if (N > 0)
			{
				int32 Clicked = FMath::Clamp((int32)(Ang / (360.0f / N)), 0, N - 1);
				SelectedPieceSlot = Clicked;
				HighlightedPieceSlot = Clicked;
				UE_LOG(LogTemp, Log, TEXT("RadialMenu: Selected piece slot %d"), Clicked);
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}
// ============================================================================
// TICK
// ============================================================================
void URadialPieceMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Categories.Num() == 0) return;
	FadeAlpha = FMath::FInterpTo(FadeAlpha, 1.0f, InDeltaTime, FadeSpeed);
	GlowPulseTime += InDeltaTime;
	float Target = (CurrentView == ERadialMenuView::Sub) ? 1.0f : 0.0f;
	ViewTransition = FMath::FInterpTo(ViewTransition, Target, InDeltaTime, ViewTransitionSpeed);
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	float MX, MY;
	PC->GetMousePosition(MX, MY);
	FVector2D VP;
	if (GEngine && GEngine->GameViewport)
		GEngine->GameViewport->GetViewportSize(VP);
	FVector2D Center = VP / 2.0f;
	float Scale = (VP.Y * 0.55f) / (OuterRadius * 2.0f);
	float sI = InnerRadius * Scale;
	float sO = OuterRadius * Scale;
	float DX = MX - Center.X;
	float DY = MY - Center.Y;
	float Dist = FMath::Sqrt(DX * DX + DY * DY);
	float Ang = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
	if (Ang < 0.0f) Ang += 360.0f;
	if (CurrentView == ERadialMenuView::Main)
	{
		int32 N = Categories.Num();
		if (Dist >= sI && Dist <= sO && N > 0)
			HighlightedCategory = FMath::Clamp((int32)(Ang / (360.0f / N)), 0, N - 1);
		else
			HighlightedCategory = -1;
		if (CategoryHoverScales.Num() != N) CategoryHoverScales.Init(0.0f, N);
		for (int32 i = 0; i < N; i++)
			CategoryHoverScales[i] = FMath::FInterpTo(CategoryHoverScales[i],
				(i == HighlightedCategory) ? 1.0f : 0.0f, InDeltaTime, 12.0f);
	}
	else if (ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		int32 N = Categories[ActiveCategory].PieceIndices.Num();
		if (Dist >= sI && Dist <= sO && N > 0)
			HighlightedPieceSlot = FMath::Clamp((int32)(Ang / (360.0f / N)), 0, N - 1);
		else
			HighlightedPieceSlot = -1;
		if (PieceHoverScales.Num() != N) PieceHoverScales.Init(0.0f, N);
		for (int32 i = 0; i < N; i++)
			PieceHoverScales[i] = FMath::FInterpTo(PieceHoverScales[i],
				(i == HighlightedPieceSlot || i == SelectedPieceSlot) ? 1.0f : 0.0f, InDeltaTime, 12.0f);
	}
	if (HighlightedCategory != PrevHighlightedCategory || HighlightedPieceSlot != PrevHighlightedPieceSlot)
	{
		PrevHighlightedCategory = HighlightedCategory;
		PrevHighlightedPieceSlot = HighlightedPieceSlot;
	}
}
// ============================================================================
// PAINT
// ============================================================================
int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& Geo,
	const FSlateRect& Cull, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& Style, bool bEnabled) const
{
	if (Categories.Num() == 0 || FadeAlpha < 0.001f) return LayerId;
	FVector2D Size = Geo.GetLocalSize();
	FVector2D Center = Size / 2.0f;
	float Scale = (Size.Y * 0.55f) / (OuterRadius * 2.0f);
	float sI = InnerRadius * Scale;
	float sO = OuterRadius * Scale;
	float sH = HubRadius * Scale;
	int32 NameSz = FMath::Clamp(FMath::RoundToInt(13.0f * Scale), 9, 20);
	int32 SmallSz = FMath::Clamp(FMath::RoundToInt(10.0f * Scale), 7, 15);
	int32 HubNameSz = FMath::Clamp(FMath::RoundToInt(14.0f * Scale), 10, 22);
	int32 HubSmallSz = FMath::Clamp(FMath::RoundToInt(9.0f * Scale), 7, 13);
	int32 HeaderSz = FMath::Clamp(FMath::RoundToInt(10.0f * Scale), 7, 14);
	int32 BigIconSz = FMath::Clamp(FMath::RoundToInt(18.0f * Scale), 12, 26);
	FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", NameSz);
	FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", SmallSz);
	FSlateFontInfo HubNameFont = FCoreStyle::GetDefaultFontStyle("Bold", HubNameSz);
	FSlateFontInfo HubSmallFont = FCoreStyle::GetDefaultFontStyle("Regular", HubSmallSz);
	FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Regular", HeaderSz);
	FSlateFontInfo BigIconFont = FCoreStyle::GetDefaultFontStyle("Bold", BigIconSz);
	bool bMain = ViewTransition < 0.5f;
	// ===== MAIN VIEW: 4 Category wedges =====
	if (bMain)
	{
		int32 N = Categories.Num();
		float Sweep = 360.0f / N;
		// Draw header text
		FVector2D HeaderPos = Center - FVector2D(0.0f, sO + 30.0f * Scale);
		FLinearColor HeaderCol = FLinearColor(0.35f, 0.40f, 0.50f, FadeAlpha * 0.7f);
		DrawTextCentered(Out, LayerId, Geo, HeaderPos, TEXT("SELECT CATEGORY"), HeaderFont, HeaderCol);
		for (int32 i = 0; i < N; i++)
		{
			float Start = i * Sweep;
			float End = Start + Sweep;
			float Gap = WedgeGapDeg / 2.0f;
			float Mid = (Start + End) / 2.0f;
			float HoverT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
			FCatStyle CS = CatStyles.IsValidIndex(i) ? CatStyles[i] : CatStyles[0];
			// Wedge fill
			FLinearColor Fill = FMath::Lerp(CS.DarkFill, CS.LitFill, HoverT);
			Fill.A *= FadeAlpha;
			DrawWedgeFill(Out, LayerId, Geo, Center, sI, sO, Start + Gap, End - Gap, Fill);
			// Thin border on outer edge
			FLinearColor BorderCol = FLinearColor::LerpUsingHSV(DividerColor, CS.Accent, HoverT * 0.5f);
			BorderCol.A *= FadeAlpha;
			DrawArc(Out, LayerId, Geo, Center, sO, Start + Gap, End - Gap, BorderCol, 1.5f);
			// Divider lines
			FVector2D P1 = PolarToCart(Center, sI, Start);
			FVector2D P2 = PolarToCart(Center, sO, Start);
			DrawRadialLine(Out, LayerId, Geo, P1, P2,
				FLinearColor(DividerColor.R, DividerColor.G, DividerColor.B, DividerColor.A * FadeAlpha), 1.0f);
			// Category name
			FVector2D NamePos = PolarToCart(Center, sI + (sO - sI) * 0.72f, Mid);
			FLinearColor NameCol = CS.TextColor;
			NameCol.A *= FadeAlpha;
			DrawTextCentered(Out, LayerId, Geo, NamePos, Categories[i].Name, NameFont, NameCol);
			// Icon (texture) at 38% of band
			float IconR = sI + (sO - sI) * 0.38f;
			FVector2D IconPos = PolarToCart(Center, IconR, Mid);
			float DrawIconSz = IconSize * Scale * 0.65f * (1.0f + 0.08f * HoverT);
			// If we have a texture icon for first piece in category, use it
			if (Categories[i].PieceIndices.Num() > 0)
			{
				int32 FirstPiece = Categories[i].PieceIndices[0];
				if (IconBrushes.IsValidIndex(FirstPiece) && IconBrushes[FirstPiece].GetResourceObject())
				{
					FLinearColor IconTint = FMath::Lerp(
						FLinearColor(0.5f, 0.55f, 0.6f, FadeAlpha),
						FLinearColor(1.0f, 1.0f, 1.0f, FadeAlpha), HoverT);
					DrawIconAt(Out, LayerId, Geo, IconPos, IconBrushes[FirstPiece], DrawIconSz, IconTint);
				}
			}
		}
	}
	// ===== SUB VIEW: Piece wedges =====
	else if (ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		const FCategoryInfo& Cat = Categories[ActiveCategory];
		int32 N = Cat.PieceIndices.Num();
		if (N > 0)
		{
			float Sweep = 360.0f / N;
			FCatStyle CS = CatStyles.IsValidIndex(ActiveCategory) ? CatStyles[ActiveCategory] : CatStyles[0];
			// Header
			FVector2D HeaderPos = Center - FVector2D(0.0f, sO + 30.0f * Scale);
			DrawTextCentered(Out, LayerId, Geo, HeaderPos,
				Cat.Name, HeaderFont, FLinearColor(CS.TextColor.R, CS.TextColor.G, CS.TextColor.B, FadeAlpha * 0.8f));
			for (int32 p = 0; p < N; p++)
			{
				float Start = p * Sweep;
				float End = Start + Sweep;
				float Gap = WedgeGapDeg / 2.0f;
				float Mid = (Start + End) / 2.0f;
				float HoverT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;
				bool bLit = (p == HighlightedPieceSlot || p == SelectedPieceSlot);
				int32 GIdx = Cat.PieceIndices[p];
				// Wedge fill - very dark, tinted by category
				FLinearColor Fill = FMath::Lerp(
					FLinearColor(0.04f, 0.04f, 0.06f, 0.90f),
					FLinearColor(CS.DarkFill.R * 1.5f, CS.DarkFill.G * 1.5f, CS.DarkFill.B * 1.5f, 0.95f),
					HoverT);
				Fill.A *= FadeAlpha;
				DrawWedgeFill(Out, LayerId, Geo, Center, sI, sO, Start + Gap, End - Gap, Fill);
				// Border
				FLinearColor BorderCol = bLit
					? FLinearColor(CS.Accent.R, CS.Accent.G, CS.Accent.B, 0.6f * FadeAlpha)
					: FLinearColor(DividerColor.R, DividerColor.G, DividerColor.B, DividerColor.A * FadeAlpha);
				DrawArc(Out, LayerId, Geo, Center, sO, Start + Gap, End - Gap, BorderCol, bLit ? 2.0f : 1.0f);
				// Divider
				FVector2D P1 = PolarToCart(Center, sI, Start);
				FVector2D P2 = PolarToCart(Center, sO, Start);
				DrawRadialLine(Out, LayerId, Geo, P1, P2,
					FLinearColor(DividerColor.R, DividerColor.G, DividerColor.B, DividerColor.A * FadeAlpha), 0.8f);
				// Icon at 38%
				float IconR = sI + (sO - sI) * 0.35f;
				FVector2D IconPos = PolarToCart(Center, IconR, Mid);
				float DrawSz = IconSize * Scale * 0.5f * (1.0f + 0.06f * HoverT);
				if (IconBrushes.IsValidIndex(GIdx) && IconBrushes[GIdx].GetResourceObject())
				{
					FLinearColor Tint = FMath::Lerp(
						FLinearColor(0.5f, 0.55f, 0.65f, FadeAlpha),
						FLinearColor(1.0f, 1.0f, 1.0f, FadeAlpha), HoverT);
					DrawIconAt(Out, LayerId, Geo, IconPos, IconBrushes[GIdx], DrawSz, Tint);
				}
				// Piece name at 65%
				FString Name = AllPieceInfos.IsValidIndex(GIdx) ? AllPieceInfos[GIdx].DisplayName : TEXT("");
				if (!Name.IsEmpty())
				{
					FVector2D NPos = PolarToCart(Center, sI + (sO - sI) * 0.68f, Mid);
					FLinearColor NCol = FMath::Lerp(TextDim, TextWhite, HoverT);
					NCol.A *= FadeAlpha;
					DrawTextCentered(Out, LayerId, Geo, NPos, Name, SmallFont, NCol);
				}
				// Subtitle at 85%
				FString Sub = AllPieceInfos.IsValidIndex(GIdx) ? AllPieceInfos[GIdx].Subtitle : TEXT("");
				if (!Sub.IsEmpty())
				{
					FVector2D SPos = PolarToCart(Center, sI + (sO - sI) * 0.88f, Mid);
					FLinearColor SCol = CS.Accent;
					SCol.A = FMath::Lerp(0.3f, 0.7f, HoverT) * FadeAlpha;
					FSlateFontInfo SubFont = FCoreStyle::GetDefaultFontStyle("Regular",
						FMath::Clamp(FMath::RoundToInt(8.0f * Scale), 6, 12));
					DrawTextCentered(Out, LayerId, Geo, SPos, Sub, SubFont, SCol);
				}
			}
		}
	}
	// ===== CENTER HUB (always) =====
	// Hub fill
	DrawCircle(Out, LayerId, Geo, Center, sH, FLinearColor(HubBg.R, HubBg.G, HubBg.B, HubBg.A * FadeAlpha));
	// Hub border
	FLinearColor HBCol = (ActiveCategory >= 0 && CatStyles.IsValidIndex(ActiveCategory))
		? FLinearColor(CatStyles[ActiveCategory].Accent.R, CatStyles[ActiveCategory].Accent.G,
			CatStyles[ActiveCategory].Accent.B, 0.4f * FadeAlpha)
		: FLinearColor(HubBorder.R, HubBorder.G, HubBorder.B, HubBorder.A * FadeAlpha);
	DrawArc(Out, LayerId, Geo, Center, sH, 0.0f, 360.0f, HBCol, 2.0f);
	// Hub content
	if (CurrentView == ERadialMenuView::Sub && ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		FCatStyle CS = CatStyles.IsValidIndex(ActiveCategory) ? CatStyles[ActiveCategory] : CatStyles[0];
		// Category letter icon
		DrawTextCentered(Out, LayerId, Geo, Center - FVector2D(0, 12.0f * Scale),
			Categories[ActiveCategory].Icon, BigIconFont,
			FLinearColor(CS.Accent.R, CS.Accent.G, CS.Accent.B, FadeAlpha * 0.8f));
		// "< BACK"
		DrawTextCentered(Out, LayerId, Geo, Center + FVector2D(0, 12.0f * Scale),
			TEXT("< BACK"), HubSmallFont,
			FLinearColor(CS.Accent.R, CS.Accent.G, CS.Accent.B, FadeAlpha * 0.5f));
	}
	else
	{
		// BUILD text + hammer
		DrawTextCentered(Out, LayerId, Geo, Center + FVector2D(0, 8.0f * Scale),
			TEXT("BUILD"), HubSmallFont,
			FLinearColor(TextDim.R, TextDim.G, TextDim.B, FadeAlpha * 0.8f));
	}
	// Outer ring border (single thin line, no tick marks)
	DrawArc(Out, LayerId, Geo, Center, sO, 0.0f, 360.0f,
		FLinearColor(DividerColor.R, DividerColor.G, DividerColor.B, DividerColor.A * FadeAlpha * 0.5f), 1.0f);
	// Inner ring border
	DrawArc(Out, LayerId, Geo, Center, sI, 0.0f, 360.0f,
		FLinearColor(DividerColor.R, DividerColor.G, DividerColor.B, DividerColor.A * FadeAlpha * 0.3f), 1.0f);
	return LayerId + 1;
}
// ============================================================================
// DRAWING HELPERS
// ============================================================================
void URadialPieceMenu::DrawWedgeFill(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float InR, float OutR,
	float StartDeg, float EndDeg, FLinearColor Color) const
{
	if (Color.A < 0.001f) return;
	// Use NON-overlapping rings to avoid alpha stacking
	float Span = OutR - InR;
	if (Span < 1.0f) return;
	// Each ring exactly touches the next - no overlap
	int32 NumRings = FMath::Max(4, FMath::CeilToInt(Span / 3.0f));
	float RingW = Span / (float)NumRings;
	for (int32 r = 0; r < NumRings; r++)
	{
		float Radius = InR + RingW * (r + 0.5f);
		TArray<FVector2D> Points;
		int32 Steps = 32;
		Points.Reserve(Steps + 1);
		for (int32 s = 0; s <= Steps; s++)
		{
			float T = (float)s / Steps;
			float Deg = FMath::Lerp(StartDeg, EndDeg, T) - 90.0f;
			float Rad = FMath::DegreesToRadians(Deg);
			Points.Add(Center + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * Radius);
		}
		// Line width = ring spacing exactly (no overlap)
		FSlateDrawElement::MakeLines(Out, LayerId, Geo.ToPaintGeometry(),
			Points, ESlateDrawEffect::None, Color, false, RingW);
	}
}
void URadialPieceMenu::DrawArc(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius,
	float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const
{
	if (Color.A < 0.001f) return;
	TArray<FVector2D> Points;
	int32 Steps = 48;
	Points.Reserve(Steps + 1);
	for (int32 i = 0; i <= Steps; i++)
	{
		float T = (float)i / Steps;
		float Deg = FMath::Lerp(StartDeg, EndDeg, T) - 90.0f;
		float Rad = FMath::DegreesToRadians(Deg);
		Points.Add(Center + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * Radius);
	}
	FSlateDrawElement::MakeLines(Out, LayerId, Geo.ToPaintGeometry(),
		Points, ESlateDrawEffect::None, Color, true, Thickness);
}
void URadialPieceMenu::DrawRadialLine(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D A, FVector2D B, FLinearColor Color, float Thickness) const
{
	if (Color.A < 0.001f) return;
	TArray<FVector2D> P;
	P.Add(A);
	P.Add(B);
	FSlateDrawElement::MakeLines(Out, LayerId, Geo.ToPaintGeometry(),
		P, ESlateDrawEffect::None, Color, true, Thickness);
}
void URadialPieceMenu::DrawCircle(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const
{
	// Fill circle using non-overlapping rings
	if (Color.A < 0.001f || Radius < 1.0f) return;
	int32 NumRings = FMath::Max(4, FMath::CeilToInt(Radius / 3.0f));
	float RingW = Radius / (float)NumRings;
	for (int32 r = 0; r < NumRings; r++)
	{
		float R = RingW * (r + 0.5f);
		TArray<FVector2D> Points;
		int32 Steps = 36;
		Points.Reserve(Steps + 1);
		for (int32 s = 0; s <= Steps; s++)
		{
			float Rad = FMath::DegreesToRadians((float)s / Steps * 360.0f);
			Points.Add(Center + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * R);
		}
		FSlateDrawElement::MakeLines(Out, LayerId, Geo.ToPaintGeometry(),
			Points, ESlateDrawEffect::None, Color, false, RingW);
	}
}
void URadialPieceMenu::DrawText(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Position, const FString& Text,
	const FSlateFontInfo& Font, FLinearColor Color) const
{
	if (Text.IsEmpty() || Color.A < 0.001f) return;
	TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	FVector2D Sz = FM->Measure(Text, Font);
	FSlateDrawElement::MakeText(Out, LayerId, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(Position)),
		Text, Font, ESlateDrawEffect::None, Color);
}
void URadialPieceMenu::DrawTextCentered(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, const FString& Text,
	const FSlateFontInfo& Font, FLinearColor Color) const
{
	if (Text.IsEmpty() || Color.A < 0.001f) return;
	TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	FVector2D Sz = FM->Measure(Text, Font);
	FVector2D Pos = Center - Sz / 2.0f;
	FSlateDrawElement::MakeText(Out, LayerId, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(Pos)),
		Text, Font, ESlateDrawEffect::None, Color);
}
void URadialPieceMenu::DrawIconAt(FSlateWindowElementList& Out, int32 LayerId,
	const FGeometry& Geo, FVector2D Center, const FSlateBrush& Brush,
	float Size, FLinearColor Tint) const
{
	if (Tint.A < 0.001f) return;
	FVector2D Sz(Size, Size);
	FVector2D Pos = Center - Sz / 2.0f;
	FSlateDrawElement::MakeBox(Out, LayerId, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(Pos)),
		&Brush, ESlateDrawEffect::None, Tint);
}
void URadialPieceMenu::PlaySoundOpen()
{
	// TODO: Play UI open sound
}
void URadialPieceMenu::PlaySoundClose()
{
	// TODO: Play UI close sound
}
