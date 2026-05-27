#include "InventoryComponent.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UInventoryComponent::AddItem(FName ItemID, int32 Quantity)
{
	if (Quantity <= 0 || ItemID == NAME_None) return 0;

	FItemDataRow* Data = GetItemDataRaw(ItemID);
	int32 MaxStack = Data ? Data->MaxStack : 99;

	// Find existing stack
	for (FInventoryItem& Item : Items)
	{
		if (Item.ItemID == ItemID)
		{
			int32 SpaceLeft = MaxStack - Item.Quantity;
			int32 ToAdd = FMath::Min(Quantity, SpaceLeft);
			Item.Quantity += ToAdd;
			UE_LOG(LogTemp, Log, TEXT("Inventory: +%d %s (now %d)"), ToAdd, *ItemID.ToString(), Item.Quantity);
			return ToAdd;
		}
	}

	// New stack
	int32 ToAdd = FMath::Min(Quantity, MaxStack);
	Items.Add(FInventoryItem(ItemID, ToAdd));
	UE_LOG(LogTemp, Log, TEXT("Inventory: +%d %s (new stack)"), ToAdd, *ItemID.ToString());
	return ToAdd;
}

bool UInventoryComponent::RemoveItem(FName ItemID, int32 Quantity)
{
	if (Quantity <= 0 || ItemID == NAME_None) return false;

	for (int32 i = 0; i < Items.Num(); i++)
	{
		if (Items[i].ItemID == ItemID)
		{
			if (Items[i].Quantity < Quantity) return false;
			Items[i].Quantity -= Quantity;
			UE_LOG(LogTemp, Log, TEXT("Inventory: -%d %s (now %d)"), Quantity, *ItemID.ToString(), Items[i].Quantity);
			if (Items[i].Quantity <= 0)
			{
				Items.RemoveAt(i);
			}
			return true;
		}
	}
	return false;
}

bool UInventoryComponent::HasItem(FName ItemID, int32 Quantity) const
{
	return GetItemCount(ItemID) >= Quantity;
}

int32 UInventoryComponent::GetItemCount(FName ItemID) const
{
	for (const FInventoryItem& Item : Items)
	{
		if (Item.ItemID == ItemID) return Item.Quantity;
	}
	return 0;
}

bool UInventoryComponent::GetItemData(FName ItemID, FItemDataRow& OutData) const
{
	FItemDataRow* Row = GetItemDataRaw(ItemID);
	if (Row)
	{
		OutData = *Row;
		return true;
	}
	return false;
}

FItemDataRow* UInventoryComponent::GetItemDataRaw(FName ItemID) const
{
	if (!ItemDataTable) return nullptr;
	return ItemDataTable->FindRow<FItemDataRow>(ItemID, TEXT("InventoryComponent::GetItemData"));
}

void UInventoryComponent::DebugLogInventory() const
{
	UE_LOG(LogTemp, Warning, TEXT("=== INVENTORY (%d items) ==="), Items.Num());
	for (const FInventoryItem& Item : Items)
	{
		UE_LOG(LogTemp, Warning, TEXT("  %s x%d"), *Item.ItemID.ToString(), Item.Quantity);
	}
	UE_LOG(LogTemp, Warning, TEXT("==========================="));
}

void UInventoryComponent::DebugGrantStillParts()
{
	UE_LOG(LogTemp, Warning, TEXT("DEBUG: Granting all still parts"));
	AddItem(TEXT("CinderBlockStand"), 1);
	AddItem(TEXT("Pot"), 1);
	AddItem(TEXT("Cap"), 1);
	AddItem(TEXT("CapArm"), 1);
	AddItem(TEXT("ThumperBody"), 1);
	AddItem(TEXT("ThumperCap"), 1);
	AddItem(TEXT("OutletPipe"), 1);
	AddItem(TEXT("WormBarrel"), 1);
	AddItem(TEXT("WormCoil"), 1);
	AddItem(TEXT("Thermometer"), 1);
	DebugLogInventory();
}
