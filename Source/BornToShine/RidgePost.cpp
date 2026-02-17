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

	// Dual-mesh components (nullptr until David provides split meshes)
	PocketMesh = nullptr;
	PostMesh = nullptr;
	OriginalMeshHeight = 0.0f;
	PocketPortionHeight = 30.5f; // ~12" fixed pocket top portion

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

		// Log full mesh bounds so David knows where to split
		FBox MeshBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
		UE_LOG(LogTemp, Warning, TEXT("RidgePost mesh bounds: Min=(%s) Max=(%s) Height=%.2fcm Origin.Z=%.2f Extent.Z=%.2f"),
			*MeshBox.Min.ToString(), *MeshBox.Max.ToString(),
			OriginalMeshHeight, Bounds.Origin.Z, Bounds.BoxExtent.Z);
		UE_LOG(LogTemp, Warning, TEXT("RidgePost: POCKET TOP should be at mesh Max.Z=%.2f, pocket starts at Z=%.2f (%.2fcm below top)"),
			MeshBox.Max.Z, MeshBox.Max.Z - PocketDepth, PocketDepth);

		// Initialize PostHeight to match the mesh (default starting height).
		// Constructor already set PostHeight = 60.96cm (24"), but if the mesh
		// is a different height, match it. Scroll wheel adjusts from here.
		if (PostHeight < 1.0f)
		{
			PostHeight = OriginalMeshHeight;
		}
	}

	// Apply Z-scale and bottom-anchor the mesh, then align sockets
	UpdateMeshScale();
	AdjustSocketsToMeshBounds();

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
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;
	if (OriginalMeshHeight < 1.0f) return;

	// Scroll wheel adjusts PostHeight by HeightIncrementCm (1 inch) per tick.
	// Z-scales the mesh while anchoring the bottom to the double top plate.
	// Pocket geometry distorts proportionally — acceptable until David provides
	// split meshes (PocketMesh + PostMesh).
	float NewHeight = PostHeight + (ScaleDelta > 0.0f ? HeightIncrementCm : -HeightIncrementCm);
	NewHeight = FMath::Clamp(NewHeight, MinPostHeight, MaxPostHeight);

	if (FMath::IsNearlyEqual(NewHeight, PostHeight, 0.01f)) return;

	PostHeight = NewHeight;
	UpdateMeshScale();
	RegenerateSockets();
	DisplayPitchInfo();

	UE_LOG(LogTemp, Log, TEXT("RidgePost: ScalePiece - Height=%.1fcm (%.1fin), %s"),
		PostHeight, GetPostHeightInches(), *GetPitchDisplayString());
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

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();

	// Z-scale the mesh to match the desired PostHeight.
	// X and Y stay at 1.0 — only vertical scaling.
	// Pocket notch distorts proportionally; acceptable until David provides
	// split meshes (PocketMesh at 1,1,1 + PostMesh Z-scaled).
	float ZScale = PostHeight / OriginalMeshHeight;
	MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, ZScale));

	// ANCHOR BOTTOM at Z=0: the mesh bottom in scaled local space is
	// (Origin.Z - Extent.Z) * ZScale. Push the mesh up so that lands at 0.
	float MeshBottomScaled = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * ZScale;
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -MeshBottomScaled));

	// Recompute socket positions to match new height.
	// Pocket socket uses the ORIGINAL (un-distorted) pocket depth so the
	// ridge beam seat stays at the correct 7.25" dimension.
	float BottomZ = 0.0f;
	float TopZ = PostHeight;
	float PocketCenterZ = TopZ - (PocketDepth / 2.0f);

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("RidgePostBottom"))
		{
			Socket.LocalPosition.Z = BottomZ;
		}
		else if (Socket.SocketName == FName("RidgePostPocket"))
		{
			Socket.LocalPosition.Z = PocketCenterZ;
		}
	}

	// Keep actor scale at 1,1,1 — only the mesh component scales
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);

	UE_LOG(LogTemp, Log, TEXT("RidgePost: Z-scale=%.3f, PostHeight=%.1fcm, bottom=%.1f top=%.1f pocket=%.1f"),
		ZScale, PostHeight, BottomZ, TopZ, PocketCenterZ);
}

void ARidgePost::AdjustSocketsToMeshBounds()
{
	// Bottom-anchored: bottom is always at Z=0, top at PostHeight.
	// UpdateMeshScale already handles mesh positioning, so just
	// make sure sockets match the current PostHeight.
	float BottomZ = 0.0f;
	float TopZ = PostHeight;
	float PocketCenterZ = TopZ - (PocketDepth / 2.0f);

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("RidgePostBottom"))
		{
			Socket.LocalPosition.Z = BottomZ;
		}
		else if (Socket.SocketName == FName("RidgePostPocket"))
		{
			Socket.LocalPosition.Z = PocketCenterZ;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("RidgePost: AdjustSockets - bottom=%.1f top=%.1f pocket=%.1f"),
		BottomZ, TopZ, PocketCenterZ);
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
