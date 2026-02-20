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
		MeshComponent->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	}

	OriginalMeshHeight = 0.0f;

	// Ridge posts require manual nailing
	bAutoNailOnPlace = false;

	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ARidgePost::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		// Capture original mesh height ONCE — used for Z-scale calculations
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		OriginalMeshHeight = Bounds.BoxExtent.Z * 2.0f;

		UE_LOG(LogTemp, Log, TEXT("RidgePost: OriginalMeshHeight=%.2fcm (will Z-scale to PostHeight)"),
			OriginalMeshHeight);

		// Initialize PostHeight to match the mesh (default starting height).
		// Constructor already set PostHeight = 60.96cm (24"), but if the mesh
		// is a different height, match it. Scroll wheel adjusts from here.
		if (PostHeight < 1.0f)
		{
			PostHeight = OriginalMeshHeight;
		}
	}

	// Z-scale mesh to PostHeight, anchor bottom at Z=0, set socket positions
	UpdateMeshScale();

	UE_LOG(LogTemp, Log, TEXT("RidgePost: BeginPlay - Height=%.1fcm (%.1fin), Pitch=%s, MeshHeight=%.1fcm, Sockets=%d"),
		PostHeight, GetPostHeightInches(), *GetPitchDisplayString(), OriginalMeshHeight, Sockets.Num());
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
	// Bottom socket at the base of the post (Z = 0, bottom-anchored)
	// Snaps to DoubleTopPlate_End or DoubleTopPlate top face
	FConstructionSocket BottomSocket;
	BottomSocket.SocketName = FName(TEXT("RidgePostBottom"));
	BottomSocket.SocketType = EConstructionSocketType::RidgePost_Bottom;
	BottomSocket.LocalPosition = FVector(0.0f, 0.0f, 0.0f);
	BottomSocket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f); // Facing downward
	BottomSocket.Orientation = ESocketOrientation::Vertical;
	BottomSocket.bIsOccupied = false;
	Sockets.Add(BottomSocket);
}

void ARidgePost::CreatePocketSocket()
{
	// Pocket socket at the top of the post (bottom-anchored: top = PostHeight)
	// The pocket is PocketDepth below the top of the outer boards
	float PocketCenterZ = PostHeight - (PocketDepth / 2.0f);

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
	// PostHeight = rise + HAF, where HAF accounts for rafter depth alignment.
	// Pitch must be computed from the actual roof RISE only (without HAF),
	// otherwise the pitch is inflated (e.g. 6.6/12 instead of 6/12).
	float ApproxAngle = FMath::Atan2(PostHeight, BuildingHalfWidthCm);
	float RafterHalfDepth = 13.97f / 2.0f; // half of 2x6 rafter depth (5.5")
	float HAF = RafterHalfDepth * FMath::Cos(ApproxAngle);
	float Rise = PostHeight - HAF;
	return (Rise / BuildingHalfWidthCm) * 12.0f;
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
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;
	if (OriginalMeshHeight < 1.0f) return;

	// Scroll wheel adjusts PostHeight by 1 inch per tick, Z-scales mesh to match.
	float NewHeight = PostHeight + (ScaleDelta > 0.0f ? HeightIncrementCm : -HeightIncrementCm);
	NewHeight = FMath::Clamp(NewHeight, MinPostHeight, MaxPostHeight);
	if (FMath::IsNearlyEqual(NewHeight, PostHeight, 0.01f)) return;

	PostHeight = NewHeight;
	UpdateMeshScale();
	RegenerateSockets();
	DisplayPitchInfo();
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
	if (OriginalMeshHeight < 1.0f) return;

	// Z-scale the mesh to fill from Z=0 (plate) to PostHeight (pocket top).
	// The pocket notch stretches visually — cosmetic compromise — but the
	// socket position is always correct so the ridge beam sits at the right
	// height and rafters connect at the right angle.
	float ZScale = PostHeight / OriginalMeshHeight;
	MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, ZScale));

	// Anchor the mesh bottom at Z=0 (sitting flush on the double top plate).
	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	float MeshBottomScaled = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * ZScale;
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -MeshBottomScaled - 3.759796f));

	// The pocket is visually stretched by ZScale, but the ridge beam is
	// always 18.415cm (7.25") tall. Position the pocket socket where the
	// ACTUAL ridge beam center sits — NOT at the stretched pocket center.
	// Pocket TOP = PostHeight. Ridge beam center = PocketDepth/2 below top.
	float PocketCenterZ = PostHeight - (PocketDepth / 2.0f);

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("RidgePostBottom"))
		{
			Socket.LocalPosition.Z = 0.0f;
		}
		else if (Socket.SocketName == FName("RidgePostPocket"))
		{
			Socket.LocalPosition.Z = PocketCenterZ;
		}
	}

	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);

	UE_LOG(LogTemp, Warning, TEXT("RidgePost: ZScale=%.3f, PostHeight=%.1f, MeshBottom=Z0, PocketSocket=%.1f"),
		ZScale, PostHeight, PocketCenterZ);
}

void ARidgePost::AdjustSocketsToMeshBounds()
{
	// UpdateMeshScale already sets sockets. This is called from
	// RegenerateSockets for the initial socket creation path.
	float PocketCenterZ = PostHeight - (PocketDepth / 2.0f);

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("RidgePostBottom"))
		{
			Socket.LocalPosition.Z = 0.0f;
		}
		else if (Socket.SocketName == FName("RidgePostPocket"))
		{
			Socket.LocalPosition.Z = PocketCenterZ;
		}
	}
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
