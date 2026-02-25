// Born To Shine - Window Frame with Stud Extension System
//
// When the window frame is placed on a wall, the king studs in the Rhino mesh
// are shorter than the full wall stud height. This leaves a gap between the
// king stud tops and the top plate bottom. The extension system:
//   1. Measures the gap (WallStudHeight - KingStudTopZ)
//   2. Spawns two 2x4 extension components (left + right king studs)
//   3. Scales them vertically to exactly fill the gap
//   4. Positions at the king stud X offsets on both frame edges

#include "WindowFrame.h"
#include "ConstructionPhaseManager.h"
#include "WallStud.h"
#include "Components/StaticMeshComponent.h"

// 2x4 actual dimensions in cm
static constexpr float Stud2x4_Width = 8.89f;   // 3-1/2"
static constexpr float Stud2x4_Depth = 3.81f;   // 1-1/2"

AWindowFrame::AWindowFrame()
{
	PieceType = EPieceType::WindowFrame;

	// Window frame dimensions (from Rhino mesh geometry)
	FrameTotalWidth = 166.441f;     // 65.52"
	RoughOpeningWidth = 69.850f;    // 27.5"
	RoughOpeningHeight = 88.304f;   // 34.77"
	RoughSillHeight = 30.0f;       // 11.81"
	WallStudHeight = 235.27f;      // 92-5/8" (standard 8ft wall)
	FrameHeight = 0.0f;            // Auto-calculated from mesh bounds
	bHasStudExtensions = false;

	// SceneRoot decouples mesh scale from actor transform (same pattern as DoorFrame)
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	MeshComponent->SetupAttachment(SceneRoot);

	// Stud extension components — created up front but hidden until needed.
	// As default subobjects attached to SceneRoot, they move/save/delete
	// together with the window frame actor.
	LeftStudExtension = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftStudExtension"));
	LeftStudExtension->SetupAttachment(SceneRoot);
	LeftStudExtension->SetVisibility(false);
	LeftStudExtension->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightStudExtension = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightStudExtension"));
	RightStudExtension->SetupAttachment(SceneRoot);
	RightStudExtension->SetVisibility(false);
	RightStudExtension->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// No scroll wheel resizing
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------
void AWindowFrame::BeginPlay()
{
	Super::BeginPlay();

	// Mesh stays at (1,1,1) — pre-modeled in Rhino at real-world scale
	if (MeshComponent)
	{
		MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	}

	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("WindowFrame: BeginPlay — FrameHeight=%.2fcm, WallStudHeight=%.2fcm, Gap=%.2fcm"),
		FrameHeight, WallStudHeight, CalculateExtensionHeight());
}

// ---------------------------------------------------------------------------
// Sockets
// ---------------------------------------------------------------------------
void AWindowFrame::InitializeSockets()
{
	Sockets.Empty();
	CreateBottomSocket();
	CreateTopSocket();
	UE_LOG(LogTemp, Log, TEXT("WindowFrame: InitializeSockets — %d sockets"), Sockets.Num());
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
// AdjustSocketsToMeshBounds — read actual mesh extents (same pattern as DoorFrame)
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

		// Update overall width from mesh X extent
		float MeshHalfX = Bounds.BoxExtent.X;
		if (MeshHalfX > 1.0f)
		{
			FrameTotalWidth = MeshHalfX * 2.0f;
		}

		for (FConstructionSocket& Socket : Sockets)
		{
			if (Socket.SocketName == FName("FrameBottom"))
				Socket.LocalPosition.Z = MeshBottomZ;
			else if (Socket.SocketName == FName("FrameTop"))
				Socket.LocalPosition.Z = MeshTopZ;
		}

		UE_LOG(LogTemp, Log,
			TEXT("WindowFrame: Mesh bounds Z=[%.2f, %.2f] height=%.2fcm, FrameTotalWidth=%.2fcm"),
			MeshBottomZ, MeshTopZ, ActualHeight, FrameTotalWidth);
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
// TryPlace — runs auto-delete + stud extensions after successful placement
// ---------------------------------------------------------------------------
bool AWindowFrame::TryPlace()
{
	if (!Super::TryPlace()) return false;

	AutoDeleteOverlappingStuds();
	SpawnStudExtensions();

	return true;
}

// ---------------------------------------------------------------------------
// SetPreviewMode — fallback for save/load path
// ---------------------------------------------------------------------------
void AWindowFrame::SetPreviewMode(bool bIsPreview)
{
	Super::SetPreviewMode(bIsPreview);

	if (!bIsPreview)
	{
		AutoDeleteOverlappingStuds();
		SpawnStudExtensions();
	}
	else
	{
		RemoveStudExtensions();
	}
}

// ============================================================================
// Stud Extension System
// ============================================================================

float AWindowFrame::CalculateKingStudHeightFromMesh() const
{
	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		// The mesh Z extent represents the full height of the window frame
		// assembly including king studs. Mesh origin at bottom = top Z is height.
		float MeshTopZ = Bounds.Origin.Z + Bounds.BoxExtent.Z;
		return MeshTopZ;
	}

	// Fallback: estimate from window dimensions
	// King stud height = sill + rough opening + header (double 2x4 ≈ 2 * 3.81cm)
	float HeaderHeight = 2.0f * Stud2x4_Depth;
	return RoughSillHeight + RoughOpeningHeight + HeaderHeight;
}

float AWindowFrame::GetKingStudTopZ() const
{
	// Use FrameHeight if it was calculated from mesh bounds
	if (FrameHeight > 0.0f)
	{
		return FrameHeight;
	}
	return CalculateKingStudHeightFromMesh();
}

float AWindowFrame::CalculateExtensionHeight() const
{
	float KingTop = GetKingStudTopZ();
	float Gap = WallStudHeight - KingTop;
	return FMath::Max(0.0f, Gap);
}

float AWindowFrame::GetLeftKingStudOffsetX() const
{
	// Left king stud at left edge of frame assembly
	// Frame centered at X=0, so left king stud center = -FrameTotalWidth/2 + StudDepth/2
	return -(FrameTotalWidth / 2.0f) + (Stud2x4_Depth / 2.0f);
}

float AWindowFrame::GetRightKingStudOffsetX() const
{
	return (FrameTotalWidth / 2.0f) - (Stud2x4_Depth / 2.0f);
}

void AWindowFrame::ConfigureExtensionMesh(UStaticMeshComponent* ExtComp, float OffsetX, float ExtensionHeight)
{
	if (!ExtComp || !StudExtensionMesh || ExtensionHeight <= 0.0f)
	{
		return;
	}

	// Assign the same 2x4 stud mesh used by wall studs
	ExtComp->SetStaticMesh(StudExtensionMesh);

	// Position the extension:
	// X = king stud horizontal position (left or right edge of frame)
	// Y = 0 (aligned with wall plane)
	// Z = top of king stud + half of extension height (mesh pivot at center)
	float KingTop = GetKingStudTopZ();
	FVector LocalPosition(OffsetX, 0.0f, KingTop + (ExtensionHeight / 2.0f));
	ExtComp->SetRelativeLocation(LocalPosition);

	// Scale vertically to fill the gap.
	// The stud mesh is modeled at its default height. We need to find the
	// unscaled mesh height and compute the Z scale factor.
	float UnscaledHeight = 1.0f;
	if (StudExtensionMesh)
	{
		FBoxSphereBounds StudBounds = StudExtensionMesh->GetBounds();
		UnscaledHeight = StudBounds.BoxExtent.Z * 2.0f;
	}

	float ZScale = (UnscaledHeight > 1.0f) ? (ExtensionHeight / UnscaledHeight) : 1.0f;
	ExtComp->SetRelativeScale3D(FVector(1.0f, 1.0f, ZScale));

	// No additional rotation — inherits from the frame actor (matches wall yaw)
	ExtComp->SetRelativeRotation(FRotator::ZeroRotator);

	// Make visible and enable collision
	ExtComp->SetVisibility(true);
	ExtComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ExtComp->SetCollisionProfileName(TEXT("BlockAll"));

	// Apply the nailed material if available (match the placed wood look)
	if (NailedMaterial)
	{
		ExtComp->SetMaterial(0, NailedMaterial);
	}
	else if (OriginalMeshMaterial)
	{
		ExtComp->SetMaterial(0, OriginalMeshMaterial);
	}
}

void AWindowFrame::SpawnStudExtensions()
{
	float ExtHeight = CalculateExtensionHeight();

	// No extension needed if gap is negligible (less than 1mm)
	if (ExtHeight < 0.1f)
	{
		UE_LOG(LogTemp, Log,
			TEXT("WindowFrame [%s]: No stud extensions needed (gap: %.3f cm)"),
			*GetName(), ExtHeight);
		return;
	}

	if (!StudExtensionMesh)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("WindowFrame [%s]: StudExtensionMesh not set — assign 2x4 stud mesh in Blueprint"),
			*GetName());
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("WindowFrame [%s]: Spawning stud extensions — KingStudTop=%.2fcm, WallStudHeight=%.2fcm, ExtHeight=%.2fcm"),
		*GetName(), GetKingStudTopZ(), WallStudHeight, ExtHeight);

	// Configure left king stud extension
	ConfigureExtensionMesh(LeftStudExtension, GetLeftKingStudOffsetX(), ExtHeight);

	// Configure right king stud extension
	ConfigureExtensionMesh(RightStudExtension, GetRightKingStudOffsetX(), ExtHeight);

	bHasStudExtensions = true;

	UE_LOG(LogTemp, Log,
		TEXT("WindowFrame [%s]: Extensions spawned — left X=%.2f, right X=%.2f, height=%.2fcm"),
		*GetName(), GetLeftKingStudOffsetX(), GetRightKingStudOffsetX(), ExtHeight);
}

void AWindowFrame::RemoveStudExtensions()
{
	if (LeftStudExtension)
	{
		LeftStudExtension->SetVisibility(false);
		LeftStudExtension->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LeftStudExtension->SetStaticMesh(nullptr);
	}

	if (RightStudExtension)
	{
		RightStudExtension->SetVisibility(false);
		RightStudExtension->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RightStudExtension->SetStaticMesh(nullptr);
	}

	bHasStudExtensions = false;
}

// ============================================================================
// Auto-Delete Overlapping Wall Studs
// ============================================================================

void AWindowFrame::AutoDeleteOverlappingStuds()
{
	if (bHasAutoDeleted) return;
	bHasAutoDeleted = true;

	if (!AConstructionPhaseManager::Instance) return;

	FVector FrameLoc = GetActorLocation();
	FRotator FrameRot = GetActorRotation();

	// Frame's local axes
	FVector AlongWall = FrameRot.RotateVector(FVector::ForwardVector);
	float HalfFrameWidth = FrameTotalWidth / 2.0f;
	const float DepthTolerance = 15.0f; // cm

	int32 DeletedCount = 0;

	TArray<ABuildablePiece*> Studs =
		AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::WallStud);

	for (ABuildablePiece* Piece : Studs)
	{
		if (!Piece || Piece == this) continue;

		FVector Delta = Piece->GetActorLocation() - FrameLoc;
		Delta.Z = 0.0f;

		float AlongDist = FMath::Abs(FVector::DotProduct(Delta, AlongWall));
		FVector ThroughWall = FrameRot.RotateVector(FVector::RightVector);
		float ThroughDist = FMath::Abs(FVector::DotProduct(Delta, ThroughWall));

		if (AlongDist < HalfFrameWidth && ThroughDist < DepthTolerance)
		{
			UE_LOG(LogTemp, Log, TEXT("WindowFrame: Deleting WallStud %s (along=%.1f, through=%.1f)"),
				*Piece->GetName(), AlongDist, ThroughDist);
			AConstructionPhaseManager::Instance->UnregisterPiece(Piece);
			Piece->Destroy();
			DeletedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("WindowFrame [%s]: Deleted %d overlapping wall studs"),
		*GetName(), DeletedCount);
}
