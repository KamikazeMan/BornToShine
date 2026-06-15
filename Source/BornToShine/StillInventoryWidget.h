// Born To Shine - Per-still loading UI: a split view of the player inventory and the still's own
// drag-fed storage container, plus a Start Distilling button.

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
 * Opened by E on a complete still's pot; operates on THAT pot's OwningStand. Shows the player's
 * inventory on the left and the still's storage container on the right. Drag items between them
 * (cross-container via the shared drag foundation); click-transfer works as a fallback. Start
 * Distilling consumes the required ingredients from the still storage.
 */
UCLASS()
class BORNTOSHINE_API UStillInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

	// Point the widget at a still and refresh. Player/storage slot counts must be set first.
	void SetupForStand(AMoonshineCharacter_Simple* InOwner, AStillPartActor* InStand);

	void Refresh();

	// True when a screen point lies within the panel (drag-release bounds test).
	bool IsScreenInsidePanel(const FVector2D& ScreenPos) const;

	// Built-grid sizing — set by the character before the widget is constructed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI") int32 PlayerSlots = 24;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI") int32 PlayerColumns = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI") int32 StorageSlots = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI") int32 StorageColumns = 4;

protected:
	UPROPERTY() UBorder* Panel = nullptr;
	UPROPERTY() UTextBlock* TitleText = nullptr;
	UPROPERTY() UTextBlock* RequirementText = nullptr;
	UPROPERTY() UTextBlock* StatusText = nullptr;
	UPROPERTY() UButton* StartButton = nullptr;

	UPROPERTY() TArray<UInventorySlotWidget*> PlayerSlotWidgets;
	UPROPERTY() TArray<UInventorySlotWidget*> StorageSlotWidgets;

	TWeakObjectPtr<AMoonshineCharacter_Simple> Owner;
	TWeakObjectPtr<AStillPartActor> Stand;
	UPROPERTY() UInventoryComponent* PlayerInv = nullptr;
	UPROPERTY() UInventoryComponent* StorageInv = nullptr;

	bool bShowShortWarning = false;

	UUniformGridPanel* BuildGrid(int32 NumSlots, int32 Columns, bool bStorageSide, TArray<UInventorySlotWidget*>& OutSlots);

	UFUNCTION() void HandlePlayerSlotClicked(int32 Index);
	UFUNCTION() void HandleStorageSlotClicked(int32 Index);
	UFUNCTION() void HandleSlotDragCancelled(UInventoryComponent* SourceInventory, int32 SourceIndex, FVector2D ScreenPos);
	UFUNCTION() void HandleInventoryChanged();
	UFUNCTION() void OnStartClicked();
	UFUNCTION() void OnCloseClicked();
};
