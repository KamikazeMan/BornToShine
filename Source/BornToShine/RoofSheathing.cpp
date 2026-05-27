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

	// Roof boundaries were set by the snap system in DetectSnapCandidates
	if (RoofRidgeDir.IsNearlyZero() || RoofSlopeDir.IsNearlyZero()) return true;

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	float UnscaledX = Bounds.BoxExtent.X * 2.0f; // along ridge
	float UnscaledY = Bounds.BoxExtent.Y * 2.0f; // along slope
	float SheetHalfX = UnscaledX / 2.0f;
	float SheetHalfY = UnscaledY / 2.0f;

	FVector ActorLoc = GetActorLocation();

	// Sheet center position in roof coordinate system
	FVector ToSheet = ActorLoc - RoofRafterOrigin;
	float SheetAlongRidge = FVector::DotProduct(ToSheet, RoofRidgeDir);
	float SheetAlongSlope = FVector::DotProduct(ToSheet, RoofSlopeDir);

	// Sheet edges in roof coordinates (absolute positions)
	float SheetRidgeLeft = SheetAlongRidge - SheetHalfX;
	float SheetRidgeRight = SheetAlongRidge + SheetHalfX;
	float SheetSlopeBottom = SheetAlongSlope - SheetHalfY; // toward ridge board
	float SheetSlopeTop = SheetAlongSlope + SheetHalfY;    // toward fascia

	// Trim to roof boundaries
	float TrimLeft = SheetRidgeLeft;
	float TrimRight = SheetRidgeRight;
	float TrimBottom = SheetSlopeBottom;
	float TrimTop = SheetSlopeTop;

	bool bNeedsTrim = false;

	// Ridge direction trim
	if (TrimLeft < RoofRidgeStart)
	{
		TrimLeft = RoofRidgeStart;
		bNeedsTrim = true;
	}
	if (TrimRight > RoofRidgeEnd)
	{
		TrimRight = RoofRidgeEnd;
		bNeedsTrim = true;
	}

	// Slope direction trim
	if (TrimBottom < 0.0f)
	{
		TrimBottom = 0.0f;
		bNeedsTrim = true;
	}
	if (TrimTop > RoofSlopeMax)
	{
		TrimTop = RoofSlopeMax;
		bNeedsTrim = true;
	}

	if (!bNeedsTrim) return true;

	float NewWidth = TrimRight - TrimLeft;
	float NewHeight = TrimTop - TrimBottom;

	if (NewWidth < 5.0f || NewHeight < 5.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("RoofSheathing: Trimmed too small (%.1f x %.1f) — skipping"), NewWidth, NewHeight);
		return true;
	}

	// Scale mesh
	float ScaleX = NewWidth / UnscaledX;
	float ScaleY = NewHeight / UnscaledY;
	FVector CurScale = MeshComponent->GetRelativeScale3D();
	MeshComponent->SetRelativeScale3D(FVector(ScaleX, ScaleY, CurScale.Z));

	// Offset mesh center to the center of the trimmed region.
	// The offset is in LOCAL sheet space (X=ridge, Y=slope).
	// TrimLeft/TrimRight are in absolute roof coords. Convert to relative to sheet center.
	float TrimCenterRidge = ((TrimLeft + TrimRight) / 2.0f) - SheetAlongRidge;
	float TrimCenterSlope = ((TrimBottom + TrimTop) / 2.0f) - SheetAlongSlope;
	MeshComponent->SetRelativeLocation(FVector(TrimCenterRidge, TrimCenterSlope, 0.0f));

	// Diagnostic: log final world-space position of mesh edges
	FVector MeshWorldCenter = MeshComponent->GetComponentLocation();
	FVector MeshSlopeDir = RoofSlopeDir;
	FVector RafterOriginWorld = RoofRafterOrigin;
	float MeshCenterAlongSlope = FVector::DotProduct(MeshWorldCenter - RafterOriginWorld, MeshSlopeDir);
	float MeshBottomEdgeAlongSlope = MeshCenterAlongSlope + (NewHeight / 2.0f);
	UE_LOG(LogTemp, Warning, TEXT("TRIM RESULT: MeshCenterAlongSlope=%.1f MeshBottomEdgeAlongSlope=%.1f RoofSlopeMax=%.1f"),
		MeshCenterAlongSlope, MeshBottomEdgeAlongSlope, RoofSlopeMax);

	UE_LOG(LogTemp, Log, TEXT("RoofSheathing: Trimmed %.1fx%.1f -> %.1fx%.1f (scale=%.3f,%.3f offset=%.1f,%.1f) Ridge=[%.1f,%.1f] Slope=[%.1f,%.1f]"),
		UnscaledX, UnscaledY, NewWidth, NewHeight, ScaleX, ScaleY, TrimCenterRidge, TrimCenterSlope,
		TrimLeft, TrimRight, TrimBottom, TrimTop);

	return true;
}
