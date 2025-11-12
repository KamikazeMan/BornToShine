// Born To Shine - Rim Board Implementation

#include "RimBoard.h"
#include "SocketManager.h"
#include "Components/StaticMeshComponent.h"

ARimBoard::ARimBoard()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set piece type
	PieceType = EPieceType::RimBoard;

	// 2x6 lumber actual dimensions
	BoardWidth = 3.81f;   // 1.5 inches
	BoardHeight = 13.97f; // 5.5 inches
	BoardLength = 243.84f; // 8 feet (default)

	// Default to 16" on-center joist spacing
	JoistSpacing = 40.64f; // 16 inches
	bUse24InchSpacing = false;

	// Rim boards are wood - require manual nailing
	bAutoNailOnPlace = false;

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

	// Generate all sockets
	CreateBottomEndSockets();
	CreateTopFaceSockets();
	CreateSideFaceSockets();
	CreateEndCornerSockets();

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Generated %d sockets"), Sockets.Num());
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
		BottomSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f); // Facing down
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
	LeftEndSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f); // Facing down
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
	RightEndSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f); // Facing down
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
	// End corner sockets for rim-to-rim 90-degree connections
	// Four corners at each end: top-left, top-right, bottom-left, bottom-right

	// Left end corners
	FVector LeftEndCenter = FVector(-BoardLength / 2.0f, 0.0f, 0.0f);

	// Top-left corner of left end
	FConstructionSocket TL_Left;
	TL_Left.SocketName = FName("EndCorner_Left_TopLeft");
	TL_Left.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	TL_Left.LocalPosition = LeftEndCenter + FVector(0.0f, -BoardWidth / 2.0f, BoardHeight / 2.0f);
	TL_Left.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	TL_Left.bIsOccupied = false;
	Sockets.Add(TL_Left);

	// Top-right corner of left end
	FConstructionSocket TR_Left;
	TR_Left.SocketName = FName("EndCorner_Left_TopRight");
	TR_Left.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	TR_Left.LocalPosition = LeftEndCenter + FVector(0.0f, BoardWidth / 2.0f, BoardHeight / 2.0f);
	TR_Left.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	TR_Left.bIsOccupied = false;
	Sockets.Add(TR_Left);

	// Bottom-left corner of left end
	FConstructionSocket BL_Left;
	BL_Left.SocketName = FName("EndCorner_Left_BottomLeft");
	BL_Left.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	BL_Left.LocalPosition = LeftEndCenter + FVector(0.0f, -BoardWidth / 2.0f, -BoardHeight / 2.0f);
	BL_Left.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	BL_Left.bIsOccupied = false;
	Sockets.Add(BL_Left);

	// Bottom-right corner of left end
	FConstructionSocket BR_Left;
	BR_Left.SocketName = FName("EndCorner_Left_BottomRight");
	BR_Left.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	BR_Left.LocalPosition = LeftEndCenter + FVector(0.0f, BoardWidth / 2.0f, -BoardHeight / 2.0f);
	BR_Left.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	BR_Left.bIsOccupied = false;
	Sockets.Add(BR_Left);

	// Right end corners
	FVector RightEndCenter = FVector(BoardLength / 2.0f, 0.0f, 0.0f);

	// Top-left corner of right end
	FConstructionSocket TL_Right;
	TL_Right.SocketName = FName("EndCorner_Right_TopLeft");
	TL_Right.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	TL_Right.LocalPosition = RightEndCenter + FVector(0.0f, -BoardWidth / 2.0f, BoardHeight / 2.0f);
	TL_Right.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	TL_Right.bIsOccupied = false;
	Sockets.Add(TL_Right);

	// Top-right corner of right end
	FConstructionSocket TR_Right;
	TR_Right.SocketName = FName("EndCorner_Right_TopRight");
	TR_Right.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	TR_Right.LocalPosition = RightEndCenter + FVector(0.0f, BoardWidth / 2.0f, BoardHeight / 2.0f);
	TR_Right.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	TR_Right.bIsOccupied = false;
	Sockets.Add(TR_Right);

	// Bottom-left corner of right end
	FConstructionSocket BL_Right;
	BL_Right.SocketName = FName("EndCorner_Right_BottomLeft");
	BL_Right.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	BL_Right.LocalPosition = RightEndCenter + FVector(0.0f, -BoardWidth / 2.0f, -BoardHeight / 2.0f);
	BL_Right.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	BL_Right.bIsOccupied = false;
	Sockets.Add(BL_Right);

	// Bottom-right corner of right end
	FConstructionSocket BR_Right;
	BR_Right.SocketName = FName("EndCorner_Right_BottomRight");
	BR_Right.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	BR_Right.LocalPosition = RightEndCenter + FVector(0.0f, BoardWidth / 2.0f, -BoardHeight / 2.0f);
	BR_Right.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	BR_Right.bIsOccupied = false;
	Sockets.Add(BR_Right);

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Created 8 end corner sockets"));
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
	// Use socket snapping if near foundation or other rim boards
	// Otherwise fall back to free placement
	Super::UpdatePreviewPosition(NewLocation, NewRotation);

	// Rim boards should align flush with foundation top surface
	// The foundation top is at SocketHeightOffset (14cm) + BlockDimensions.Z (30.48cm) = 44.48cm
	// Rim board bottom should sit at this height
	// So rim board center should be at: 44.48 + (BoardHeight / 2.0f)

	if (!bIsSnapped && MeshComponent)
	{
		FVector CurrentLocation = MeshComponent->GetComponentLocation();
		float TargetHeight = 44.48f + (BoardHeight / 2.0f); // Foundation top + half rim board height
		MeshComponent->SetWorldLocation(FVector(CurrentLocation.X, CurrentLocation.Y, TargetHeight));
	}
}
