#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryItemTypes.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BORNTOSHINE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryChanged OnInventoryChanged;

	// Data table assigned in Blueprint defaults — define all items (still parts, ingredients, etc.) here
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	UDataTable* ItemDataTable;

	// Maximum number of stacks (matches the inventory grid: 6 columns x 4 rows).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 MaxSlots = 24;

	// Add to inventory. Fills existing stacks first, then overflows into new stacks while slots
	// are available. Returns actual amount added; logs a warning when less than requested fits.
	UFUNCTION(BlueprintCallable, Category="Inventory")
	int32 AddItem(FName ItemID, int32 Quantity);

	// Remove from inventory. Returns true if had enough to remove.
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool RemoveItem(FName ItemID, int32 Quantity);

	// Remove everything (used by save/load restore).
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void ClearInventory();

	// Drag-and-drop foundation: move/swap stack at FromIndex onto ToIndex. Merges when both hold
	// the same item under MaxStack, otherwise swaps. No-op for invalid/empty targets.
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void MoveOrMergeStack(int32 FromIndex, int32 ToIndex);

	// Cross-container drag: move/merge/swap the stack at Source[FromIndex] onto this[ToIndex].
	// When Source == this it behaves like MoveOrMergeStack. An empty target slot pulls the whole
	// source stack into this container (capacity permitting).
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void TransferFrom(UInventoryComponent* Source, int32 FromIndex, int32 ToIndex);

	// Check if inventory has at least Quantity of ItemID.
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool HasItem(FName ItemID, int32 Quantity = 1) const;

	// Get current quantity of an item (0 if none).
	UFUNCTION(BlueprintCallable, Category="Inventory")
	int32 GetItemCount(FName ItemID) const;

	// Look up a data row by ItemID. Returns true if found.
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool GetItemData(FName ItemID, FItemDataRow& OutData) const;

	// C++ only: raw pointer lookup (returns nullptr if not found)
	FItemDataRow* GetItemDataRaw(FName ItemID) const;

	// Get all items currently in inventory (for UI later).
	const TArray<FInventoryItem>& GetItems() const { return Items; }

	// Debug: dump inventory to log.
	UFUNCTION(BlueprintCallable, Category="Inventory|Debug")
	void DebugLogInventory() const;

	// Debug: grant all still parts at once for testing placement.
	UFUNCTION(BlueprintCallable, Category="Inventory|Debug")
	void DebugGrantStillParts();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	TArray<FInventoryItem> Items;
};
