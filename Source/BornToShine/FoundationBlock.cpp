// Born To Shine - Foundation Block with Corner Sockets

#include "FoundationBlock.h"
#include "Components/StaticMeshComponent.h"

AFoundationBlock::AFoundationBlock()
{
	PieceType = EPieceType::Foundation;

	// 8 feet = 243.84 cm in Unreal units (1 unit = 1 cm)
	GridSize = 243.84f;

	// 8 cm from ground for socket height (rim board seating depth)
	SocketHeightOffset = 8.0f;

	// Standard foundation block dimensions (example: 2ft x 2ft x 1ft)
	// In Unreal: 60.96cm x 60.96cm x 30.48cm
	BlockDimensions = FVector(60.96f, 60.96f, 30.48f);

	// Foundation blocks are always supported (on ground)
	bIsSnapped = true;

	// Foundation blocks auto-nail on placement (concrete doesn't need nailing!)
	bAutoNailOnPlace = true;
}

void AFoundationBlock::BeginPlay()
{
	Super::BeginPlay();

	// Foundation blocks start in a valid placement state
	PieceState = EPieceState::Preview;
}

void AFoundationBlock::InitializeSockets()
{
	Super::InitializeSockets();

	CreateCornerSockets();
	CreateCenterSocket();
	CreateSideSockets();

	UE_LOG(LogTemp, Log, TEXT("Foundation Block: Initialized %d sockets"), Sockets.Num());
}

void AFoundationBlock::CreateCornerSockets()
{
	// Calculate half dimensions for corner placement
	float HalfWidth = BlockDimensions.X / 2.0f;
	float HalfLength = BlockDimensions.Y / 2.0f;

	// Socket should be at the groove depth (14cm from ground)
	// With foundation actor at Z=0 (pivot at bottom, bottom at ground):
	// Groove at 14cm world = local Z of 14cm
	float SocketZPosition = SocketHeightOffset;

	// Create 4 corner sockets at the groove of the foundation block
	// These sockets will accept rim board bottom ends

	// Northeast corner
	FConstructionSocket CornerNE;
	CornerNE.SocketName = FName("Foundation_Corner_NE");
	CornerNE.SocketType = EConstructionSocketType::Foundation_Corner;
	CornerNE.LocalPosition = FVector(HalfWidth, HalfLength, SocketZPosition);
	CornerNE.LocalRotation = FRotator::ZeroRotator;
	CornerNE.Orientation = ESocketOrientation::Vertical;
	CornerNE.bIsOccupied = false;
	Sockets.Add(CornerNE);

	// Northwest corner
	FConstructionSocket CornerNW;
	CornerNW.SocketName = FName("Foundation_Corner_NW");
	CornerNW.SocketType = EConstructionSocketType::Foundation_Corner;
	CornerNW.LocalPosition = FVector(HalfWidth, -HalfLength, SocketZPosition);
	CornerNW.LocalRotation = FRotator::ZeroRotator;
	CornerNW.Orientation = ESocketOrientation::Vertical;
	CornerNW.bIsOccupied = false;
	Sockets.Add(CornerNW);

	// Southeast corner
	FConstructionSocket CornerSE;
	CornerSE.SocketName = FName("Foundation_Corner_SE");
	CornerSE.SocketType = EConstructionSocketType::Foundation_Corner;
	CornerSE.LocalPosition = FVector(-HalfWidth, HalfLength, SocketZPosition);
	CornerSE.LocalRotation = FRotator::ZeroRotator;
	CornerSE.Orientation = ESocketOrientation::Vertical;
	CornerSE.bIsOccupied = false;
	Sockets.Add(CornerSE);

	// Southwest corner
	FConstructionSocket CornerSW;
	CornerSW.SocketName = FName("Foundation_Corner_SW");
	CornerSW.SocketType = EConstructionSocketType::Foundation_Corner;
	CornerSW.LocalPosition = FVector(-HalfWidth, -HalfLength, SocketZPosition);
	CornerSW.LocalRotation = FRotator::ZeroRotator;
	CornerSW.Orientation = ESocketOrientation::Vertical;
	CornerSW.bIsOccupied = false;
	Sockets.Add(CornerSW);

	UE_LOG(LogTemp, Log, TEXT("Foundation: Corner sockets at local Z=%.2f (%.2fcm from ground)"),
		SocketZPosition, SocketHeightOffset);
}

void AFoundationBlock::CreateCenterSocket()
{
	// Socket at groove depth (actor pivot at bottom)
	float SocketZPosition = SocketHeightOffset;

	// Center socket for pier posts or center beam support
	FConstructionSocket CenterSocket;
	CenterSocket.SocketName = FName("Foundation_Center");
	CenterSocket.SocketType = EConstructionSocketType::Foundation_Corner; // Can accept same connections
	CenterSocket.LocalPosition = FVector(0.0f, 0.0f, SocketZPosition);
	CenterSocket.LocalRotation = FRotator::ZeroRotator;
	CenterSocket.Orientation = ESocketOrientation::Vertical;
	CenterSocket.bIsOccupied = false;
	Sockets.Add(CenterSocket);
}

void AFoundationBlock::CreateSideSockets()
{
	// Create sockets at the CENTER of the foundation block
	// The groove runs through the center (0,0), so all sockets are centered
	// This ensures rim boards sit in the groove regardless of mesh pivot offset

	// Socket at groove depth (actor pivot at bottom)
	float SocketZPosition = SocketHeightOffset;

	// Single center socket for rim board connections
	// Rim boards can connect here and extend in any direction
	FConstructionSocket CenterGroove;
	CenterGroove.SocketName = FName("Foundation_Side_Center");
	CenterGroove.SocketType = EConstructionSocketType::Foundation_Side;
	CenterGroove.LocalPosition = FVector(0.0f, 0.0f, SocketZPosition); // At the center
	CenterGroove.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	CenterGroove.Orientation = ESocketOrientation::Horizontal;
	CenterGroove.bIsOccupied = false;
	Sockets.Add(CenterGroove);

	UE_LOG(LogTemp, Log, TEXT("Foundation: Created center groove socket at (0, 0, %.1f)"), SocketZPosition);
}

FVector AFoundationBlock::SnapToGrid(const FVector& Location) const
{
	// Snap to 8ft (243.84cm) grid intervals
	FVector SnappedLocation;

	SnappedLocation.X = FMath::RoundToFloat(Location.X / GridSize) * GridSize;
	SnappedLocation.Y = FMath::RoundToFloat(Location.Y / GridSize) * GridSize;

	// Foundation bottom should be at ground level (Z=0)
	// If mesh pivot is at bottom: Z=0 places bottom at ground
	// If mesh pivot is at center: Z=BlockDimensions.Z/2 places bottom at ground
	// Setting to 0 assumes pivot at bottom (most common for foundation pieces)
	SnappedLocation.Z = 0.0f;

	return SnappedLocation;
}

void AFoundationBlock::UpdatePreviewPosition(const FVector& NewLocation, const FRotator& NewRotation)
{
	if (PieceState != EPieceState::Preview) return;

	// Snap to grid first
	FVector SnappedLocation = SnapToGrid(NewLocation);

	// Foundation blocks are always horizontal (no rotation)
	FRotator FixedRotation = FRotator(0.0f, 0.0f, 0.0f);

	SetActorLocation(SnappedLocation);
	SetActorRotation(FixedRotation);

	// Foundation is always "snapped" since it sits on ground
	bIsSnapped = true;
}
