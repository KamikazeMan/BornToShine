// Born To Shine - Two-Tier Radial Piece Selection Menu (Cyan Glow Sci-fi)
//
// Rebuilt to match the React prototype: dark background (#0a0e17),
// cyan glow (#00e5ff) on all edges/splits/borders, two-tier layout
// (inner categories + outer piece cards), center hub with crosshair,
// connector lines, tick marks, smooth animations.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConstructionTypes.h"
#include "Styling/SlateBrush.h"
#include "RadialPieceMenu.generated.h"

/** Category definition for the inner ring */
USTRUCT()
struct FCategoryInfo
{
	GENERATED_BODY()

	FString Name;
	FString Icon; // Unicode icon character
	TArray<int32> PieceIndices; // Indices into the full FPieceTypeInfo array
};

UCLASS()
class BORNTOSHINE_API URadialPieceMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	URadialPieceMenu(const FObjectInitializer& ObjectInitializer);

	/** Populate from FPieceTypeInfo; CurrentIndex pre-highlights that piece */
	void InitMenu(const TArray<FPieceTypeInfo>& InInfos, int32 CurrentIndex);

	/** Final selected piece index (-1 = none) in the full PieceTypeInfos array */
	int32 GetHighlightedIndex() const;

	void PlaySoundOpen();
	void PlaySoundClose();
	void PlaySoundHover();

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// --- Full piece data ---
	TArray<FPieceTypeInfo> AllPieceInfos;

	// --- Categories (inner ring) ---
	TArray<FCategoryInfo> Categories;
	int32 HighlightedCategory;     // -1 = none
	int32 PrevHighlightedCategory;

	// --- Pieces in active category (outer ring) ---
	int32 HighlightedPieceSlot;    // Index within the category's PieceIndices (-1 = none)
	int32 PrevHighlightedPieceSlot;

	// --- Geometry (unscaled reference pixels; actual size is 65% of screen height) ---
	float CenterHubRadius;  // 70
	float InnerRingInner;   // 70 (flush with hub)
	float InnerRingOuter;   // 155
	float OuterRingInner;   // 175
	float OuterRingOuter;   // 290
	float DeadZone;         // 50

	// --- Animation ---
	float FadeAlpha;
	float FadeSpeed;
	mutable float GlowPulseTime;
	mutable TArray<float> CategoryHoverScales;  // 0→1 interp per category
	mutable TArray<float> PieceHoverScales;     // 0→1 interp per piece slot

	// --- Icon cache (one per piece in AllPieceInfos) ---
	TArray<FSlateBrush> IconBrushes;
	float SegmentIconSize;
	float CenterIconSize;

	// --- Color palette (matched to React prototype) ---
	// Background
	FLinearColor DarkBg;              // #0a0e17
	FLinearColor BgOverlayColor;      // dark vignette overlay

	// Cyan glow family
	FLinearColor Cyan;                // #00e5ff full brightness
	FLinearColor CyanDim;             // #00e5ff at ~27% alpha
	FLinearColor CyanMid;             // #00e5ff at ~53% alpha
	FLinearColor CyanGlow;            // #00e5ff at ~80% alpha

	// Wedge / card fills
	FLinearColor DarkWedge;           // #0f1520
	FLinearColor DarkHover;           // #141e2d
	FLinearColor DarkCard;            // #0d1219
	FLinearColor ActiveWedgeFill;     // #0c1a2a

	// Text
	FLinearColor TextWhite;
	FLinearColor TextDimmed;          // #4a6575
	FLinearColor TextUnavailable;     // #2a3e4a
	FLinearColor SubtitleColor;       // = Cyan

	// Legacy aliases (kept for code that still references them)
	FLinearColor SegmentFillColor;
	FLinearColor SegmentHoverFillColor;
	FLinearColor SegmentUnavailableColor;
	FLinearColor CenterFillColor;
	FLinearColor CenterBorderColor;
	FLinearColor BorderAccentColor;

	// --- Gap between wedges (degrees) ---
	float WedgeGapDeg; // 0.6

	// --- Drawing helpers ---
	void DrawFilledArc(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float InR, float OutR,
		float StartDeg, float EndDeg, FLinearColor Color, int32 ArcSteps = 48) const;

	void DrawArcOutline(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius,
		float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const;

	void DrawCircleFill(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const;

	void DrawLine(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D A, FVector2D B, FLinearColor Color, float Thickness) const;

	FLinearColor Faded(FLinearColor Color) const;
	FLinearColor WithAlpha(FLinearColor Color, float Alpha) const;

	// Build the 4 categories from the piece info list
	void BuildCategories();

	// Given a global piece index, find which category it belongs to
	int32 FindCategoryForPieceIndex(int32 PieceIndex) const;
};
