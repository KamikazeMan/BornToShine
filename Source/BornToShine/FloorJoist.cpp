// Born To Shine - Floor Joist Implementation

#include "FloorJoist.h"
#include "Components/StaticMeshComponent.h"

AFloorJoist::AFloorJoist()
{
	PieceType = EPieceType::FloorJoist;

	// Same 2x6 dimensions as rim board (inherited)
	// BoardWidth = 3.81cm, BoardHeight = 13.97cm, BoardLength = 243.84cm
}

void AFloorJoist::InitializeSockets()
{
	Sockets.Empty();

	CreateJoistEndSockets();
	CreateJoistTopSockets();

	UE_LOG(LogTemp, Log, TEXT("FloorJoist: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void AFloorJoist::CreateJoistEndSockets()
{
	float HalfLen = GetEffectiveLength() / 2.0f;

	// Left end socket — snaps to RimBoard_Top_Face
	FConstructionSocket LeftEnd;
	LeftEnd.SocketName = FName(TEXT("JoistEnd_Left"));
	LeftEnd.SocketType = EConstructionSocketType::Joist_End;
	LeftEnd.LocalPosition = FVector(-HalfLen, 0.0f, 0.0f);
	LeftEnd.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	LeftEnd.bIsOccupied = false;
	Sockets.Add(LeftEnd);

	// Right end socket — snaps to RimBoard_Top_Face
	FConstructionSocket RightEnd;
	RightEnd.SocketName = FName(TEXT("JoistEnd_Right"));
	RightEnd.SocketType = EConstructionSocketType::Joist_End;
	RightEnd.LocalPosition = FVector(HalfLen, 0.0f, 0.0f);
	RightEnd.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RightEnd.bIsOccupied = false;
	Sockets.Add(RightEnd);

	UE_LOG(LogTemp, Log, TEXT("FloorJoist: Created 2 end sockets at X=%.2f and X=%.2f"),
		-HalfLen, HalfLen);
}

void AFloorJoist::CreateJoistTopSockets()
{
	// Top face sockets for plywood sheathing — same spacing pattern as rim board
	float Spacing = bUse24InchSpacing ? 60.96f : 40.64f;
	float HalfLen = GetEffectiveLength() / 2.0f;
	float StartOffset = Spacing;
	float CurrentX = -HalfLen + StartOffset;
	int32 Count = 0;

	while (CurrentX < HalfLen - StartOffset / 2.0f)
	{
		FConstructionSocket TopSocket;
		TopSocket.SocketName = FName(*FString::Printf(TEXT("JoistTop_%d"), Count));
		TopSocket.SocketType = EConstructionSocketType::Joist_Top_Face;
		TopSocket.LocalPosition = FVector(CurrentX, 0.0f, BoardHeight / 2.0f);
		TopSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f);
		TopSocket.bIsOccupied = false;
		Sockets.Add(TopSocket);

		CurrentX += Spacing;
		Count++;
	}

	UE_LOG(LogTemp, Log, TEXT("FloorJoist: Created %d top face sockets at %.2f cm spacing"), Count, Spacing);
}
