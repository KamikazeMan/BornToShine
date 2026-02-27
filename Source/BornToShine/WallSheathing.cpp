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
			const float TopExt = 3.81f;       // extends above double top plate
			const float SideExt = 1.905f;     // extends one side to cover corner post face (half of 2x4 width)

			float TotalHeight = WallCavityHeight + BottomExt + TopExt;
			float TotalWidth = SheetWidth + SideExt;

			FVector CurScale = MeshComponent->GetRelativeScale3D();
			float ScaleX = TotalWidth / MeshWidth;
			float ScaleZ = TotalHeight / MeshHeight;
			MeshComponent->SetRelativeScale3D(FVector(ScaleX, CurScale.Y, ScaleZ));

			// Default: extension centered (split both sides). SetCornerExtensionSide()
			// will shift it to the correct side when the snap detects a corner.
			// ZShift centers the mesh so BottomExt hangs below and TopExt above the wall cavity
			float ZShift = (TopExt - BottomExt) / 2.0f;
			MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, ZShift));

			// Keep SheetHeight at wall cavity for socket positioning
			SheetHeight = WallCavityHeight;

			UE_LOG(LogTemp, Log, TEXT("WallSheathing: Scaled - TotalW=%.1f TotalH=%.1f (SideExt=%.2f BottomExt=%.2f TopExt=%.2f)"),
				TotalWidth, TotalHeight, SideExt, BottomExt, TopExt);
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
	Sockets.Empty();

	// Single bottom-center socket — snaps to bottom plate top face.
	// All positioning (Z, yaw, interior/exterior offset) is handled in DetectSnapCandidates.
	FConstructionSocket Socket;
	Socket.SocketName = FName(TEXT("WallSheathing_Bottom"));
	Socket.SocketType = EConstructionSocketType::WallSheathing_Face;
	Socket.LocalPosition = FVector(0.0f, 0.0f, -SheetHeight / 2.0f);
	Socket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	Socket.Orientation = ESocketOrientation::Vertical;
	Socket.bIsOccupied = false;
	Sockets.Add(Socket);
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

void AWallSheathing::SetCornerExtensionSide(int32 Side)
{
	if (!MeshComponent) return;

	const float SideExt = 1.905f; // half of 2x4 width
	FVector Loc = MeshComponent->GetRelativeLocation();

	if (Side > 0)
	{
		// Extension on +X (right side in local space)
		Loc.X = SideExt / 2.0f;
	}
	else if (Side < 0)
	{
		// Extension on -X (left side in local space)
		Loc.X = -SideExt / 2.0f;
	}
	else
	{
		// No corner — center the extension
		Loc.X = 0.0f;
	}

	MeshComponent->SetRelativeLocation(Loc);
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
