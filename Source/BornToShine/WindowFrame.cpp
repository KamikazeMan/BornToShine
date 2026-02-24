// Born To Shine - Window Frame (single pre-modeled mesh)

#include "WindowFrame.h"
#include "ConstructionPhaseManager.h"
#include "WallStud.h"
#include "Components/StaticMeshComponent.h"

AWindowFrame::AWindowFrame()
{
	PieceType = EPieceType::WindowFrame;

	// Default dimensions — overridden by actual mesh bounds in BeginPlay
	FrameHeight = 243.84f;          // 96" = 8ft (king studs match wall stud height)
	RoughOpeningWidth = 69.85f;     // 27.5" rough opening (gap between trimmers)
	FrameOverallWidth = 166.441f;   // 65.52" = 5 studs at 16" OC
	RoughOpeningHeight = 88.304f;   // 34.77" rough opening height
	RoughSillHeight = 30.0f;        // 11.81" sill height from bottom plate

	// SceneRoot decouples mesh scale from actor transform
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	MeshComponent->SetupAttachment(SceneRoot);

	// CRITICAL: Kill ALL mesh collision immediately — BEFORE UE5 creates any
	// physics state for this component.  The mesh's convex hull covers the
	// window opening, so ANY collision on the mesh will block the player.
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));

	// --- Default geometry for collision box placement ---
	const float DefSideWidth = 7.62f;  // king stud + trimmer = 3" = 7.62cm
	const float DefDepth     = 8.89f;  // 2x4 depth = 3.5" = 8.89cm
	const float DefHalfH     = FrameHeight / 2.0f;

	// LEFT/RIGHT post collision (full-height king studs + trimmers)
	const float DefLeftX  = -(FrameOverallWidth / 2.0f) + (DefSideWidth / 2.0f);
	const float DefRightX = (FrameOverallWidth / 2.0f) - (DefSideWidth / 2.0f);

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

	// HEADER collision (band above opening)
	float DefOpenTopZ = -DefHalfH + RoughSillHeight + RoughOpeningHeight;
	float DefBandH    = DefHalfH - DefOpenTopZ;
	float DefBandCtrZ = DefOpenTopZ + DefBandH / 2.0f;

	HeaderCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("HeaderCollision"));
	HeaderCollision->SetupAttachment(SceneRoot);
	HeaderCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeaderCollision->SetHiddenInGame(true);
	HeaderCollision->ShapeColor = FColor::Magenta;
	HeaderCollision->SetLineThickness(2.0f);
	HeaderCollision->SetBoxExtent(FVector(FrameOverallWidth / 2.0f, DefDepth / 2.0f, FMath::Max(DefBandH / 2.0f, 1.0f)));
	HeaderCollision->SetRelativeLocation(FVector(0.0f, 0.0f, DefBandCtrZ));

	// SILL collision (band below opening, above bottom plate)
	float DefSillBandH = RoughSillHeight;
	float DefSillCtrZ  = -DefHalfH + DefSillBandH / 2.0f;

	SillCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SillCollision"));
	SillCollision->SetupAttachment(SceneRoot);
	SillCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SillCollision->SetHiddenInGame(true);
	SillCollision->ShapeColor = FColor::Green;
	SillCollision->SetLineThickness(2.0f);
	SillCollision->SetBoxExtent(FVector(FrameOverallWidth / 2.0f, DefDepth / 2.0f, FMath::Max(DefSillBandH / 2.0f, 1.0f)));
	SillCollision->SetRelativeLocation(FVector(0.0f, 0.0f, DefSillCtrZ));

	bAutoSizeCollisionBoxes = false;
	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------
void AWindowFrame::BeginPlay()
{
	Super::BeginPlay();

	// Mesh stays at (1,1,1) — pre-modeled in Rhino at real-world scale
	MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

	// AGGRESSIVELY kill mesh collision
	KillMeshCollision();

	// Align sockets to actual mesh extents
	AdjustSocketsToMeshBounds();

	// Auto-size collision boxes from actual mesh bounds
	if (bAutoSizeCollisionBoxes)
	{
		SetupCollisionBoxes();
	}

	UE_LOG(LogTemp, Log, TEXT("WindowFrame: BeginPlay - Height=%.1fcm, RoughOpening=%.1fcm, FrameOverall=%.1fcm, SillH=%.1fcm, Sockets: %d"),
		FrameHeight, RoughOpeningWidth, FrameOverallWidth, RoughSillHeight, Sockets.Num());
}

// ---------------------------------------------------------------------------
// Sockets
// ---------------------------------------------------------------------------
void AWindowFrame::InitializeSockets()
{
	Sockets.Empty();
	CreateBottomSocket();
	CreateTopSocket();
	UE_LOG(LogTemp, Log, TEXT("WindowFrame: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void AWindowFrame::CreateBottomSocket()
{
	FConstructionSocket BottomSocket;
	BottomSocket.SocketName = FName(TEXT("FrameBottom"));
	BottomSocket.SocketType = EConstructionSocketType::WindowFrame_Bottom;
	BottomSocket.LocalPosition = FVector(0.0f, 0.0f, -FrameHeight / 2.0f);
	BottomSocket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
	BottomSocket.Orientation = ESocketOrientation::Vertical;
	BottomSocket.bIsOccupied = false;
	Sockets.Add(BottomSocket);
}

void AWindowFrame::CreateTopSocket()
{
	FConstructionSocket TopSocket;
	TopSocket.SocketName = FName(TEXT("FrameTop"));
	TopSocket.SocketType = EConstructionSocketType::WindowFrame_Top;
	TopSocket.LocalPosition = FVector(0.0f, 0.0f, FrameHeight / 2.0f);
	TopSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f);
	TopSocket.Orientation = ESocketOrientation::Vertical;
	TopSocket.bIsOccupied = false;
	Sockets.Add(TopSocket);
}

// ---------------------------------------------------------------------------
// Adjust sockets to match actual mesh bounds (same pattern as DoorFrame)
// ---------------------------------------------------------------------------
void AWindowFrame::AdjustSocketsToMeshBounds()
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

		// Full mesh extent drives stud overlap deletion
		float MeshHalfX = Bounds.BoxExtent.X;
		if (MeshHalfX > 1.0f)
		{
			FrameOverallWidth = MeshHalfX * 2.0f;
		}

		// Sockets at wall stud height, NOT mesh bounds.
		// The mesh BB (245.75cm) extends beyond the visible king stud
		// geometry, so using mesh bounds puts sockets too far from the
		// actual stud ends.  King studs must match wall stud height
		// exactly: 243.84cm = 96" = 8ft minus two plate thicknesses.
		const float WallStudHalfHeight = 243.84f / 2.0f; // 121.92cm
		for (FConstructionSocket& Socket : Sockets)
		{
			if (Socket.SocketName == FName("FrameBottom"))
				Socket.LocalPosition.Z = -WallStudHalfHeight;
			else if (Socket.SocketName == FName("FrameTop"))
				Socket.LocalPosition.Z = WallStudHalfHeight;
		}

		UE_LOG(LogTemp, Log,
			TEXT("WindowFrame: Mesh bounds Z=[%.2f, %.2f] height=%.2fcm, SocketZ=[%.2f, %.2f] (stud height), FrameOverall=%.2fcm"),
			MeshBottomZ, MeshTopZ, ActualHeight, -WallStudHalfHeight, WallStudHalfHeight, FrameOverallWidth);
	}
}

// ---------------------------------------------------------------------------
// UpdatePreviewPosition — only follow cursor to valid snap positions.
// Window frames must snap to a bottom plate; don't let the preview land
// on non-snappable surfaces (rim boards, joists, ground, etc.).
// ---------------------------------------------------------------------------
void AWindowFrame::UpdatePreviewPosition(const FVector& NewLocation, const FRotator& NewRotation)
{
	if (PieceState != EPieceState::Preview) return;

	// Save current position in case no snap is found
	FVector PrevLocation = GetActorLocation();
	FRotator PrevRotation = GetActorRotation();

	// Temporarily move to cursor position so snap detection searches
	// for nearby pieces around the aimed-at location
	SetActorLocation(NewLocation);
	SetActorRotation(NewRotation);

	TArray<FSnapCandidate> Candidates = DetectSnapCandidates();
	FSnapCandidate Best = SelectBestCandidate(Candidates);

	if (Best.IsValid())
	{
		ApplySnap(Best);
	}
	else
	{
		// No valid snap — restore previous position so the frame
		// doesn't sit on rim boards or other non-wall surfaces.
		bIsSnapped = false;
		CurrentSnapCandidate = FSnapCandidate();
		SetActorLocation(PrevLocation);
		SetActorRotation(PrevRotation);
	}
}

// ---------------------------------------------------------------------------
// ScalePiece — disabled (no scroll wheel resizing)
// ---------------------------------------------------------------------------
void AWindowFrame::ScalePiece(float ScaleDelta)
{
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
}

// ---------------------------------------------------------------------------
// TryPlace override — runs auto-delete after successful placement
// ---------------------------------------------------------------------------
bool AWindowFrame::TryPlace()
{
	if (!Super::TryPlace()) return false;

	// Now that the window frame is registered and positioned, remove overlaps
	AutoDeleteOverlappingStuds();

	// Enable collision boxes
	EnableWindowCollision(true);

	return true;
}

// ---------------------------------------------------------------------------
// SetPreviewMode override — fallback for save/load path
// ---------------------------------------------------------------------------
void AWindowFrame::SetPreviewMode(bool bIsPreview)
{
	Super::SetPreviewMode(bIsPreview);

	// Kill mesh collision that base class may have re-enabled
	KillMeshCollision();

	if (!bIsPreview)
	{
		AutoDeleteOverlappingStuds();
		EnableWindowCollision(true);
	}
	else
	{
		EnableWindowCollision(false);
	}
}

// ---------------------------------------------------------------------------
// SetupCollisionBoxes — sizes/positions collision boxes to match the frame
// structure (left post, right post, header band, sill band).
// ---------------------------------------------------------------------------
void AWindowFrame::SetupCollisionBoxes()
{
	if (!LeftPostCollision || !RightPostCollision || !HeaderCollision || !SillCollision) return;

	float MeshDepth = 8.89f;
	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		float BoundsDepth = Bounds.BoxExtent.Y * 2.0f;
		if (BoundsDepth > 1.0f) MeshDepth = BoundsDepth;
	}

	float BottomZ = -FrameHeight / 2.0f;
	float TopZ = FrameHeight / 2.0f;
	for (const FConstructionSocket& S : Sockets)
	{
		if (S.SocketName == FName("FrameBottom")) BottomZ = S.LocalPosition.Z;
		else if (S.SocketName == FName("FrameTop")) TopZ = S.LocalPosition.Z;
	}

	const float KingPlusTrimmer = 7.62f;
	float SideWidth = (FrameOverallWidth - RoughOpeningWidth) / 2.0f;
	if (SideWidth < KingPlusTrimmer)
	{
		SideWidth = KingPlusTrimmer;
		FrameOverallWidth = RoughOpeningWidth + SideWidth * 2.0f;
	}

	float PostHeight = TopZ - BottomZ;
	float PostCenterZ = (BottomZ + TopZ) / 2.0f;
	float PostHalfHeight = PostHeight / 2.0f;

	// === LEFT POST ===
	float LeftX = -(FrameOverallWidth / 2.0f) + (SideWidth / 2.0f);
	LeftPostCollision->SetBoxExtent(FVector(SideWidth / 2.0f, MeshDepth / 2.0f, PostHalfHeight));
	LeftPostCollision->SetRelativeLocation(FVector(LeftX, 0.0f, PostCenterZ));

	// === RIGHT POST ===
	float RightX = (FrameOverallWidth / 2.0f) - (SideWidth / 2.0f);
	RightPostCollision->SetBoxExtent(FVector(SideWidth / 2.0f, MeshDepth / 2.0f, PostHalfHeight));
	RightPostCollision->SetRelativeLocation(FVector(RightX, 0.0f, PostCenterZ));

	// === HEADER (band above the opening) ===
	float OpeningTopZ = BottomZ + RoughSillHeight + RoughOpeningHeight;
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
		HeaderCollision->SetBoxExtent(FVector::ZeroVector);
	}

	// === SILL (band below the opening) ===
	if (RoughSillHeight > 1.0f)
	{
		float SillCenterZ = BottomZ + RoughSillHeight / 2.0f;
		SillCollision->SetBoxExtent(FVector(FrameOverallWidth / 2.0f, MeshDepth / 2.0f, RoughSillHeight / 2.0f));
		SillCollision->SetRelativeLocation(FVector(0.0f, 0.0f, SillCenterZ));
	}
	else
	{
		SillCollision->SetBoxExtent(FVector::ZeroVector);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("WindowFrame COLLISION BOXES:"
		     "\n  FrameOverall=%.1fcm  RoughOpening=%.1fcm  SideWidth=%.1fcm  MeshDepth=%.1fcm"
		     "\n  LEFT  post: X=%.1f  extent=(%.1f, %.1f, %.1f)"
		     "\n  RIGHT post: X=%.1f  extent=(%.1f, %.1f, %.1f)"
		     "\n  HEADER:     band=%.1fcm"
		     "\n  SILL:       band=%.1fcm"
		     "\n  OPENING GAP: X=[%.1f to %.1f] = %.1fcm wide"),
		FrameOverallWidth, RoughOpeningWidth, SideWidth, MeshDepth,
		LeftX, SideWidth / 2.0f, MeshDepth / 2.0f, PostHalfHeight,
		RightX, SideWidth / 2.0f, MeshDepth / 2.0f, PostHalfHeight,
		TopBandHeight,
		RoughSillHeight,
		LeftX + SideWidth / 2.0f, RightX - SideWidth / 2.0f,
		RoughOpeningWidth);
}

// ---------------------------------------------------------------------------
// KillMeshCollision
// ---------------------------------------------------------------------------
void AWindowFrame::KillMeshCollision()
{
	if (!MeshComponent) return;

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));

	if (MeshComponent->IsPhysicsStateCreated())
	{
		MeshComponent->DestroyPhysicsState();
	}
}

// ---------------------------------------------------------------------------
// EnableWindowCollision
// ---------------------------------------------------------------------------
void AWindowFrame::EnableWindowCollision(bool bEnable)
{
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
	SetBox(SillCollision);

	if (bEnable)
	{
		UE_LOG(LogTemp, Log, TEXT("WindowFrame: Collision ENABLED — 4 separate boxes (Left=%s Right=%s Header=%s Sill=%s), mesh=NoCollision"),
			LeftPostCollision ? TEXT("ON") : TEXT("null"),
			RightPostCollision ? TEXT("ON") : TEXT("null"),
			HeaderCollision ? TEXT("ON") : TEXT("null"),
			SillCollision ? TEXT("ON") : TEXT("null"));
	}
}

// ---------------------------------------------------------------------------
// AutoDeleteOverlappingStuds
// Deletes wall studs that fall inside the window frame's width.
// Unlike door frame, NO bottom plate splitting (window sits on the wall,
// the bottom plate remains intact).
// ---------------------------------------------------------------------------
void AWindowFrame::AutoDeleteOverlappingStuds()
{
	if (bHasAutoDeleted) return;
	bHasAutoDeleted = true;

	if (!AConstructionPhaseManager::Instance) return;

	FVector FrameLoc = GetActorLocation();
	FRotator FrameRot = GetActorRotation();

	FVector AlongWallDir = FrameRot.RotateVector(FVector::ForwardVector);
	FVector ThroughWallDir = FrameRot.RotateVector(FVector::RightVector);

	float StudHalfWidth = FrameOverallWidth / 2.0f;
	const float DepthTolerance = 15.0f;

	int32 DeletedStuds = 0;

	UE_LOG(LogTemp, Warning, TEXT("WindowFrame: AutoDelete — Loc=(%.1f,%.1f,%.1f) Yaw=%.1f StudHalfW=%.1f"),
		FrameLoc.X, FrameLoc.Y, FrameLoc.Z, FrameRot.Yaw, StudHalfWidth);

	// Delete overlapping wall studs (uses full frame width)
	TArray<ABuildablePiece*> Studs =
		AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::WallStud);

	UE_LOG(LogTemp, Log, TEXT("WindowFrame: Checking %d wall studs for overlap"), Studs.Num());

	for (ABuildablePiece* Piece : Studs)
	{
		if (!Piece || Piece == this) continue;

		FVector Delta = Piece->GetActorLocation() - FrameLoc;
		Delta.Z = 0.0f;

		float AlongWall = FMath::Abs(FVector::DotProduct(Delta, AlongWallDir));
		float ThroughWall = FMath::Abs(FVector::DotProduct(Delta, ThroughWallDir));

		if (AlongWall < StudHalfWidth && ThroughWall < DepthTolerance)
		{
			UE_LOG(LogTemp, Log, TEXT("WindowFrame: Deleting WallStud %s (along=%.1f, through=%.1f)"),
				*Piece->GetName(), AlongWall, ThroughWall);
			AConstructionPhaseManager::Instance->UnregisterPiece(Piece);
			Piece->Destroy();
			DeletedStuds++;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("WindowFrame: AutoDelete complete — deleted %d studs"), DeletedStuds);
}
