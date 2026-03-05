// RadialPieceMenu.h
// Born To Shine - In-Place Radial Piece Selection Menu
//
// NEW DESIGN (replaces old two-tier card layout):
//   - Main view: 4 category wedges fill the ring (Foundation, Floor, Walls, Roof)
//   - Sub view: click a category → wheel REPLACES with that category's piece wedges
//   - Center hub shows category info + BACK button when in sub view
//   - All text curves along the arc of each wedge
//   - Each wedge has an icon slot (UTexture2D) above the curved text
//   - Transparent background (see through to game world)
//   - Cyan glow (#00e5ff) on all edges with pulsing bloom
//   - Smooth animated transition between main ↔ sub views

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConstructionTypes.h"
#include "Styling/SlateBrush.h"
#include "RadialPieceMenu.generated.h"

/** Category grouping for the main wheel */
USTRUCT()
struct FCategoryInfo
{
	GENERATED_BODY()

	FString Name;
	FString Icon; // Unicode fallback icon
	TArray<int32> PieceIndices; // Indices into AllPieceInfos
};

/** Current display state of the wheel */
UENUM()
enum class ERadialMenuView : uint8
{
	Main,   // Showing 4 category wedges
	Sub     // Showing piece wedges for the active category
};

UCLASS()
class BORNTOSHINE_API URadialPieceMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	URadialPieceMenu(const FObjectInitializer& ObjectInitializer);

	/** Populate from FPieceTypeInfo array; CurrentIndex pre-highlights that piece */
	void InitMenu(const TArray<FPieceTypeInfo>& InInfos, int32 CurrentIndex);

	/** Returns the globally selected piece index (-1 = none) */
	int32 GetHighlightedIndex() const;

	// Sound hooks (implement in BP or override in C++)
	void PlaySoundOpen();
	void PlaySoundClose();
	void PlaySoundHover();
	void PlaySoundSelect();
	void PlaySoundBack();

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	// ===== DATA =====
	TArray<FPieceTypeInfo> AllPieceInfos;
	TArray<FCategoryInfo> Categories;

	// ===== STATE =====
	ERadialMenuView CurrentView;
	int32 ActiveCategory;           // Which category is expanded (-1 = none)
	int32 HighlightedCategory;      // Hovered category in main view (-1 = none)
	int32 HighlightedPieceSlot;     // Hovered piece in sub view (-1 = none)
	int32 SelectedPieceSlot;        // Tapped/clicked piece that stays lit (-1 = none)
	int32 PrevHighlightedCategory;
	int32 PrevHighlightedPieceSlot;

	// ===== ANIMATION =====
	float FadeAlpha;
	float FadeSpeed;
	mutable float GlowPulseTime;

	// View transition: 0.0 = fully main, 1.0 = fully sub
	float ViewTransition;
	float ViewTransitionSpeed;

	// Per-wedge hover interpolation
	mutable TArray<float> CategoryHoverScales;   // main view
	mutable TArray<float> PieceHoverScales;      // sub view

	// ===== GEOMETRY (unscaled reference pixels) =====
	// The wheel scales to 65% of viewport height
	float InnerRadius;     // 50  - inner edge of wedge ring / hub outer edge
	float OuterRadius;     // 155 - outer edge of wedge ring
	float HubRadius;       // 46  - center hub fill radius
	float DeadZone;        // 30  - ignore mouse inside this radius
	float WedgeGapDeg;     // 1.0 - gap between wedges in degrees

	// ===== ICONS =====
	float IconSize;        // Reference icon size in wedge (unscaled)
	TArray<FSlateBrush> IconBrushes; // One per AllPieceInfos entry

	// ===== COLOR PALETTE =====
	// Backgrounds
	FLinearColor DarkBg;             // #060810 - hub fill
	FLinearColor DarkWedge;          // #0f1520 - default wedge fill
	FLinearColor DarkHover;          // #141e2d - hovered wedge fill
	FLinearColor ActiveWedgeFill;    // #0c1a2a - active category wedge

	// Cyan glow family
	FLinearColor Cyan;               // #00e5ff - primary glow
	FLinearColor CyanDim;            // #00e5ff @ 27% alpha
	FLinearColor CyanMid;            // #00e5ff @ 53% alpha

	// Text
	FLinearColor TextWhite;
	FLinearColor TextDimmed;         // #4a6575
	FLinearColor TextUnavailable;    // #2a3e4a

	// Per-category accent colors (used in sub view for variety)
	// These tint the wedge slightly so each category feels distinct
	struct FCategoryColors
	{
		FLinearColor Accent;     // The category's signature color
		FLinearColor WedgeDim;   // Dim fill for piece wedge
		FLinearColor WedgeLit;   // Lit fill for hovered/selected piece wedge
	};
	TArray<FCategoryColors> CategoryColorTable;

	// ===== BUILDING HELPERS =====
	void BuildCategories();
	int32 FindCategoryForPieceIndex(int32 PieceIndex) const;

	// ===== DRAWING HELPERS =====
	/** Draw filled annular sector (wedge) */
	void DrawFilledArc(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float InR, float OutR,
		float StartDeg, float EndDeg, FLinearColor Color, int32 Steps = 48) const;

	/** Draw arc outline stroke at given radius */
	void DrawArcOutline(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius,
		float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const;

	/** Draw filled circle */
	void DrawCircleFill(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const;

	/** Draw line between two points */
	void DrawLine(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D A, FVector2D B, FLinearColor Color, float Thickness) const;

	/** Draw text curved along an arc.
	 *  Text is centered at MidAngleDeg, placed at Radius from Center.
	 *  Each character is individually rotated to follow the curve. */
	void DrawCurvedText(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius,
		float MidAngleDeg, const FString& Text, const FSlateFontInfo& Font,
		FLinearColor Color, float CharSpacingDeg = 4.5f) const;

	/** Draw a texture icon at a polar position */
	void DrawIconAtAngle(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius, float AngleDeg,
		const FSlateBrush& Brush, float DrawSize, FLinearColor Tint) const;

	// Color utilities
	FLinearColor Faded(FLinearColor Color) const;
	FLinearColor WithAlpha(FLinearColor Color, float Alpha) const;

	// ===== PAINT SUB-FUNCTIONS =====
	// Broken out for readability — each draws one layer of the wheel

	/** Paint the main view: 4 category wedges + center hub */
	int32 PaintMainView(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Scale) const;

	/** Paint the sub view: N piece wedges for ActiveCategory + center back button */
	int32 PaintSubView(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Scale) const;

	/** Paint the center hub (shared by both views) */
	int32 PaintCenterHub(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Scale) const;

	/** Paint pulsing outer ring decorations + tick marks */
	int32 PaintRingDecorations(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Scale) const;
};
