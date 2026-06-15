#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryGridWidget.generated.h"

class UInventoryComponent;
class UInventorySlotWidget;
class UUniformGridPanel;
class UTextBlock;
class UImage;

UCLASS()
class BORNTOSHINE_API UInventoryGridWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void SetInventoryComponent(UInventoryComponent* InInventory);
	void RefreshGrid();

	// True when a screen-space point lies within the visible grid panel (drag-release bounds test).
	bool IsScreenInsidePanel(const FVector2D& ScreenPos) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	UTexture2D* BackgroundTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 NumColumns = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 NumRows = 4;

	// Gap between grid cells (px).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	float SlotPadding = 6.0f;

protected:
	UPROPERTY() UUniformGridPanel* SlotGrid;
	UPROPERTY() UTextBlock* TitleText;
	UPROPERTY() UTextBlock* StatusText;
	UPROPERTY() UImage* BackgroundImage;

	UPROPERTY() TArray<UInventorySlotWidget*> SlotWidgets;
	UPROPERTY() UInventoryComponent* InventoryRef;

	int32 SelectedSlotIndex = -1;

	UFUNCTION() void HandleSlotClicked(int32 SlotIndex);
	UFUNCTION() void HandleInventoryChanged();

	// Drag-and-drop: a stack released off any slot. The character decides cancel vs world-drop.
	UFUNCTION() void HandleSlotDragCancelled(class UInventoryComponent* SourceInventory, int32 SourceIndex, FVector2D ScreenPos);

	// Drag-and-drop: a stack dropped onto a slot. The character decides whole-move vs split slider.
	UFUNCTION() void HandleSlotDrop(class UInventoryComponent* SourceInventory, int32 SourceIndex, class UInventoryComponent* TargetInventory, int32 TargetIndex, int32 Count, bool bShiftDown);
};
