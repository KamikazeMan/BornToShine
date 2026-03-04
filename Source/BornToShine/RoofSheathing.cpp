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

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	float UnscaledX = Bounds.BoxExtent.X * 2.0f; // 243.84 along ridge
	float UnscaledY = Bounds.BoxExtent.Y * 2.0f; // 121.92 along slope

	FRotator ActorRot = GetActorRotation();
	FVector ActorLoc = GetActorLocation();
	FVector SheetRidgeDir = ActorRot.RotateVector(FVector::ForwardVector);
	FVector SheetSlopeDir = ActorRot.RotateVector(FVector::RightVector);

	// Find all same-side rafters
	TArray<AActor*> AllRafters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARafter::StaticClass(), AllRafters);

	float MinRidgeProj = FLT_MAX;
	float MaxRidgeProj = -FLT_MAX;
	float MaxSlopeLen = 0.0f;
	int32 RafterCount = 0;

	for (AActor* A : AllRafters)
	{
		ARafter* R = Cast<ARafter>(A);
		if (!R) continue;
		if (FVector::Dist(R->GetActorLocation(), ActorLoc) > 500.0f) continue;

		FVector RafterFwd = R->GetActorRotation().RotateVector(FVector::ForwardVector);
		if (FVector::DotProduct(RafterFwd, SheetSlopeDir) < 0.5f) continue;

		RafterCount++;
		float RidgeProj = FVector::DotProduct(R->GetActorLocation() - ActorLoc, SheetRidgeDir);
		MinRidgeProj = FMath::Min(MinRidgeProj, RidgeProj);
		MaxRidgeProj = FMath::Max(MaxRidgeProj, RidgeProj);
		MaxSlopeLen = FMath::Max(MaxSlopeLen, R->GetSlopeLengthCm());
	}

	if (RafterCount < 2) return true;

	// Roof boundaries relative to sheet center
	const float RafterHalfW = 1.905f;
	float RoofLeft = MinRidgeProj - RafterHalfW;
	float RoofRight = MaxRidgeProj + RafterHalfW;

	// Find nearest rafter to get slope reference
	ARafter* NearestRafter = nullptr;
	float NearestDist = FLT_MAX;
	for (AActor* A : AllRafters)
	{
		ARafter* R = Cast<ARafter>(A);
		if (!R) continue;
		FVector RafterFwd = R->GetActorRotation().RotateVector(FVector::ForwardVector);
		if (FVector::DotProduct(RafterFwd, SheetSlopeDir) < 0.5f) continue;
		float D = FVector::Dist(R->GetActorLocation(), ActorLoc);
		if (D < NearestDist) { NearestDist = D; NearestRafter = R; }
	}
	if (!NearestRafter) return true;

	// Sheet center position along slope, relative to rafter ridge end
	float SheetCenterSlope = FVector::DotProduct(ActorLoc - NearestRafter->GetActorLocation(), SheetSlopeDir);

	// Sheet edges in local space
	float SheetHalfX = UnscaledX / 2.0f;
	float SheetHalfY = UnscaledY / 2.0f;

	// Clamp ridge edges
	float LeftEdge = -SheetHalfX;
	float RightEdge = SheetHalfX;

	if (LeftEdge < RoofLeft) LeftEdge = RoofLeft;
	if (RightEdge > RoofRight) RightEdge = RoofRight;

	// Clamp slope edges (relative to sheet center)
	float BottomEdge = -SheetHalfY; // toward ridge board
	float TopEdge = SheetHalfY;     // toward fascia

	// Absolute slope positions of sheet edges
	float AbsSlopeBottom = SheetCenterSlope + BottomEdge;
	float AbsSlopeTop = SheetCenterSlope + TopEdge;

	if (AbsSlopeBottom < 0.0f)
	{
		BottomEdge = -SheetCenterSlope; // trim at ridge board (slope=0)
	}
	if (AbsSlopeTop > MaxSlopeLen)
	{
		TopEdge = MaxSlopeLen - SheetCenterSlope; // trim at fascia
	}

	// Check if any trimming needed
	float NewWidth = RightEdge - LeftEdge;
	float NewHeight = TopEdge - BottomEdge;

	bool bTrimmedX = FMath::Abs(NewWidth - UnscaledX) > 1.0f;
	bool bTrimmedY = FMath::Abs(NewHeight - UnscaledY) > 1.0f;

	if (!bTrimmedX && !bTrimmedY) return true;
	if (NewWidth < 5.0f || NewHeight < 5.0f) return true;

	// Apply scale
	float ScaleX = NewWidth / UnscaledX;
	float ScaleY = NewHeight / UnscaledY;
	FVector CurScale = MeshComponent->GetRelativeScale3D();
	MeshComponent->SetRelativeScale3D(FVector(ScaleX, ScaleY, CurScale.Z));

	// Offset mesh to center of trimmed region
	float OffsetX = (LeftEdge + RightEdge) / 2.0f;
	float OffsetY = (BottomEdge + TopEdge) / 2.0f;
	MeshComponent->SetRelativeLocation(FVector(OffsetX, OffsetY, 0.0f));

	UE_LOG(LogTemp, Log, TEXT("RoofSheathing: Trimmed %.1fx%.1f -> %.1fx%.1f (scale=%.3f,%.3f offset=%.1f,%.1f)"),
		UnscaledX, UnscaledY, NewWidth, NewHeight, ScaleX, ScaleY, OffsetX, OffsetY);

	return true;
}
