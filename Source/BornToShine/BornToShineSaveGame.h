// Born To Shine - Save game data (v1: inventory, money, placed still parts)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BornToShineSaveGame.generated.h"

/** One inventory stack. */
USTRUCT()
struct FSavedInventoryItem
{
	GENERATED_BODY()

	UPROPERTY()
	FName ItemID = NAME_None;

	UPROPERTY()
	int32 Count = 0;
};

/** One placed still part. Parts are identified by FName PartID (matches DT_Items row keys). */
USTRUCT()
struct FSavedStillPart
{
	GENERATED_BODY()

	UPROPERTY()
	FName PartID = NAME_None;

	UPROPERTY()
	FTransform Transform;

	UPROPERTY()
	bool bIsFull = false;

	UPROPERTY()
	bool bIsSealed = false;

	// v2: index of the owning CinderBlockStand within this save's stand list (-1 for stands
	// themselves / unowned). Stands are indexed in the order they appear in StillParts.
	UPROPERTY()
	int32 StandIndex = -1;

	// v3: per-stand operating state (EStillState as uint8; meaningful on stands only). Mid-batch
	// states (Lit/Running) are saved as Empty — same v1 limitation, per stand now. Defaults to
	// Empty for v1/v2 saves.
	UPROPERTY()
	uint8 StillState = 0;

	// v4 (LEGACY, read-only for migration): per-stand ingredient counters. Replaced by StorageItems
	// in v6; still loaded from old saves to migrate them into the storage container.
	UPROPERTY()
	int32 StoredWater = 0;

	UPROPERTY()
	int32 StoredMash = 0;

	UPROPERTY()
	int32 StoredFirewood = 0;

	// v6: full still storage container contents (any items, not just ingredients).
	UPROPERTY()
	TArray<FSavedInventoryItem> StorageItems;
};

/** One dropped item sitting in the world (v5). */
USTRUCT()
struct FSavedWorldPickup
{
	GENERATED_BODY()

	UPROPERTY()
	FName ItemId = NAME_None;

	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	FTransform Transform;
};

/**
 * v1 save: inventory stacks, money, placed still parts.
 * Mid-batch brew state is intentionally NOT saved (still reloads as Empty; consumed
 * ingredients are not refunded).
 */
UCLASS()
class BORNTOSHINE_API UBornToShineSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FSavedInventoryItem> InventoryItems;

	UPROPERTY()
	int32 Money = 0;

	UPROPERTY()
	TArray<FSavedStillPart> StillParts;

	// v5: dropped world pickups.
	UPROPERTY()
	TArray<FSavedWorldPickup> WorldPickups;

	// v1: no part ownership. v2: per-part StandIndex. v3: per-stand StillState.
	// v4: per-stand ingredient counters. v5: world pickups. v6: still storage container contents.
	UPROPERTY()
	int32 SaveVersion = 6;
};
