// Born To Shine - Player hotbar: a view onto the first N inventory slots (not a separate container).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HotbarWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UInventorySlotWidget;
class UInventoryComponent;

/**
 * Always-on bottom-center row showing inventory slots 0..NumSlots-1. Reuses InventorySlotWidget,
 * so drag/drop, world-drop, merge/swap all work through the shared inventory drag foundation.
 */
UCLASS()
class BORNTOSHINE_API UHotbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void SetInventoryComponent(UInventoryComponent* InInventory);
	void RefreshSlots();
	void SetActiveSlot(int32 Index);

	// True when a screen-space point lies within the hotbar panel (drag-release bounds test).
	bool IsScreenInsidePanel(const FVector2D& ScreenPos) const;

	// Number of inventory slots mirrored (set by the character before the widget is built).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	int32 NumSlots = 6;

protected:
	UPROPERTY() UBorder* PanelBackground;
	UPROPERTY() UHorizontalBox* Row;
	UPROPERTY() TArray<UInventorySlotWidget*> SlotWidgets;
	UPROPERTY() UInventoryComponent* InventoryRef;

	int32 ActiveSlot = 0;

	UFUNCTION() void HandleSlotClicked(int32 SlotIndex);
	UFUNCTION() void HandleSlotDropped(int32 FromIndex, int32 ToIndex);
	UFUNCTION() void HandleSlotDragCancelled(int32 SourceIndex, FVector2D ScreenPos);
	UFUNCTION() void HandleInventoryChanged();
};
