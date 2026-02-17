// Born To Shine - Two-Tier Radial Piece Selection Menu (Sci-fi holographic style)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConstructionTypes.h"
#include "Styling/SlateBrush.h"
#include "RadialPieceMenu.generated.h"

/**
 * Two-tier radial menu for piece selection:
 *   Inner ring: 4 categories (Foundation, Floor, Walls, Roof)
 *   Outer ring: pieces within the highlighted category
 *
 * Hold Tab to open, move mouse to highlight, release to select.
 *
 * Visual design (sci-fi holographic blueprint):
 *   - Dark navy/charcoal backdrop (75% opacity)
 *   - Inner ring: category segments with icons
 *   - Outer ring: expands to show pieces in hovered category
 *   - Selected segment: bright turquoise/cyan highlight
 *   - Center hub: piece name + subtitle + lock message
 *   - Smooth fade-in, per-segment hover interpolation, breathing glow pulse
 *   - Unavailable pieces dimmed out
 *
 * All rendering in C++ NativePaint.
 */

/** Category definition for the inner ring */
USTRUCT()
struct FCategoryInfo
{
	GENERATED_BODY()

	FString Name;
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

	// --- Geometry (pixels) ---
	float InnerRingInner;   // Inner edge of category ring
	float InnerRingOuter;   // Outer edge of category ring
	float OuterRingInner;   // Inner edge of piece ring
	float OuterRingOuter;   // Outer edge of piece ring
	float CenterHubRadius;
	float DeadZone;

	// --- Animation ---
	float FadeAlpha;
	float FadeSpeed;
	mutable float GlowPulseTime;
	mutable TArray<float> CategoryHoverScales;
	mutable TArray<float> PieceHoverScales;

	// --- Icon cache (one per piece in AllPieceInfos) ---
	TArray<FSlateBrush> IconBrushes;
	float SegmentIconSize;
	float CenterIconSize;

	// --- Color palette ---
	FLinearColor BgOverlayColor;
	FLinearColor SegmentFillColor;
	FLinearColor SegmentHoverFillColor;
	FLinearColor SegmentHoverGlowColor;
	FLinearColor SegmentUnavailableColor;
	FLinearColor DividerColor;
	FLinearColor BorderAccentColor;
	FLinearColor CenterFillColor;
	FLinearColor CenterBorderColor;
	FLinearColor TextWhite;
	FLinearColor TextDimmed;
	FLinearColor TextUnavailable;
	FLinearColor SubtitleColor;
	FLinearColor CategoryTextColor;

	// --- Drawing helpers ---
	void DrawFilledArc(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float InR, float OutR,
		float StartDeg, float EndDeg, FLinearColor Color, int32 ArcSteps = 48) const;

	void DrawArcOutline(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius,
		float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const;

	void DrawCircleFill(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const;

	FLinearColor Faded(FLinearColor Color) const;

	// Build the 4 categories from the piece info list
	void BuildCategories();

	// Given a global piece index, find which category it belongs to
	int32 FindCategoryForPieceIndex(int32 PieceIndex) const;
};
