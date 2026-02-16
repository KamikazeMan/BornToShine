// Born To Shine - Ridge Post Implementation

#include "RidgePost.h"
#include "Components/StaticMeshComponent.h"

ARidgePost::ARidgePost()
{
	PieceType = EPieceType::RidgePost;

	// 2x6 lumber actual dimensions
	BoardThickness = 3.81f;   // 1.5 inches
	BoardFaceWidth = 13.97f;  // 5.5 inches

	// Total post assembly: 3 laminated 2x6s
	PostWidth = BoardThickness * 3.0f; // 4.5" = 11.43cm
	PostDepth = BoardFaceWidth;        // 5.5" = 13.97cm

	// Pocket for 2x8 ridge beam
	PocketWidth = BoardThickness;      // 1.5" = 3.81cm (middle board)
	PocketDepth = 18.415f;             // 7.25" (2x8 actual height)

	// Default post height: 60.96cm (24") — gives 6/12 pitch on an 8ft-wide building
	PostHeight = 60.96f;

	// Default building half-width: 4ft = 121.92cm (for 8ft wide building)
	BuildingHalfWidthCm = 121.92f;

	// Scroll wheel: 1 inch per tick
	HeightIncrementCm = 2.54f;

	// Height limits
	MinPostHeight = 30.48f;   // 12" — shallow pitch
	MaxPostHeight = 243.84f;  // 96" — very steep pitch

	// Scene root for clean actor transform
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	// Ridge posts require manual nailing
	bAutoNailOnPlace = false;

	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ARidgePost::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComponent)
	{
		MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	}

	// Adjust sockets to real mesh bounds
	AdjustSocketsToMeshBounds();

	// Scale mesh Z to match PostHeight
	UpdateMeshScale();

	UE_LOG(LogTemp, Log, TEXT("RidgePost: BeginPlay - Height=%.1fcm (%.1fin), Pitch=%s, Sockets=%d"),
		PostHeight, GetPostHeightInches(), *GetPitchDisplayString(), Sockets.Num());
}

void ARidgePost::InitializeSockets()
{
	Sockets.Empty();

	CreateBottomSocket();
	CreatePocketSocket();

	UE_LOG(LogTemp, Log, TEXT("RidgePost: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ARidgePost::CreateBottomSocket()
{
	// Bottom socket at the base of the post
	// Snaps to DoubleTopPlate_End or DoubleTopPlate top face
	FConstructionSocket BottomSocket;
	BottomSocket.SocketName = FName(TEXT("RidgePostBottom"));
	BottomSocket.SocketType = EConstructionSocketType::RidgePost_Bottom;
	BottomSocket.LocalPosition = FVector(0.0f, 0.0f, -PostHeight / 2.0f);
	BottomSocket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f); // Facing downward
	BottomSocket.Orientation = ESocketOrientation::Vertical;
	BottomSocket.bIsOccupied = false;
	Sockets.Add(BottomSocket);
}

void ARidgePost::CreatePocketSocket()
{
	// Pocket socket at the top of the post
	// The pocket is PocketDepth below the top of the outer boards
	// Ridge board center sits at: top - PocketDepth/2
	float PocketCenterZ = (PostHeight / 2.0f) - (PocketDepth / 2.0f);

	FConstructionSocket PocketSocket;
	PocketSocket.SocketName = FName(TEXT("RidgePostPocket"));
	PocketSocket.SocketType = EConstructionSocketType::RidgePost_Pocket;
	PocketSocket.LocalPosition = FVector(0.0f, 0.0f, PocketCenterZ);
	PocketSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f); // Facing upward
	PocketSocket.Orientation = ESocketOrientation::Vertical;
	PocketSocket.bIsOccupied = false;
	Sockets.Add(PocketSocket);
}

float ARidgePost::GetPitchRatio() const
{
	if (BuildingHalfWidthCm <= 0.0f) return 0.0f;
	// Pitch = (rise / run) * 12 = (PostHeight / BuildingHalfWidthCm) * 12
	return (PostHeight / BuildingHalfWidthCm) * 12.0f;
}

FString ARidgePost::GetPitchDisplayString() const
{
	float Ratio = GetPitchRatio();
	// Round to nearest 0.5 for clean display
	float Rounded = FMath::RoundToFloat(Ratio * 2.0f) / 2.0f;

	// Show as fraction if whole or half number
	if (FMath::IsNearlyEqual(Rounded, FMath::RoundToFloat(Rounded), 0.01f))
	{
		int32 WholeNum = FMath::RoundToInt(Rounded);
		return FString::Printf(TEXT("%d/12 pitch"), WholeNum);
	}
	return FString::Printf(TEXT("%.1f/12 pitch"), Rounded);
}

float ARidgePost::GetPostHeightInches() const
{
	return PostHeight / 2.54f;
}

void ARidgePost::SetPostHeightCm(float HeightCm)
{
	HeightCm = FMath::Clamp(HeightCm, MinPostHeight, MaxPostHeight);

	if (!FMath::IsNearlyEqual(HeightCm, PostHeight, 0.01f))
	{
		PostHeight = HeightCm;
		RegenerateSockets();
		UpdateMeshScale();
		DisplayPitchInfo();

		UE_LOG(LogTemp, Log, TEXT("RidgePost: Height changed to %.1fcm (%.1fin), %s"),
			PostHeight, GetPostHeightInches(), *GetPitchDisplayString());
	}
}

void ARidgePost::SetBuildingHalfWidth(float HalfWidthCm)
{
	if (HalfWidthCm > 0.0f)
	{
		BuildingHalfWidthCm = HalfWidthCm;

		UE_LOG(LogTemp, Log, TEXT("RidgePost: Building half-width set to %.1fcm (%.1fin), %s"),
			BuildingHalfWidthCm, BuildingHalfWidthCm / 2.54f, *GetPitchDisplayString());
	}
}

void ARidgePost::ScalePiece(float ScaleDelta)
{
	// Override: scroll wheel adjusts post height (and thus roof pitch)
	// ScaleDelta is positive for scroll up, negative for scroll down
	float NewHeight = PostHeight + (ScaleDelta > 0.0f ? HeightIncrementCm : -HeightIncrementCm);
	SetPostHeightCm(NewHeight);

	// Keep actor scale at 1,1,1
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ARidgePost::RegenerateSockets()
{
	Sockets.Empty();

	CreateBottomSocket();
	CreatePocketSocket();

	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("RidgePost: Regenerated %d sockets for height %.1fcm"),
		Sockets.Num(), PostHeight);
}

void ARidgePost::UpdateMeshScale()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	float UnscaledHeight = Bounds.BoxExtent.Z * 2.0f;

	if (UnscaledHeight < 1.0f) return;

	// Scale Z to match PostHeight
	float ScaleZ = PostHeight / UnscaledHeight;
	FVector CurrentMeshScale = MeshComponent->GetRelativeScale3D();
	MeshComponent->SetRelativeScale3D(FVector(CurrentMeshScale.X, CurrentMeshScale.Y, ScaleZ));

	// Recompute socket positions for the scaled mesh
	FVector MeshRelLoc = MeshComponent->GetRelativeLocation();
	float NewBottomZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * ScaleZ + MeshRelLoc.Z;
	float NewTopZ    = (Bounds.Origin.Z + Bounds.BoxExtent.Z) * ScaleZ + MeshRelLoc.Z;

	float ActualHeight = NewTopZ - NewBottomZ;
	float PocketCenterZ = NewTopZ - (PocketDepth / 2.0f);

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("RidgePostBottom"))
		{
			Socket.LocalPosition.Z = NewBottomZ;
		}
		else if (Socket.SocketName == FName("RidgePostPocket"))
		{
			Socket.LocalPosition.Z = PocketCenterZ;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RidgePost: Scaled mesh Z by %.4f -> height %.1fcm, bottom=%.1f top=%.1f pocket=%.1f"),
		ScaleZ, ActualHeight, NewBottomZ, NewTopZ, PocketCenterZ);
}

void ARidgePost::AdjustSocketsToMeshBounds()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	FVector MeshRelLoc = MeshComponent->GetRelativeLocation();
	FVector MeshScale = MeshComponent->GetRelativeScale3D();

	float MeshBottomZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * MeshScale.Z + MeshRelLoc.Z;
	float MeshTopZ    = (Bounds.Origin.Z + Bounds.BoxExtent.Z) * MeshScale.Z + MeshRelLoc.Z;
	float ActualHeight = MeshTopZ - MeshBottomZ;

	if (ActualHeight < 1.0f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RidgePost: Mesh height too small (%.2fcm) - skipping socket adjustment"),
			ActualHeight);
		return;
	}

	float PocketCenterZ = MeshTopZ - (PocketDepth / 2.0f);

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("RidgePostBottom"))
		{
			Socket.LocalPosition.Z = MeshBottomZ;
		}
		else if (Socket.SocketName == FName("RidgePostPocket"))
		{
			Socket.LocalPosition.Z = PocketCenterZ;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("RidgePost: AdjustSockets - MeshZ=[%.2f, %.2f] height=%.2fcm, pocket=%.2f"),
		MeshBottomZ, MeshTopZ, ActualHeight, PocketCenterZ);
}

void ARidgePost::DisplayPitchInfo() const
{
	if (GEngine)
	{
		FString PitchStr = GetPitchDisplayString();
		FString HeightStr = FString::Printf(TEXT("%.0f\""), GetPostHeightInches());

		GEngine->AddOnScreenDebugMessage(
			9999, // Unique key so it replaces previous message
			2.0f,
			FColor::Yellow,
			FString::Printf(TEXT("Ridge Post: %s  |  Height: %s"), *PitchStr, *HeightStr)
		);
	}
}
