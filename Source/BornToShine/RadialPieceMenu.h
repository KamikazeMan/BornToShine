// RadialPieceMenu.h - Born To Shine
// Professional radial wheel using FSlateDrawElement::MakeCustomVerts
// for artifact-free filled wedges. No line-stroke hacks.
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
	float WedgeGapDeg;
	float IconSize;
	TArray<FSlateBrush> IconBrushes;
	FSlateBrush WhiteBrush; // Cached white brush for MakeCustomVerts
	struct FCatStyle
	{
		FLinearColor DarkFill;
		FLinearColor LitFill;
		FLinearColor Accent;
		FLinearColor TextColor;
	};
	TArray<FCatStyle> CatStyles;
	FLinearColor HubBg;
	FLinearColor HubBorder;
	FLinearColor TextWhite;
	FLinearColor TextDim;
	FLinearColor DividerColor;
	void BuildCategories();
	int32 FindCategoryForPieceIndex(int32 PieceIndex) const;
	// Professional drawing: triangle-based fills, clean arcs, centered text
	void DrawFilledWedge(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float InR, float OutR,
		float StartDeg, float EndDeg, FLinearColor Color) const;
	void DrawFilledCircle(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius, FLinearColor Color) const;
	void DrawArc(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Center, float Radius,
		float StartDeg, float EndDeg, FLinearColor Color, float Thickness) const;
	void DrawLine2D(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D A, FVector2D B,
		FLinearColor Color, float Thickness) const;
	void DrawTextCentered(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Pos, const FString& Text,
		const FSlateFontInfo& Font, FLinearColor Color) const;
	void DrawIcon(FSlateWindowElementList& Out, int32 LayerId,
		const FGeometry& Geo, FVector2D Pos, const FSlateBrush& Brush,
		float Size, FLinearColor Tint) const;
	FVector2D Polar(FVector2D Center, float Radius, float Deg) const;
};
