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

	// Get mesh info
	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	FVector MeshScale = MeshComponent->GetRelativeScale3D();
	float UnscaledX = Bounds.BoxExtent.X * 2.0f; // 243.84 (8ft along ridge)
	float UnscaledY = Bounds.BoxExtent.Y * 2.0f; // 121.92 (4ft along slope)

	// Sheet's world-space axes
	FRotator ActorRot = GetActorRotation();
	FVector ActorLoc = GetActorLocation();
	FVector SheetRidgeDir = ActorRot.RotateVector(FVector::ForwardVector);  // mesh X = along ridge
	FVector SheetSlopeDir = ActorRot.RotateVector(FVector::RightVector);    // mesh Y = along slope
	FVector SheetNormal = ActorRot.RotateVector(FVector::UpVector);         // mesh Z = roof normal

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

		FVector ToRafter = R->GetActorLocation() - ActorLoc;
		if (ToRafter.Size() > 500.0f) continue;

		// Same side check
		FVector RafterFwd = R->GetActorRotation().RotateVector(FVector::ForwardVector);
		float SlopeDot = FVector::DotProduct(RafterFwd, SheetSlopeDir);
		if (SlopeDot < 0.5f) continue;

		RafterCount++;

		// Project rafter position onto ridge direction
		float RidgeProj = FVector::DotProduct(ToRafter, SheetRidgeDir);
		MinRidgeProj = FMath::Min(MinRidgeProj, RidgeProj);
		MaxRidgeProj = FMath::Max(MaxRidgeProj, RidgeProj);

		MaxSlopeLen = FMath::Max(MaxSlopeLen, R->GetSlopeLengthCm());
	}

	if (RafterCount < 2) return true; // Not enough rafters to determine bounds

	// Roof boundaries relative to this sheet's center
	const float RafterHalfW = 1.905f;
	float RoofRidgeMin = MinRidgeProj - RafterHalfW; // left edge of roof along ridge
	float RoofRidgeMax = MaxRidgeProj + RafterHalfW;  // right edge of roof along ridge

	// Sheet's current extent relative to its center (unscaled, in local coords)
	float SheetHalfX = UnscaledX / 2.0f; // half of 8ft along ridge
	float SheetHalfY = UnscaledY / 2.0f; // half of 4ft along slope

	// Sheet edges in roof-projected space (relative to actor center)
	float SheetLeftRidge = -SheetHalfX;
	float SheetRightRidge = SheetHalfX;
	float SheetStartSlope = -SheetHalfY; // toward ridge board (up slope)
	float SheetEndSlope = SheetHalfY;     // toward fascia (down slope)

	// The sheet center's position along the slope relative to rafter origin
	// is determined by the snap system. We need to know where the rafter origin
	// (ridge end) is relative to this sheet center.
	// Find nearest rafter to get reference
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

	// Sheet center position along slope relative to rafter ridge end
	FVector ToSheetFromRafter = ActorLoc - NearestRafter->GetActorLocation();
	float SheetCenterAlongSlope = FVector::DotProduct(ToSheetFromRafter, SheetSlopeDir);

	// Slope boundaries: 0 = ridge end of rafter, MaxSlopeLen = tail end
	float SlopeStart = 0.0f;            // ridge board
	float SlopeEnd = MaxSlopeLen;        // rafter tail / fascia

	// Sheet edges in slope space (absolute, not relative to center)
	float SheetSlopeMin = SheetCenterAlongSlope - SheetHalfY;
	float SheetSlopeMax = SheetCenterAlongSlope + SheetHalfY;

	// Determine how much to trim
	bool bNeedsTrim = false;

	// Trim along ridge: clamp to roof edges
	float TrimmedLeftRidge = SheetLeftRidge;
	float TrimmedRightRidge = SheetRightRidge;

	if (SheetLeftRidge < RoofRidgeMin)
	{
		TrimmedLeftRidge = RoofRidgeMin;
		bNeedsTrim = true;
	}
	if (SheetRightRidge > RoofRidgeMax)
	{
		TrimmedRightRidge = RoofRidgeMax;
		bNeedsTrim = true;
	}

	// Trim along slope: clamp to rafter extent
	float TrimmedStartSlope = SheetStartSlope;
	float TrimmedEndSlope = SheetEndSlope;

	if (SheetSlopeMin < SlopeStart)
	{
		// Sheet extends past ridge board — trim the ridge-side edge
		float OverlapAtRidge = SlopeStart - SheetSlopeMin;
		TrimmedStartSlope = SheetStartSlope + OverlapAtRidge;
		bNeedsTrim = true;
	}
	if (SheetSlopeMax > SlopeEnd)
	{
		// Sheet extends past fascia — trim the fascia-side edge
		float OverlapAtFascia = SheetSlopeMax - SlopeEnd;
		TrimmedEndSlope = SheetEndSlope - OverlapAtFascia;
		bNeedsTrim = true;
	}

	// Now check for overlap with OTHER placed roof sheathing sheets.
	// If this sheet overlaps an already-placed sheet, trim this one back.
	const float SlopeGridSize = 121.92f;
	const float RidgeGridSize = 243.84f;

	TArray<AActor*> AllSheathing;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARoofSheathing::StaticClass(), AllSheathing);

	for (AActor* A : AllSheathing)
	{
		ARoofSheathing* Other = Cast<ARoofSheathing>(A);
		if (!Other || Other == this) continue;
		if (Other->GetPieceState() != EPieceState::Placed && Other->GetPieceState() != EPieceState::Nailed) continue;

		// Check if on same side of roof (similar normal direction)
		FVector OtherNormal = Other->GetActorRotation().RotateVector(FVector::UpVector);
		if (FVector::DotProduct(SheetNormal, OtherNormal) < 0.5f) continue;

		FVector ToOther = Other->GetActorLocation() - ActorLoc;
		float OtherAlongRidge = FVector::DotProduct(ToOther, SheetRidgeDir);
		float OtherAlongSlope = FVector::DotProduct(ToOther, SheetSlopeDir);

		// Get other sheet's current dimensions (may already be trimmed)
		FVector OtherScale = Other->MeshComponent ? Other->MeshComponent->GetRelativeScale3D() : FVector(1,1,1);
		float OtherHalfX = (UnscaledX * OtherScale.X) / 2.0f;
		float OtherHalfY = (UnscaledY * OtherScale.Y) / 2.0f;

		// Other sheet edges relative to THIS sheet's center
		float OtherLeftRidge = OtherAlongRidge - OtherHalfX;
		float OtherRightRidge = OtherAlongRidge + OtherHalfX;
		float OtherStartSlope = OtherAlongSlope - OtherHalfY;
		float OtherEndSlope = OtherAlongSlope + OtherHalfY;

		// Check ridge overlap (sheets in same row)
		bool bSameRow = FMath::Abs(OtherAlongSlope) < SlopeGridSize * 0.75f;
		if (bSameRow)
		{
			// If other sheet is to our right and we overlap
			if (OtherAlongRidge > 0.0f && TrimmedRightRidge > OtherLeftRidge)
			{
				TrimmedRightRidge = OtherLeftRidge;
				bNeedsTrim = true;
			}
			// If other sheet is to our left and we overlap
			if (OtherAlongRidge < 0.0f && TrimmedLeftRidge < OtherRightRidge)
			{
				TrimmedLeftRidge = OtherRightRidge;
				bNeedsTrim = true;
			}
		}

		// Check slope overlap (sheets in same column)
		bool bSameCol = FMath::Abs(OtherAlongRidge) < RidgeGridSize * 0.75f;
		if (bSameCol)
		{
			// If other sheet is above us (toward ridge) and we overlap
			if (OtherAlongSlope < 0.0f && TrimmedStartSlope < OtherEndSlope)
			{
				TrimmedStartSlope = OtherEndSlope;
				bNeedsTrim = true;
			}
			// If other sheet is below us (toward fascia) and we overlap
			if (OtherAlongSlope > 0.0f && TrimmedEndSlope > OtherStartSlope)
			{
				TrimmedEndSlope = OtherStartSlope;
				bNeedsTrim = true;
			}
		}
	}

	if (!bNeedsTrim) return true;

	// Calculate trimmed dimensions
	float NewWidth = TrimmedRightRidge - TrimmedLeftRidge;
	float NewHeight = TrimmedEndSlope - TrimmedStartSlope;

	// Don't trim to nothing
	if (NewWidth < 5.0f || NewHeight < 5.0f) return true;

	// Scale mesh to trimmed size
	float ScaleX = NewWidth / UnscaledX;
	float ScaleY = NewHeight / UnscaledY;
	MeshComponent->SetRelativeScale3D(FVector(ScaleX, ScaleY, MeshScale.Z));

	// Offset mesh center to match trimmed region
	float NewCenterX = (TrimmedLeftRidge + TrimmedRightRidge) / 2.0f;
	float NewCenterY = (TrimmedStartSlope + TrimmedEndSlope) / 2.0f;

	// Convert the center offset from roof-projected space back to mesh-local offset
	// NewCenterX is along SheetRidgeDir (mesh X), NewCenterY is along SheetSlopeDir (mesh Y)
	MeshComponent->SetRelativeLocation(FVector(NewCenterX, NewCenterY, 0.0f));

	UE_LOG(LogTemp, Log, TEXT("RoofSheathing: Trimmed to %.1f x %.1f (scale=%.3f, %.3f) offset=(%.1f, %.1f)"),
		NewWidth, NewHeight, ScaleX, ScaleY, NewCenterX, NewCenterY);

	return true;
}
