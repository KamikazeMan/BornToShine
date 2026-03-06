// RadialPieceMenu.cpp - Born To Shine
// Professional radial wheel with triangle-fan wedge fills via MakeCustomVerts.
// Zero line artifacts. Clean alpha blending. Matches React prototype.
#include "RadialPieceMenu.h"
#include "ConstructionPhaseManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Engine/Texture2D.h"
FVector2D URadialPieceMenu::Polar(FVector2D Center, float Radius, float Deg) const
{
	float Rad = FMath::DegreesToRadians(Deg - 90.0f);
	return Center + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * Radius;
}
URadialPieceMenu::URadialPieceMenu(const FObjectInitializer& OI)
	: Super(OI)
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
	HubRadius   = 48.0f;
	WedgeGapDeg = 0.0f;
	IconSize    = 55.0f;
	FadeAlpha = 0.0f;
	FadeSpeed = 8.0f;
	GlowPulseTime = 0.0f;
	ViewTransition = 0.0f;
	ViewTransitionSpeed = 10.0f;
	// ---- Category styles: dark fills with subtle color tint ----
	CatStyles.SetNum(4);
	// Framing - dark warm
	CatStyles[0].DarkFill  = FLinearColor(0.07f, 0.03f, 0.02f, 0.94f);
	CatStyles[0].LitFill   = FLinearColor(0.16f, 0.07f, 0.05f, 0.96f);
	CatStyles[0].Accent    = FLinearColor(0.95f, 0.45f, 0.30f, 1.0f);
	CatStyles[0].TextColor = FLinearColor(0.95f, 0.50f, 0.35f, 1.0f);
	// Roofing - dark green
	CatStyles[1].DarkFill  = FLinearColor(0.02f, 0.06f, 0.03f, 0.94f);
	CatStyles[1].LitFill   = FLinearColor(0.05f, 0.14f, 0.08f, 0.96f);
	CatStyles[1].Accent    = FLinearColor(0.30f, 0.85f, 0.50f, 1.0f);
	CatStyles[1].TextColor = FLinearColor(0.35f, 0.88f, 0.55f, 1.0f);
	// Sheathing - dark blue
	CatStyles[2].DarkFill  = FLinearColor(0.02f, 0.03f, 0.07f, 0.94f);
	CatStyles[2].LitFill   = FLinearColor(0.05f, 0.06f, 0.18f, 0.96f);
	CatStyles[2].Accent    = FLinearColor(0.40f, 0.62f, 1.0f, 1.0f);
	CatStyles[2].TextColor = FLinearColor(0.45f, 0.65f, 1.0f, 1.0f);
	// Foundation - dark warm grey
	CatStyles[3].DarkFill  = FLinearColor(0.05f, 0.04f, 0.03f, 0.94f);
	CatStyles[3].LitFill   = FLinearColor(0.10f, 0.09f, 0.07f, 0.96f);
	CatStyles[3].Accent    = FLinearColor(0.70f, 0.60f, 0.45f, 1.0f);
	CatStyles[3].TextColor = FLinearColor(0.75f, 0.65f, 0.50f, 1.0f);
	HubBg       = FLinearColor(0.02f, 0.025f, 0.05f, 0.96f);
	HubBorder   = FLinearColor(0.18f, 0.22f, 0.30f, 0.5f);
	TextWhite   = FLinearColor(0.92f, 0.92f, 0.92f, 1.0f);
	TextDim     = FLinearColor(0.42f, 0.47f, 0.55f, 1.0f);
	DividerColor = FLinearColor(0.22f, 0.26f, 0.32f, 0.6f);
	// White brush for MakeCustomVerts
	WhiteBrush = *FCoreStyle::Get().GetBrush("WhiteBrush");
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
}
// ============================================================================
void URadialPieceMenu::BuildCategories()
{
	Categories.Empty();
	FCategoryInfo C0; C0.Name = TEXT("FRAMING");    C0.Icon = TEXT("F");
	FCategoryInfo C1; C1.Name = TEXT("ROOFING");    C1.Icon = TEXT("R");
	FCategoryInfo C2; C2.Name = TEXT("SHEATHING");  C2.Icon = TEXT("S");
	FCategoryInfo C3; C3.Name = TEXT("FOUNDATION"); C3.Icon = TEXT("B");
	for (int32 i = 0; i < AllPieceInfos.Num(); i++)
	{
		switch (AllPieceInfos[i].PieceType)
		{
		case EPieceType::WallStud: case EPieceType::WallPlate: case EPieceType::CornerPost:
		case EPieceType::TopPlate: case EPieceType::DoubleTopPlate:
		case EPieceType::DoorFrame: case EPieceType::WindowFrame: case EPieceType::Header:
		case EPieceType::RimBoard: case EPieceType::FloorJoist:
			C0.PieceIndices.Add(i); break;
		case EPieceType::RidgePost: case EPieceType::RidgeBoard:
		case EPieceType::Rafter: case EPieceType::FasciaBoard:
			C1.PieceIndices.Add(i); break;
		case EPieceType::Plywood:
			C2.PieceIndices.Add(i); break;
		case EPieceType::Foundation:
			C3.PieceIndices.Add(i); break;
		default:
			C0.PieceIndices.Add(i); break;
		}
	}
	if (C0.PieceIndices.Num() > 0) Categories.Add(C0);
	if (C1.PieceIndices.Num() > 0) Categories.Add(C1);
	if (C2.PieceIndices.Num() > 0) Categories.Add(C2);
	if (C3.PieceIndices.Num() > 0) Categories.Add(C3);
}
int32 URadialPieceMenu::FindCategoryForPieceIndex(int32 PieceIdx) const
{
	for (int32 c = 0; c < Categories.Num(); c++)
		if (Categories[c].PieceIndices.Contains(PieceIdx)) return c;
	return -1;
}
void URadialPieceMenu::InitMenu(const TArray<FPieceTypeInfo>& Infos, int32 Cur)
{
	AllPieceInfos = Infos;
	BuildCategories();
	CurrentView = ERadialMenuView::Main;
	ActiveCategory = -1;
	HighlightedCategory = FindCategoryForPieceIndex(Cur);
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
		UTexture2D* T = AllPieceInfos[i].Icon.LoadSynchronous();
		if (T)
		{
			IconBrushes[i].SetResourceObject(T);
			IconBrushes[i].ImageSize = FVector2D(IconSize, IconSize);
			IconBrushes[i].DrawAs = ESlateBrushDrawType::Image;
			IconBrushes[i].Tiling = ESlateBrushTileType::NoTile;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RadialMenu: %d pieces, %d cats"), AllPieceInfos.Num(), Categories.Num());
}
int32 URadialPieceMenu::GetHighlightedIndex() const
{
	int32 Result = -1;
	if (ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		const auto& P = Categories[ActiveCategory].PieceIndices;
		int32 S = (SelectedPieceSlot >= 0) ? SelectedPieceSlot : HighlightedPieceSlot;
		if (S >= 0 && P.IsValidIndex(S)) Result = P[S];
		else if (P.Num() > 0) Result = P[0];
	}
	UE_LOG(LogTemp, Warning, TEXT("RadialMenu: GetHighlightedIndex() ActiveCat=%d SelectedSlot=%d HoverSlot=%d => %d"),
		ActiveCategory, SelectedPieceSlot, HighlightedPieceSlot, Result);
	return Result;
}
// ============================================================================
// INPUT
// ============================================================================
FReply URadialPieceMenu::NativeOnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
	if (Categories.Num() == 0) return FReply::Unhandled();
	FVector2D Sz = G.GetLocalSize();
	FVector2D C = Sz / 2.0f;
	float Sc = (FMath::Min(Sz.X, Sz.Y) * 0.55f) / (OuterRadius * 2.0f);
	FVector2D M = G.AbsoluteToLocal(E.GetScreenSpacePosition());
	float DX = M.X - C.X, DY = M.Y - C.Y;
	float Dist = FMath::Sqrt(DX*DX + DY*DY);
	float sI = InnerRadius * Sc, sO = OuterRadius * Sc, sH = HubRadius * Sc;
	float Ang = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
	if (Ang < 0) Ang += 360.0f;
	if (CurrentView == ERadialMenuView::Sub)
	{
		if (Dist <= sH + 8.0f * Sc)
		{
			CurrentView = ERadialMenuView::Main;
			ActiveCategory = -1;
			HighlightedPieceSlot = -1;
			SelectedPieceSlot = -1;
			PieceHoverScales.Empty();
			return FReply::Handled();
		}
		if (Dist >= sI && Dist <= sO && ActiveCategory >= 0)
		{
			int32 N = Categories[ActiveCategory].PieceIndices.Num();
			if (N > 0)
			{
				int32 Hit = FMath::Clamp((int32)(Ang / (360.0f / N)), 0, N - 1);
				SelectedPieceSlot = Hit;
				HighlightedPieceSlot = Hit;
				int32 GlobalIdx = Categories[ActiveCategory].PieceIndices[Hit];
				UE_LOG(LogTemp, Warning, TEXT("RadialMenu: PIECE CLICKED slot=%d globalIdx=%d name=%s"),
					Hit, GlobalIdx,
					AllPieceInfos.IsValidIndex(GlobalIdx) ? *AllPieceInfos[GlobalIdx].DisplayName : TEXT("?"));
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}
// ============================================================================
// TICK
// ============================================================================
void URadialPieceMenu::NativeTick(const FGeometry& MyGeo, float DT)
{
	Super::NativeTick(MyGeo, DT);
	if (Categories.Num() == 0) return;
	FadeAlpha = FMath::FInterpTo(FadeAlpha, 1.0f, DT, FadeSpeed);
	GlowPulseTime += DT;
	ViewTransition = FMath::FInterpTo(ViewTransition,
		(CurrentView == ERadialMenuView::Sub) ? 1.0f : 0.0f, DT, ViewTransitionSpeed);
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	float MX, MY;
	PC->GetMousePosition(MX, MY);
	FVector2D VP;
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(VP);
	FVector2D C = VP / 2.0f;
	float Sc = (FMath::Min(VP.X, VP.Y) * 0.55f) / (OuterRadius * 2.0f);
	float sI = InnerRadius * Sc, sO = OuterRadius * Sc;
	float DX = MX - C.X, DY = MY - C.Y;
	float Dist = FMath::Sqrt(DX*DX + DY*DY);
	float Ang = FMath::RadiansToDegrees(FMath::Atan2(DX, -DY));
	if (Ang < 0) Ang += 360.0f;
	if (CurrentView == ERadialMenuView::Main)
	{
		int32 N = Categories.Num();
		HighlightedCategory = (Dist >= sI && Dist <= sO && N > 0)
			? FMath::Clamp((int32)(Ang / (360.0f / N)), 0, N - 1) : -1;
		if (CategoryHoverScales.Num() != N) CategoryHoverScales.Init(0.0f, N);
		for (int32 i = 0; i < N; i++)
			CategoryHoverScales[i] = FMath::FInterpTo(CategoryHoverScales[i],
				(i == HighlightedCategory) ? 1.0f : 0.0f, DT, 12.0f);
		// Auto-enter sub view when hovering a category
		if (HighlightedCategory >= 0 && CategoryHoverScales.IsValidIndex(HighlightedCategory)
			&& CategoryHoverScales[HighlightedCategory] > 0.8f)
		{
			ActiveCategory = HighlightedCategory;
			HighlightedPieceSlot = -1;
			SelectedPieceSlot = -1;
			PieceHoverScales.Empty();
			CurrentView = ERadialMenuView::Sub;
		}
	}
	else if (ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		// If mouse is over center hub area, go back to main view
		float sH = HubRadius * Sc;
		if (Dist <= sH + 5.0f * Sc)
		{
			CurrentView = ERadialMenuView::Main;
			ActiveCategory = -1;
			HighlightedPieceSlot = -1;
			SelectedPieceSlot = -1;
			PieceHoverScales.Empty();
		}
		int32 N = Categories[ActiveCategory].PieceIndices.Num();
		HighlightedPieceSlot = (Dist >= sI && Dist <= sO && N > 0)
			? FMath::Clamp((int32)(Ang / (360.0f / N)), 0, N - 1) : -1;
		if (PieceHoverScales.Num() != N) PieceHoverScales.Init(0.0f, N);
		for (int32 i = 0; i < N; i++)
			PieceHoverScales[i] = FMath::FInterpTo(PieceHoverScales[i],
				(i == HighlightedPieceSlot || i == SelectedPieceSlot) ? 1.0f : 0.0f, DT, 12.0f);
	}
	PrevHighlightedCategory = HighlightedCategory;
	PrevHighlightedPieceSlot = HighlightedPieceSlot;
}
// ============================================================================
// PAINT
// ============================================================================
int32 URadialPieceMenu::NativePaint(const FPaintArgs& Args, const FGeometry& Geo,
	const FSlateRect& Cull, FSlateWindowElementList& Out,
	int32 LId, const FWidgetStyle& WS, bool bE) const
{
	if (Categories.Num() == 0 || FadeAlpha < 0.001f) return LId;
	FVector2D Sz = Geo.GetLocalSize();
	FVector2D C = Sz / 2.0f;
	float Sc = (FMath::Min(Sz.X, Sz.Y) * 0.55f) / (OuterRadius * 2.0f);
	float sI = InnerRadius * Sc;
	float sO = OuterRadius * Sc;
	float sH = HubRadius * Sc;
	float Gap = WedgeGapDeg / 2.0f;
	// Fonts
	int32 NS = FMath::Clamp(FMath::RoundToInt(15.0f * Sc), 10, 22);
	int32 SS = FMath::Clamp(FMath::RoundToInt(11.0f * Sc), 8, 16);
	int32 HS = FMath::Clamp(FMath::RoundToInt(11.0f * Sc), 8, 16);
	int32 BI = FMath::Clamp(FMath::RoundToInt(20.0f * Sc), 14, 28);
	FSlateFontInfo NameF  = FCoreStyle::GetDefaultFontStyle("Bold", NS);
	FSlateFontInfo SmallF = FCoreStyle::GetDefaultFontStyle("Regular", SS);
	FSlateFontInfo HeadF  = FCoreStyle::GetDefaultFontStyle("Regular", HS);
	FSlateFontInfo BigF   = FCoreStyle::GetDefaultFontStyle("Bold", BI);
	bool bMain = ViewTransition < 0.5f;
	if (bMain)
	{
		// ===== MAIN: Category wedges =====
		int32 N = Categories.Num();
		float Sw = 360.0f / N;
		// Header
		DrawTextCentered(Out, LId, Geo, C - FVector2D(0, sO + 28.0f * Sc),
			TEXT("SELECT CATEGORY"), HeadF,
			FLinearColor(0.38f, 0.42f, 0.52f, FadeAlpha * 0.7f));
		// Pass 1: Draw all wedge fills
		for (int32 i = 0; i < N; i++)
		{
			float S = i * Sw, E = S + Sw;
			float HT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
			FCatStyle CS = CatStyles.IsValidIndex(i) ? CatStyles[i] : CatStyles[0];
			FLinearColor Fill = FMath::Lerp(CS.DarkFill, CS.LitFill, HT);
			Fill.A *= FadeAlpha;
			DrawFilledWedge(Out, LId, Geo, C, sI, sO, S + Gap, E - Gap, Fill);
		}
		LId++;
		// Pass 2: Draw borders, dividers, text, and icons
		for (int32 i = 0; i < N; i++)
		{
			float S = i * Sw, E = S + Sw, M = (S + E) / 2.0f;
			float HT = CategoryHoverScales.IsValidIndex(i) ? CategoryHoverScales[i] : 0.0f;
			FCatStyle CS = CatStyles.IsValidIndex(i) ? CatStyles[i] : CatStyles[0];
			// Full border glow around entire wedge
			FLinearColor Bord = CS.Accent;
			Bord.A = FMath::Lerp(0.45f, 0.90f, HT) * FadeAlpha;
			float BordW = FMath::Lerp(3.0f, 5.0f, HT);
			// Outer arc
			DrawArc(Out, LId, Geo, C, sO, S + Gap, E - Gap, Bord, BordW);
			// Inner arc
			DrawArc(Out, LId, Geo, C, sI, S + Gap, E - Gap, Bord, BordW * 0.8f);
			// Left edge
			DrawLine2D(Out, LId, Geo, Polar(C, sI, S + Gap), Polar(C, sO, S + Gap), Bord, BordW * 0.6f);
			// Right edge
			DrawLine2D(Out, LId, Geo, Polar(C, sI, E - Gap), Polar(C, sO, E - Gap), Bord, BordW * 0.6f);
			// Icon at 38%
			float IR = sI + (sO - sI) * 0.38f;
			FVector2D IP = Polar(C, IR, M);
			float ISz = IconSize * Sc * 0.65f * (1.0f + 0.08f * HT);
			if (Categories[i].PieceIndices.Num() > 0)
			{
				int32 FP = Categories[i].PieceIndices[0];
				if (IconBrushes.IsValidIndex(FP) && IconBrushes[FP].GetResourceObject())
				{
					FLinearColor IT = FMath::Lerp(FLinearColor(0.55f, 0.6f, 0.65f, FadeAlpha),
						FLinearColor(1, 1, 1, FadeAlpha), HT);
					DrawIcon(Out, LId, Geo, IP, IconBrushes[FP], ISz, IT);
				}
			}
			// Name at 72%
			FVector2D NP = Polar(C, sI + (sO - sI) * 0.72f, M);
			FLinearColor NC = CS.TextColor; NC.A *= FadeAlpha;
			DrawTextCentered(Out, LId, Geo, NP, Categories[i].Name, NameF, NC);
		}
	}
	else if (ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		// ===== SUB: Piece wedges =====
		const FCategoryInfo& Cat = Categories[ActiveCategory];
		int32 N = Cat.PieceIndices.Num();
		FCatStyle CS = CatStyles.IsValidIndex(ActiveCategory) ? CatStyles[ActiveCategory] : CatStyles[0];
		if (N > 0)
		{
			float Sw = 360.0f / N;
			// Header
			DrawTextCentered(Out, LId, Geo, C - FVector2D(0, sO + 28.0f * Sc),
				Cat.Name, HeadF, FLinearColor(CS.TextColor.R, CS.TextColor.G, CS.TextColor.B, FadeAlpha * 0.8f));
			// Pass 1: Draw all wedge fills
			for (int32 p = 0; p < N; p++)
			{
				float S = p * Sw, E = S + Sw;
				float HT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;
				FLinearColor Fill = FMath::Lerp(
					FLinearColor(0.05f, 0.05f, 0.07f, 0.93f),
					FLinearColor(CS.DarkFill.R*2, CS.DarkFill.G*2, CS.DarkFill.B*2, 0.96f), HT);
				Fill.A *= FadeAlpha;
				DrawFilledWedge(Out, LId, Geo, C, sI, sO, S + Gap, E - Gap, Fill);
			}
			LId++;
			// Pass 2: Draw borders, dividers, text, and icons
			for (int32 p = 0; p < N; p++)
			{
				float S = p * Sw, E = S + Sw, M = (S + E) / 2.0f;
				float HT = PieceHoverScales.IsValidIndex(p) ? PieceHoverScales[p] : 0.0f;
				bool bLit = (p == HighlightedPieceSlot || p == SelectedPieceSlot);
				int32 GI = Cat.PieceIndices[p];
				// Full border glow around entire piece wedge
				FLinearColor Bord = bLit ? CS.Accent : FLinearColor(CS.Accent.R, CS.Accent.G, CS.Accent.B, 0.3f);
				Bord.A = (bLit ? FMath::Lerp(0.5f, 0.85f, HT) : 0.3f) * FadeAlpha;
				float BordW = bLit ? 5.0f : 2.5f;
				// Outer arc
				DrawArc(Out, LId, Geo, C, sO, S + Gap, E - Gap, Bord, BordW);
				// Inner arc
				DrawArc(Out, LId, Geo, C, sI, S + Gap, E - Gap, Bord, BordW * 0.8f);
				// Left edge
				DrawLine2D(Out, LId, Geo, Polar(C, sI, S + Gap), Polar(C, sO, S + Gap), Bord, BordW * 0.6f);
				// Right edge
				DrawLine2D(Out, LId, Geo, Polar(C, sI, E - Gap), Polar(C, sO, E - Gap), Bord, BordW * 0.6f);
				// Icon at 33%
				float IR = sI + (sO - sI) * 0.33f;
				float DSz = IconSize * Sc * 0.5f * (1 + 0.06f * HT);
				if (IconBrushes.IsValidIndex(GI) && IconBrushes[GI].GetResourceObject())
				{
					FLinearColor IT = FMath::Lerp(FLinearColor(0.5f, 0.55f, 0.6f, FadeAlpha),
						FLinearColor(1, 1, 1, FadeAlpha), HT);
					DrawIcon(Out, LId, Geo, Polar(C, IR, M), IconBrushes[GI], DSz, IT);
				}
				// Piece name at 65%
				FString Nm = AllPieceInfos.IsValidIndex(GI) ? AllPieceInfos[GI].DisplayName : TEXT("");
				if (!Nm.IsEmpty())
				{
					FLinearColor NC = FMath::Lerp(TextDim, TextWhite, HT); NC.A *= FadeAlpha;
					DrawTextCentered(Out, LId, Geo, Polar(C, sI + (sO-sI)*0.65f, M), Nm, SmallF, NC);
				}
				// Subtitle at 85%
				FString Sub = AllPieceInfos.IsValidIndex(GI) ? AllPieceInfos[GI].Subtitle : TEXT("");
				if (!Sub.IsEmpty())
				{
					FLinearColor SC = CS.Accent; SC.A = FMath::Lerp(0.3f, 0.65f, HT) * FadeAlpha;
					FSlateFontInfo SF = FCoreStyle::GetDefaultFontStyle("Regular",
						FMath::Clamp(FMath::RoundToInt(9.0f * Sc), 6, 13));
					DrawTextCentered(Out, LId, Geo, Polar(C, sI + (sO-sI)*0.85f, M), Sub, SF, SC);
				}
			}
		}
	}
	// ===== CENTER HUB =====
	// Check if mouse is near center
	float HubDist = 999.0f;
	{
		APlayerController* PC = GetOwningPlayer();
		if (PC)
		{
			float MX, MY;
			PC->GetMousePosition(MX, MY);
			FVector2D VP;
			if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(VP);
			float DX = MX - VP.X / 2.0f;
			float DY = MY - VP.Y / 2.0f;
			HubDist = FMath::Sqrt(DX * DX + DY * DY);
		}
	}
	bool bHubHovered = HubDist <= sH + 5.0f * Sc;

	// Hub glow ring (outer)
	FLinearColor GlowC;
	if (bHubHovered)
		GlowC = FLinearColor(1.0f, 1.0f, 1.0f, 0.4f * FadeAlpha);
	else if (ActiveCategory >= 0 && CatStyles.IsValidIndex(ActiveCategory))
		GlowC = FLinearColor(CatStyles[ActiveCategory].Accent.R, CatStyles[ActiveCategory].Accent.G,
			CatStyles[ActiveCategory].Accent.B, 0.25f * FadeAlpha);
	else
		GlowC = FLinearColor(HubBorder.R, HubBorder.G, HubBorder.B, 0.2f * FadeAlpha);
	DrawArc(Out, LId, Geo, C, sH + 3.0f * Sc, 0, 360, GlowC, 4.0f * Sc);
	DrawArc(Out, LId, Geo, C, sH + 1.5f * Sc, 0, 360, GlowC, 2.0f * Sc);

	// Hub fill — white when hovered, dark when not
	FLinearColor HubFill;
	if (bHubHovered)
		HubFill = FLinearColor(0.25f, 0.27f, 0.32f, 0.97f * FadeAlpha);
	else
		HubFill = FLinearColor(HubBg.R, HubBg.G, HubBg.B, HubBg.A * FadeAlpha);
	DrawFilledCircle(Out, LId, Geo, C, sH, HubFill);

	// Hub border ring
	FLinearColor HBC;
	if (bHubHovered)
		HBC = FLinearColor(1.0f, 1.0f, 1.0f, 0.7f * FadeAlpha);
	else if (ActiveCategory >= 0 && CatStyles.IsValidIndex(ActiveCategory))
		HBC = FLinearColor(CatStyles[ActiveCategory].Accent.R, CatStyles[ActiveCategory].Accent.G,
			CatStyles[ActiveCategory].Accent.B, 0.35f * FadeAlpha);
	else
		HBC = FLinearColor(HubBorder.R, HubBorder.G, HubBorder.B, HubBorder.A * FadeAlpha);
	DrawArc(Out, LId, Geo, C, sH, 0, 360, HBC, bHubHovered ? 3.0f : 2.0f);

	// Hub text content
	if (CurrentView == ERadialMenuView::Sub && ActiveCategory >= 0 && Categories.IsValidIndex(ActiveCategory))
	{
		FCatStyle CS = CatStyles.IsValidIndex(ActiveCategory) ? CatStyles[ActiveCategory] : CatStyles[0];

		// Show hovered piece name if hovering a piece, otherwise show category info
		if (HighlightedPieceSlot >= 0 && Categories[ActiveCategory].PieceIndices.IsValidIndex(HighlightedPieceSlot))
		{
			int32 GI = Categories[ActiveCategory].PieceIndices[HighlightedPieceSlot];
			FString PName = AllPieceInfos.IsValidIndex(GI) ? AllPieceInfos[GI].DisplayName : TEXT("");
			FString PSub = AllPieceInfos.IsValidIndex(GI) ? AllPieceInfos[GI].Subtitle : TEXT("");

			// Piece name
			if (!PName.IsEmpty())
				DrawTextCentered(Out, LId, Geo, C - FVector2D(0, 8.0f * Sc), PName, SmallF,
					FLinearColor(TextWhite.R, TextWhite.G, TextWhite.B, FadeAlpha * 0.95f));
			// Piece subtitle
			if (!PSub.IsEmpty())
				DrawTextCentered(Out, LId, Geo, C + FVector2D(0, 8.0f * Sc), PSub,
					FCoreStyle::GetDefaultFontStyle("Regular", FMath::Clamp(FMath::RoundToInt(9.0f * Sc), 6, 13)),
					FLinearColor(CS.Accent.R, CS.Accent.G, CS.Accent.B, FadeAlpha * 0.7f));
		}
		else if (!bHubHovered)
		{
			// Show category icon + back
			DrawTextCentered(Out, LId, Geo, C - FVector2D(0, 10.0f * Sc),
				Categories[ActiveCategory].Icon, BigF,
				FLinearColor(CS.Accent.R, CS.Accent.G, CS.Accent.B, FadeAlpha * 0.75f));
			DrawTextCentered(Out, LId, Geo, C + FVector2D(0, 14.0f * Sc),
				TEXT("< BACK"), SmallF,
				FLinearColor(CS.Accent.R, CS.Accent.G, CS.Accent.B, FadeAlpha * 0.45f));
		}
		else
		{
			// Hovered over hub — show "BACK" prominently
			DrawTextCentered(Out, LId, Geo, C,
				TEXT("BACK"), NameF,
				FLinearColor(1.0f, 1.0f, 1.0f, FadeAlpha * 0.9f));
		}
	}
	else
	{
		// Main view — show hovered category name or BUILD
		if (HighlightedCategory >= 0 && Categories.IsValidIndex(HighlightedCategory))
		{
			FCatStyle CS = CatStyles.IsValidIndex(HighlightedCategory) ? CatStyles[HighlightedCategory] : CatStyles[0];
			DrawTextCentered(Out, LId, Geo, C,
				Categories[HighlightedCategory].Name, SmallF,
				FLinearColor(CS.TextColor.R, CS.TextColor.G, CS.TextColor.B, FadeAlpha * 0.9f));
		}
		else
		{
			DrawTextCentered(Out, LId, Geo, C + FVector2D(0, 6.0f * Sc),
				TEXT("BUILD"), SmallF,
				FLinearColor(TextDim.R, TextDim.G, TextDim.B, FadeAlpha * 0.75f));
		}
	}
	// Thin outer + inner ring borders
	FLinearColor RC = DividerColor; RC.A *= FadeAlpha * 0.4f;
	DrawArc(Out, LId, Geo, C, sO, 0, 360, RC, 1.0f);
	DrawArc(Out, LId, Geo, C, sI, 0, 360, RC, 0.8f);
	return LId + 1;
}
// ============================================================================
// DRAWING: Triangle-fan filled wedge via MakeCustomVerts
// ============================================================================
void URadialPieceMenu::DrawFilledWedge(FSlateWindowElementList& Out, int32 LId,
	const FGeometry& Geo, FVector2D Center, float InR, float OutR,
	float StartDeg, float EndDeg, FLinearColor Color) const
{
	if (Color.A < 0.001f) return;
	const int32 Steps = 24;
	const FSlateRenderTransform& RT = Geo.GetAccumulatedRenderTransform();
	// Build triangle strip: inner[i], outer[i], inner[i+1], outer[i+1]...
	TArray<FSlateVertex> Verts;
	TArray<SlateIndex> Indices;
	Verts.Reserve((Steps + 1) * 2);
	Indices.Reserve(Steps * 6);
	FColor VertColor = Color.ToFColor(true);
	for (int32 i = 0; i <= Steps; i++)
	{
		float T = (float)i / Steps;
		float Deg = FMath::Lerp(StartDeg, EndDeg, T) - 90.0f;
		float Rad = FMath::DegreesToRadians(Deg);
		FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad));
		FVector2D InnerPt = Center + Dir * InR;
		FVector2D OuterPt = Center + Dir * OutR;
		// Transform to render space
		FVector2D InT = RT.TransformPoint(InnerPt);
		FVector2D OutT = RT.TransformPoint(OuterPt);
		FSlateVertex InV;
		InV.Position = FVector2f(InT.X, InT.Y);
		InV.Color = VertColor;
		InV.TexCoords[0] = 0.0f; InV.TexCoords[1] = 0.0f;
		InV.TexCoords[2] = 1.0f; InV.TexCoords[3] = 1.0f;
		FSlateVertex OutV;
		OutV.Position = FVector2f(OutT.X, OutT.Y);
		OutV.Color = VertColor;
		OutV.TexCoords[0] = 1.0f; OutV.TexCoords[1] = 0.0f;
		OutV.TexCoords[2] = 1.0f; OutV.TexCoords[3] = 1.0f;
		Verts.Add(InV);
		Verts.Add(OutV);
	}
	// Build triangle indices
	for (int32 i = 0; i < Steps; i++)
	{
		int32 Base = i * 2;
		// Triangle 1: inner[i], outer[i], inner[i+1]
		Indices.Add(Base);
		Indices.Add(Base + 1);
		Indices.Add(Base + 2);
		// Triangle 2: outer[i], outer[i+1], inner[i+1]
		Indices.Add(Base + 1);
		Indices.Add(Base + 3);
		Indices.Add(Base + 2);
	}
	const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(WhiteBrush);
	FSlateDrawElement::MakeCustomVerts(Out, LId, Handle, Verts, Indices, nullptr, 0, 0);
}
void URadialPieceMenu::DrawFilledCircle(FSlateWindowElementList& Out, int32 LId,
	const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const
{
	if (Color.A < 0.001f || Radius < 1.0f) return;
	const int32 Steps = 32;
	const FSlateRenderTransform& RT = Geo.GetAccumulatedRenderTransform();
	FColor VC = Color.ToFColor(true);
	TArray<FSlateVertex> Verts;
	TArray<SlateIndex> Indices;
	Verts.Reserve(Steps + 2);
	Indices.Reserve(Steps * 3);
	// Center vertex
	FVector2D CT = RT.TransformPoint(Center);
	FSlateVertex CV;
	CV.Position = FVector2f(CT.X, CT.Y);
	CV.Color = VC;
	CV.TexCoords[0] = 0.5f; CV.TexCoords[1] = 0.5f;
	CV.TexCoords[2] = 1.0f; CV.TexCoords[3] = 1.0f;
	Verts.Add(CV);
	// Edge vertices
	for (int32 i = 0; i <= Steps; i++)
	{
		float Rad = FMath::DegreesToRadians((float)i / Steps * 360.0f);
		FVector2D P = Center + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * Radius;
		FVector2D PT = RT.TransformPoint(P);
		FSlateVertex V;
		V.Position = FVector2f(PT.X, PT.Y);
		V.Color = VC;
		V.TexCoords[0] = 0.5f + FMath::Cos(Rad) * 0.5f;
		V.TexCoords[1] = 0.5f + FMath::Sin(Rad) * 0.5f;
		V.TexCoords[2] = 1.0f; V.TexCoords[3] = 1.0f;
		Verts.Add(V);
	}
	for (int32 i = 0; i < Steps; i++)
	{
		Indices.Add(0);
		Indices.Add(i + 1);
		Indices.Add(i + 2);
	}
	const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(WhiteBrush);
	FSlateDrawElement::MakeCustomVerts(Out, LId, Handle, Verts, Indices, nullptr, 0, 0);
}
void URadialPieceMenu::DrawArc(FSlateWindowElementList& Out, int32 LId,
	const FGeometry& Geo, FVector2D Center, float Radius,
	float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const
{
	if (Color.A < 0.001f) return;
	TArray<FVector2D> Pts;
	int32 Steps = 48;
	Pts.Reserve(Steps + 1);
	for (int32 i = 0; i <= Steps; i++)
	{
		float Deg = FMath::Lerp(StartDeg, EndDeg, (float)i / Steps) - 90.0f;
		float R = FMath::DegreesToRadians(Deg);
		Pts.Add(Center + FVector2D(FMath::Cos(R), FMath::Sin(R)) * Radius);
	}
	FSlateDrawElement::MakeLines(Out, LId, Geo.ToPaintGeometry(),
		Pts, ESlateDrawEffect::None, Color, true, Thickness);
}
void URadialPieceMenu::DrawLine2D(FSlateWindowElementList& Out, int32 LId,
	const FGeometry& Geo, FVector2D A, FVector2D B, FLinearColor Color, float Thickness) const
{
	if (Color.A < 0.001f) return;
	TArray<FVector2D> P; P.Add(A); P.Add(B);
	FSlateDrawElement::MakeLines(Out, LId, Geo.ToPaintGeometry(),
		P, ESlateDrawEffect::None, Color, true, Thickness);
}
void URadialPieceMenu::DrawTextCentered(FSlateWindowElementList& Out, int32 LId,
	const FGeometry& Geo, FVector2D Pos, const FString& Text,
	const FSlateFontInfo& Font, FLinearColor Color) const
{
	if (Text.IsEmpty() || Color.A < 0.001f) return;
	TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	FVector2D Sz = FM->Measure(Text, Font);
	FVector2D TL = Pos - Sz / 2.0f;
	FSlateDrawElement::MakeText(Out, LId, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(TL)),
		Text, Font, ESlateDrawEffect::None, Color);
}
void URadialPieceMenu::DrawIcon(FSlateWindowElementList& Out, int32 LId,
	const FGeometry& Geo, FVector2D Pos, const FSlateBrush& Brush,
	float Size, FLinearColor Tint) const
{
	if (Tint.A < 0.001f) return;
	FVector2D S(Size, Size);
	FVector2D TL = Pos - S / 2.0f;
	FSlateDrawElement::MakeBox(Out, LId, Geo.ToPaintGeometry(S, FSlateLayoutTransform(TL)),
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
