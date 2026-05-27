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

	// Filter rafters by which side of the ridge they slope toward.
	// Use rafter forward vector dotted with ridge right (not pitch sign,
	// since both sides can have the same pitch sign with different yaw).
	FVector RidgeRight = RidgeBoard->GetActorRightVector();
	RidgeRight.Z = 0.0f;
	RidgeRight.Normalize();

	float DesiredSideSign = (Side == ERoofSide::Left) ? -1.0f : 1.0f;

	UE_LOG(LogTemp, Warning, TEXT("RebuildRoofSideGrid Side=%d: Examining %d total rafters, DesiredSideSign=%.1f RidgeRight=%s"),
		(int32)Side, Rafters.Num(), DesiredSideSign, *RidgeRight.ToString());

	TArray<ABuildablePiece*> SideRafters;
	for (ABuildablePiece* P : Rafters)
	{
		if (!P) continue;

		FVector RafterFwd = P->GetActorForwardVector();
		RafterFwd.Z = 0.0f;
		if (!RafterFwd.Normalize())
		{
			UE_LOG(LogTemp, Warning, TEXT("  Rafter %s: Forward is vertical (skipped)"), *P->GetName());
			continue;
		}

		float SideDot = FVector::DotProduct(RafterFwd, RidgeRight);
		float SideSign = FMath::Sign(SideDot);
		bool bMatchesSide = (SideSign == DesiredSideSign);

		UE_LOG(LogTemp, Warning, TEXT("  Rafter %s: Yaw=%.1f Fwd=%s SideDot=%.3f Sign=%.1f Matches=%s"),
			*P->GetName(), P->GetActorRotation().Yaw, *RafterFwd.ToString(),
			SideDot, SideSign, bMatchesSide ? TEXT("YES") : TEXT("no"));

		if (bMatchesSide)
		{
			SideRafters.Add(P);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("RebuildRoofSideGrid Side=%d: Found %d matching rafters"),
		(int32)Side, SideRafters.Num());

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
	if (!Grid.IsValid())
	{
		return false;
	}

	// Project the world point into grid-local coordinates
	const FVector ToPoint = WorldPoint - Grid.Origin;
	const float RidgeCoord = FVector::DotProduct(ToPoint, Grid.RidgeDir);
	const float SlopeCoord = FVector::DotProduct(ToPoint, Grid.SlopeDir);

	// Allow a small tolerance at the edges
	const float Tolerance = 0.5f;

	if (RidgeCoord < Grid.RidgeStart - Tolerance || RidgeCoord > Grid.RidgeEnd + Tolerance)
	{
		return false;
	}
	if (SlopeCoord < Grid.SlopeStart - Tolerance || SlopeCoord > Grid.SlopeEnd + Tolerance)
	{
		return false;
	}

	// Find which column the ridge coord falls into
	int32 Column = INDEX_NONE;
	for (int32 i = 0; i < Grid.RidgeEdges.Num() - 1; ++i)
	{
		if (RidgeCoord >= Grid.RidgeEdges[i] - Tolerance && RidgeCoord <= Grid.RidgeEdges[i + 1] + Tolerance)
		{
			Column = i;
			break;
		}
	}

	// Find which row the slope coord falls into
	int32 Row = INDEX_NONE;
	for (int32 i = 0; i < Grid.SlopeEdges.Num() - 1; ++i)
	{
		if (SlopeCoord >= Grid.SlopeEdges[i] - Tolerance && SlopeCoord <= Grid.SlopeEdges[i + 1] + Tolerance)
		{
			Row = i;
			break;
		}
	}

	if (Column == INDEX_NONE || Row == INDEX_NONE)
	{
		return false;
	}

	// Populate the cell
	OutCell.Column = Column;
	OutCell.Row = Row;
	OutCell.RidgeMin = Grid.RidgeEdges[Column];
	OutCell.RidgeMax = Grid.RidgeEdges[Column + 1];
	OutCell.SlopeMin = Grid.SlopeEdges[Row];
	OutCell.SlopeMax = Grid.SlopeEdges[Row + 1];

	// Compute world-space center of the cell
	const float CenterRidge = (OutCell.RidgeMin + OutCell.RidgeMax) * 0.5f;
	const float CenterSlope = (OutCell.SlopeMin + OutCell.SlopeMax) * 0.5f;

	OutCell.CenterWorld =
		Grid.Origin
		+ Grid.RidgeDir * CenterRidge
		+ Grid.SlopeDir * CenterSlope;

	return true;
}
