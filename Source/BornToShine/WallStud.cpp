// Born To Shine - Wall Stud Implementation

#include "WallStud.h"
#include "DoorFrame.h"
#include "Components/StaticMeshComponent.h"

AWallStud::AWallStud()
{
	PieceType = EPieceType::WallStud;

	// 2x4 lumber actual cross-section dimensions
	StudWidth = 3.81f;    // 1.5 inches (along wall)
	StudDepth = 8.89f;    // 3.5 inches (in/out, perpendicular to wall)

	// Standard stud height for 8ft walls: 92-5/8"
	// (8ft wall = 96" total, minus 1.5" bottom plate, minus 1.5" top plate, minus 3/8" gap)
	StudHeight = 235.27f; // 92.625 inches in cm

	// Scene root for clean actor transform (same pattern as RimBoard/BottomPlate)
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	// Wall studs require manual nailing
	bAutoNailOnPlace = false;

	// Actor scale stays at (1,1,1) — SceneRoot is unscaled
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void AWallStud::BeginPlay()
{
	Super::BeginPlay();

	// Mesh stays at scale (1,1,1) — the user's 2x4 mesh is already correctly proportioned.
	// Only sockets need to know the height for positioning.
	if (MeshComponent)
	{
		MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	}

	// Re-align sockets to the actual mesh extents so the StudTop socket
	// matches the real mesh top regardless of Rhino export pivot position.
	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Warning, TEXT("=== HEIGHT DIAGNOSTIC === WallStud mesh height: %.2fcm (%.2f in)"),
		StudHeight, StudHeight / 2.54f);

	// Scale to match door frame king stud height if reference is available
	float RefHeight = ADoorFrame::GetKingStudHeight();
	if (RefHeight > 0.0f)
	{
		ScaleToReferenceHeight(RefHeight);
	}

	UE_LOG(LogTemp, Log, TEXT("WallStud: BeginPlay - Height=%.1fcm (%.2f in), Total sockets: %d"),
		StudHeight, GetStudHeightInches(), Sockets.Num());
}

void AWallStud::InitializeSockets()
{
	Sockets.Empty();

	CreateBottomSocket();
	CreateTopSocket();

	UE_LOG(LogTemp, Log, TEXT("WallStud: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void AWallStud::CreateBottomSocket()
{
	// Bottom socket at the base of the stud
	// This snaps to Wall_Bottom_Plate sockets on the bottom plate's top face
	FConstructionSocket BottomSocket;
	BottomSocket.SocketName = FName(TEXT("StudBottom"));
	BottomSocket.SocketType = EConstructionSocketType::Wall_Stud_Bottom;
	BottomSocket.LocalPosition = FVector(0.0f, 0.0f, -StudHeight / 2.0f);
	BottomSocket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f); // Facing downward
	BottomSocket.Orientation = ESocketOrientation::Vertical;
	BottomSocket.bIsOccupied = false;
	Sockets.Add(BottomSocket);
}

void AWallStud::CreateTopSocket()
{
	// Top socket at the top of the stud
	// For future top plate attachment
	FConstructionSocket TopSocket;
	TopSocket.SocketName = FName(TEXT("StudTop"));
	TopSocket.SocketType = EConstructionSocketType::Wall_Stud_Top;
	TopSocket.LocalPosition = FVector(0.0f, 0.0f, StudHeight / 2.0f);
	TopSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f); // Facing upward
	TopSocket.Orientation = ESocketOrientation::Vertical;
	TopSocket.bIsOccupied = false;
	Sockets.Add(TopSocket);
}

void AWallStud::SetStudHeightInches(float HeightInInches)
{
	float NewHeightCm = HeightInInches * 2.54f;

	if (!FMath::IsNearlyEqual(NewHeightCm, StudHeight, 0.01f))
	{
		StudHeight = NewHeightCm;
		RegenerateSockets();

		UE_LOG(LogTemp, Log, TEXT("WallStud: Height changed to %s"), *GetHeightDisplayString());
	}
}

float AWallStud::GetStudHeightInches() const
{
	return StudHeight / 2.54f;
}

FString AWallStud::GetHeightDisplayString() const
{
	float Inches = GetStudHeightInches();
	int32 WholeInches = FMath::FloorToInt(Inches);
	float Fraction = Inches - WholeInches;

	// Display as feet-inches with fraction
	int32 Feet = WholeInches / 12;
	int32 RemainInches = WholeInches % 12;

	if (FMath::IsNearlyEqual(Fraction, 0.625f, 0.01f))
	{
		return FString::Printf(TEXT("%d' %d-5/8\" (%.1f cm)"), Feet, RemainInches, StudHeight);
	}
	return FString::Printf(TEXT("%.1f\" (%.1f cm)"), Inches, StudHeight);
}

void AWallStud::ScalePiece(float ScaleDelta)
{
	// Wall studs: Scroll wheel scaling is DISABLED
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void AWallStud::RegenerateSockets()
{
	Sockets.Empty();

	CreateBottomSocket();
	CreateTopSocket();

	UE_LOG(LogTemp, Log, TEXT("WallStud: Regenerated %d sockets for height %.1fcm"), Sockets.Num(), StudHeight);
}

void AWallStud::UpdateMeshScale()
{
	// No-op: the user's 2x4 mesh is already correctly proportioned.
	// Mesh stays at (1,1,1). Socket positions handle all geometry.
}

void AWallStud::AdjustSocketsToMeshBounds()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	FVector MeshRelLoc = MeshComponent->GetRelativeLocation();

	// Mesh bottom/top in actor-local space (accounts for any BP mesh offset)
	float MeshBottomZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) + MeshRelLoc.Z;
	float MeshTopZ    = (Bounds.Origin.Z + Bounds.BoxExtent.Z) + MeshRelLoc.Z;
	float ActualHeight = MeshTopZ - MeshBottomZ;

	if (ActualHeight < 1.0f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("WallStud: Mesh height too small (%.2f cm) — skipping socket adjustment"),
			ActualHeight);
		return;
	}

	float OldBottomZ = -StudHeight / 2.0f;
	float OldTopZ    =  StudHeight / 2.0f;

	// Update StudHeight to reflect the real mesh
	StudHeight = ActualHeight;

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("StudBottom"))
		{
			Socket.LocalPosition.Z = MeshBottomZ;
		}
		else if (Socket.SocketName == FName("StudTop"))
		{
			Socket.LocalPosition.Z = MeshTopZ;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("WallStud: Mesh bounds Z=[%.2f, %.2f] height=%.2fcm, MeshRelZ=%.2f → "
		     "StudBottom Z: %.2f→%.2f, StudTop Z: %.2f→%.2f"),
		Bounds.Origin.Z - Bounds.BoxExtent.Z, Bounds.Origin.Z + Bounds.BoxExtent.Z,
		ActualHeight, MeshRelLoc.Z,
		OldBottomZ, MeshBottomZ, OldTopZ, MeshTopZ);
}

void AWallStud::ScaleToReferenceHeight(float TargetHeightCm)
{
	if (TargetHeightCm < 1.0f || !MeshComponent || !MeshComponent->GetStaticMesh()) return;
	if (FMath::IsNearlyEqual(StudHeight, TargetHeightCm, 0.1f)) return;

	// Read the unscaled mesh bounds
	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	float UnscaledHeight = Bounds.BoxExtent.Z * 2.0f;
	if (UnscaledHeight < 1.0f) return;

	// Remember old bottom socket Z for position correction
	float OldBottomLocalZ = 0.0f;
	for (const FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("StudBottom"))
		{
			OldBottomLocalZ = Socket.LocalPosition.Z;
			break;
		}
	}

	// Scale mesh Z to reach the target height
	float ScaleZ = TargetHeightCm / UnscaledHeight;
	MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, ScaleZ));

	// Recompute socket positions with the scaled mesh
	FVector MeshRelLoc = MeshComponent->GetRelativeLocation();
	float NewBottomZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * ScaleZ + MeshRelLoc.Z;
	float NewTopZ    = (Bounds.Origin.Z + Bounds.BoxExtent.Z) * ScaleZ + MeshRelLoc.Z;

	float OldStudHeight = StudHeight;
	StudHeight = NewTopZ - NewBottomZ;

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("StudBottom"))
			Socket.LocalPosition.Z = NewBottomZ;
		else if (Socket.SocketName == FName("StudTop"))
			Socket.LocalPosition.Z = NewTopZ;
	}

	// Adjust actor position so the bottom stays at the same world Z
	float DeltaZ = NewBottomZ - OldBottomLocalZ;
	if (FMath::Abs(DeltaZ) > 0.01f)
	{
		FVector Loc = GetActorLocation();
		Loc.Z -= DeltaZ;
		SetActorLocation(Loc);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("WallStud: Scaled Z by %.4f to match ref height %.2fcm (was %.2fcm, delta=%.2fcm)"),
		ScaleZ, TargetHeightCm, OldStudHeight, TargetHeightCm - OldStudHeight);
}
