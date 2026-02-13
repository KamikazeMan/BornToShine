// Born To Shine - Corner Post Implementation

#include "CornerPost.h"
#include "DoorFrame.h"
#include "Components/StaticMeshComponent.h"

ACornerPost::ACornerPost()
{
	PieceType = EPieceType::CornerPost;

	// Same height as wall studs: 92-5/8" for standard 8ft walls
	PostHeight = 235.27f;

	// Default flush offset: 1" = 2.54cm inward along plate's local Y
	// Adjust in BP_CornerPost to match the Rhino mesh's outer face offset
	FlushInwardOffset = 2.54f;

	// Scene root for clean actor transform
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ACornerPost::BeginPlay()
{
	Super::BeginPlay();

	// Mesh stays at (1,1,1) — the Rhino model is already correctly sized
	if (MeshComponent)
	{
		MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	}

	// Re-align sockets to the actual mesh extents so the post sits flush
	// on the bottom plate regardless of Rhino export pivot position.
	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Warning, TEXT("=== HEIGHT DIAGNOSTIC === CornerPost mesh height: %.2fcm (%.2f in)"),
		PostHeight, PostHeight / 2.54f);

	// Scale to match door frame king stud height if reference is available
	float RefHeight = ADoorFrame::GetKingStudHeight();
	if (RefHeight > 0.0f)
	{
		ScaleToReferenceHeight(RefHeight);
	}

	UE_LOG(LogTemp, Log, TEXT("CornerPost: BeginPlay - Height=%.1fcm, Total sockets: %d"),
		PostHeight, Sockets.Num());
}

void ACornerPost::InitializeSockets()
{
	Sockets.Empty();

	CreateBottomSocket();
	CreateTopSocket();

	UE_LOG(LogTemp, Log, TEXT("CornerPost: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ACornerPost::CreateBottomSocket()
{
	FConstructionSocket BottomSocket;
	BottomSocket.SocketName = FName(TEXT("PostBottom"));
	BottomSocket.SocketType = EConstructionSocketType::CornerPost_Bottom;
	BottomSocket.LocalPosition = FVector(0.0f, 0.0f, -PostHeight / 2.0f);
	BottomSocket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
	BottomSocket.Orientation = ESocketOrientation::Vertical;
	BottomSocket.bIsOccupied = false;
	Sockets.Add(BottomSocket);
}

void ACornerPost::CreateTopSocket()
{
	FConstructionSocket TopSocket;
	TopSocket.SocketName = FName(TEXT("PostTop"));
	TopSocket.SocketType = EConstructionSocketType::CornerPost_Top;
	TopSocket.LocalPosition = FVector(0.0f, 0.0f, PostHeight / 2.0f);
	TopSocket.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f);
	TopSocket.Orientation = ESocketOrientation::Vertical;
	TopSocket.bIsOccupied = false;
	Sockets.Add(TopSocket);
}

void ACornerPost::AdjustSocketsToMeshBounds()
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
			TEXT("CornerPost: Mesh height too small (%.2f cm) — skipping socket adjustment"),
			ActualHeight);
		return;
	}

	float OldBottomZ = -PostHeight / 2.0f;
	float OldTopZ    =  PostHeight / 2.0f;

	// Update PostHeight to reflect the real mesh
	PostHeight = ActualHeight;

	// Align both sockets to real mesh bounds so the top plate
	// sits on top of the actual mesh, not the geometric center.
	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("PostBottom"))
		{
			Socket.LocalPosition.Z = MeshBottomZ;
		}
		else if (Socket.SocketName == FName("PostTop"))
		{
			Socket.LocalPosition.Z = MeshTopZ;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("CornerPost: Mesh bounds Z=[%.2f, %.2f] height=%.2fcm, MeshRelZ=%.2f → "
		     "PostBottom Z: %.2f→%.2f, PostTop Z: %.2f→%.2f"),
		Bounds.Origin.Z - Bounds.BoxExtent.Z, Bounds.Origin.Z + Bounds.BoxExtent.Z,
		ActualHeight, MeshRelLoc.Z,
		OldBottomZ, MeshBottomZ, OldTopZ, MeshTopZ);
}

void ACornerPost::ScalePiece(float ScaleDelta)
{
	// Corner posts: scroll wheel scaling is DISABLED
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ACornerPost::ScaleToReferenceHeight(float TargetHeightCm)
{
	if (TargetHeightCm < 1.0f || !MeshComponent || !MeshComponent->GetStaticMesh()) return;
	if (FMath::IsNearlyEqual(PostHeight, TargetHeightCm, 0.1f)) return;

	// Read the unscaled mesh bounds
	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	float UnscaledHeight = Bounds.BoxExtent.Z * 2.0f;
	if (UnscaledHeight < 1.0f) return;

	// Remember old bottom socket Z for position correction
	float OldBottomLocalZ = 0.0f;
	for (const FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("PostBottom"))
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

	float OldPostHeight = PostHeight;
	PostHeight = NewTopZ - NewBottomZ;

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("PostBottom"))
			Socket.LocalPosition.Z = NewBottomZ;
		else if (Socket.SocketName == FName("PostTop"))
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
		TEXT("CornerPost: Scaled Z by %.4f to match ref height %.2fcm (was %.2fcm, delta=%.2fcm)"),
		ScaleZ, TargetHeightCm, OldPostHeight, TargetHeightCm - OldPostHeight);
}
