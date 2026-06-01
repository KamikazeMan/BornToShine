#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "InventoryItemTypes.generated.h"

class ABuildablePiece;

USTRUCT(BlueprintType)
struct FInventoryItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Quantity = 0;

	FInventoryItem() : ItemID(NAME_None), Quantity(0) {}
	FInventoryItem(FName InID, int32 InQty) : ItemID(InID), Quantity(InQty) {}
};

USTRUCT(BlueprintType)
struct FItemDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* Icon = nullptr;

	// The buildable piece class to spawn when this item is placed (still parts, framing, etc.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<ABuildablePiece> PlaceableClass;

	// Static mesh spawned when this item is placed from the inventory (assigned per-row in the data table)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxStack = 99;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Category = TEXT("General");
};
