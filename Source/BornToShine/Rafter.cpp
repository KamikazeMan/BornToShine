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
	// Total slope length from ridge end to tail end
	float RiseTotal = (PitchRatio / 12.0f) * RunDistanceCm;
	float MainSlope = FMath::Sqrt(RunDistanceCm * RunDistanceCm + RiseTotal * RiseTotal);

	// Add overhang along the slope
	float OverhangSlope = OverhangCm / FMath::Cos(GetPitchAngleRadians());

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
	float SlopeLen = GetSlopeLengthCm();
	float HalfSlope = SlopeLen / 2.0f;

	// Ridge end at the -X end of the board
	FConstructionSocket RidgeSocket;
	RidgeSocket.SocketName = FName(TEXT("RafterRidge"));
	RidgeSocket.SocketType = EConstructionSocketType::Rafter_Ridge;
	RidgeSocket.LocalPosition = FVector(-HalfSlope, 0.0f, 0.0f);
	RidgeSocket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RidgeSocket.Orientation = ESocketOrientation::Any;
	RidgeSocket.bIsOccupied = false;
	Sockets.Add(RidgeSocket);
}

void ARafter::CreateBirdsmouthSocket()
{
	float SlopeLen = GetSlopeLengthCm();
	float HalfSlope = SlopeLen / 2.0f;

	// Main slope from ridge to wall (without overhang)
	float RiseTotal = (PitchRatio / 12.0f) * RunDistanceCm;
	float MainSlope = FMath::Sqrt(RunDistanceCm * RunDistanceCm + RiseTotal * RiseTotal);

	// Birdsmouth is MainSlope from the ridge end along the board
	float BirdsmouthX = -HalfSlope + MainSlope;

	FConstructionSocket BirdsmouthSocket;
	BirdsmouthSocket.SocketName = FName(TEXT("RafterBirdsmouth"));
	BirdsmouthSocket.SocketType = EConstructionSocketType::Rafter_BirdsMouth;
	BirdsmouthSocket.LocalPosition = FVector(BirdsmouthX, 0.0f, 0.0f);
	BirdsmouthSocket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	BirdsmouthSocket.Orientation = ESocketOrientation::Any;
	BirdsmouthSocket.bIsOccupied = false;
	Sockets.Add(BirdsmouthSocket);
}

void ARafter::CreateTailEndSocket()
{
	float SlopeLen = GetSlopeLengthCm();
	float HalfSlope = SlopeLen / 2.0f;

	// Tail end at the +X end of the board
	FConstructionSocket TailSocket;
	TailSocket.SocketName = FName(TEXT("RafterTail"));
	TailSocket.SocketType = EConstructionSocketType::Rafter_Tail;
	TailSocket.LocalPosition = FVector(HalfSlope, 0.0f, 0.0f);
	TailSocket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	TailSocket.Orientation = ESocketOrientation::Any;
	TailSocket.bIsOccupied = false;
	Sockets.Add(TailSocket);
}

void ARafter::RegenerateSockets()
{
	Sockets.Empty();

	CreateRidgeEndSocket();
	CreateBirdsmouthSocket();
	CreateTailEndSocket();

	UE_LOG(LogTemp, Log, TEXT("Rafter: Regenerated %d sockets"), Sockets.Num());
}

void ARafter::UpdateRafterLength()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;
	if (MeshDefaultLength < 1.0f) return;

	float SlopeLen = GetSlopeLengthCm();
	float XScale = SlopeLen / MeshDefaultLength;
	MeshComponent->SetRelativeScale3D(FVector(XScale, 1.0f, 1.0f));

	// Center the mesh on the SceneRoot (mesh origin assumed at center)
	MeshComponent->SetRelativeLocation(FVector::ZeroVector);

	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);

	UE_LOG(LogTemp, Log, TEXT("Rafter: XScale=%.3f, SlopeLength=%.1fcm, MeshDefault=%.1fcm"),
		XScale, SlopeLen, MeshDefaultLength);
}
