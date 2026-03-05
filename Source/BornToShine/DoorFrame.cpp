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
	FrameOverallWidth = 106.68f; // 42" = opening(36") + 2 king studs(3") + 2 trimmers(3")
	RoughOpeningHeight = 205.74f; // 81" (standard 6'8" door trimmer height)

	// SceneRoot decouples mesh scale from actor transform
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	MeshComponent->SetupAttachment(SceneRoot);

	// CRITICAL: Kill ALL mesh collision immediately — BEFORE UE5 creates any
	// physics state for this component.  The parent ABuildablePiece constructor
	// set it to QueryOnly + Block(All), which creates a physics body from the
	// shared BodySetup.  The door frame mesh's convex hull covers the door
	// opening, so ANY collision on the mesh will block the player.
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));

	// --- Default geometry for collision box placement ---
	// These match the constructor dimension defaults above.
	const float DefSideWidth = 7.62f;  // king stud + trimmer = 3" = 7.62cm
	const float DefDepth     = 8.89f;  // 2x4 depth = 3.5" = 8.89cm
	const float DefHalfH     = FrameHeight / 2.0f;           // 117.635cm
	const float DefLeftX     = -(FrameOverallWidth / 2.0f) + (DefSideWidth / 2.0f); // -49.53
	const float DefRightX    = (FrameOverallWidth / 2.0f) - (DefSideWidth / 2.0f);  //  49.53
	const float DefOpenTopZ  = -DefHalfH + RoughOpeningHeight;                       //  88.105
	const float DefBandH     = DefHalfH - DefOpenTopZ;                               //  29.53
	const float DefBandCtrZ  = DefOpenTopZ + DefBandH / 2.0f;                        // 102.87

	// THREE separate collision boxes — one per framing member.
	// These are the ONLY collision primitives on the door frame.
	// The door opening between them must have ZERO collision.
	// EditAnywhere so they can be fine-tuned in the Blueprint viewport.
	LeftPostCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftPostCollision"));
	LeftPostCollision->SetupAttachment(SceneRoot);
	LeftPostCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftPostCollision->SetHiddenInGame(true);
	LeftPostCollision->ShapeColor = FColor::Cyan;
	LeftPostCollision->SetLineThickness(2.0f);
	LeftPostCollision->SetBoxExtent(FVector(DefSideWidth / 2.0f, DefDepth / 2.0f, DefHalfH));
	LeftPostCollision->SetRelativeLocation(FVector(DefLeftX, 0.0f, 0.0f));

	RightPostCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("RightPostCollision"));
	RightPostCollision->SetupAttachment(SceneRoot);
	RightPostCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightPostCollision->SetHiddenInGame(true);
	RightPostCollision->ShapeColor = FColor::Yellow;
	RightPostCollision->SetLineThickness(2.0f);
	RightPostCollision->SetBoxExtent(FVector(DefSideWidth / 2.0f, DefDepth / 2.0f, DefHalfH));
	RightPostCollision->SetRelativeLocation(FVector(DefRightX, 0.0f, 0.0f));

	HeaderCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("HeaderCollision"));
	HeaderCollision->SetupAttachment(SceneRoot);
	HeaderCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeaderCollision->SetHiddenInGame(true);
	HeaderCollision->ShapeColor = FColor::Magenta;
	HeaderCollision->SetLineThickness(2.0f);
	HeaderCollision->SetBoxExtent(FVector(FrameOverallWidth / 2.0f, DefDepth / 2.0f, DefBandH / 2.0f));
	HeaderCollision->SetRelativeLocation(FVector(0.0f, 0.0f, DefBandCtrZ));

	bAutoSizeCollisionBoxes = false;
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

	// AGGRESSIVELY kill mesh collision.  The base class BeginPlay may have
	// called UpdateVisualFeedback() which can re-enable mesh collision.
	// The mesh's convex hull covers the door opening — ANY collision from
	// the mesh creates a single invisible wall blocking the opening.
	KillMeshCollision();

	// Align sockets to actual mesh extents
	AdjustSocketsToMeshBounds();

	// Auto-size collision boxes from actual mesh bounds.
	// Uncheck bAutoSizeCollisionBoxes in the Blueprint to keep manual tweaks.
	if (bAutoSizeCollisionBoxes)
	{
		SetupCollisionBoxes();
	}

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

	// Enable the 3 separate collision boxes (posts + header).
	// This also re-kills mesh collision as a safeguard.
	EnableDoorCollision(true);

	// --- Diagnostic: scan ALL placed door frames for collision corruption ---
	if (AConstructionPhaseManager::Instance)
	{
		TArray<ABuildablePiece*> AllDoors =
			AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::DoorFrame);
		for (ABuildablePiece* P : AllDoors)
		{
			ADoorFrame* DF = Cast<ADoorFrame>(P);
			if (!DF || DF == this) continue;
			UStaticMeshComponent* M = DF->GetMeshComponent();
			if (M)
			{
				ECollisionEnabled::Type CE = M->GetCollisionEnabled();
				if (CE != ECollisionEnabled::NoCollision)
				{
					UE_LOG(LogTemp, Error,
						TEXT("COLLISION CORRUPTION: DoorFrame [%s] mesh has CollisionEnabled=%d (expected 0=NoCollision). Force-killing."),
						*DF->GetName(), (int32)CE);
					DF->KillMeshCollision();
				}
			}
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// SetPreviewMode override — fallback for save/load path
// ---------------------------------------------------------------------------
void ADoorFrame::SetPreviewMode(bool bIsPreview)
{
	Super::SetPreviewMode(bIsPreview);

	// Super::SetPreviewMode sets QueryOnly (preview) or QueryAndPhysics (placed)
	// on the mesh, which creates physics from the shared BodySetup and
	// re-enables the convex hull that covers the door opening.  Kill it.
	KillMeshCollision();

	if (!bIsPreview)
	{
		AutoDeleteOverlappingPieces();
		EnableDoorCollision(true);
	}
	else
	{
		EnableDoorCollision(false);
	}
}

// ---------------------------------------------------------------------------
// SetupCollisionBoxes — sizes/positions the three collision boxes to match
// the frame structure (left post, right post, header+cripples band).
// Called from AdjustSocketsToMeshBounds after real dimensions are known.
// ---------------------------------------------------------------------------
void ADoorFrame::SetupCollisionBoxes()
{
	if (!LeftPostCollision || !RightPostCollision || !HeaderCollision) return;

	// Mesh depth (through-wall direction) — 2x4 depth = 3.5" = 8.89cm
	float MeshDepth = 8.89f;
	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		float BoundsDepth = Bounds.BoxExtent.Y * 2.0f;
		if (BoundsDepth > 1.0f) MeshDepth = BoundsDepth;
	}

	// Frame bottom/top in local space (from sockets)
	float BottomZ = -FrameHeight / 2.0f;
	float TopZ = FrameHeight / 2.0f;
	for (const FConstructionSocket& S : Sockets)
	{
		if (S.SocketName == FName("FrameBottom")) BottomZ = S.LocalPosition.Z;
		else if (S.SocketName == FName("FrameTop")) TopZ = S.LocalPosition.Z;
	}

	// Each side of the frame has a king stud (1.5") + trimmer (1.5") = 3" = 7.62cm.
	// Compute SideWidth from (OverallWidth - RoughOpening) / 2, but enforce a
	// minimum of 7.62cm (king + trimmer) so the posts are never paper-thin.
	const float KingPlusTrimmer = 7.62f; // 3" = king stud + trimmer stud face widths
	float SideWidth = (FrameOverallWidth - RoughOpeningWidth) / 2.0f;
	if (SideWidth < KingPlusTrimmer)
	{
		SideWidth = KingPlusTrimmer;
		// Recalculate FrameOverallWidth to match
		FrameOverallWidth = RoughOpeningWidth + SideWidth * 2.0f;
	}

	float PostHeight = TopZ - BottomZ;
	float PostCenterZ = (BottomZ + TopZ) / 2.0f;
	float PostHalfHeight = PostHeight / 2.0f;

	// === LEFT POST (king stud + trimmer on the left side) ===
	float LeftX = -(FrameOverallWidth / 2.0f) + (SideWidth / 2.0f);
	LeftPostCollision->SetBoxExtent(FVector(SideWidth / 2.0f, MeshDepth / 2.0f, PostHalfHeight));
	LeftPostCollision->SetRelativeLocation(FVector(LeftX, 0.0f, PostCenterZ));

	// === RIGHT POST (king stud + trimmer on the right side) ===
	float RightX = (FrameOverallWidth / 2.0f) - (SideWidth / 2.0f);
	RightPostCollision->SetBoxExtent(FVector(SideWidth / 2.0f, MeshDepth / 2.0f, PostHalfHeight));
	RightPostCollision->SetRelativeLocation(FVector(RightX, 0.0f, PostCenterZ));

	// === HEADER + CRIPPLES (band above the opening, spans full width) ===
	float OpeningTopZ = BottomZ + RoughOpeningHeight;
	if (OpeningTopZ > TopZ) OpeningTopZ = TopZ;

	float TopBandHeight = TopZ - OpeningTopZ;
	if (TopBandHeight > 1.0f)
	{
		float TopBandCenterZ = OpeningTopZ + TopBandHeight / 2.0f;
		HeaderCollision->SetBoxExtent(FVector(FrameOverallWidth / 2.0f, MeshDepth / 2.0f, TopBandHeight / 2.0f));
		HeaderCollision->SetRelativeLocation(FVector(0.0f, 0.0f, TopBandCenterZ));
	}
	else
	{
		// No header band — hide the component
		HeaderCollision->SetBoxExtent(FVector::ZeroVector);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("DoorFrame COLLISION BOXES:"
		     "\n  FrameOverall=%.1fcm  RoughOpening=%.1fcm  SideWidth=%.1fcm  MeshDepth=%.1fcm"
		     "\n  LEFT  post: X=%.1f  extent=(%.1f, %.1f, %.1f)"
		     "\n  RIGHT post: X=%.1f  extent=(%.1f, %.1f, %.1f)"
		     "\n  HEADER:     Z=%.1f  extent=(%.1f, %.1f, %.1f)  band=%.1fcm"
		     "\n  OPENING GAP: X=[%.1f to %.1f] = %.1fcm wide, Z=[%.1f to %.1f]"),
		FrameOverallWidth, RoughOpeningWidth, SideWidth, MeshDepth,
		LeftX, SideWidth / 2.0f, MeshDepth / 2.0f, PostHalfHeight,
		RightX, SideWidth / 2.0f, MeshDepth / 2.0f, PostHalfHeight,
		OpeningTopZ + TopBandHeight / 2.0f,
		FrameOverallWidth / 2.0f, MeshDepth / 2.0f, TopBandHeight / 2.0f, TopBandHeight,
		LeftX + SideWidth / 2.0f, RightX - SideWidth / 2.0f,
		RoughOpeningWidth,
		BottomZ, OpeningTopZ);
}

// ---------------------------------------------------------------------------
// KillMeshCollision — completely disable mesh collision so the convex hull
// (which covers the door opening) never blocks the player.
// ---------------------------------------------------------------------------
void ADoorFrame::KillMeshCollision()
{
	if (!MeshComponent) return;

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));

	// Destroy any physics body that was created from the mesh's BodySetup.
	// Without this, UE5 may keep a physics body alive from the shared
	// BodySetup even though the component says NoCollision.
	if (MeshComponent->IsPhysicsStateCreated())
	{
		MeshComponent->DestroyPhysicsState();
	}
}

// ---------------------------------------------------------------------------
// EnableDoorCollision — toggles the three separate collision boxes on/off.
// Also re-kills mesh collision as a safeguard every time this is called.
// ---------------------------------------------------------------------------
void ADoorFrame::EnableDoorCollision(bool bEnable)
{
	// ALWAYS re-kill mesh collision — defence against any code path
	// that might have re-enabled it (base class SetPreviewMode, etc.)
	KillMeshCollision();

	auto SetBox = [bEnable](UBoxComponent* Box)
	{
		if (!Box) return;
		if (bEnable)
		{
			Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Ignore);
			Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		}
		else
		{
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	};

	SetBox(LeftPostCollision);
	SetBox(RightPostCollision);
	SetBox(HeaderCollision);

	if (bEnable)
	{
		UE_LOG(LogTemp, Log, TEXT("DoorFrame: Collision ENABLED — 3 separate boxes (Left=%s Right=%s Header=%s), mesh=NoCollision"),
			LeftPostCollision ? TEXT("ON") : TEXT("null"),
			RightPostCollision ? TEXT("ON") : TEXT("null"),
			HeaderCollision ? TEXT("ON") : TEXT("null"));
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

	// Fallback: search nearby if snap reference isn't a plate.
	// Must check BOTH through-wall distance AND that the door center
	// falls within the plate's length to avoid picking a plate from
	// a completely different wall.
	if (!OrigPlate)
	{
		TArray<ABuildablePiece*> Nearby =
			AConstructionPhaseManager::Instance->GetNearbyPieces(DoorLoc, 500.0f);
		float BestPlateDist = FLT_MAX;
		for (ABuildablePiece* P : Nearby)
		{
			ABottomPlate* Plate = Cast<ABottomPlate>(P);
			if (!Plate) continue;
			FVector Delta = Plate->GetActorLocation() - DoorLoc;
			Delta.Z = 0.0f;

			// Check through-wall distance (perpendicular to door frame)
			float ThroughDist = FMath::Abs(FVector::DotProduct(Delta, DoorThroughWall));
			if (ThroughDist >= DepthTolerance) continue;

			// Check along-plate distance: door center must fall within the plate's length
			FVector PlateDir = Plate->GetActorRotation().RotateVector(FVector::ForwardVector);
			float AlongDist = FMath::Abs(FVector::DotProduct(Delta, PlateDir));
			float PlateHalf = Plate->BoardLength / 2.0f;
			if (AlongDist > PlateHalf) continue;

			// Pick closest plate
			float TotalDist = Delta.Size();
			if (TotalDist < BestPlateDist)
			{
				BestPlateDist = TotalDist;
				OrigPlate = Plate;
			}
		}
		if (OrigPlate)
		{
			UE_LOG(LogTemp, Warning, TEXT("DoorFrame: Fallback found plate %s (dist=%.1f)"),
				*OrigPlate->GetName(), BestPlateDist);
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

	// Project door centre onto plate axis (signed offset from plate centre).
	// DeltaDP is relative to the plate, so DoorCentreOnPlate is bounded by ±PlateHalfLen.
	FVector DeltaDP = DoorLoc - PlatePos;
	DeltaDP.Z = 0.0f;
	float DoorCentreOnPlate = FVector::DotProduct(DeltaDP, PlateForward);

	// Sanity check: clamp offset to plate bounds
	if (FMath::Abs(DoorCentreOnPlate) > PlateHalfLen)
	{
		UE_LOG(LogTemp, Error,
			TEXT("DoorFrame: DoorOffset=%.1f exceeds PlateHalfLen=%.1f! "
			     "DoorLoc=(%.1f,%.1f) PlatePos=(%.1f,%.1f) PlateYaw=%.1f — clamping"),
			DoorCentreOnPlate, PlateHalfLen,
			DoorLoc.X, DoorLoc.Y, PlatePos.X, PlatePos.Y, PlateRot.Yaw);
		DoorCentreOnPlate = FMath::Clamp(DoorCentreOnPlate, -PlateHalfLen, PlateHalfLen);
	}

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

