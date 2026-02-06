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
	bIsOutsideBoard = true;

	// Rim boards are wood - require manual nailing
	bAutoNailOnPlace = false;

	// CRITICAL: Force actor scale to (1,1,1) - we use mesh scale for dimensions
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);

	// Set mesh scale to match dimensions
	if (MeshComponent)
	{
		MeshComponent->SetRelativeScale3D(FVector(
			GetEffectiveLength() / 100.0f,
			BoardWidth / 100.0f,
			BoardHeight / 100.0f
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
	float EffectiveLen = GetEffectiveLength();
	float HalfLen = EffectiveLen / 2.0f;

	// Left end socket
	FConstructionSocket LeftEndSocket;
	LeftEndSocket.SocketName = FName("BottomEnd_Left");
	LeftEndSocket.SocketType = EConstructionSocketType::RimBoard_Bottom_End;
	LeftEndSocket.LocalPosition = FVector(
		-HalfLen,
		0.0f,
		-BoardHeight / 2.0f
	);
	LeftEndSocket.LocalRotation = FRotator::ZeroRotator;
	LeftEndSocket.bIsOccupied = false;
	Sockets.Add(LeftEndSocket);

	// Right end socket
	FConstructionSocket RightEndSocket;
	RightEndSocket.SocketName = FName("BottomEnd_Right");
	RightEndSocket.SocketType = EConstructionSocketType::RimBoard_Bottom_End;
	RightEndSocket.LocalPosition = FVector(
		HalfLen,
		0.0f,
		-BoardHeight / 2.0f
	);
	RightEndSocket.LocalRotation = FRotator::ZeroRotator;
	RightEndSocket.bIsOccupied = false;
	Sockets.Add(RightEndSocket);

	// Center socket
	FConstructionSocket CenterSocket;
	CenterSocket.SocketName = FName("BottomEnd_Center");
	CenterSocket.SocketType = EConstructionSocketType::RimBoard_Bottom_End;
	CenterSocket.LocalPosition = FVector(
		0.0f,
		0.0f,
		-BoardHeight / 2.0f
	);
	CenterSocket.LocalRotation = FRotator::ZeroRotator;
	CenterSocket.bIsOccupied = false;
	Sockets.Add(CenterSocket);

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Created 3 bottom sockets (left, center, right) for %s board"),
		bIsOutsideBoard ? TEXT("OUTSIDE") : TEXT("INSIDE"));
}

void ARimBoard::CreateTopFaceSockets()
{
	float Spacing = bUse24InchSpacing ? 60.96f : 40.64f;

	TArray<FVector> SocketPositions = CalculateJoistSocketPositions();

	for (int32 i = 0; i < SocketPositions.Num(); i++)
	{
		FConstructionSocket TopSocket;
		TopSocket.SocketName = FName(*FString::Printf(TEXT("TopFace_%d"), i));
		TopSocket.SocketType = EConstructionSocketType::RimBoard_Top_Face;
		TopSocket.LocalPosition = SocketPositions[i];
		TopSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f);
		TopSocket.bIsOccupied = false;
		Sockets.Add(TopSocket);
	}

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Created %d top face sockets at %.2f cm spacing"),
		SocketPositions.Num(), Spacing);
}

void ARimBoard::CreateSideFaceSockets()
{
	float Spacing = bUse24InchSpacing ? 60.96f : 40.64f;
	TArray<FVector> JoistPositions = CalculateJoistSocketPositions();

	for (int32 i = 0; i < JoistPositions.Num(); i++)
	{
		FConstructionSocket LeftSideSocket;
		LeftSideSocket.SocketName = FName(*FString::Printf(TEXT("SideFace_Left_%d"), i));
		LeftSideSocket.SocketType = EConstructionSocketType::RimBoard_Side_Face;
		LeftSideSocket.LocalPosition = FVector(
			JoistPositions[i].X,
			-BoardWidth / 2.0f,
			0.0f
		);
		LeftSideSocket.LocalRotation = FRotator(0.0f, -90.0f, 0.0f);
		LeftSideSocket.bIsOccupied = false;
		Sockets.Add(LeftSideSocket);

		FConstructionSocket RightSideSocket;
		RightSideSocket.SocketName = FName(*FString::Printf(TEXT("SideFace_Right_%d"), i));
		RightSideSocket.SocketType = EConstructionSocketType::RimBoard_Side_Face;
		RightSideSocket.LocalPosition = FVector(
			JoistPositions[i].X,
			BoardWidth / 2.0f,
			0.0f
		);
		RightSideSocket.LocalRotation = FRotator(0.0f, 90.0f, 0.0f);
		RightSideSocket.bIsOccupied = false;
		Sockets.Add(RightSideSocket);
	}

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Created %d side face sockets"), JoistPositions.Num() * 2);
}

void ARimBoard::CreateEndCornerSockets()
{
	// CORNER SOCKET STRATEGY:
	// Always use CALCULATED positions based on current effective length.
	// Mesh sockets (Snap_Corner_Left/Right) are unreliable after resize because
	// GetSocketTransform returns the original mesh-space positions, not scaled positions.
	//
	// By always calculating from GetEffectiveLength(), sockets are guaranteed to be
	// at the correct board ends regardless of resizing.

	float EffectiveLen = GetEffectiveLength();
	float HalfLen = EffectiveLen / 2.0f;
	float HalfWidth = BoardWidth / 2.0f; // 1.905 cm — OUTER EDGE offset

	// LEFT end corner socket — at outer edge to prevent T-shape
	FConstructionSocket LeftEnd;
	LeftEnd.SocketName = FName("EndCorner_Left");
	LeftEnd.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	LeftEnd.LocalPosition = FVector(-HalfLen, HalfWidth, 0.0f);
	LeftEnd.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	LeftEnd.bIsOccupied = false;
	Sockets.Add(LeftEnd);

	// RIGHT end corner socket — at outer edge
	FConstructionSocket RightEnd;
	RightEnd.SocketName = FName("EndCorner_Right");
	RightEnd.SocketType = EConstructionSocketType::RimBoard_End_Corner;
	RightEnd.LocalPosition = FVector(HalfLen, HalfWidth, 0.0f);
	RightEnd.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RightEnd.bIsOccupied = false;
	Sockets.Add(RightEnd);

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Corner sockets CALCULATED at Left=(%.2f, %.2f, 0) Right=(%.2f, %.2f, 0) for %s board (EffLen=%.1f cm)"),
		-HalfLen, HalfWidth, HalfLen, HalfWidth,
		bIsOutsideBoard ? TEXT("OUTSIDE") : TEXT("INSIDE"),
		EffectiveLen);
}

TArray<FVector> ARimBoard::CalculateJoistSocketPositions() const
{
	TArray<FVector> Positions;

	float EffectiveLen = GetEffectiveLength();
	float HalfLen = EffectiveLen / 2.0f;

	float Spacing = bUse24InchSpacing ? 60.96f : 40.64f;
	float StartOffset = Spacing;

	float CurrentX = -HalfLen + StartOffset;

	while (CurrentX < HalfLen - StartOffset / 2.0f)
	{
		FVector SocketPos = FVector(
			CurrentX,
			0.0f,
			BoardHeight / 2.0f
		);
		Positions.Add(SocketPos);
		CurrentX += Spacing;
	}

	return Positions;
}

void ARimBoard::UpdatePreviewPosition(const FVector& NewLocation, const FRotator& NewRotation)
{
	Super::UpdatePreviewPosition(NewLocation, NewRotation);
}

void ARimBoard::SetBoardLengthFeet(int32 LengthInFeet)
{
	LengthInFeet = FMath::Clamp(LengthInFeet, MinLengthFeet, MaxLengthFeet);

	if (LengthInFeet != CurrentLengthFeet)
	{
		CurrentLengthFeet = LengthInFeet;
		BoardLength = CurrentLengthFeet * 30.48f;

		// Update mesh scale
		if (MeshComponent)
		{
			float EffectiveLen = GetEffectiveLength();
			MeshComponent->SetRelativeScale3D(FVector(
				EffectiveLen / 100.0f,
				BoardWidth / 100.0f,
				BoardHeight / 100.0f
			));
		}

		// Regenerate sockets — calculated positions will use new effective length
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
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ARimBoard::RegenerateSockets()
{
	Sockets.Empty();

	CreateBottomEndSockets();
	CreateTopFaceSockets();
	CreateSideFaceSockets();
	CreateEndCornerSockets();

	UE_LOG(LogTemp, Log, TEXT("RimBoard: Regenerated %d sockets for %d ft board"), Sockets.Num(), CurrentLengthFeet);
}

void ARimBoard::ToggleBoardType()
{
	bIsOutsideBoard = !bIsOutsideBoard;

	RegenerateSockets();

	if (MeshComponent)
	{
		FVector MeshScale = MeshComponent->GetRelativeScale3D();
		float OldEffectiveLen = bIsOutsideBoard ? (BoardLength - 2.0f * BoardWidth) : BoardLength;
		float NewEffectiveLen = GetEffectiveLength();

		float LengthDiff = NewEffectiveLen - OldEffectiveLen;
		float LengthRatio = NewEffectiveLen / OldEffectiveLen;

		FVector CurrentMeshPos = MeshComponent->GetRelativeLocation();
		MeshComponent->SetRelativeLocation(FVector(
			CurrentMeshPos.X - (LengthDiff / 2.0f),
			CurrentMeshPos.Y,
			CurrentMeshPos.Z
		));

		MeshComponent->SetRelativeScale3D(FVector(
			MeshScale.X * LengthRatio,
			MeshScale.Y,
			MeshScale.Z
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
		return BoardLength;
	}
	else
	{
		return BoardLength - (2.0f * BoardWidth);
	}
}
