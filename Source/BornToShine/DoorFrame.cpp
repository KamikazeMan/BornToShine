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
	RoughOpeningWidth = 91.44f;  // 36" rough opening (gap between trimmers)
	FrameOverallWidth = 91.44f;  // Full mesh footprint (king studs + trimmers + opening)

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

	UE_LOG(LogTemp, Warning, TEXT("=== HEIGHT DIAGNOSTIC === DoorFrame king stud height: %.2fcm (%.2f in)"),
		FrameHeight, FrameHeight / 2.54f);

	UE_LOG(LogTemp, Log, TEXT("DoorFrame: BeginPlay - Height=%.1fcm, RoughOpening=%.1fcm, FrameOverall=%.1fcm, Sockets: %d"),
		FrameHeight, RoughOpeningWidth, FrameOverallWidth, Sockets.Num());
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

		// Full mesh extent drives stud overlap deletion (covers king studs + trimmers)
		// RoughOpeningWidth stays at the configured value (36" default) for the plate cut
		float MeshHalfX = Bounds.BoxExtent.X;
		if (MeshHalfX > 1.0f)
		{
			FrameOverallWidth = MeshHalfX * 2.0f;
		}

		for (FConstructionSocket& Socket : Sockets)
		{
			if (Socket.SocketName == FName("FrameBottom"))
				Socket.LocalPosition.Z = MeshBottomZ;
			else if (Socket.SocketName == FName("FrameTop"))
				Socket.LocalPosition.Z = MeshTopZ;
		}

		UE_LOG(LogTemp, Log,
			TEXT("DoorFrame: Mesh bounds Z=[%.2f, %.2f] height=%.2fcm, FrameOverall=%.2fcm, RoughOpening=%.2fcm"),
			MeshBottomZ, MeshTopZ, ActualHeight, FrameOverallWidth, RoughOpeningWidth);
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

	if (!bIsPreview)
	{
		// Door frame must not block pawn movement — player needs to walk through the opening.
		// Super sets ECR_Block for ECC_Pawn; override to ECR_Ignore for door frames.
		if (MeshComponent)
		{
			MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		}

		// Remove overlapping pieces (save/load path)
		AutoDeleteOverlappingPieces();
	}
}

// ---------------------------------------------------------------------------
// AutoDeleteOverlappingPieces
// 1. Deletes wall studs that fall inside the door opening.
// 2. Splits the bottom plate under the door: removes the original and
//    spawns two shorter remnant plates on each side of the opening.
// ---------------------------------------------------------------------------
void ADoorFrame::AutoDeleteOverlappingPieces()
{
	// Guard: run only once (TryPlace and SetPreviewMode both call this)
	if (bHasAutoDeleted) return;
	bHasAutoDeleted = true;

	if (!AConstructionPhaseManager::Instance) return;

	FVector DoorLoc = GetActorLocation();
	FRotator DoorRot = GetActorRotation();

	// Door frame's local axes (matches plate axes since we take plate yaw)
	FVector DoorAlongWall = DoorRot.RotateVector(FVector::ForwardVector);
	FVector DoorThroughWall = DoorRot.RotateVector(FVector::RightVector);

	// Full frame footprint for stud deletion (king studs + trimmers + opening)
	float StudHalfWidth = FrameOverallWidth / 2.0f;
	// Rough opening only for plate cut (the gap the door occupies in the floor)
	float PlateHalfCut = RoughOpeningWidth / 2.0f;
	const float DepthTolerance = 15.0f; // cm

	int32 DeletedStuds = 0;

	UE_LOG(LogTemp, Warning, TEXT("DoorFrame: AutoDelete — Loc=(%.1f,%.1f,%.1f) Yaw=%.1f StudHalfW=%.1f PlateHalfCut=%.1f"),
		DoorLoc.X, DoorLoc.Y, DoorLoc.Z, DoorRot.Yaw, StudHalfWidth, PlateHalfCut);

	// --- 1. Delete overlapping wall studs (uses full frame width) ---
	{
		TArray<ABuildablePiece*> Studs =
			AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::WallStud);

		UE_LOG(LogTemp, Log, TEXT("DoorFrame: Checking %d wall studs for overlap"), Studs.Num());

		for (ABuildablePiece* Piece : Studs)
		{
			if (!Piece || Piece == this) continue;

			FVector Delta = Piece->GetActorLocation() - DoorLoc;
			Delta.Z = 0.0f;

			float AlongWall = FMath::Abs(FVector::DotProduct(Delta, DoorAlongWall));
			float ThroughWall = FMath::Abs(FVector::DotProduct(Delta, DoorThroughWall));

			if (AlongWall < StudHalfWidth && ThroughWall < DepthTolerance)
			{
				UE_LOG(LogTemp, Log, TEXT("DoorFrame: Deleting WallStud %s (along=%.1f, through=%.1f)"),
					*Piece->GetName(), AlongWall, ThroughWall);
				AConstructionPhaseManager::Instance->UnregisterPiece(Piece);
				Piece->Destroy();
				DeletedStuds++;
			}
		}
	}

	// --- 2. Split the bottom plate under the door opening ---
	// Find the plate we snapped to
	ABottomPlate* OrigPlate = Cast<ABottomPlate>(SnappedToPiece);

	UE_LOG(LogTemp, Warning, TEXT("DoorFrame: SnappedToPiece=%s  Cast=%s"),
		SnappedToPiece ? *SnappedToPiece->GetName() : TEXT("null"),
		OrigPlate ? *OrigPlate->GetName() : TEXT("null"));

	// Fallback: search nearby if snap reference isn't a plate
	if (!OrigPlate)
	{
		TArray<ABuildablePiece*> Nearby =
			AConstructionPhaseManager::Instance->GetNearbyPieces(DoorLoc, 500.0f);
		for (ABuildablePiece* P : Nearby)
		{
			ABottomPlate* Plate = Cast<ABottomPlate>(P);
			if (!Plate) continue;
			FVector Delta = Plate->GetActorLocation() - DoorLoc;
			Delta.Z = 0.0f;
			if (FMath::Abs(FVector::DotProduct(Delta, DoorThroughWall)) < DepthTolerance)
			{
				OrigPlate = Plate;
				UE_LOG(LogTemp, Warning, TEXT("DoorFrame: Fallback found plate %s"), *Plate->GetName());
				break;
			}
		}
	}

	if (!OrigPlate)
	{
		UE_LOG(LogTemp, Warning, TEXT("DoorFrame: No bottom plate found to split"));
		return;
	}

	// --- Capture original plate properties BEFORE destroying ---
	FVector  PlatePos    = OrigPlate->GetActorLocation();
	FRotator PlateRot    = OrigPlate->GetActorRotation();
	float    PlateLength = OrigPlate->BoardLength;        // cm
	float    PlateHalfLen = PlateLength / 2.0f;
	UClass*  PlateClass  = OrigPlate->GetClass();

	// Plate forward axis (along its length)
	FVector PlateForward = PlateRot.RotateVector(FVector::ForwardVector);

	// Project door centre onto plate axis (signed offset from plate centre)
	FVector DeltaDP = DoorLoc - PlatePos;
	DeltaDP.Z = 0.0f;
	float DoorCentreOnPlate = FVector::DotProduct(DeltaDP, PlateForward);

	// Remnant lengths on each side of the opening (uses RoughOpeningWidth, NOT full frame)
	float LeftLength  = PlateHalfLen + DoorCentreOnPlate - PlateHalfCut;
	float RightLength = PlateHalfLen - DoorCentreOnPlate - PlateHalfCut;

	UE_LOG(LogTemp, Warning, TEXT("DoorFrame: Plate split — PlateLen=%.1f DoorOffset=%.1f PlateHalfCut=%.1f → Left=%.1f  Right=%.1f"),
		PlateLength, DoorCentreOnPlate, PlateHalfCut, LeftLength, RightLength);

	// Minimum remnant: 1 ft (30.48 cm).  Shorter pieces would be clamped up
	// by SetBoardLengthCm and overlap the door frame, so skip them.
	const float MinRemnant = 30.48f;

	// --- Destroy original plate ---
	AConstructionPhaseManager::Instance->UnregisterPiece(OrigPlate);
	SnappedToPiece = nullptr; // clear dangling reference
	OrigPlate->Destroy();

	// Force AlwaysSpawn so pending-destroy collision doesn't block the spawn
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// --- Spawn left remnant ---
	if (LeftLength >= MinRemnant)
	{
		// Centre of left remnant in plate-local X
		float LeftCentre = -PlateHalfLen + LeftLength / 2.0f;
		FVector LeftPos = PlatePos + PlateForward * LeftCentre;
		LeftPos.Z = PlatePos.Z;

		ABottomPlate* LeftPlate = GetWorld()->SpawnActor<ABottomPlate>(
			PlateClass, LeftPos, PlateRot, SpawnParams);
		if (LeftPlate)
		{
			LeftPlate->SetBoardLengthCm(LeftLength);
			LeftPlate->SetPreviewMode(false);
			AConstructionPhaseManager::Instance->RegisterPlacedPiece(LeftPlate);
			UE_LOG(LogTemp, Warning, TEXT("DoorFrame: Spawned LEFT remnant plate (%.1f cm) at (%.1f,%.1f,%.1f)"),
				LeftLength, LeftPos.X, LeftPos.Y, LeftPos.Z);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("DoorFrame: FAILED to spawn LEFT remnant plate! Class=%s Pos=(%.1f,%.1f,%.1f)"),
				PlateClass ? *PlateClass->GetName() : TEXT("null"), LeftPos.X, LeftPos.Y, LeftPos.Z);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DoorFrame: LEFT remnant too short (%.1f < %.1f), skipping"), LeftLength, MinRemnant);
	}

	// --- Spawn right remnant ---
	if (RightLength >= MinRemnant)
	{
		float RightCentre = PlateHalfLen - RightLength / 2.0f;
		FVector RightPos = PlatePos + PlateForward * RightCentre;
		RightPos.Z = PlatePos.Z;

		ABottomPlate* RightPlate = GetWorld()->SpawnActor<ABottomPlate>(
			PlateClass, RightPos, PlateRot, SpawnParams);
		if (RightPlate)
		{
			RightPlate->SetBoardLengthCm(RightLength);
			RightPlate->SetPreviewMode(false);
			AConstructionPhaseManager::Instance->RegisterPlacedPiece(RightPlate);
			UE_LOG(LogTemp, Warning, TEXT("DoorFrame: Spawned RIGHT remnant plate (%.1f cm) at (%.1f,%.1f,%.1f)"),
				RightLength, RightPos.X, RightPos.Y, RightPos.Z);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("DoorFrame: FAILED to spawn RIGHT remnant plate! Class=%s Pos=(%.1f,%.1f,%.1f)"),
				PlateClass ? *PlateClass->GetName() : TEXT("null"), RightPos.X, RightPos.Y, RightPos.Z);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DoorFrame: RIGHT remnant too short (%.1f < %.1f), skipping"), RightLength, MinRemnant);
	}

	UE_LOG(LogTemp, Warning, TEXT("DoorFrame: Split complete — deleted %d studs, split plate into %s remnants"),
		DeletedStuds,
		(LeftLength >= MinRemnant && RightLength >= MinRemnant) ? TEXT("2") :
		(LeftLength >= MinRemnant || RightLength >= MinRemnant) ? TEXT("1") : TEXT("0"));
}
