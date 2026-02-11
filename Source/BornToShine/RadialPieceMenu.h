// Born To Shine - Radial Piece Selection Menu (C++ UMG Widget)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RadialPieceMenu.generated.h"

/**
 * Radial wheel menu for selecting piece types.
 * Hold Tab to open, move mouse to highlight, release to select.
 * All rendering done in C++ via NativePaint — no Blueprint needed.
 */
UCLASS()
class BORNTOSHINE_API URadialPieceMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	URadialPieceMenu(const FObjectInitializer& ObjectInitializer);

	/** Set up segments with display names; CurrentIndex pre-highlights that segment */
	void InitMenu(const TArray<FString>& InNames, int32 CurrentIndex);

	/** Which segment is the mouse over (-1 = none) */
	int32 GetHighlightedIndex() const { return HighlightedIndex; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TArray<FString> SegmentNames;
	int32 HighlightedIndex;
	int32 NumSegments;

	// Geometry (pixels)
	float OuterRadius;
	float InnerRadius;
	float DeadZone;
	float SegmentGapDeg; // degree gap between segments

	// Drawing helpers
	void DrawFilledArc(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float InR, float OutR,
		float StartDeg, float EndDeg, FLinearColor Color) const;

	void DrawArcOutline(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius,
		float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const;
};
