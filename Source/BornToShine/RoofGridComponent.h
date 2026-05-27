#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoofGridComponent.generated.h"

UENUM()
enum class ERoofSide : uint8
{
	Left,
	Right
};

USTRUCT()
struct FRoofGridCell
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Column = INDEX_NONE;

	UPROPERTY()
	int32 Row = INDEX_NONE;

	UPROPERTY()
	float RidgeMin = 0.0f;

	UPROPERTY()
	float RidgeMax = 0.0f;

	UPROPERTY()
	float SlopeMin = 0.0f;

	UPROPERTY()
	float SlopeMax = 0.0f;

	UPROPERTY()
	FVector CenterWorld = FVector::ZeroVector;
};

USTRUCT()
struct FRoofSideGrid
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GridId;

	UPROPERTY()
	ERoofSide Side = ERoofSide::Left;

	UPROPERTY()
	int32 Version = 0;

	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	UPROPERTY()
	FVector RidgeDir = FVector::ZeroVector;

	UPROPERTY()
	FVector SlopeDir = FVector::ZeroVector;

	UPROPERTY()
	FVector RoofNormal = FVector::ZeroVector;

	UPROPERTY()
	float RidgeStart = 0.0f;

	UPROPERTY()
	float RidgeEnd = 0.0f;

	UPROPERTY()
	float SlopeStart = 0.0f;

	UPROPERTY()
	float SlopeEnd = 0.0f;

	UPROPERTY()
	float SheetRidgeSize = 243.84f;

	UPROPERTY()
	float SheetSlopeSize = 121.92f;

	UPROPERTY()
	TArray<float> RidgeEdges;

	UPROPERTY()
	TArray<float> SlopeEdges;

	bool IsValid() const
	{
		return !Origin.IsNearlyZero()
			&& !RidgeDir.IsNearlyZero()
			&& !SlopeDir.IsNearlyZero()
			&& RidgeEnd > RidgeStart
			&& SlopeEnd > SlopeStart
			&& RidgeEdges.Num() >= 2
			&& SlopeEdges.Num() >= 2;
	}
};

class ABuildablePiece;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BORNTOSHINE_API URoofGridComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoofGridComponent();

	// Grid data for each side of the roof
	UPROPERTY()
	FRoofSideGrid LeftGrid;

	UPROPERTY()
	FRoofSideGrid RightGrid;

	// Stub: Rebuild grid for one side (called when roof framing changes).
	// Phase 2 will implement the actual logic.
	void RebuildRoofSideGrid(
		ERoofSide Side,
		const TArray<ABuildablePiece*>& Rafters,
		ABuildablePiece* RidgeBoard,
		ABuildablePiece* FasciaBoard);

	// Stub: Quantize a world-space point to a grid cell.
	// Returns true if the point falls inside a valid cell.
	bool QuantizeRoofPointToCell(
		const FRoofSideGrid& Grid,
		const FVector& WorldPoint,
		FRoofGridCell& OutCell) const;

	// Get the grid for a given side
	const FRoofSideGrid& GetGridForSide(ERoofSide Side) const
	{
		return Side == ERoofSide::Left ? LeftGrid : RightGrid;
	}
};
