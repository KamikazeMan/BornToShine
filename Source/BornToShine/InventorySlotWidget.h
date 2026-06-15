#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryItemTypes.h"
#include "InventorySlotWidget.generated.h"

class UBorder;
class UTextBlock;
class UImage;
class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotClicked, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSlotDragCancelled, UInventoryComponent*, SourceInventory, int32, SourceIndex, FVector2D, ScreenPos);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnSlotDrop, UInventoryComponent*, SourceInventory, int32, SourceIndex, UInventoryComponent*, TargetInventory, int32, TargetIndex, int32, Count, bool, bShiftDown);

/** Payload carried while dragging an inventory stack (container-aware for cross-container drops). */
UCLASS()
class BORNTOSHINE_API UInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY() FName ItemID;
	UPROPERTY() int32 Count = 0;
	UPROPERTY() int32 SourceIndex = -1;
	UPROPERTY() TWeakObjectPtr<UInventoryComponent> SourceInventory;
};

UCLASS()
class BORNTOSHINE_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Input: clicks (placement) and drag-and-drop (move/swap/world-drop).
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geo, const FPointerEvent& Event, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& Geo, const FDragDropEvent& Event, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& Event, UDragDropOperation* InOperation) override;

	void SetSlotData(FName InItemID, int32 InQuantity, UInventoryComponent* InInventoryRef);
	void ClearSlot();
	void SetSelected(bool bSelected);

	int32 SlotIndex = -1;
	FOnSlotClicked OnSlotClicked;
	FOnSlotDragCancelled OnSlotDragCancelled; // (SourceInventory, SourceIndex, ScreenPos) — released off any slot
	FOnSlotDrop OnSlotDrop;                    // (Src, SrcIdx, Tgt, TgtIdx, Count, bShift) — dropped onto this slot

	UInventoryComponent* GetSlotInventory() const { return SlotInventory; }

protected:
	UPROPERTY() UBorder* SelectionBorder;
	UPROPERTY() UImage* ColorRect;
	UPROPERTY() UTextBlock* QuantityText;

	// Applies the cell background brush (gold outline when selected, subtle otherwise).
	void ApplyCellBrush(bool bSelected);

	UPROPERTY() UInventoryComponent* SlotInventory = nullptr;

	FName CurrentItemID;
	int32 CurrentQuantity = 0;
	bool bIsSelected = false;

	FLinearColor GetCategoryColor(const FString& Category) const;
};
