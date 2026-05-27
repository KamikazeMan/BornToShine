#include "RoofGridComponent.h"
#include "BuildablePiece.h"

URoofGridComponent::URoofGridComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URoofGridComponent::RebuildRoofSideGrid(
	ERoofSide Side,
	const TArray<ABuildablePiece*>& Rafters,
	ABuildablePiece* RidgeBoard,
	ABuildablePiece* FasciaBoard)
{
	// Phase 2 will implement this
	UE_LOG(LogTemp, Warning, TEXT("URoofGridComponent::RebuildRoofSideGrid stub called for side %d"), (int32)Side);
}

bool URoofGridComponent::QuantizeRoofPointToCell(
	const FRoofSideGrid& Grid,
	const FVector& WorldPoint,
	FRoofGridCell& OutCell) const
{
	// Phase 2 will implement this
	return false;
}
