// Born To Shine - Corner Post Implementation

#include "CornerPost.h"
#include "Components/StaticMeshComponent.h"

ACornerPost::ACornerPost()
{
	PieceType = EPieceType::CornerPost;

	// Same height as wall studs: 92-5/8" for standard 8ft walls
	PostHeight = 235.27f;

	// Scene root for clean actor transform
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ACornerPost::BeginPlay()
{
	Super::BeginPlay();

	// Mesh stays at (1,1,1) — the Rhino model is already correctly sized
	if (MeshComponent)
	{
		MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	}

	UE_LOG(LogTemp, Log, TEXT("CornerPost: BeginPlay - Height=%.1fcm, Total sockets: %d"),
		PostHeight, Sockets.Num());
}

void ACornerPost::InitializeSockets()
{
	Sockets.Empty();

	CreateBottomSocket();
	CreateTopSocket();

	UE_LOG(LogTemp, Log, TEXT("CornerPost: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ACornerPost::CreateBottomSocket()
{
	FConstructionSocket BottomSocket;
	BottomSocket.SocketName = FName(TEXT("PostBottom"));
	BottomSocket.SocketType = EConstructionSocketType::CornerPost_Bottom;
	BottomSocket.LocalPosition = FVector(0.0f, 0.0f, -PostHeight / 2.0f);
	BottomSocket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
	BottomSocket.Orientation = ESocketOrientation::Vertical;
	BottomSocket.bIsOccupied = false;
	Sockets.Add(BottomSocket);
}

void ACornerPost::CreateTopSocket()
{
	FConstructionSocket TopSocket;
	TopSocket.SocketName = FName(TEXT("PostTop"));
	TopSocket.SocketType = EConstructionSocketType::CornerPost_Top;
	TopSocket.LocalPosition = FVector(0.0f, 0.0f, PostHeight / 2.0f);
	TopSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f);
	TopSocket.Orientation = ESocketOrientation::Vertical;
	TopSocket.bIsOccupied = false;
	Sockets.Add(TopSocket);
}

void ACornerPost::ScalePiece(float ScaleDelta)
{
	// Corner posts: scroll wheel scaling is DISABLED
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}
