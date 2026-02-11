// Born To Shine - Polished Radial Piece Selection Menu (C++ UMG Widget)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConstructionTypes.h"
#include "Styling/SlateBrush.h"
#include "RadialPieceMenu.generated.h"

/**
 * Polished radial wheel menu for selecting construction piece types.
 * Hold Tab to open, move mouse to highlight segment, release to select.
 *
 * Visual features:
 *   - Dark frosted-glass overlay with warm wood-tone accent colors
 *   - Icon textures per segment (set via FPieceTypeInfo)
 *   - Smooth fade-in, per-segment hover scale + glow
 *   - Hovered piece name displayed in center hub
 *   - Unavailable segments grayed out
 *   - Decorative border ring with wood accent
 *
 * All rendering done in C++ via NativePaint — no Blueprint widget needed.
 */
UCLASS()
class BORNTOSHINE_API URadialPieceMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	URadialPieceMenu(const FObjectInitializer& ObjectInitializer);

	/** Populate segments from FPieceTypeInfo; CurrentIndex pre-highlights that segment */
	void InitMenu(const TArray<FPieceTypeInfo>& InInfos, int32 CurrentIndex);

	/** Which segment is the mouse over (-1 = none) */
	int32 GetHighlightedIndex() const { return HighlightedIndex; }

	// --- Sound effect hooks (stubs — wire up audio later) ---
	void PlaySoundOpen();
	void PlaySoundClose();
	void PlaySoundHover();

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// --- Segment data ---
	TArray<FPieceTypeInfo> SegmentInfos;
	int32 HighlightedIndex;
	int32 PrevHighlightedIndex;
	int32 NumSegments;

	// --- Geometry (pixels) ---
	float OuterRadius;
	float InnerRadius;
	float DeadZone;
	float SegmentGapDeg;
	float HoverGlowExtend;
	float BorderWidth;

	// --- Animation ---
	float FadeAlpha;
	float FadeSpeed;
	TArray<float> SegmentHoverScales; // Per-segment 0→1 hover interpolation

	// --- Icon brush cache ---
	TArray<FSlateBrush> IconBrushes;
	float IconDisplaySize;

	// --- Color palette (warm construction theme) ---
	FLinearColor BgOverlayColor;
	FLinearColor SegmentFillColor;
	FLinearColor SegmentHoverColor;
	FLinearColor SegmentGlowColor;
	FLinearColor SegmentUnavailableColor;
	FLinearColor DividerColor;
	FLinearColor OuterOutlineColor;
	FLinearColor InnerOutlineColor;
	FLinearColor CenterFillColor;
	FLinearColor BorderRingColor;
	FLinearColor TextNormalColor;
	FLinearColor TextHighlightColor;
	FLinearColor TextUnavailableColor;
	FLinearColor SubtitleNormalColor;

	// --- Drawing helpers ---
	void DrawFilledArc(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float InR, float OutR,
		float StartDeg, float EndDeg, FLinearColor Color) const;

	void DrawArcOutline(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius,
		float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const;

	// Apply fade alpha to a color
	FLinearColor Faded(FLinearColor Color) const;
};
