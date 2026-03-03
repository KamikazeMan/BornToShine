#include "RoofSheathing.h"
#include "Components/StaticMeshComponent.h"

ARoofSheathing::ARoofSheathing()
{
	PieceType = EPieceType::RoofSheathing;

	SheetLength = 243.84f;     // 8ft along ridge
	SheetWidth = 121.92f;      // 4ft up slope
	SheetThickness = 1.27f;    // 0.50"

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ARoofSheathing::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		float MeshX = Bounds.BoxExtent.X * 2.0f;  // should be ~243.84
		float MeshY = Bounds.BoxExtent.Y * 2.0f;  // should be ~121.92
		float MeshZ = Bounds.BoxExtent.Z * 2.0f;  // should be ~1.27

		UE_LOG(LogTemp, Log, TEXT("RoofSheathing: BeginPlay - Mesh=%.1fx%.1fx%.1f, SheetLen=%.1f SheetW=%.1f"),
			MeshX, MeshY, MeshZ, SheetLength, SheetWidth);
	}
}

void ARoofSheathing::InitializeSockets()
{
	Sockets.Empty();
	CreateFaceSockets();
	UE_LOG(LogTemp, Log, TEXT("RoofSheathing: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ARoofSheathing::CreateFaceSockets()
{
	Sockets.Empty();

	// Single bottom-center socket on the underside of the sheet.
	// This snaps to Rafter_Top_Face sockets on rafters.
	// All positioning (pitch matching, ridge alignment, tiling) handled in DetectSnapCandidates.
	FConstructionSocket Socket;
	Socket.SocketName = FName(TEXT("RoofSheathing_Bottom"));
	Socket.SocketType = EConstructionSocketType::RoofSheathing_Face;
	Socket.LocalPosition = FVector(0.0f, 0.0f, -SheetThickness / 2.0f);
	Socket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	Socket.Orientation = ESocketOrientation::Horizontal;
	Socket.bIsOccupied = false;
	Sockets.Add(Socket);
}

void ARoofSheathing::ScalePiece(float ScaleDelta)
{
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}
