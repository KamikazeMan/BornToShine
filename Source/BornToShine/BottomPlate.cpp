// Born To Shine - Bottom Plate Implementation

#include "BottomPlate.h"
#include "Components/StaticMeshComponent.h"

ABottomPlate::ABottomPlate()
{
	PieceType = EPieceType::WallPlate;

	// 2x4 lumber actual dimensions
	BoardWidth = 3.81f;   // 1.5 inches
	BoardHeight = 8.89f;  // 3.5 inches
	CurrentLengthFeet = 8;
	BoardLength = CurrentLengthFeet * 30.48f; // Convert feet to cm

	// Scene root for clean actor transform (same pattern as RimBoard)
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	// Bottom plates require manual nailing
	bAutoNailOnPlace = false;

	// Actor scale stays at (1,1,1) — SceneRoot is unscaled
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ABottomPlate::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("BottomPlate: BeginPlay - %d ft (%.1f cm), Total sockets: %d"),
		CurrentLengthFeet, BoardLength, Sockets.Num());
}

void ABottomPlate::InitializeSockets()
{
	Sockets.Empty();

	CreateBottomSockets();
	CreateEndSockets();
	CreateTopFaceSockets();

	UE_LOG(LogTemp, Log, TEXT("BottomPlate: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ABottomPlate::CreateBottomSockets()
{
	float HalfLen = BoardLength / 2.0f;
	float SocketZ = -BoardHeight / 2.0f;
	float Spacing = 40.64f; // 16" OC

	// End bottom sockets (at both ends of the plate)
	{
		FConstructionSocket LeftBottom;
		LeftBottom.SocketName = FName(TEXT("PlateBottom_Left"));
		LeftBottom.SocketType = EConstructionSocketType::BottomPlate_Bottom;
		LeftBottom.LocalPosition = FVector(-HalfLen, 0.0f, SocketZ);
		LeftBottom.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
		LeftBottom.bIsOccupied = false;
		Sockets.Add(LeftBottom);

		FConstructionSocket RightBottom;
		RightBottom.SocketName = FName(TEXT("PlateBottom_Right"));
		RightBottom.SocketType = EConstructionSocketType::BottomPlate_Bottom;
		RightBottom.LocalPosition = FVector(HalfLen, 0.0f, SocketZ);
		RightBottom.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
		RightBottom.bIsOccupied = false;
		Sockets.Add(RightBottom);
	}

	// Center bottom socket
	{
		FConstructionSocket CenterBottom;
		CenterBottom.SocketName = FName(TEXT("PlateBottom_Center"));
		CenterBottom.SocketType = EConstructionSocketType::BottomPlate_Bottom;
		CenterBottom.LocalPosition = FVector(0.0f, 0.0f, SocketZ);
		CenterBottom.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
		CenterBottom.bIsOccupied = false;
		Sockets.Add(CenterBottom);
	}

	// Intermediate bottom sockets at 16" OC
	int32 Count = 0;
	float CurrentX = -HalfLen + Spacing;
	while (CurrentX < HalfLen - Spacing / 2.0f)
	{
		FConstructionSocket BottomSocket;
		BottomSocket.SocketName = FName(*FString::Printf(TEXT("PlateBottom_%d"), Count));
		BottomSocket.SocketType = EConstructionSocketType::BottomPlate_Bottom;
		BottomSocket.LocalPosition = FVector(CurrentX, 0.0f, SocketZ);
		BottomSocket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
		BottomSocket.bIsOccupied = false;
		Sockets.Add(BottomSocket);

		CurrentX += Spacing;
		Count++;
	}

	UE_LOG(LogTemp, Log, TEXT("BottomPlate: Created %d bottom sockets (incl. ends + center)"), Count + 3);
}

void ABottomPlate::CreateEndSockets()
{
	float HalfLen = BoardLength / 2.0f;

	// Left end socket for plate-to-plate corner connections
	FConstructionSocket LeftEnd;
	LeftEnd.SocketName = FName(TEXT("PlateEnd_Left"));
	LeftEnd.SocketType = EConstructionSocketType::BottomPlate_End;
	LeftEnd.LocalPosition = FVector(-HalfLen, 0.0f, 0.0f);
	LeftEnd.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	LeftEnd.bIsOccupied = false;
	Sockets.Add(LeftEnd);

	// Right end socket
	FConstructionSocket RightEnd;
	RightEnd.SocketName = FName(TEXT("PlateEnd_Right"));
	RightEnd.SocketType = EConstructionSocketType::BottomPlate_End;
	RightEnd.LocalPosition = FVector(HalfLen, 0.0f, 0.0f);
	RightEnd.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RightEnd.bIsOccupied = false;
	Sockets.Add(RightEnd);

	UE_LOG(LogTemp, Log, TEXT("BottomPlate: Created 2 end sockets at X=%.2f and X=%.2f"), -HalfLen, HalfLen);
}

void ABottomPlate::CreateTopFaceSockets()
{
	// Top face sockets at 16" OC for wall stud attachment
	float Spacing = 40.64f; // 16" OC
	float HalfLen = BoardLength / 2.0f;
	float SocketZ = BoardHeight / 2.0f;

	float CurrentX = -HalfLen + Spacing;
	int32 Count = 0;

	while (CurrentX < HalfLen - Spacing / 2.0f)
	{
		FConstructionSocket TopSocket;
		TopSocket.SocketName = FName(*FString::Printf(TEXT("PlateTop_%d"), Count));
		TopSocket.SocketType = EConstructionSocketType::Wall_Bottom_Plate;
		TopSocket.LocalPosition = FVector(CurrentX, 0.0f, SocketZ);
		TopSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f);
		TopSocket.bIsOccupied = false;
		Sockets.Add(TopSocket);

		CurrentX += Spacing;
		Count++;
	}

	UE_LOG(LogTemp, Log, TEXT("BottomPlate: Created %d top face sockets at 16\" OC for stud placement"), Count);
}

void ABottomPlate::SetBoardLengthFeet(int32 LengthInFeet)
{
	LengthInFeet = FMath::Clamp(LengthInFeet, MinLengthFeet, MaxLengthFeet);

	if (LengthInFeet != CurrentLengthFeet)
	{
		CurrentLengthFeet = LengthInFeet;
		BoardLength = CurrentLengthFeet * 30.48f;

		// Update mesh scale
		if (MeshComponent)
		{
			MeshComponent->SetRelativeScale3D(FVector(
				BoardLength / 100.0f,
				BoardWidth / 100.0f,
				BoardHeight / 100.0f
			));
		}

		// Regenerate sockets with new length
		RegenerateSockets();

		UE_LOG(LogTemp, Log, TEXT("BottomPlate: Length changed to %s"),
			*GetLengthDisplayString());
	}
}

int32 ABottomPlate::GetBoardLengthFeet() const
{
	return CurrentLengthFeet;
}

FString ABottomPlate::GetLengthDisplayString() const
{
	return FString::Printf(TEXT("%d ft (%.1f cm)"), CurrentLengthFeet, BoardLength);
}

void ABottomPlate::ScalePiece(float ScaleDelta)
{
	// Bottom plates: Scroll wheel scaling is DISABLED
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ABottomPlate::RegenerateSockets()
{
	Sockets.Empty();

	CreateBottomSockets();
	CreateEndSockets();
	CreateTopFaceSockets();

	UE_LOG(LogTemp, Log, TEXT("BottomPlate: Regenerated %d sockets for %d ft plate"), Sockets.Num(), CurrentLengthFeet);
}

void ABottomPlate::ExtendMeshForFlushCorners()
{
	if (!MeshComponent) return;

	// Extend X by the ratio (BoardLength + BoardWidth) / BoardLength
	// to add HalfWidth (1.905cm) to each end — same formula as rim boards.
	FVector CurrentScale3D = MeshComponent->GetRelativeScale3D();
	float Ratio = (BoardLength + BoardWidth) / BoardLength;

	MeshComponent->SetRelativeScale3D(FVector(
		CurrentScale3D.X * Ratio,
		CurrentScale3D.Y,
		CurrentScale3D.Z
	));

	UE_LOG(LogTemp, Log, TEXT("BottomPlate [%s]: ExtendMesh ratio=%.4f scale X: %.4f -> %.4f"),
		*GetName(), Ratio, CurrentScale3D.X, CurrentScale3D.X * Ratio);
}
