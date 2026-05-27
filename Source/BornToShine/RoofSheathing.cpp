#include "RoofSheathing.h"
#include "Rafter.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ARoofSheathing::ARoofSheathing()
{
	PieceType = EPieceType::RoofSheathing;

	SheetLength = 243.84f;     // 8ft along ridge
	SheetWidth = 121.92f;      // 4ft up slope
	SheetThickness = 1.27f;    // 0.50"

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ARoofSheathing::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		float MeshX = Bounds.BoxExtent.X * 2.0f;  // should be ~243.84
		float MeshY = Bounds.BoxExtent.Y * 2.0f;  // should be ~121.92
		float MeshZ = Bounds.BoxExtent.Z * 2.0f;  // should be ~1.27

		UE_LOG(LogTemp, Log, TEXT("RoofSheathing: BeginPlay - Mesh=%.1fx%.1fx%.1f, SheetLen=%.1f SheetW=%.1f"),
			MeshX, MeshY, MeshZ, SheetLength, SheetWidth);
	}
}

void ARoofSheathing::InitializeSockets()
{
	Sockets.Empty();
	CreateFaceSockets();
	UE_LOG(LogTemp, Log, TEXT("RoofSheathing: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ARoofSheathing::CreateFaceSockets()
{
	Sockets.Empty();

	// Single bottom-center socket on the underside of the sheet.
	// This snaps to Rafter_Top_Face sockets on rafters.
	// All positioning (pitch matching, ridge alignment, tiling) handled in DetectSnapCandidates.
	FConstructionSocket Socket;
	Socket.SocketName = FName(TEXT("RoofSheathing_Bottom"));
	Socket.SocketType = EConstructionSocketType::RoofSheathing_Face;
	Socket.LocalPosition = FVector(0.0f, 0.0f, -SheetThickness / 2.0f);
	Socket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	Socket.Orientation = ESocketOrientation::Horizontal;
	Socket.bIsOccupied = false;
	Sockets.Add(Socket);
}

void ARoofSheathing::ScalePiece(float ScaleDelta)
{
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

bool ARoofSheathing::TryPlace()
{
	if (!Super::TryPlace()) return false;
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return true;

	// Roof boundaries were set by the snap system (Phase 4) using the cached grid
	// RoofRidgeStart/End = cell ridge bounds (NOT roof bounds)
	// RoofSlopeMax = cell slope max (NOT roof slope max)
	// RoofRafterOrigin = grid origin (NOT rafter location)
	if (RoofRidgeDir.IsNearlyZero() || RoofSlopeDir.IsNearlyZero()) return true;

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	const float UnscaledX = Bounds.BoxExtent.X * 2.0f; // 243.84 (full sheet ridge size)
	const float UnscaledY = Bounds.BoxExtent.Y * 2.0f; // 121.92 (full sheet slope size)

	// Cell dimensions from stored cell bounds
	const float CellWidth = RoofRidgeEnd - RoofRidgeStart;
	// Cell slope range: need to recover SlopeMin. The actor was placed at cell center,
	// and cell SlopeMax is stored as RoofSlopeMax. We compute cell SlopeMin by projecting
	// actor location.
	FVector ActorLoc = GetActorLocation();
	float SheetAlongSlope = FVector::DotProduct(ActorLoc - RoofRafterOrigin, RoofSlopeDir);
	float CellSlopeMin = (2.0f * SheetAlongSlope) - RoofSlopeMax;
	float CellHeight = RoofSlopeMax - CellSlopeMin;

	if (CellWidth < 5.0f || CellHeight < 5.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("RoofSheathing: Cell too small (%.1f x %.1f) — skipping"), CellWidth, CellHeight);
		return true;
	}

	// Scale mesh to fit the cell
	float ScaleX = CellWidth / UnscaledX;
	float ScaleY = CellHeight / UnscaledY;
	FVector CurScale = MeshComponent->GetRelativeScale3D();
	MeshComponent->SetRelativeScale3D(FVector(ScaleX, ScaleY, CurScale.Z));

	// Actor is at cell center, so mesh local offset is zero
	MeshComponent->SetRelativeLocation(FVector::ZeroVector);

	UE_LOG(LogTemp, Warning, TEXT("CELL PLACE: Col=%d Row=%d CellW=%.1f CellH=%.1f Scale=(%.3f,%.3f)"),
		RoofColumnIndex, RoofRowIndex, CellWidth, CellHeight, ScaleX, ScaleY);

	return true;
}
