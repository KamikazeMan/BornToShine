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

	// The sheet is oriented with:
	//   Actor X (forward) = ridge direction
	//   Actor Y (right)   = slope direction (ridge-to-fascia)
	//   Actor Z (up)      = roof normal
	// Actor rotation was set by the snap system (Yaw=RidgeYaw, Pitch=RafterPitch).

	FVector ActorLoc = GetActorLocation();
	FRotator ActorRot = GetActorRotation();

	// Extract the sheet's local axes in world space
	FVector RidgeDir = ActorRot.RotateVector(FVector::ForwardVector);  // along ridge (mesh X)
	FVector SlopeDir = ActorRot.RotateVector(FVector::RightVector);    // along slope (mesh Y)

	// Find all rafters on the same side of the roof
	TArray<AActor*> AllRafters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARafter::StaticClass(), AllRafters);

	float MinRidgeProj = FLT_MAX;
	float MaxRidgeProj = -FLT_MAX;
	float MinSlopeProj = FLT_MAX;
	float MaxSlopeProj = -FLT_MAX;
	int32 RafterCount = 0;

	for (AActor* A : AllRafters)
	{
		ARafter* Rafter = Cast<ARafter>(A);
		if (!Rafter) continue;

		// Only rafters within reasonable distance
		FVector ToRafter = Rafter->GetActorLocation() - ActorLoc;
		if (ToRafter.Size() > 500.0f) continue;

		// Only rafters on the same side — check that the rafter's slope direction
		// roughly matches this sheet's slope direction
		FVector RafterFwd = Rafter->GetActorRotation().RotateVector(FVector::ForwardVector);
		float SlopeDot = FVector::DotProduct(RafterFwd, SlopeDir);
		if (SlopeDot < 0.5f) continue; // Not on the same side

		RafterCount++;

		// Project rafter position onto sheet's ridge and slope axes
		float RidgeProj = FVector::DotProduct(ToRafter, RidgeDir);
		MinRidgeProj = FMath::Min(MinRidgeProj, RidgeProj);
		MaxRidgeProj = FMath::Max(MaxRidgeProj, RidgeProj);

		// Project rafter endpoints (ridge end and tail end) onto slope axis
		// Rafter origin is at ridge end, tail is at SlopeLength along rafter's +X
		FVector RafterOrigin = Rafter->GetActorLocation();
		float SlopeLen = Rafter->GetSlopeLengthCm();
		FVector TailPos = RafterOrigin + RafterFwd * SlopeLen;

		FVector OriginToSheet = RafterOrigin - ActorLoc;
		FVector TailToSheet = TailPos - ActorLoc;

		float SlopeProjOrigin = FVector::DotProduct(OriginToSheet, SlopeDir);
		float SlopeProjTail = FVector::DotProduct(TailToSheet, SlopeDir);

		MinSlopeProj = FMath::Min(MinSlopeProj, FMath::Min(SlopeProjOrigin, SlopeProjTail));
		MaxSlopeProj = FMath::Max(MaxSlopeProj, FMath::Max(SlopeProjOrigin, SlopeProjTail));
	}

	if (RafterCount == 0) return true; // No rafters found, keep full size

	// Add gable overhang (30.48cm past end rafters on each side)
	const float GableOverhang = 30.48f;
	float RoofMinRidge = MinRidgeProj - GableOverhang;
	float RoofMaxRidge = MaxRidgeProj + GableOverhang;
	float RoofMinSlope = MinSlopeProj;
	float RoofMaxSlope = MaxSlopeProj;

	// Get mesh bounds (unscaled)
	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	float MeshHalfX = Bounds.BoxExtent.X; // half of ridge dimension
	float MeshHalfY = Bounds.BoxExtent.Y; // half of slope dimension

	// Sheet edges in the sheet's local coordinate frame (relative to actor center)
	float SheetLeft = -MeshHalfX;   // ridge min side
	float SheetRight = MeshHalfX;   // ridge max side
	float SheetBottom = -MeshHalfY; // slope min side (toward ridge board)
	float SheetTop = MeshHalfY;     // slope max side (toward fascia)

	// Clamp sheet edges to roof surface boundaries
	float ClampedLeft = FMath::Max(SheetLeft, RoofMinRidge);
	float ClampedRight = FMath::Min(SheetRight, RoofMaxRidge);
	float ClampedBottom = FMath::Max(SheetBottom, RoofMinSlope);
	float ClampedTop = FMath::Min(SheetTop, RoofMaxSlope);

	// If the sheet is entirely within the roof, no trimming needed
	bool bNeedsTrimX = (ClampedLeft > SheetLeft + 1.0f) || (ClampedRight < SheetRight - 1.0f);
	bool bNeedsTrimY = (ClampedBottom > SheetBottom + 1.0f) || (ClampedTop < SheetTop - 1.0f);

	if (!bNeedsTrimX && !bNeedsTrimY) return true;

	// Compute new dimensions
	float NewWidth = ClampedRight - ClampedLeft;
	float NewHeight = ClampedTop - ClampedBottom;

	if (NewWidth < 1.0f || NewHeight < 1.0f) return true; // Too small, skip

	// Scale mesh to the trimmed size
	float ScaleX = NewWidth / (MeshHalfX * 2.0f);
	float ScaleY = NewHeight / (MeshHalfY * 2.0f);
	FVector CurScale = MeshComponent->GetRelativeScale3D();
	MeshComponent->SetRelativeScale3D(FVector(ScaleX, ScaleY, CurScale.Z));

	// Offset mesh center to match the trimmed region center
	float NewCenterX = (ClampedLeft + ClampedRight) / 2.0f;
	float NewCenterY = (ClampedBottom + ClampedTop) / 2.0f;
	MeshComponent->SetRelativeLocation(FVector(NewCenterX, NewCenterY, 0.0f));

	UE_LOG(LogTemp, Log, TEXT("RoofSheathing: Trimmed to %.1fx%.1f (scale=%.3f,%.3f) offset=(%.1f,%.1f) from %d rafters"),
		NewWidth, NewHeight, ScaleX, ScaleY, NewCenterX, NewCenterY, RafterCount);

	return true;
}
