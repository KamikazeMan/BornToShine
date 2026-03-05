// RadialPieceMenu.h
// Born To Shine - In-Place Radial Piece Selection Menu

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConstructionTypes.h"
#include "Styling/SlateBrush.h"
#include "RadialPieceMenu.generated.h"

USTRUCT()
struct FCategoryInfo
{
	GENERATED_BODY()

	FString Name;
	FString Icon;
	TArray<int32> PieceIndices;
};

UENUM()
enum class ERadialMenuView : uint8
{
	Main,
	Sub
};

UCLASS()
class BORNTOSHINE_API URadialPieceMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	URadialPieceMenu(const FObjectInitializer& ObjectInitializer);

	void InitMenu(const TArray<FPieceTypeInfo>& InInfos, int32 CurrentIndex);
	int32 GetHighlightedIndex() const;

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
	TArray<FPieceTypeInfo> AllPieceInfos;
	TArray<FCategoryInfo> Categories;

	ERadialMenuView CurrentView;
	int32 ActiveCategory;
	int32 HighlightedCategory;
	int32 HighlightedPieceSlot;
	int32 SelectedPieceSlot;
	int32 PrevHighlightedCategory;
	int32 PrevHighlightedPieceSlot;

	float FadeAlpha;
	float FadeSpeed;
	mutable float GlowPulseTime;
	float ViewTransition;
	float ViewTransitionSpeed;
	mutable TArray<float> CategoryHoverScales;
	mutable TArray<float> PieceHoverScales;

	float InnerRadius;
	float OuterRadius;
	float HubRadius;
	float DeadZone;
	float WedgeGapDeg;
	float IconSize;

	TArray<FSlateBrush> IconBrushes;

	FLinearColor DarkBg;
	FLinearColor DarkWedge;
	FLinearColor DarkHover;
	FLinearColor ActiveWedgeFill;
	FLinearColor Cyan;
	FLinearColor CyanDim;
	FLinearColor CyanMid;
	FLinearColor TextWhite;
	FLinearColor TextDimmed;
	FLinearColor TextUnavailable;

	struct FCategoryColors
	{
		FLinearColor Accent;
		FLinearColor WedgeDim;
		FLinearColor WedgeLit;
	};
	TArray<FCategoryColors> CategoryColorTable;

	void BuildCategories();
	int32 FindCategoryForPieceIndex(int32 PieceIndex) const;

	void DrawFilledArc(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float InR, float OutR,
		float StartDeg, float EndDeg, FLinearColor Color, int32 Steps = 48) const;

	void DrawArcOutline(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius,
		float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const;

	void DrawCircleFill(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const;

	void DrawLine(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D A, FVector2D B, FLinearColor Color, float Thickness) const;

	void DrawTextAtAngle(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius, float AngleDeg,
		const FString& Text, const FSlateFontInfo& Font, FLinearColor Color) const;

	void DrawIconAtAngle(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius, float AngleDeg,
		const FSlateBrush& Brush, float DrawSize, FLinearColor Tint) const;

	FLinearColor Faded(FLinearColor Color) const;
	FLinearColor WithAlpha(FLinearColor Color, float Alpha) const;

	int32 PaintMainView(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Scale) const;
	int32 PaintSubView(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Scale) const;
	int32 PaintCenterHub(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Scale) const;
	int32 PaintRingDecorations(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Scale) const;
};
