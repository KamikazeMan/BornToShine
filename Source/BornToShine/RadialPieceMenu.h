// Born To Shine - Radial Piece Selection Menu (Rust/Fortnite quality)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConstructionTypes.h"
#include "Styling/SlateBrush.h"
#include "RadialPieceMenu.generated.h"

/**
 * Production-quality radial wheel menu for selecting construction piece types.
 * Hold Tab to open, move mouse to highlight segment, release to select.
 *
 * Visual design (Rust / Satisfactory reference):
 *   - Dark frosted semi-transparent backdrop (70% opacity)
 *   - Large prominent icons per segment (64x64+)
 *   - Piece name + size subtitle under each icon
 *   - Selected segment: warm wood-tone highlight with soft glow
 *   - Center hub: large icon + name of hovered piece
 *   - Thin gold/bronze accent border, clean thin dividers
 *   - Smooth fade-in, per-segment hover interpolation
 *   - Unavailable segments dimmed out
 *   - No ruler/tick marks on outer ring
 *
 * All rendering in C++ NativePaint — no Blueprint widgets.
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

	// Sound effect hooks (stubs — wire up audio later)
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

	// --- Geometry (pixels) — doubled from original ---
	float OuterRadius;
	float InnerRadius;
	float DeadZone;
	float CenterHubRadius;

	// --- Animation ---
	float FadeAlpha;
	float FadeSpeed;
	mutable TArray<float> SegmentHoverScales; // Per-segment 0→1 hover interpolation

	// --- Icon brush cache ---
	TArray<FSlateBrush> IconBrushes;
	float SegmentIconSize;        // Icon size in radial segments (64+)
	float CenterIconSize;         // Enlarged icon in center hub

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

	// --- Drawing helpers ---
	void DrawFilledArc(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float InR, float OutR,
		float StartDeg, float EndDeg, FLinearColor Color, int32 ArcSteps = 48) const;

	void DrawArcOutline(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius,
		float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const;

	void DrawCircleFill(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const;

	// Multiply color alpha by FadeAlpha
	FLinearColor Faded(FLinearColor Color) const;
};
