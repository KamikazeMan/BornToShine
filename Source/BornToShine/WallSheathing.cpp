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

	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		float MeshWidth = Bounds.BoxExtent.X * 2.0f;
		float MeshHeight = Bounds.BoxExtent.Z * 2.0f;

		if (MeshWidth > 1.0f && MeshHeight > 1.0f)
		{
			const float WallCavityHeight = 247.66f;
			const float BottomExt = 3.82f;   // extends below bottom plate to cover rim board
			const float SideExt = 1.905f;     // extends one side to be flush with corner post face

			float TotalHeight = WallCavityHeight + BottomExt;
			float TotalWidth = SheetWidth + SideExt;

			FVector CurScale = MeshComponent->GetRelativeScale3D();
			float ScaleX = TotalWidth / MeshWidth;
			float ScaleZ = TotalHeight / MeshHeight;
			MeshComponent->SetRelativeScale3D(FVector(ScaleX, CurScale.Y, ScaleZ));

			// Shift mesh so extensions are on the correct sides:
			// +X by SideExt/2 → extension on the +X edge (toward corner)
			// -Z by BottomExt/2 → extension hangs below the wall cavity bottom
			float XShift = SideExt / 2.0f;
			float ZShift = -BottomExt / 2.0f;
			MeshComponent->SetRelativeLocation(FVector(XShift, 0.0f, ZShift));

			// Keep SheetHeight at wall cavity for socket positioning
			SheetHeight = WallCavityHeight;

			UE_LOG(LogTemp, Log, TEXT("WallSheathing: Scaled with extensions - TotalW=%.1f TotalH=%.1f (SideExt=%.2f BottomExt=%.2f)"),
				TotalWidth, TotalHeight, SideExt, BottomExt);
		}
	}

	// Set bottom socket Z to wall cavity bottom (not extended mesh bottom)
	{
		float HalfCavity = SheetHeight / 2.0f;
		for (FConstructionSocket& Socket : Sockets)
		{
			Socket.LocalPosition.Z = -HalfCavity;
		}
	}

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
	// Only along the BOTTOM edge so the sheet always anchors at the bottom
	// plate level and hangs downward correctly.

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
