// RadialPieceMenu.h - Born To Shine
// Matches the React prototype EXACTLY:
//   Dark background wheel, 4 colored category wedges, emoji icons
//   Click category -> wheel swaps to piece wedges in-place
//   Click center -> back to categories
//   NO tick marks, NO pulsing outer rings, NO old radial wheel elements
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
	// Per-category colors (dark fills + accent borders)
	struct FCatStyle
	{
		FLinearColor DarkFill;   // The dark tinted wedge background
		FLinearColor LitFill;    // Hovered/selected wedge fill
		FLinearColor Accent;     // Border and text glow color
		FLinearColor TextColor;  // Category name color
	};
	TArray<FCatStyle> CatStyles;
	// Shared colors
	FLinearColor HubBg;
	FLinearColor HubBorder;
	FLinearColor TextWhite;
	FLinearColor TextDim;
	FLinearColor DividerColor;
	void BuildCategories();
	int32 FindCategoryForPieceIndex(int32 PieceIndex) const;
	// Drawing helpers - simple and clean
	void DrawWedgeFill(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float InR, float OutR,
		float StartDeg, float EndDeg, FLinearColor Color) const;
	void DrawArc(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius,
		float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const;
	void DrawRadialLine(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D A, FVector2D B, FLinearColor Color, float Thickness) const;
	void DrawCircle(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const;
	void DrawText(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Position, const FString& Text,
		const FSlateFontInfo& Font, FLinearColor Color) const;
	void DrawTextCentered(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, const FString& Text,
		const FSlateFontInfo& Font, FLinearColor Color) const;
	void DrawIconAt(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, const FSlateBrush& Brush,
		float Size, FLinearColor Tint) const;
	FVector2D PolarToCart(FVector2D Center, float Radius, float Deg) const;
};
