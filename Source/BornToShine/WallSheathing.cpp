#include "WallSheathing.h"
#include "ConstructionPhaseManager.h"
#include "Components/StaticMeshComponent.h"

AWallSheathing::AWallSheathing()
{
	PieceType = EPieceType::WallSheathing;

	SheetWidth = 121.92f;      // 4ft
	SheetHeight = 243.84f;     // 8ft
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

void AWallSheathing::BeginPlay()
{
	Super::BeginPlay();

	// Scale mesh Z to fill wall cavity (247.66cm) instead of 8ft (243.84cm)
	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		float MeshHeight = Bounds.BoxExtent.Z * 2.0f;

		if (MeshHeight > 1.0f)
		{
			const float WallCavityHeight = 247.66f;
			float ScaleZ = WallCavityHeight / MeshHeight;
			FVector CurScale = MeshComponent->GetRelativeScale3D();
			MeshComponent->SetRelativeScale3D(FVector(CurScale.X, CurScale.Y, ScaleZ));
			SheetHeight = WallCavityHeight;

			UE_LOG(LogTemp, Log, TEXT("WallSheathing: Scaled Z by %.4f to fill wall cavity (%.1f -> %.1fcm)"),
				ScaleZ, MeshHeight, WallCavityHeight);
		}
	}

	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("WallSheathing: BeginPlay - %.1f x %.1f x %.1fcm, Sockets=%d"),
		SheetWidth, SheetHeight, SheetThickness, Sockets.Num());
}

void AWallSheathing::InitializeSockets()
{
	Sockets.Empty();
	CreateFaceSockets();
	UE_LOG(LogTemp, Log, TEXT("WallSheathing: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void AWallSheathing::CreateFaceSockets()
{
	// Create sockets on the back face (Y = -SheetThickness/2) of the sheet.
	// These snap to Wall_Stud_Top or Wall_Stud_Bottom sockets on studs.
	// Sockets at 16" OC along the width and at top/bottom/middle of height.

	float HalfWidth = SheetWidth / 2.0f;
	float HalfHeight = SheetHeight / 2.0f;
	float BackY = -SheetThickness / 2.0f;

	const float Spacing = 40.64f; // 16" OC
	int32 Count = 0;

	// Sockets along the bottom edge (snap to bottom plate top / stud bottoms)
	for (float X = -HalfWidth; X <= HalfWidth + 0.1f; X += Spacing)
	{
		FConstructionSocket Socket;
		Socket.SocketName = FName(*FString::Printf(TEXT("WallSheathing_Bot_%d"), Count));
		Socket.SocketType = EConstructionSocketType::WallSheathing_Face;
		Socket.LocalPosition = FVector(X, BackY, -HalfHeight);
		Socket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
		Socket.Orientation = ESocketOrientation::Vertical;
		Socket.bIsOccupied = false;
		Sockets.Add(Socket);
		Count++;
	}

	// Sockets along the middle (snap to stud faces at mid-height)
	for (float X = -HalfWidth; X <= HalfWidth + 0.1f; X += Spacing)
	{
		FConstructionSocket Socket;
		Socket.SocketName = FName(*FString::Printf(TEXT("WallSheathing_Mid_%d"), Count));
		Socket.SocketType = EConstructionSocketType::WallSheathing_Face;
		Socket.LocalPosition = FVector(X, BackY, 0.0f);
		Socket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
		Socket.Orientation = ESocketOrientation::Vertical;
		Socket.bIsOccupied = false;
		Sockets.Add(Socket);
		Count++;
	}

	// Sockets along the top edge
	for (float X = -HalfWidth; X <= HalfWidth + 0.1f; X += Spacing)
	{
		FConstructionSocket Socket;
		Socket.SocketName = FName(*FString::Printf(TEXT("WallSheathing_Top_%d"), Count));
		Socket.SocketType = EConstructionSocketType::WallSheathing_Face;
		Socket.LocalPosition = FVector(X, BackY, HalfHeight);
		Socket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
		Socket.Orientation = ESocketOrientation::Vertical;
		Socket.bIsOccupied = false;
		Sockets.Add(Socket);
		Count++;
	}
}

void AWallSheathing::AdjustSocketsToMeshBounds()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	FVector MeshScale = MeshComponent->GetRelativeScale3D();
	FVector MeshRelLoc = MeshComponent->GetRelativeLocation();

	float MeshBottomZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * MeshScale.Z + MeshRelLoc.Z;
	float MeshTopZ = (Bounds.Origin.Z + Bounds.BoxExtent.Z) * MeshScale.Z + MeshRelLoc.Z;

	SheetHeight = MeshTopZ - MeshBottomZ;

	// Update socket Z positions to match scaled mesh
	for (FConstructionSocket& Socket : Sockets)
	{
		FString Name = Socket.SocketName.ToString();
		if (Name.Contains(TEXT("_Bot_")))
			Socket.LocalPosition.Z = MeshBottomZ;
		else if (Name.Contains(TEXT("_Mid_")))
			Socket.LocalPosition.Z = (MeshBottomZ + MeshTopZ) / 2.0f;
		else if (Name.Contains(TEXT("_Top_")))
			Socket.LocalPosition.Z = MeshTopZ;
	}

	UE_LOG(LogTemp, Log, TEXT("WallSheathing: AdjustSockets - MeshZ=[%.2f, %.2f] height=%.2fcm"),
		MeshBottomZ, MeshTopZ, SheetHeight);
}

void AWallSheathing::ScalePiece(float ScaleDelta)
{
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

bool AWallSheathing::TryPlace()
{
	if (!Super::TryPlace()) return false;

	// Phase 2: Window cutout logic will go here

	return true;
}
