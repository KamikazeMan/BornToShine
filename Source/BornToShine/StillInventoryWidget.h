// Born To Shine - Per-still loading UI: the still's own ingredient storage container, fed by
// dragging from the hotbar; plus the batch requirement readout and Start Distilling.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StillInventoryWidget.generated.h"

class UBorder;
class UTextBlock;
class UButton;
class UUniformGridPanel;
class UInventorySlotWidget;
class UInventoryComponent;
class AMoonshineCharacter_Simple;
class AStillPartActor;

/**
 * Opened by E on a complete still's pot; operates on THAT pot's OwningStand. Shows ONLY the still's
 * storage container (no header label), the requirement readout, Start Distilling, and Close.
 * Ingredients are loaded by dragging from the hotbar (which stays visible behind this panel).
 */
UCLASS()
class BORNTOSHINE_API UStillInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

	void SetupForStand(AMoonshineCharacter_Simple* InOwner, AStillPartActor* InStand);
	void Refresh();

	// True when a screen point lies within the panel (drag-release bounds test).
	bool IsScreenInsidePanel(const FVector2D& ScreenPos) const;

	// Built-grid sizing — set by the character before the widget is constructed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI") int32 StorageSlots = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI") int32 StorageColumns = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI") float SlotPadding = 6.0f;

protected:
	UPROPERTY() UBorder* Panel = nullptr;
	UPROPERTY() UTextBlock* RequirementText = nullptr;
	UPROPERTY() UTextBlock* StatusText = nullptr;
	UPROPERTY() UButton* StartButton = nullptr;

	UPROPERTY() TArray<UInventorySlotWidget*> StorageSlotWidgets;

	TWeakObjectPtr<AMoonshineCharacter_Simple> Owner;
	TWeakObjectPtr<AStillPartActor> Stand;
	UPROPERTY() UInventoryComponent* PlayerInv = nullptr;
	UPROPERTY() UInventoryComponent* StorageInv = nullptr;

	bool bShowShortWarning = false;

	UFUNCTION() void HandleStorageSlotClicked(int32 Index);
	UFUNCTION() void HandleSlotDragCancelled(UInventoryComponent* SourceInventory, int32 SourceIndex, FVector2D ScreenPos);
	UFUNCTION() void HandleSlotDrop(UInventoryComponent* SourceInventory, int32 SourceIndex, UInventoryComponent* TargetInventory, int32 TargetIndex, int32 Count, bool bShiftDown);
	UFUNCTION() void HandleInventoryChanged();
	UFUNCTION() void OnStartClicked();
	UFUNCTION() void OnCloseClicked();
};
