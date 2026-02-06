// Born To Shine - Rim Board Implementation

#include "RimBoard.h"
#include "SocketManager.h"
#include "ConstructionPhaseManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshSocket.h"

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

	// Default to outside board (full length)
	// Inside boards are 3" shorter to butt against outside boards
	bIsOutsideBoard = true;

	// Rim boards are wood - require manual nailing
	bAutoNailOnPlace = false;

	// CRITICAL: Force actor scale to (1,1,1) - we use mesh scale for dimensions
	// This prevents mouse wheel scaling from affecting width/height
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);

	// Set mesh scale to match dimensions (using effective length for outside/inside board type)
	if (MeshComponent)
	{
		// Assuming a base mesh of 1m cube, scale to rim board dimensions
		// Note: GetEffectiveLength() returns BoardLength for outside boards (default)
		MeshComponent->SetRelativeScale3D(FVector(
			GetEffectiveLength() / 100.0f,  // Length (X-axis) - accounts for board type
			BoardWidth / 100.0f,            // Width (Y-axis)
			BoardHeight / 100.0f            // Height (Z-axis)
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
	// Bottom sockets ONLY at the exact ends for proper foundation alignment
	// Having intermediate sockets causes misalignment when the board snaps
	// to a middle socket instead of an end socket

	float EffectiveLen = GetEffectiveLength();
	float HalfLen = EffectiveLen / 2.0f;

	// Left end socket - for snapping to left foundation
	FConstructionSocket LeftEndSocket;
	LeftEndSocket.SocketName = FName("BottomEnd_Left");
	LeftEndSocket.SocketType = EConstructionSocketType::RimBoard_Bottom_End;
	LeftEndSocket.LocalPosition = FVector(
		-HalfLen,              // Left end
		0.0f,                  // Centered on width (Y=0)
		-BoardHeight / 2.0f    // Bottom face
	);
	LeftEndSocket.LocalRotation = FRotator::ZeroRotator;
	LeftEndSocket.bIsOccupied = false;
	Sockets.Add(LeftEndSocket);

	// Right end socket - for snapping to right foundation
	FConstructionSocket RightEndSocket;
	RightEndSocket.SocketName = FName("BottomEnd_Right");
	RightEndSocket.SocketType = EConstructionSocketType::RimBoard_Bottom_End;
	RightEndSocket.LocalPosition = FVector(
		HalfLen,               // Right end
		0.0f,                  // Centered on width (Y=0)
		-BoardHeight / 2.0f    // Bottom face
	);
	RightEndSocket.LocalRotation = FRotator::ZeroRotator;
	RightEndSocket.bIsOccupied = false;
	Sockets.Add(RightEndSocket);

	// Center socket - for middle foundation support on longer boards
	FConstructionSocket CenterSocket;
	CenterSocket.SocketName = FName("BottomEnd_Center");
	CenterSocket.SocketType = EConstructionSocketType::RimBoard_Bottom_End;
	CenterSocket.LocalPosition = FVector(
		0.0f,                  // Center of board
		0.0f,                  // Centered on width (Y=0)
		-BoardHeight / 2.0f    // Bottom face
	);
	CenterSocket.LocalRotation = FRotator::ZeroRotator;
	CenterSocket.bIsOccupied = false;
	Sockets.Add(CenterSocket);

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Created 3 bottom sockets (left, center, right) for %s board"),
		bIsOutsideBoard ? TEXT("OUTSIDE") : TEXT("INSIDE"));
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
	// MESH SOCKET SYSTEM:
	// Use mesh-defined sockets (Snap_Corner_Left, Snap_Corner_Right) for precise positioning
	// These sockets are placed at exact mesh edges in the Static Mesh Editor
	// Falls back to calculated positions for each socket that doesn't exist

	float EffectiveLen = GetEffectiveLength();
	float HalfLen = EffectiveLen / 2.0f;

	bool bLeftFromMesh = false;
	bool bRightFromMesh = false;

	if (MeshComponent)
	{
		// Debug: List all sockets on the mesh to verify names
		if (UStaticMesh* Mesh = MeshComponent->GetStaticMesh())
		{
			UE_LOG(LogTemp, Log, TEXT("RimBoard: Mesh has %d sockets:"), Mesh->Sockets.Num());
			for (UStaticMeshSocket* Socket : Mesh->Sockets)
			{
				if (Socket)
				{
					UE_LOG(LogTemp, Log, TEXT("  - Socket: '%s' at %s"), *Socket->SocketName.ToString(), *Socket->RelativeLocation.ToString());
				}
			}
		}

		// Try LEFT mesh socket
		if (MeshComponent->DoesSocketExist(FName("Snap_Corner_Left")))
		{
			FTransform LeftSocketTransform = MeshComponent->GetSocketTransform(FName("Snap_Corner_Left"), ERelativeTransformSpace::RTS_Component);

			FConstructionSocket LeftEnd;
			LeftEnd.SocketName = FName("EndCorner_Left");
			LeftEnd.SocketType = EConstructionSocketType::RimBoard_End_Corner;
			LeftEnd.LocalPosition = LeftSocketTransform.GetLocation();
			LeftEnd.LocalRotation = LeftSocketTransform.GetRotation().Rotator();
			LeftEnd.bIsOccupied = false;
			Sockets.Add(LeftEnd);

			UE_LOG(LogTemp, Log, TEXT("RimBoard: Using MESH socket Snap_Corner_Left at %s, rot %s"),
				*LeftEnd.LocalPosition.ToString(), *LeftEnd.LocalRotation.ToString());

			bLeftFromMesh = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RimBoard: Snap_Corner_Left mesh socket NOT FOUND - using fallback"));
		}

		// Try RIGHT mesh socket
		if (MeshComponent->DoesSocketExist(FName("Snap_Corner_Right")))
		{
			FTransform RightSocketTransform = MeshComponent->GetSocketTransform(FName("Snap_Corner_Right"), ERelativeTransformSpace::RTS_Component);

			FConstructionSocket RightEnd;
			RightEnd.SocketName = FName("EndCorner_Right");
			RightEnd.SocketType = EConstructionSocketType::RimBoard_End_Corner;
			RightEnd.LocalPosition = RightSocketTransform.GetLocation();
			RightEnd.LocalRotation = RightSocketTransform.GetRotation().Rotator();
			RightEnd.bIsOccupied = false;
			Sockets.Add(RightEnd);

			UE_LOG(LogTemp, Log, TEXT("RimBoard: Using MESH socket Snap_Corner_Right at %s, rot %s"),
				*RightEnd.LocalPosition.ToString(), *RightEnd.LocalRotation.ToString());

			bRightFromMesh = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RimBoard: Snap_Corner_Right mesh socket NOT FOUND - using fallback"));
		}
	}

	// Fallback for LEFT socket if not found in mesh
	if (!bLeftFromMesh)
	{
		FConstructionSocket LeftEnd;
		LeftEnd.SocketName = FName("EndCorner_Left");
		LeftEnd.SocketType = EConstructionSocketType::RimBoard_End_Corner;
		LeftEnd.LocalPosition = FVector(-HalfLen, 0.0f, 0.0f);
		LeftEnd.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
		LeftEnd.bIsOccupied = false;
		Sockets.Add(LeftEnd);
	}

	// Fallback for RIGHT socket if not found in mesh
	if (!bRightFromMesh)
	{
		FConstructionSocket RightEnd;
		RightEnd.SocketName = FName("EndCorner_Right");
		RightEnd.SocketType = EConstructionSocketType::RimBoard_End_Corner;
		RightEnd.LocalPosition = FVector(HalfLen, 0.0f, 0.0f);
		RightEnd.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
		RightEnd.bIsOccupied = false;
		Sockets.Add(RightEnd);
	}

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Corner sockets for %s board - Left:%s, Right:%s"),
		bIsOutsideBoard ? TEXT("OUTSIDE") : TEXT("INSIDE"),
		bLeftFromMesh ? TEXT("MESH") : TEXT("CALC"),
		bRightFromMesh ? TEXT("MESH") : TEXT("CALC"));
}

TArray<FVector> ARimBoard::CalculateJoistSocketPositions() const
{
	TArray<FVector> Positions;

	float EffectiveLen = GetEffectiveLength();
	float HalfLen = EffectiveLen / 2.0f;

	float Spacing = bUse24InchSpacing ? 60.96f : 40.64f; // 24" or 16" OC
	float StartOffset = Spacing; // Start one spacing in from the end

	// Calculate how many sockets fit along the board length
	float CurrentX = -HalfLen + StartOffset;

	while (CurrentX < HalfLen - StartOffset / 2.0f)
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
	// Auto-scale board when closing a rectangle
	// Only triggers when BOTH corners find targets on DIFFERENT boards
	if (PieceState == EPieceState::Preview && AConstructionPhaseManager::Instance)
	{
		TArray<ABuildablePiece*> NearbyPieces = AConstructionPhaseManager::Instance->GetNearbyPieces(
			GetActorLocation(),
			SnapSearchRadius
		);

		// Find target positions for each corner socket - must be on DIFFERENT boards
		FVector LeftCornerTarget = FVector::ZeroVector;
		FVector RightCornerTarget = FVector::ZeroVector;
		ABuildablePiece* LeftTargetBoard = nullptr;
		ABuildablePiece* RightTargetBoard = nullptr;
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

						if (Distance < 150.0f)
						{
							if (bIsLeftSocket && !bLeftFound)
							{
								LeftCornerTarget = TargetWorldLocation;
								LeftTargetBoard = Piece;
								bLeftFound = true;
								UE_LOG(LogTemp, Log, TEXT("Auto-scale: Left corner target found at %.1fcm on %s"), Distance, *Piece->GetName());
							}
							else if (!bIsLeftSocket && !bRightFound)
							{
								RightCornerTarget = TargetWorldLocation;
								RightTargetBoard = Piece;
								bRightFound = true;
								UE_LOG(LogTemp, Log, TEXT("Auto-scale: Right corner target found at %.1fcm on %s"), Distance, *Piece->GetName());
							}
							break; // Found a match for this socket, move to next socket
						}
					}
				}
				if ((bIsLeftSocket && bLeftFound) || (!bIsLeftSocket && bRightFound))
					break; // Found target for this socket, move to next socket
			}
		}

		// If BOTH corners found targets on DIFFERENT boards, we're closing a rectangle
		// This prevents triggering when a new board spawns on top of a placed board
		if (bLeftFound && bRightFound && LeftTargetBoard != RightTargetBoard)
		{
			bAutoScalingActive = true;

			float GapDistance = FVector::Dist(LeftCornerTarget, RightCornerTarget);

			// Current socket-to-socket distance on mesh = 121.2 + 122.7 = 243.9cm for 8ft board
			float CurrentSocketToSocket = 243.9f;
			float CurrentMeshLength = GetEffectiveLength();

			// Scale factor to fit the gap
			float ScaleFactor = GapDistance / CurrentSocketToSocket;
			float NewLength = CurrentMeshLength * ScaleFactor;

			// Clamp to reasonable limits (4ft to 12ft)
			float MinLength = 4 * 30.48f;
			float MaxLength = 12 * 30.48f;
			NewLength = FMath::Clamp(NewLength, MinLength, MaxLength);

			// Update mesh scale if needed
			if (FMath::Abs(NewLength - CurrentMeshLength) > 1.0f)
			{
				UE_LOG(LogTemp, Warning, TEXT("🔧 Auto-scaling closing board: Gap=%.1fcm, Current=%.1fcm -> New=%.1fcm"),
					GapDistance, CurrentMeshLength, NewLength);

				BoardLength = NewLength / (bIsOutsideBoard ? 1.0f : (1.0f - (2.0f * BoardWidth / (8 * 30.48f))));

				if (MeshComponent)
				{
					float EffectiveLen = GetEffectiveLength();
					FVector CurrentMeshScale = MeshComponent->GetRelativeScale3D();
					MeshComponent->SetRelativeScale3D(FVector(
						EffectiveLen / 100.0f,
						CurrentMeshScale.Y,
						CurrentMeshScale.Z
					));
				}
				RegenerateSockets();
			}

			// CRITICAL: Position board CENTERED between the two target corners
			// This ensures BOTH corners align, not just one
			FVector CenterPoint = (LeftCornerTarget + RightCornerTarget) / 2.0f;

			// Calculate rotation to face along the line from left to right target
			FVector Direction = (RightCornerTarget - LeftCornerTarget).GetSafeNormal();
			FRotator TargetRotation = Direction.Rotation();

			// Position the board at the center, with the correct rotation
			SetActorLocation(CenterPoint);
			SetActorRotation(TargetRotation);

			// Mark as snapped for visual feedback
			bIsSnapped = true;
			PieceState = EPieceState::Preview;
			UpdateVisualFeedback();

			UE_LOG(LogTemp, Warning, TEXT("🔧 Closing board positioned: Center=%s, Rot=%.1f°"),
				*CenterPoint.ToString(), TargetRotation.Yaw);

			return; // Skip normal snap system - we've handled positioning
		}
		else
		{
			bAutoScalingActive = false;
		}
	}

	// Use socket snapping system for non-closing boards
	Super::UpdatePreviewPosition(NewLocation, NewRotation);
}

void ARimBoard::SetBoardLengthFeet(int32 LengthInFeet)
{
	// Clamp to valid range
	LengthInFeet = FMath::Clamp(LengthInFeet, MinLengthFeet, MaxLengthFeet);

	if (LengthInFeet != CurrentLengthFeet)
	{
		CurrentLengthFeet = LengthInFeet;
		BoardLength = CurrentLengthFeet * 30.48f; // Convert feet to cm

		// Update mesh scale using effective length (accounts for outside/inside board type)
		if (MeshComponent)
		{
			float EffectiveLen = GetEffectiveLength();
			MeshComponent->SetRelativeScale3D(FVector(
				EffectiveLen / 100.0f,
				BoardWidth / 100.0f,
				BoardHeight / 100.0f
			));
		}

		// Regenerate sockets for new length
		RegenerateSockets();

		UE_LOG(LogTemp, Log, TEXT("Rim Board length changed to: %s (%s board, effective: %.2f cm)"),
			*GetLengthDisplayString(),
			bIsOutsideBoard ? TEXT("OUTSIDE") : TEXT("INSIDE"),
			GetEffectiveLength());
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
	// Rim boards: Scroll wheel scaling is DISABLED
	// Board length is controlled automatically when closing rectangles
	// This prevents accidental scaling that breaks corner alignment

	// Keep actor scale at 1,1,1
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);

	// Intentionally do nothing with ScaleDelta
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

void ARimBoard::ToggleBoardType()
{
	bIsOutsideBoard = !bIsOutsideBoard;

	// Regenerate sockets to update positions
	RegenerateSockets();

	// Update mesh scale - ONLY change the length (X), preserve width and height
	// Also adjust mesh position to keep it centered (compensate for pivot not being at center)
	if (MeshComponent)
	{
		FVector MeshScale = MeshComponent->GetRelativeScale3D();
		float OldEffectiveLen = bIsOutsideBoard ? (BoardLength - 2.0f * BoardWidth) : BoardLength;
		float NewEffectiveLen = GetEffectiveLength();

		// Calculate the length difference
		float LengthDiff = NewEffectiveLen - OldEffectiveLen;

		// Calculate the ratio to apply to X scale
		float LengthRatio = NewEffectiveLen / OldEffectiveLen;

		// Adjust mesh position to keep it centered
		// When shrinking (LengthDiff negative), mesh pivot at one end causes that end to stay fixed
		// We need to shift the mesh in OPPOSITE direction of the length change to center it
		FVector CurrentMeshPos = MeshComponent->GetRelativeLocation();
		MeshComponent->SetRelativeLocation(FVector(
			CurrentMeshPos.X - (LengthDiff / 2.0f),  // SUBTRACT (opposite of length change)
			CurrentMeshPos.Y,
			CurrentMeshPos.Z
		));

		// Only modify X (length), keep Y and Z the same
		MeshComponent->SetRelativeScale3D(FVector(
			MeshScale.X * LengthRatio,
			MeshScale.Y,  // Keep width unchanged
			MeshScale.Z   // Keep height unchanged
		));

		UE_LOG(LogTemp, Warning, TEXT("RimBoard: Adjusted mesh position by %.2f cm to keep centered"), LengthDiff / 2.0f);
	}

	UE_LOG(LogTemp, Warning, TEXT("RimBoard: Toggled to %s board (effective length: %.2f cm)"),
		bIsOutsideBoard ? TEXT("OUTSIDE") : TEXT("INSIDE"), GetEffectiveLength());
}

float ARimBoard::GetEffectiveLength() const
{
	if (bIsOutsideBoard)
	{
		// Outside boards: full length
		return BoardLength;
	}
	else
	{
		// Inside boards: 3" shorter (7.62cm) to butt against outside boards on both ends
		// Each end butts against an outside board's thickness (1.5" = 3.81cm)
		return BoardLength - (2.0f * BoardWidth); // Subtract 3.81cm from each end = 7.62cm total
	}
}
