#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryItemTypes.h"
#include "InventoryComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BORNTOSHINE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// Data table assigned in Blueprint defaults — define all items (still parts, ingredients, etc.) here
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	UDataTable* ItemDataTable;

	// Add to inventory. Returns actual amount added (capped at max stack).
	UFUNCTION(BlueprintCallable, Category="Inventory")
	int32 AddItem(FName ItemID, int32 Quantity);

	// Remove from inventory. Returns true if had enough to remove.
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool RemoveItem(FName ItemID, int32 Quantity);

	// Check if inventory has at least Quantity of ItemID.
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool HasItem(FName ItemID, int32 Quantity = 1) const;

	// Get current quantity of an item (0 if none).
	UFUNCTION(BlueprintCallable, Category="Inventory")
	int32 GetItemCount(FName ItemID) const;

	// Look up a data row by ItemID. Returns nullptr if not found.
	UFUNCTION(BlueprintCallable, Category="Inventory")
	FItemDataRow* GetItemData(FName ItemID) const;

	// Get all items currently in inventory (for UI later).
	UFUNCTION(BlueprintCallable, Category="Inventory")
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
