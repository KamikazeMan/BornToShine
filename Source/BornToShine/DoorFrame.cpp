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
// TryPlace override — runs auto-delete after successful placement
// ---------------------------------------------------------------------------
bool ADoorFrame::TryPlace()
{
	if (!Super::TryPlace()) return false;

	// Now that the door frame is registered and positioned, remove overlaps
	AutoDeleteOverlappingPieces();
	return true;
}

// ---------------------------------------------------------------------------
// SetPreviewMode override — fallback for save/load path
// ---------------------------------------------------------------------------
void ADoorFrame::SetPreviewMode(bool bIsPreview)
{
	Super::SetPreviewMode(bIsPreview);

	// When transitioning from preview to placed (save/load path), remove overlapping pieces
	if (!bIsPreview)
	{
		AutoDeleteOverlappingPieces();
	}
}

// ---------------------------------------------------------------------------
// AutoDeleteOverlappingPieces
// Removes wall studs and bottom plate sections that overlap the door frame.
// Uses XY projection along the door frame's local axes (Z-independent).
// ---------------------------------------------------------------------------
void ADoorFrame::AutoDeleteOverlappingPieces()
{
	if (!AConstructionPhaseManager::Instance) return;

	FVector DoorLoc = GetActorLocation();
	FRotator DoorRot = GetActorRotation();

	// Door frame's local axes:
	//   ForwardVector (X) = along the wall = along the door opening width
	//   RightVector   (Y) = through the wall = door depth
	FVector DoorAlongWall = DoorRot.RotateVector(FVector::ForwardVector);
	FVector DoorThroughWall = DoorRot.RotateVector(FVector::RightVector);

	// Half-width of the door frame opening (along the wall)
	float HalfWidth = RoughOpeningWidth / 2.0f;
	// Depth tolerance — studs sit roughly in-line through the wall
	const float DepthTolerance = 15.0f; // cm

	int32 DeletedStuds = 0;
	int32 DeletedPlates = 0;

	UE_LOG(LogTemp, Log, TEXT("DoorFrame: AutoDelete check — Loc=(%.1f,%.1f,%.1f) Yaw=%.1f HalfWidth=%.1f"),
		DoorLoc.X, DoorLoc.Y, DoorLoc.Z, DoorRot.Yaw, HalfWidth);

	// --- Delete overlapping wall studs ---
	{
		TArray<ABuildablePiece*> Studs =
			AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::WallStud);

		UE_LOG(LogTemp, Log, TEXT("DoorFrame: Checking %d wall studs for overlap"), Studs.Num());

		for (ABuildablePiece* Piece : Studs)
		{
			if (!Piece || Piece == this) continue;

			FVector Delta = Piece->GetActorLocation() - DoorLoc;
			Delta.Z = 0.0f; // Ignore height — studs and door are on the same wall

			float AlongWall = FMath::Abs(FVector::DotProduct(Delta, DoorAlongWall));
			float ThroughWall = FMath::Abs(FVector::DotProduct(Delta, DoorThroughWall));

			if (AlongWall < HalfWidth && ThroughWall < DepthTolerance)
			{
				UE_LOG(LogTemp, Log, TEXT("DoorFrame: Auto-deleting WallStud %s (alongWall=%.1f, throughWall=%.1f)"),
					*Piece->GetName(), AlongWall, ThroughWall);

				AConstructionPhaseManager::Instance->UnregisterPiece(Piece);
				Piece->Destroy();
				DeletedStuds++;
			}
		}
	}

	// --- Delete bottom plate section under the door opening ---
	{
		TArray<ABuildablePiece*> Nearby =
			AConstructionPhaseManager::Instance->GetNearbyPieces(DoorLoc, 500.0f);

		for (ABuildablePiece* P : Nearby)
		{
			if (!P || P == this) continue;

			ABottomPlate* Plate = Cast<ABottomPlate>(P);
			if (!Plate) continue;

			FVector Delta = Plate->GetActorLocation() - DoorLoc;
			Delta.Z = 0.0f;

			float AlongWall = FMath::Abs(FVector::DotProduct(Delta, DoorAlongWall));
			float ThroughWall = FMath::Abs(FVector::DotProduct(Delta, DoorThroughWall));

			if (AlongWall < HalfWidth && ThroughWall < DepthTolerance)
			{
				UE_LOG(LogTemp, Log, TEXT("DoorFrame: Auto-deleting BottomPlate %s (alongWall=%.1f, throughWall=%.1f)"),
					*Plate->GetName(), AlongWall, ThroughWall);

				AConstructionPhaseManager::Instance->UnregisterPiece(Plate);
				Plate->Destroy();
				DeletedPlates++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("DoorFrame: Auto-deleted %d studs and %d plates (HalfWidth=%.1f)"),
		DeletedStuds, DeletedPlates, HalfWidth);
}
