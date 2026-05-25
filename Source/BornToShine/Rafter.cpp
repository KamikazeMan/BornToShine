// Born To Shine - Rafter Implementation (Static Mesh with X-Scale for Length)

#include "Rafter.h"
#include "Components/StaticMeshComponent.h"

ARafter::ARafter()
{
	PieceType = EPieceType::Rafter;

	// 2x6 lumber cross-section
	RafterWidth = 3.81f;   // 1.5 inches
	RafterDepth = 13.97f;  // 5.5 inches

	// Default 6/12 pitch
	PitchRatio = 6.0f;

	// Default run = 4ft (half of 8ft building)
	RunDistanceCm = 121.92f;

	// Default 12" overhang
	OverhangCm = 30.48f;

	// Scene root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
	MeshDefaultLength = 0.0f;
}

void ARafter::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		// Capture original mesh length along X — used for X-scale calculations
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		MeshDefaultLength = Bounds.BoxExtent.X * 2.0f;

		UE_LOG(LogTemp, Log, TEXT("Rafter: MeshDefaultLength=%.2fcm (will X-scale to SlopeLength)"),
			MeshDefaultLength);
	}

	// X-scale mesh to slope length, set socket positions
	UpdateRafterLength();

	UE_LOG(LogTemp, Log, TEXT("Rafter: BeginPlay - Pitch=%.1f/12 (%.1f deg), Run=%.1fcm, Slope=%.1fcm, Sockets=%d"),
		PitchRatio, GetPitchAngleDegrees(), RunDistanceCm, GetSlopeLengthCm(), Sockets.Num());
}

void ARafter::InitializeSockets()
{
	Sockets.Empty();

	CreateRidgeEndSocket();
	CreateBirdsmouthSocket();
	CreateTailEndSocket();
	CreateTopFaceSocket();

	UE_LOG(LogTemp, Log, TEXT("Rafter: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

float ARafter::GetPitchAngleRadians() const
{
	// pitch = rise/run where PitchRatio = rise per 12 units of run
	// tan(angle) = PitchRatio / 12
	return FMath::Atan2(PitchRatio, 12.0f);
}

float ARafter::GetPitchAngleDegrees() const
{
	return FMath::RadiansToDegrees(GetPitchAngleRadians());
}

float ARafter::GetSlopeLengthCm() const
{
	// If fascia trimmed the rafter, return the trimmed length instead of computed
	if (TrimmedSlopeLength > 0.0f) return TrimmedSlopeLength;

	// Total slope length from ridge end to tail end
	float RiseTotal = (PitchRatio / 12.0f) * RunDistanceCm;
	float MainSlope = FMath::Sqrt(RunDistanceCm * RunDistanceCm + RiseTotal * RiseTotal);

	// Overhang along the slope, trimmed so rafter tail ends behind fascia outer face.
	// Fascia board (1x8) is 0.75" = 1.905cm thick, nailed to the rafter tail.
	// Subtract that thickness so the rafter end-grain doesn't poke past the fascia.
	float OverhangSlope = OverhangCm / FMath::Cos(GetPitchAngleRadians());
	const float FasciaThickness = 1.905f; // 3/4" fascia board
	OverhangSlope = FMath::Max(0.0f, OverhangSlope - FasciaThickness);

	return MainSlope + OverhangSlope;
}

void ARafter::SetPitch(float NewPitchRatio, float NewRunCm)
{
	PitchRatio = FMath::Clamp(NewPitchRatio, 1.0f, 24.0f);
	if (NewRunCm > 0.0f) RunDistanceCm = NewRunCm;

	UpdateRafterLength();
	RegenerateSockets();

	UE_LOG(LogTemp, Log, TEXT("Rafter: Pitch set to %.1f/12 (%.1f deg), Run=%.1fcm, Slope=%.1fcm"),
		PitchRatio, GetPitchAngleDegrees(), RunDistanceCm, GetSlopeLengthCm());
}

FString ARafter::GetRafterDisplayString() const
{
	return FString::Printf(TEXT("%.0f/12 pitch, %.0f\" run, %.1f\" slope"),
		PitchRatio, RunDistanceCm / 2.54f, GetSlopeLengthCm() / 2.54f);
}

void ARafter::ScalePiece(float ScaleDelta)
{
	// Rafters: scroll wheel disabled
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

FVector ARafter::GetTailEndWorldPosition() const
{
	// The tail end socket position in world space
	for (const FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("RafterTail"))
		{
			return GetActorTransform().TransformPosition(Socket.LocalPosition);
		}
	}
	return GetActorLocation();
}

void ARafter::CreateRidgeEndSocket()
{
	// Ridge end at actor origin. Mesh was exported with origin at the ridge end
	// (X=0 to 243.84), so no offset is needed — the min-X edge IS at (0,0,0).
	FConstructionSocket RidgeSocket;
	RidgeSocket.SocketName = FName(TEXT("RafterRidge"));
	RidgeSocket.SocketType = EConstructionSocketType::Rafter_Ridge;
	RidgeSocket.LocalPosition = FVector(0.0f, 0.0f, 0.0f);
	RidgeSocket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RidgeSocket.Orientation = ESocketOrientation::Any;
	RidgeSocket.bIsOccupied = false;
	Sockets.Add(RidgeSocket);
}

void ARafter::CreateBirdsmouthSocket()
{
	// Main slope from ridge to wall (without overhang)
	float RiseTotal = (PitchRatio / 12.0f) * RunDistanceCm;
	float MainSlope = FMath::Sqrt(RunDistanceCm * RunDistanceCm + RiseTotal * RiseTotal);

	// Birdsmouth is MainSlope distance from ridge end along the board (+X).
	// Mesh origin is at ridge end, so birdsmouth is simply at (MainSlope, 0, 0).
	FConstructionSocket BirdsmouthSocket;
	BirdsmouthSocket.SocketName = FName(TEXT("RafterBirdsmouth"));
	BirdsmouthSocket.SocketType = EConstructionSocketType::Rafter_BirdsMouth;
	BirdsmouthSocket.LocalPosition = FVector(MainSlope, 0.0f, 0.0f);
	BirdsmouthSocket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	BirdsmouthSocket.Orientation = ESocketOrientation::Any;
	BirdsmouthSocket.bIsOccupied = false;
	Sockets.Add(BirdsmouthSocket);
}

void ARafter::CreateTailEndSocket()
{
	float SlopeLen = GetSlopeLengthCm();

	// Tail end at SlopeLength from ridge end along +X.
	// Mesh origin is at ridge end, so tail is at (SlopeLen, 0, 0).
	FConstructionSocket TailSocket;
	TailSocket.SocketName = FName(TEXT("RafterTail"));
	TailSocket.SocketType = EConstructionSocketType::Rafter_Tail;
	TailSocket.LocalPosition = FVector(SlopeLen, 0.0f, 0.0f);
	TailSocket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	TailSocket.Orientation = ESocketOrientation::Any;
	TailSocket.bIsOccupied = false;
	Sockets.Add(TailSocket);
}

void ARafter::CreateTopFaceSocket()
{
	// Multiple top face sockets along the rafter slope so roof sheathing
	// can snap from anywhere along the rafter, not just the midpoint.
	float SlopeLen = GetSlopeLengthCm();

	const float Fractions[] = { 0.25f, 0.5f, 0.75f };
	for (int32 i = 0; i < 3; i++)
	{
		FName SocketName = FName(*FString::Printf(TEXT("RafterTopFace_%d"), i));
		FConstructionSocket TopFaceSocket;
		TopFaceSocket.SocketName = SocketName;
		TopFaceSocket.SocketType = EConstructionSocketType::Rafter_Top_Face;
		TopFaceSocket.LocalPosition = FVector(SlopeLen * Fractions[i], 0.0f, 0.0f);
		TopFaceSocket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
		TopFaceSocket.Orientation = ESocketOrientation::Any;
		TopFaceSocket.bIsOccupied = false;
		Sockets.Add(TopFaceSocket);
	}
}

void ARafter::RegenerateSockets()
{
	Sockets.Empty();

	CreateRidgeEndSocket();
	CreateBirdsmouthSocket();
	CreateTailEndSocket();
	CreateTopFaceSocket();

	UE_LOG(LogTemp, Log, TEXT("Rafter: Regenerated %d sockets"), Sockets.Num());
}

void ARafter::UpdateRafterLength()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;
	if (MeshDefaultLength < 1.0f) return;

	float SlopeLen = GetSlopeLengthCm();
	float XScale = SlopeLen / MeshDefaultLength;
	MeshComponent->SetRelativeScale3D(FVector(XScale, 1.0f, 1.0f));

	// Diagnostic: log mesh bounds to verify the mesh origin.
	// Re-exported mesh should have: Origin.X ≈ 121.92, Extent.X ≈ 121.92, MinX ≈ 0
	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	float MeshMinX = Bounds.Origin.X - Bounds.BoxExtent.X;

	UE_LOG(LogTemp, Error, TEXT(">>> RAFTER MESH BOUNDS: Origin.X=%.2f Extent.X=%.2f MinX=%.2f"),
		Bounds.Origin.X, Bounds.BoxExtent.X, MeshMinX);

	// Mesh was re-exported from Rhino with origin at the ridge end (MinX=0).
	// No offset needed — ridge end IS at actor origin.
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);

	UE_LOG(LogTemp, Warning, TEXT("Rafter: XScale=%.3f, SlopeLen=%.1fcm, MeshDefault=%.1fcm, RelLoc=(0,0,0)"),
		XScale, SlopeLen, MeshDefaultLength);
}

void ARafter::TrimToFasciaFace(float FasciaFaceWorldZ, const FVector& FasciaLocation, const FVector& FasciaForward)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;
	if (MeshDefaultLength < 1.0f) return;

	// Calculate where the fascia face intersects this rafter's slope line.
	// The rafter extends from actor origin (ridge end) along +X rotated by yaw/pitch.
	// We want to find the distance along the rafter where it hits the fascia plane.

	// Get the rafter's slope direction in world space
	FVector RafterDir = GetActorRotation().RotateVector(FVector::ForwardVector);

	// Project fascia location onto rafter line to find trim distance
	FVector RafterOrigin = GetActorLocation();
	FVector ToFascia = FasciaLocation - RafterOrigin;
	float TrimDist = FVector::DotProduct(ToFascia, RafterDir);

	// Add fascia board thickness (1.905cm = 3/4") so rafter ends at fascia back face
	const float FasciaThickness = 1.905f;
	TrimDist += FasciaThickness;

	if (TrimDist < 10.0f || TrimDist > GetSlopeLengthCm()) return; // Sanity check

	// Re-scale mesh X to the trimmed length
	float NewXScale = TrimDist / MeshDefaultLength;
	FVector CurrentScale3D = MeshComponent->GetRelativeScale3D();
	MeshComponent->SetRelativeScale3D(FVector(NewXScale, CurrentScale3D.Y, CurrentScale3D.Z));

	// Update tail socket position
	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("RafterTail"))
		{
			Socket.LocalPosition.X = TrimDist;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("Rafter trimmed: OldSlope=%.1f NewLength=%.1f XScale=%.3f"),
		GetSlopeLengthCm(), TrimDist, NewXScale);
}

void ARafter::ReplaceWithProceduralPlumbCutRafter(
	float TrimDistanceAlongSlopeCm,
	float PitchAngleDegrees,
	float RafterWidthCm,
	float RafterDepthCm)
{
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("PlumbCutRafter: MeshComponent is null."));
		return;
	}

	TrimDistanceAlongSlopeCm = FMath::Max(TrimDistanceAlongSlopeCm, 1.0f);
	RafterWidthCm = FMath::Max(RafterWidthCm, 0.1f);
	RafterDepthCm = FMath::Max(RafterDepthCm, 0.1f);

	const float PitchRad = FMath::DegreesToRadians(PitchAngleDegrees);
	const float TanPitch = FMath::Tan(PitchRad);

	const float HalfWidth = RafterWidthCm * 0.5f;
	const float HalfDepth = RafterDepthCm * 0.5f;

	const float ZTop = +HalfDepth;
	const float ZBottom = -HalfDepth;

	auto XOnPlumbCut = [TanPitch](float CutStationCm, float LocalZ)
	{
		return CutStationCm - LocalZ * TanPitch;
	};

	const float StartStation = 0.0f;
	const float EndStation = TrimDistanceAlongSlopeCm;

	const float StartTopX = XOnPlumbCut(StartStation, ZTop);
	const float StartBottomX = XOnPlumbCut(StartStation, ZBottom);
	const float EndTopX = XOnPlumbCut(EndStation, ZTop);
	const float EndBottomX = XOnPlumbCut(EndStation, ZBottom);

	const FVector SBL(StartBottomX, -HalfWidth, ZBottom);
	const FVector SBR(StartBottomX, +HalfWidth, ZBottom);
	const FVector STL(StartTopX,    -HalfWidth, ZTop);
	const FVector STR(StartTopX,    +HalfWidth, ZTop);

	const FVector EBL(EndBottomX, -HalfWidth, ZBottom);
	const FVector EBR(EndBottomX, +HalfWidth, ZBottom);
	const FVector ETL(EndTopX,    -HalfWidth, ZTop);
	const FVector ETR(EndTopX,    +HalfWidth, ZTop);

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	Vertices.Reserve(24);
	Triangles.Reserve(36);
	Normals.Reserve(24);
	UV0.Reserve(24);
	VertexColors.Reserve(24);
	Tangents.Reserve(24);

	const float UVTileCm = 30.48f;

	auto AddQuad = [&](
		FVector P0, FVector P1, FVector P2, FVector P3,
		const FVector& DesiredNormal)
	{
		FVector N = DesiredNormal.GetSafeNormal();
		if (N.IsNearlyZero()) N = FVector::UpVector;

		const FVector CurrentNormal = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
		if (FVector::DotProduct(CurrentNormal, N) < 0.0f) Swap(P1, P3);

		const int32 BaseIndex = Vertices.Num();
		Vertices.Add(P0);
		Vertices.Add(P1);
		Vertices.Add(P2);
		Vertices.Add(P3);

		Triangles.Add(BaseIndex + 0);
		Triangles.Add(BaseIndex + 1);
		Triangles.Add(BaseIndex + 2);
		Triangles.Add(BaseIndex + 0);
		Triangles.Add(BaseIndex + 2);
		Triangles.Add(BaseIndex + 3);

		const float USize = FVector::Distance(P0, P1) / UVTileCm;
		const float VSize = FVector::Distance(P0, P3) / UVTileCm;
		UV0.Add(FVector2D(0.0f, 0.0f));
		UV0.Add(FVector2D(USize, 0.0f));
		UV0.Add(FVector2D(USize, VSize));
		UV0.Add(FVector2D(0.0f, VSize));

		for (int32 i = 0; i < 4; ++i)
		{
			Normals.Add(N);
			VertexColors.Add(FLinearColor::White);
		}

		FVector TangentX = (P1 - P0).GetSafeNormal();
		if (TangentX.IsNearlyZero()) TangentX = FVector::CrossProduct(FVector::UpVector, N).GetSafeNormal();
		if (TangentX.IsNearlyZero()) TangentX = FVector::ForwardVector;

		for (int32 i = 0; i < 4; ++i)
		{
			Tangents.Add(FProcMeshTangent(TangentX, false));
		}
	};

	const FVector TopNormal(0.0f, 0.0f, +1.0f);
	const FVector BottomNormal(0.0f, 0.0f, -1.0f);
	const FVector LeftNormal(0.0f, -1.0f, 0.0f);
	const FVector RightNormal(0.0f, +1.0f, 0.0f);
	const FVector PlumbFaceNormalLocal(FMath::Cos(PitchRad), 0.0f, FMath::Sin(PitchRad));

	AddQuad(STL, STR, ETR, ETL, TopNormal);
	AddQuad(SBL, EBL, EBR, SBR, BottomNormal);
	AddQuad(SBL, STL, ETL, EBL, LeftNormal);
	AddQuad(SBR, EBR, ETR, STR, RightNormal);
	AddQuad(SBL, SBR, STR, STL, -PlumbFaceNormalLocal);
	AddQuad(EBL, ETL, ETR, EBR, PlumbFaceNormalLocal);

	if (!ProceduralRafterMesh)
	{
		ProceduralRafterMesh = NewObject<UProceduralMeshComponent>(this, TEXT("ProceduralRafterMesh"));
		ProceduralRafterMesh->CreationMethod = EComponentCreationMethod::Instance;
		ProceduralRafterMesh->bUseAsyncCooking = true;
		AddInstanceComponent(ProceduralRafterMesh);

		USceneComponent* Parent = MeshComponent->GetAttachParent();
		if (!Parent) Parent = RootComponent;
		if (Parent)
		{
			ProceduralRafterMesh->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		}
		ProceduralRafterMesh->RegisterComponent();
	}

	ProceduralRafterMesh->ClearAllMeshSections();
	ProceduralRafterMesh->SetRelativeLocation(MeshComponent->GetRelativeLocation());
	ProceduralRafterMesh->SetRelativeRotation(MeshComponent->GetRelativeRotation());
	ProceduralRafterMesh->SetRelativeScale3D(FVector::OneVector);

	ProceduralRafterMesh->CreateMeshSection_LinearColor(
		0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, true, true);

	UMaterialInterface* ExistingMaterial = MeshComponent->GetMaterial(0);
	if (ExistingMaterial) ProceduralRafterMesh->SetMaterial(0, ExistingMaterial);

	ProceduralRafterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ProceduralRafterMesh->SetCollisionObjectType(ECC_WorldStatic);

	MeshComponent->SetVisibility(false, false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UE_LOG(LogTemp, Warning,
		TEXT("PlumbCutRafter: Trim=%.2fcm Pitch=%.2fdeg Tan=%.4f TopEndX=%.2f BottomEndX=%.2f Difference=%.2fcm"),
		TrimDistanceAlongSlopeCm, PitchAngleDegrees, TanPitch,
		EndTopX, EndBottomX, EndBottomX - EndTopX);
}

bool ARafter::ComputeCutStationFromFasciaPlane(
	const FVector& FasciaBackFaceWorldPoint,
	const FVector& FasciaBackFaceWorldNormal,
	float& OutCutStationCm) const
{
	if (!MeshComponent) return false;

	const FTransform MeshWorld = MeshComponent->GetComponentTransform();
	const FVector LocalPlanePoint = MeshWorld.InverseTransformPosition(FasciaBackFaceWorldPoint);
	const FVector LocalPlaneNormal = MeshWorld.InverseTransformVectorNoScale(FasciaBackFaceWorldNormal).GetSafeNormal();

	if (FMath::Abs(LocalPlaneNormal.X) < KINDA_SMALL_NUMBER) return false;

	OutCutStationCm = FVector::DotProduct(LocalPlanePoint, LocalPlaneNormal) / LocalPlaneNormal.X;
	return OutCutStationCm > 0.0f;
}
