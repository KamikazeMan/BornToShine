#include "RoofGridComponent.h"
#include "BuildablePiece.h"
#include "Rafter.h"
#include "FasciaBoard.h"
#include "RidgeBoard.h"

static void BuildGridEdges(float Start, float End, float Step, TArray<float>& OutEdges)
{
	OutEdges.Reset();
	OutEdges.Add(Start);

	float X = Start + Step;
	while (X < End - 0.01f)
	{
		OutEdges.Add(X);
		X += Step;
	}

	if (!FMath::IsNearlyEqual(OutEdges.Last(), End, 0.01f))
	{
		OutEdges.Add(End);
	}
}

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
	FRoofSideGrid& Grid = (Side == ERoofSide::Left) ? LeftGrid : RightGrid;

	// Preserve GridId across rebuilds; only create on first build
	if (!Grid.GridId.IsValid())
	{
		Grid.GridId = FGuid::NewGuid();
	}
	Grid.Side = Side;
	Grid.Version++;

	if (!RidgeBoard)
	{
		UE_LOG(LogTemp, Error, TEXT("RebuildRoofSideGrid: RidgeBoard is null"));
		return;
	}

	// === DIRECTION VECTORS (stable from ridge board) ===
	// Ridge board's forward axis = direction along the ridge
	FRotator RidgeRot = RidgeBoard->GetActorRotation();
	FVector RidgeForward = RidgeRot.RotateVector(FVector::ForwardVector);
	Grid.RidgeDir = RidgeForward.GetSafeNormal();

	// Filter rafters to only this side based on pitch sign
	// Left side rafters have one pitch sign, right side the opposite
	TArray<ABuildablePiece*> SideRafters;
	float ReferencePitchSign = 0.0f;
	for (ABuildablePiece* P : Rafters)
	{
		if (!P) continue;
		float Pitch = P->GetActorRotation().Pitch;
		if (FMath::IsNearlyZero(Pitch)) continue;

		if (ReferencePitchSign == 0.0f)
		{
			// First valid rafter — pick its pitch sign based on which side we want
			// Left = negative pitch (slopes down to left), Right = positive pitch
			float DesiredSign = (Side == ERoofSide::Left) ? -1.0f : 1.0f;
			if (FMath::Sign(Pitch) == DesiredSign)
			{
				ReferencePitchSign = FMath::Sign(Pitch);
				SideRafters.Add(P);
			}
		}
		else if (FMath::Sign(Pitch) == ReferencePitchSign)
		{
			SideRafters.Add(P);
		}
	}

	if (SideRafters.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("RebuildRoofSideGrid: No rafters found for side %d"), (int32)Side);
		return;
	}

	// Use first side rafter to derive slope direction (the rafter's forward axis)
	ABuildablePiece* RefRafter = SideRafters[0];
	FRotator RafterRot = RefRafter->GetActorRotation();
	Grid.SlopeDir = RafterRot.RotateVector(FVector::ForwardVector).GetSafeNormal();
	Grid.RoofNormal = RafterRot.RotateVector(FVector::UpVector).GetSafeNormal();

	// === ORIGIN: leftmost rafter's location projected onto ridge ===
	// Find the rafter with the minimum projection onto RidgeDir
	ABuildablePiece* LeftmostRafter = SideRafters[0];
	float MinProj = FVector::DotProduct(LeftmostRafter->GetActorLocation(), Grid.RidgeDir);

	for (ABuildablePiece* P : SideRafters)
	{
		float Proj = FVector::DotProduct(P->GetActorLocation(), Grid.RidgeDir);
		if (Proj < MinProj)
		{
			MinProj = Proj;
			LeftmostRafter = P;
		}
	}

	// Origin = leftmost rafter location, but shifted outward by half rafter width
	// so that the grid starts at the outer face of the end rafter
	const float RafterHalfWidth = 1.905f; // half of 3.81cm (2x4 width)
	Grid.Origin = LeftmostRafter->GetActorLocation() - Grid.RidgeDir * RafterHalfWidth;

	// === RIDGE EXTENT: from leftmost to rightmost rafter (with half rafter widths on each side) ===
	float MaxProj = MinProj;
	for (ABuildablePiece* P : SideRafters)
	{
		float Proj = FVector::DotProduct(P->GetActorLocation(), Grid.RidgeDir);
		if (Proj > MaxProj) MaxProj = Proj;
	}

	Grid.RidgeStart = 0.0f; // Always 0 — Origin is already shifted to start point
	Grid.RidgeEnd = (MaxProj - MinProj) + RafterHalfWidth * 2.0f;

	// === SLOPE EXTENT: 0 to MaxSlopeLen (trimmed at fascia if present) ===
	float MaxSlopeLen = 0.0f;
	for (ABuildablePiece* P : SideRafters)
	{
		ARafter* R = Cast<ARafter>(P);
		if (R)
		{
			MaxSlopeLen = FMath::Max(MaxSlopeLen, R->GetSlopeLengthCm());
		}
	}

	// Apply ridge board half-thickness so Row 0 sheets meet at ridge centerline
	const float RidgeBoardHalfThickness = 1.905f;
	Grid.SlopeStart = -RidgeBoardHalfThickness;

	// Trim slope max to fascia outer face if fascia is placed
	if (FasciaBoard)
	{
		FVector FasciaLoc = FasciaBoard->GetActorLocation();
		float FasciaAlongSlope = FVector::DotProduct(FasciaLoc - LeftmostRafter->GetActorLocation(), Grid.SlopeDir);
		const float FasciaHalfThickness = 1.905f;
		const float OverhangCorrection = 5.0f; // 2" inward to match fascia front
		float TrimmedMax = FasciaAlongSlope + FasciaHalfThickness - OverhangCorrection;
		if (TrimmedMax > 0.0f && TrimmedMax < MaxSlopeLen)
		{
			MaxSlopeLen = TrimmedMax;
		}
	}

	Grid.SlopeEnd = MaxSlopeLen;

	// === BUILD EDGE ARRAYS ===
	BuildGridEdges(Grid.RidgeStart, Grid.RidgeEnd, Grid.SheetRidgeSize, Grid.RidgeEdges);
	BuildGridEdges(Grid.SlopeStart, Grid.SlopeEnd, Grid.SheetSlopeSize, Grid.SlopeEdges);

	UE_LOG(LogTemp, Warning, TEXT("RoofGrid Rebuild Side=%d Version=%d Origin=%s RidgeDir=%s SlopeDir=%s RidgeEnd=%.1f SlopeEnd=%.1f RidgeEdges=%d SlopeEdges=%d"),
		(int32)Side, Grid.Version,
		*Grid.Origin.ToString(),
		*Grid.RidgeDir.ToString(),
		*Grid.SlopeDir.ToString(),
		Grid.RidgeEnd,
		Grid.SlopeEnd,
		Grid.RidgeEdges.Num(),
		Grid.SlopeEdges.Num());

	for (int32 i = 0; i < Grid.RidgeEdges.Num(); ++i)
	{
		UE_LOG(LogTemp, Warning, TEXT("RoofGrid RidgeEdge[%d]=%.2f"), i, Grid.RidgeEdges[i]);
	}
	for (int32 i = 0; i < Grid.SlopeEdges.Num(); ++i)
	{
		UE_LOG(LogTemp, Warning, TEXT("RoofGrid SlopeEdge[%d]=%.2f"), i, Grid.SlopeEdges[i]);
	}
}

bool URoofGridComponent::QuantizeRoofPointToCell(
	const FRoofSideGrid& Grid,
	const FVector& WorldPoint,
	FRoofGridCell& OutCell) const
{
	// Phase 2 will implement this
	return false;
}
