// Born To Shine - Door Frame (single pre-modeled mesh)

#include "DoorFrame.h"
#include "ConstructionPhaseManager.h"
#include "WallStud.h"
#include "BottomPlate.h"
#include "Components/StaticMeshComponent.h"

ADoorFrame::ADoorFrame()
{
	PieceType = EPieceType::DoorFrame;

	// Default dimensions — overridden by actual mesh bounds in BeginPlay
	FrameHeight = 235.27f;       // 92-5/8" (standard 8ft wall stud height)
	RoughOpeningWidth = 91.44f;  // 36" standard door

	// SceneRoot decouples mesh scale from actor transform
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	MeshComponent->SetupAttachment(SceneRoot);

	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------
void ADoorFrame::BeginPlay()
{
	Super::BeginPlay();

	// Mesh stays at (1,1,1) — pre-modeled in Rhino at real-world scale
	MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

	// Align sockets to actual mesh extents
	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("DoorFrame: BeginPlay - Height=%.1fcm, RoughOpening=%.1fcm, Sockets: %d"),
		FrameHeight, RoughOpeningWidth, Sockets.Num());
}

// ---------------------------------------------------------------------------
// Sockets
// ---------------------------------------------------------------------------
void ADoorFrame::InitializeSockets()
{
	Sockets.Empty();
	CreateBottomSocket();
	CreateTopSocket();
	UE_LOG(LogTemp, Log, TEXT("DoorFrame: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ADoorFrame::CreateBottomSocket()
{
	FConstructionSocket BottomSocket;
	BottomSocket.SocketName = FName(TEXT("FrameBottom"));
	BottomSocket.SocketType = EConstructionSocketType::DoorFrame_Bottom;
	BottomSocket.LocalPosition = FVector(0.0f, 0.0f, -FrameHeight / 2.0f);
	BottomSocket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
	BottomSocket.Orientation = ESocketOrientation::Vertical;
	BottomSocket.bIsOccupied = false;
	Sockets.Add(BottomSocket);
}

void ADoorFrame::CreateTopSocket()
{
	FConstructionSocket TopSocket;
	TopSocket.SocketName = FName(TEXT("FrameTop"));
	TopSocket.SocketType = EConstructionSocketType::DoorFrame_Top;
	TopSocket.LocalPosition = FVector(0.0f, 0.0f, FrameHeight / 2.0f);
	TopSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f);
	TopSocket.Orientation = ESocketOrientation::Vertical;
	TopSocket.bIsOccupied = false;
	Sockets.Add(TopSocket);
}

// ---------------------------------------------------------------------------
// Adjust sockets to match actual mesh bounds (same pattern as CornerPost)
// ---------------------------------------------------------------------------
void ADoorFrame::AdjustSocketsToMeshBounds()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	FVector MeshRelLoc = MeshComponent->GetRelativeLocation();

	float MeshBottomZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) + MeshRelLoc.Z;
	float MeshTopZ    = (Bounds.Origin.Z + Bounds.BoxExtent.Z) + MeshRelLoc.Z;
	float ActualHeight = MeshTopZ - MeshBottomZ;

	if (ActualHeight >= 1.0f)
	{
		FrameHeight = ActualHeight;

		// Update rough opening width from mesh X extent
		float MeshHalfX = Bounds.BoxExtent.X;
		if (MeshHalfX > 1.0f)
		{
			RoughOpeningWidth = MeshHalfX * 2.0f;
		}

		for (FConstructionSocket& Socket : Sockets)
		{
			if (Socket.SocketName == FName("FrameBottom"))
				Socket.LocalPosition.Z = MeshBottomZ;
			else if (Socket.SocketName == FName("FrameTop"))
				Socket.LocalPosition.Z = MeshTopZ;
		}

		UE_LOG(LogTemp, Log,
			TEXT("DoorFrame: Mesh bounds Z=[%.2f, %.2f] height=%.2fcm, width=%.2fcm"),
			MeshBottomZ, MeshTopZ, ActualHeight, RoughOpeningWidth);
	}
}

// ---------------------------------------------------------------------------
// ScalePiece — disabled (no scroll wheel resizing)
// ---------------------------------------------------------------------------
void ADoorFrame::ScalePiece(float ScaleDelta)
{
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
}

// ---------------------------------------------------------------------------
// SetPreviewMode override — triggers auto-delete when placed
// ---------------------------------------------------------------------------
void ADoorFrame::SetPreviewMode(bool bIsPreview)
{
	Super::SetPreviewMode(bIsPreview);

	// When transitioning from preview to placed, remove overlapping pieces
	if (!bIsPreview)
	{
		AutoDeleteOverlappingPieces();
	}
}

// ---------------------------------------------------------------------------
// AutoDeleteOverlappingPieces
// Removes wall studs and bottom plate sections that overlap the door frame.
// ---------------------------------------------------------------------------
void ADoorFrame::AutoDeleteOverlappingPieces()
{
	if (!AConstructionPhaseManager::Instance) return;

	// Get our world-space bounding box (with some tolerance)
	FBox DoorBox = MeshComponent->Bounds.GetBox();

	// Shrink slightly so we don't catch studs just outside the frame
	const float Tolerance = 2.0f; // cm
	DoorBox = DoorBox.ExpandBy(-Tolerance);

	int32 DeletedStuds = 0;
	int32 DeletedPlates = 0;

	// --- Delete overlapping wall studs ---
	{
		TArray<ABuildablePiece*> Studs =
			AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::WallStud);

		for (ABuildablePiece* Piece : Studs)
		{
			if (!Piece || Piece == this) continue;

			// Check if stud center is inside the door frame box
			FVector StudLoc = Piece->GetActorLocation();
			if (DoorBox.IsInside(StudLoc))
			{
				UE_LOG(LogTemp, Log, TEXT("DoorFrame: Auto-deleting WallStud %s at (%.1f, %.1f, %.1f)"),
					*Piece->GetName(), StudLoc.X, StudLoc.Y, StudLoc.Z);

				AConstructionPhaseManager::Instance->UnregisterPiece(Piece);
				Piece->Destroy();
				DeletedStuds++;
			}
		}
	}

	// --- Delete bottom plate section under the door opening ---
	// A bottom plate whose center falls inside the door frame's XY footprint
	// is the plate section that needs to be cut out for the door.
	{
		TArray<ABuildablePiece*> Plates =
			AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::WallPlate);

		// Also check BottomPlate type pieces that might be registered differently
		TArray<ABuildablePiece*> BottomPlates;
		// WallPlate type might not be used — check for actual BottomPlate actors
		// by iterating nearby pieces
		if (AConstructionPhaseManager::Instance)
		{
			// Get all nearby pieces and filter for bottom plates
			TArray<ABuildablePiece*> Nearby =
				AConstructionPhaseManager::Instance->GetNearbyPieces(GetActorLocation(), 500.0f);

			for (ABuildablePiece* P : Nearby)
			{
				if (!P || P == this) continue;

				ABottomPlate* Plate = Cast<ABottomPlate>(P);
				if (!Plate) continue;

				// Check if this plate overlaps with the door frame XY footprint
				// A plate under the door has its center near the door's XY position
				FVector PlateLoc = Plate->GetActorLocation();

				// Use the door frame box but extend Z to catch the plate below
				FBox PlateCheckBox = DoorBox;
				PlateCheckBox.Min.Z -= 50.0f; // Extend downward to catch plate below
				PlateCheckBox.Max.Z = DoorBox.Min.Z + 20.0f; // Only check below door frame

				if (PlateCheckBox.IsInside(PlateLoc))
				{
					UE_LOG(LogTemp, Log, TEXT("DoorFrame: Auto-deleting BottomPlate %s at (%.1f, %.1f, %.1f)"),
						*Plate->GetName(), PlateLoc.X, PlateLoc.Y, PlateLoc.Z);

					AConstructionPhaseManager::Instance->UnregisterPiece(Plate);
					Plate->Destroy();
					DeletedPlates++;
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("DoorFrame: Auto-deleted %d studs and %d plate sections"),
		DeletedStuds, DeletedPlates);
}
