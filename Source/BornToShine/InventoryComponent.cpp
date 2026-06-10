#include "InventoryComponent.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UInventoryComponent::AddItem(FName ItemID, int32 Quantity)
{
	if (Quantity <= 0 || ItemID == NAME_None) return 0;

	FItemDataRow* Data = GetItemDataRaw(ItemID);
	const int32 MaxStack = Data ? Data->MaxStack : 99;

	int32 Remaining = Quantity;

	// Top up existing stacks first.
	for (FInventoryItem& Item : Items)
	{
		if (Remaining <= 0) break;
		if (Item.ItemID == ItemID && Item.Quantity < MaxStack)
		{
			const int32 ToAdd = FMath::Min(Remaining, MaxStack - Item.Quantity);
			Item.Quantity += ToAdd;
			Remaining -= ToAdd;
		}
	}

	// Overflow the remainder into new stacks while grid slots are available.
	while (Remaining > 0 && Items.Num() < MaxSlots)
	{
		const int32 ToAdd = FMath::Min(Remaining, MaxStack);
		Items.Add(FInventoryItem(ItemID, ToAdd));
		Remaining -= ToAdd;
	}

	const int32 Added = Quantity - Remaining;
	if (Added < Quantity)
	{
		// Never silent: every shortfall (including +0) is logged.
		UE_LOG(LogTemp, Warning, TEXT("Inventory: wanted +%d %s, added %d (full)"), Quantity, *ItemID.ToString(), Added);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Inventory: +%d %s (now %d)"), Added, *ItemID.ToString(), GetItemCount(ItemID));
	}

	if (Added > 0)
	{
		OnInventoryChanged.Broadcast();
	}
	return Added;
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
			OnInventoryChanged.Broadcast();
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
	AddItem(TEXT("MasonJar"), 1);
	AddItem(TEXT("MasonJarLid"), 1);
	AddItem(TEXT("Water"), 5);
	AddItem(TEXT("Mash"), 5);
	AddItem(TEXT("Firewood"), 9);
	DebugLogInventory();
}
