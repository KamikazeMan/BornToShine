// Born To Shine - Rim Board Implementation

#include "RimBoard.h"
#include "SocketManager.h"
#include "ConstructionPhaseManager.h"
#include "Components/StaticMeshComponent.h"

ARimBoard::ARimBoard()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set piece type
	PieceType = EPieceType::RimBoard;

	// 2x6 lumber actual dimensions
	BoardWidth = 3.81f;   // 1.5 inches
	BoardHeight = 13.97f; // 5.5 inches
	CurrentLengthFeet = 8; // Default 8 feet
	BoardLength = CurrentLengthFeet * 30.48f; // Convert feet to cm

	// Default to 16" on-center joist spacing
	JoistSpacing = 40.64f; // 16 inches
	bUse24InchSpacing = false;

	// Rim boards are wood - require manual nailing
	bAutoNailOnPlace = false;

	// CRITICAL: Force actor scale to (1,1,1) - we use mesh scale for dimensions
	// This prevents mouse wheel scaling from affecting width/height
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);

	// Set mesh scale to match dimensions
	if (MeshComponent)
	{
		// Assuming a base mesh of 1m cube, scale to rim board dimensions
		MeshComponent->SetRelativeScale3D(FVector(
			BoardLength / 100.0f,  // Length (X-axis)
			BoardWidth / 100.0f,   // Width (Y-axis)
			BoardHeight / 100.0f   // Height (Z-axis)
		));
	}
}

void ARimBoard::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("RimBoard: BeginPlay - Total sockets: %d"), Sockets.Num());
}

void ARimBoard::InitializeSockets()
{
	Super::InitializeSockets();

	// Clear any existing sockets first
	Sockets.Empty();

	// Generate all sockets
	CreateBottomEndSockets();
	CreateTopFaceSockets();
	CreateSideFaceSockets();
	CreateEndCornerSockets();

	UE_LOG(LogTemp, Log, TEXT("RimBoard: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ARimBoard::CreateBottomEndSockets()
{
	// Bottom sockets run along the entire bottom edge of the rim board
	// These connect to foundation corner and side sockets
	// Spacing matches foundation block width (243.84cm = 8ft)
	// Create sockets every 60cm along the bottom edge for flexible snapping

	float SocketSpacing = 60.0f; // 60cm spacing along bottom edge
	float StartOffset = SocketSpacing / 2.0f; // Start half-spacing from end
	int32 SocketCount = 0;

	// Calculate number of sockets needed along the length
	float CurrentX = -BoardLength / 2.0f + StartOffset;

	while (CurrentX <= BoardLength / 2.0f - StartOffset / 2.0f)
	{
		FConstructionSocket BottomSocket;
		BottomSocket.SocketName = FName(*FString::Printf(TEXT("BottomEdge_%d"), SocketCount));
		BottomSocket.SocketType = EConstructionSocketType::RimBoard_Bottom_End;
		BottomSocket.LocalPosition = FVector(
			CurrentX,              // Along the length
			0.0f,                  // Centered on width
			-BoardHeight / 2.0f    // Bottom face
		);
		BottomSocket.LocalRotation = FRotator::ZeroRotator; // Match foundation socket orientation
		BottomSocket.bIsOccupied = false;
		Sockets.Add(BottomSocket);

		CurrentX += SocketSpacing;
		SocketCount++;
	}

	// Also add sockets at the exact ends for corner connections
	FConstructionSocket LeftEndSocket;
	LeftEndSocket.SocketName = FName("BottomEnd_Left");
	LeftEndSocket.SocketType = EConstructionSocketType::RimBoard_Bottom_End;
	LeftEndSocket.LocalPosition = FVector(
		-BoardLength / 2.0f,   // Left end
		0.0f,                  // Centered on width
		-BoardHeight / 2.0f    // Bottom face
	);
	LeftEndSocket.LocalRotation = FRotator::ZeroRotator; // Match foundation socket orientation
	LeftEndSocket.bIsOccupied = false;
	Sockets.Add(LeftEndSocket);

	FConstructionSocket RightEndSocket;
	RightEndSocket.SocketName = FName("BottomEnd_Right");
	RightEndSocket.SocketType = EConstructionSocketType::RimBoard_Bottom_End;
	RightEndSocket.LocalPosition = FVector(
		BoardLength / 2.0f,    // Right end
		0.0f,                  // Centered on width
		-BoardHeight / 2.0f    // Bottom face
	);
	RightEndSocket.LocalRotation = FRotator::ZeroRotator; // Match foundation socket orientation
	RightEndSocket.bIsOccupied = false;
	Sockets.Add(RightEndSocket);

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Created %d bottom sockets along bottom edge"), SocketCount + 2);
}

void ARimBoard::CreateTopFaceSockets()
{
	// Top face sockets for floor joists at 16" or 24" intervals
	float Spacing = bUse24InchSpacing ? 60.96f : 40.64f; // 24" or 16"

	TArray<FVector> SocketPositions = CalculateJoistSocketPositions();

	for (int32 i = 0; i < SocketPositions.Num(); i++)
	{
		FConstructionSocket TopSocket;
		TopSocket.SocketName = FName(*FString::Printf(TEXT("TopFace_%d"), i));
		TopSocket.SocketType = EConstructionSocketType::RimBoard_Top_Face;
		TopSocket.LocalPosition = SocketPositions[i];
		TopSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f); // Facing up
		TopSocket.bIsOccupied = false;
		Sockets.Add(TopSocket);
	}

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Created %d top face sockets at %.2f cm spacing"),
		SocketPositions.Num(), Spacing);
}

void ARimBoard::CreateSideFaceSockets()
{
	// Side face sockets for perpendicular joists that butt into the rim
	// These are at the same intervals as top sockets, but on the side face

	float Spacing = bUse24InchSpacing ? 60.96f : 40.64f;
	TArray<FVector> JoistPositions = CalculateJoistSocketPositions();

	for (int32 i = 0; i < JoistPositions.Num(); i++)
	{
		// Left side face socket
		FConstructionSocket LeftSideSocket;
		LeftSideSocket.SocketName = FName(*FString::Printf(TEXT("SideFace_Left_%d"), i));
		LeftSideSocket.SocketType = EConstructionSocketType::RimBoard_Side_Face;
		LeftSideSocket.LocalPosition = FVector(
			JoistPositions[i].X,  // Same X as top socket
			-BoardWidth / 2.0f,   // Left side
			0.0f                  // Centered vertically
		);
		LeftSideSocket.LocalRotation = FRotator(0.0f, -90.0f, 0.0f); // Facing left
		LeftSideSocket.bIsOccupied = false;
		Sockets.Add(LeftSideSocket);

		// Right side face socket
		FConstructionSocket RightSideSocket;
		RightSideSocket.SocketName = FName(*FString::Printf(TEXT("SideFace_Right_%d"), i));
		RightSideSocket.SocketType = EConstructionSocketType::RimBoard_Side_Face;
		RightSideSocket.LocalPosition = FVector(
			JoistPositions[i].X,  // Same X as top socket
			BoardWidth / 2.0f,    // Right side
			0.0f                  // Centered vertically
		);
		RightSideSocket.LocalRotation = FRotator(0.0f, 90.0f, 0.0f); // Facing right
		RightSideSocket.bIsOccupied = false;
		Sockets.Add(RightSideSocket);
	}

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Created %d side face sockets"), JoistPositions.Num() * 2);
}

void ARimBoard::CreateEndCornerSockets()
{
	// SIMPLIFIED: Only 2 corner sockets per board at OUTER EDGES
	// This makes corner snapping simpler - outer edge to outer edge only

	float OuterEdgeOffset = BoardWidth / 2.0f; // 1.905cm to outer edge

	// LEFT END - Outer edge only
	FConstructionSocket LeftOuter;
	LeftOuter.SocketName = FName("EndCorner_Left");
	LeftOuter.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	LeftOuter.LocalPosition = FVector(
		-BoardLength / 2.0f,    // Left end
		OuterEdgeOffset,        // Outer edge (+Y)
		-BoardHeight / 2.0f     // Bottom
	);
	LeftOuter.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	LeftOuter.bIsOccupied = false;
	Sockets.Add(LeftOuter);

	// RIGHT END - Outer edge only
	FConstructionSocket RightOuter;
	RightOuter.SocketName = FName("EndCorner_Right");
	RightOuter.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	RightOuter.LocalPosition = FVector(
		BoardLength / 2.0f,     // Right end
		OuterEdgeOffset,        // Outer edge (+Y)
		-BoardHeight / 2.0f     // Bottom
	);
	RightOuter.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RightOuter.bIsOccupied = false;
	Sockets.Add(RightOuter);

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Created 2 outer corner sockets at Y=+%.2fcm"), OuterEdgeOffset);
}

TArray<FVector> ARimBoard::CalculateJoistSocketPositions() const
{
	TArray<FVector> Positions;

	float Spacing = bUse24InchSpacing ? 60.96f : 40.64f; // 24" or 16" OC
	float StartOffset = Spacing; // Start one spacing in from the end

	// Calculate how many sockets fit along the board length
	float CurrentX = -BoardLength / 2.0f + StartOffset;

	while (CurrentX < BoardLength / 2.0f - StartOffset / 2.0f)
	{
		FVector SocketPos = FVector(
			CurrentX,
			0.0f,                // Centered on width
			BoardHeight / 2.0f   // Top face
		);
		Positions.Add(SocketPos);
		CurrentX += Spacing;
	}

	return Positions;
}

void ARimBoard::UpdatePreviewPosition(const FVector& NewLocation, const FRotator& NewRotation)
{
	// EXPERIMENTAL: Auto-scale board when closing a rectangle
	// Detect if both corner sockets can snap to different rim boards
	if (PieceState == EPieceState::Preview && AConstructionPhaseManager::Instance)
	{
		TArray<ABuildablePiece*> NearbyPieces = AConstructionPhaseManager::Instance->GetNearbyPieces(
			GetActorLocation(),
			SnapSearchRadius
		);

		// Find target positions for each corner socket
		FVector LeftCornerTarget = FVector::ZeroVector;
		FVector RightCornerTarget = FVector::ZeroVector;
		bool bLeftFound = false;
		bool bRightFound = false;

		for (const FConstructionSocket& Socket : Sockets)
		{
			if (Socket.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;

			FVector SocketWorldLocation = GetActorTransform().TransformPosition(Socket.LocalPosition);
			bool bIsLeftSocket = Socket.SocketName.ToString().Contains("Left");

			// Look for nearby rim board corner sockets
			for (ABuildablePiece* Piece : NearbyPieces)
			{
				if (!Piece || Piece == this) continue;

				// Check if this piece is a RimBoard
				ARimBoard* RimBoardPiece = Cast<ARimBoard>(Piece);
				if (!RimBoardPiece) continue;

				TArray<FConstructionSocket> TargetSockets = Piece->GetAllSockets();
				for (const FConstructionSocket& TargetSocket : TargetSockets)
				{
					if (TargetSocket.SocketType == EConstructionSocketType::RimBoard_End_Corner)
					{
						FVector TargetWorldLocation = Piece->GetActorTransform().TransformPosition(TargetSocket.LocalPosition);
						float Distance = FVector::Dist(SocketWorldLocation, TargetWorldLocation);

						if (Distance < 50.0f) // Within snap distance
						{
							if (bIsLeftSocket && !bLeftFound)
							{
								LeftCornerTarget = TargetWorldLocation;
								bLeftFound = true;
							}
							else if (!bIsLeftSocket && !bRightFound)
							{
								RightCornerTarget = TargetWorldLocation;
								bRightFound = true;
							}
							break; // Found a match for this socket, move to next socket
						}
					}
				}
				if ((bIsLeftSocket && bLeftFound) || (!bIsLeftSocket && bRightFound))
					break; // Found target for this socket, move to next socket
			}
		}

		// If BOTH corners found targets, we're closing a rectangle - auto-scale!
		if (bLeftFound && bRightFound)
		{
			float RequiredLength = FVector::Dist(LeftCornerTarget, RightCornerTarget);

			// Account for the flush offset (1.905cm per corner = 3.81cm total)
			// Add the board width since corners need to extend beyond the gap
			RequiredLength += BoardWidth;

			// Convert to cm and round to nearest foot
			int32 RequiredFeet = FMath::RoundToInt(RequiredLength / 30.48f);
			RequiredFeet = FMath::Clamp(RequiredFeet, MinLengthFeet, MaxLengthFeet);

			if (RequiredFeet != CurrentLengthFeet)
			{
				UE_LOG(LogTemp, Warning, TEXT("🔧 Auto-scaling closing board: %.1fcm (%.1fft) -> %dft (gap=%.1fcm)"),
					BoardLength, BoardLength / 30.48f, RequiredFeet, RequiredLength - BoardWidth);
				SetBoardLengthFeet(RequiredFeet);
			}
		}
	}

	// Use socket snapping system - let it handle all positioning
	Super::UpdatePreviewPosition(NewLocation, NewRotation);

	// No manual height override - socket snapping handles everything
}

void ARimBoard::SetBoardLengthFeet(int32 LengthInFeet)
{
	// Clamp to valid range
	LengthInFeet = FMath::Clamp(LengthInFeet, MinLengthFeet, MaxLengthFeet);

	if (LengthInFeet != CurrentLengthFeet)
	{
		CurrentLengthFeet = LengthInFeet;
		BoardLength = CurrentLengthFeet * 30.48f; // Convert feet to cm

		// Update mesh scale
		if (MeshComponent)
		{
			MeshComponent->SetRelativeScale3D(FVector(
				BoardLength / 100.0f,
				BoardWidth / 100.0f,
				BoardHeight / 100.0f
			));
		}

		// Regenerate sockets for new length
		RegenerateSockets();

		UE_LOG(LogTemp, Log, TEXT("Rim Board length changed to: %s"), *GetLengthDisplayString());
	}
}

int32 ARimBoard::GetBoardLengthFeet() const
{
	return CurrentLengthFeet;
}

FString ARimBoard::GetLengthDisplayString() const
{
	return FString::Printf(TEXT("%d ft (%.1f cm)"), CurrentLengthFeet, BoardLength);
}

void ARimBoard::ScalePiece(float ScaleDelta)
{
	// For rim boards, scaling changes length, not visual scale
	int32 NewLength = CurrentLengthFeet;

	if (ScaleDelta > 0)
	{
		NewLength++; // Increase by 1 foot
	}
	else if (ScaleDelta < 0)
	{
		NewLength--; // Decrease by 1 foot
	}

	SetBoardLengthFeet(NewLength);
}

void ARimBoard::RegenerateSockets()
{
	// Clear existing sockets
	Sockets.Empty();

	// Recreate all sockets with new board dimensions
	CreateBottomEndSockets();
	CreateTopFaceSockets();
	CreateSideFaceSockets();
	CreateEndCornerSockets();

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Regenerated %d sockets for %d ft board"), Sockets.Num(), CurrentLengthFeet);
}
