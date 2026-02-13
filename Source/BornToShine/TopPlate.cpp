// Born To Shine - Top Plate Implementation

#include "TopPlate.h"
#include "Components/StaticMeshComponent.h"

ATopPlate::ATopPlate()
{
	PieceType = EPieceType::TopPlate;

	// 2x4 lumber actual dimensions (same as bottom plate)
	BoardWidth = 3.81f;   // 1.5 inches
	BoardHeight = 8.89f;  // 3.5 inches
	CurrentLengthFeet = 8;
	BoardLength = CurrentLengthFeet * 30.48f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ATopPlate::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("TopPlate: BeginPlay - %d ft (%.1f cm), Total sockets: %d"),
		CurrentLengthFeet, BoardLength, Sockets.Num());
}

void ATopPlate::InitializeSockets()
{
	Sockets.Empty();

	CreateBottomSockets();
	CreateEndSockets();
	CreateTopFaceSockets();

	UE_LOG(LogTemp, Log, TEXT("TopPlate: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ATopPlate::CreateBottomSockets()
{
	float HalfLen = BoardLength / 2.0f;
	float SocketZ = -BoardHeight / 2.0f;

	// End bottom sockets
	{
		FConstructionSocket LeftBottom;
		LeftBottom.SocketName = FName(TEXT("TopPlateBottom_Left"));
		LeftBottom.SocketType = EConstructionSocketType::TopPlate_Bottom;
		LeftBottom.LocalPosition = FVector(-HalfLen, 0.0f, SocketZ);
		LeftBottom.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
		LeftBottom.bIsOccupied = false;
		Sockets.Add(LeftBottom);

		FConstructionSocket RightBottom;
		RightBottom.SocketName = FName(TEXT("TopPlateBottom_Right"));
		RightBottom.SocketType = EConstructionSocketType::TopPlate_Bottom;
		RightBottom.LocalPosition = FVector(HalfLen, 0.0f, SocketZ);
		RightBottom.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
		RightBottom.bIsOccupied = false;
		Sockets.Add(RightBottom);
	}

	// Center bottom socket
	{
		FConstructionSocket CenterBottom;
		CenterBottom.SocketName = FName(TEXT("TopPlateBottom_Center"));
		CenterBottom.SocketType = EConstructionSocketType::TopPlate_Bottom;
		CenterBottom.LocalPosition = FVector(0.0f, 0.0f, SocketZ);
		CenterBottom.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
		CenterBottom.bIsOccupied = false;
		Sockets.Add(CenterBottom);
	}
}

void ATopPlate::CreateEndSockets()
{
	float HalfLen = BoardLength / 2.0f;

	FConstructionSocket LeftEnd;
	LeftEnd.SocketName = FName(TEXT("TopPlateEnd_Left"));
	LeftEnd.SocketType = EConstructionSocketType::TopPlate_End;
	LeftEnd.LocalPosition = FVector(-HalfLen, 0.0f, 0.0f);
	LeftEnd.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	LeftEnd.bIsOccupied = false;
	Sockets.Add(LeftEnd);

	FConstructionSocket RightEnd;
	RightEnd.SocketName = FName(TEXT("TopPlateEnd_Right"));
	RightEnd.SocketType = EConstructionSocketType::TopPlate_End;
	RightEnd.LocalPosition = FVector(HalfLen, 0.0f, 0.0f);
	RightEnd.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RightEnd.bIsOccupied = false;
	Sockets.Add(RightEnd);
}

void ATopPlate::CreateTopFaceSockets()
{
	float HalfLen = BoardLength / 2.0f;
	float SocketZ = BoardHeight / 2.0f;

	// Top face sockets for DoubleTopPlate (at ends + center)
	{
		FConstructionSocket LeftTop;
		LeftTop.SocketName = FName(TEXT("TopPlateTop_Left"));
		LeftTop.SocketType = EConstructionSocketType::TopPlate_Top;
		LeftTop.LocalPosition = FVector(-HalfLen, 0.0f, SocketZ);
		LeftTop.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f);
		LeftTop.bIsOccupied = false;
		Sockets.Add(LeftTop);

		FConstructionSocket RightTop;
		RightTop.SocketName = FName(TEXT("TopPlateTop_Right"));
		RightTop.SocketType = EConstructionSocketType::TopPlate_Top;
		RightTop.LocalPosition = FVector(HalfLen, 0.0f, SocketZ);
		RightTop.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f);
		RightTop.bIsOccupied = false;
		Sockets.Add(RightTop);

		FConstructionSocket CenterTop;
		CenterTop.SocketName = FName(TEXT("TopPlateTop_Center"));
		CenterTop.SocketType = EConstructionSocketType::TopPlate_Top;
		CenterTop.LocalPosition = FVector(0.0f, 0.0f, SocketZ);
		CenterTop.LocalRotation = FRotator(-90.0f, 0.0f, 0.0f);
		CenterTop.bIsOccupied = false;
		Sockets.Add(CenterTop);
	}
}

void ATopPlate::SetBoardLengthFeet(int32 LengthInFeet)
{
	LengthInFeet = FMath::Clamp(LengthInFeet, MinLengthFeet, MaxLengthFeet);

	if (LengthInFeet != CurrentLengthFeet)
	{
		float OldLength = BoardLength;
		CurrentLengthFeet = LengthInFeet;
		BoardLength = CurrentLengthFeet * 30.48f;
		bMeshExtended = false;

		if (MeshComponent && OldLength > 0.0f)
		{
			FVector S = MeshComponent->GetRelativeScale3D();
			float Ratio = BoardLength / OldLength;
			MeshComponent->SetRelativeScale3D(FVector(S.X * Ratio, S.Y, S.Z));
		}

		RegenerateSockets();

		UE_LOG(LogTemp, Log, TEXT("TopPlate: Length changed to %s"),
			*GetLengthDisplayString());
	}
}

void ATopPlate::SetBoardLengthCm(float LengthCm)
{
	const float MinCm = MinLengthFeet * 30.48f;
	const float MaxCm = MaxLengthFeet * 30.48f;
	LengthCm = FMath::Clamp(LengthCm, MinCm, MaxCm);

	float OldLength = BoardLength;
	BoardLength = LengthCm;
	CurrentLengthFeet = FMath::RoundToInt(LengthCm / 30.48f);
	if (CurrentLengthFeet < MinLengthFeet) CurrentLengthFeet = MinLengthFeet;
	bMeshExtended = false;

	if (MeshComponent && OldLength > 0.0f)
	{
		FVector S = MeshComponent->GetRelativeScale3D();
		float Ratio = BoardLength / OldLength;
		MeshComponent->SetRelativeScale3D(FVector(S.X * Ratio, S.Y, S.Z));
	}

	RegenerateSockets();

	UE_LOG(LogTemp, Log, TEXT("TopPlate: Length set to %.1f cm (~%d ft)"),
		BoardLength, CurrentLengthFeet);
}

int32 ATopPlate::GetBoardLengthFeet() const
{
	return CurrentLengthFeet;
}

FString ATopPlate::GetLengthDisplayString() const
{
	return FString::Printf(TEXT("%d ft (%.1f cm)"), CurrentLengthFeet, BoardLength);
}

void ATopPlate::ScalePiece(float ScaleDelta)
{
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ATopPlate::RegenerateSockets()
{
	Sockets.Empty();

	CreateBottomSockets();
	CreateEndSockets();
	CreateTopFaceSockets();

	UE_LOG(LogTemp, Log, TEXT("TopPlate: Regenerated %d sockets for %d ft plate"), Sockets.Num(), CurrentLengthFeet);
}

void ATopPlate::ExtendMeshForFlushCorners()
{
	if (!MeshComponent) return;
	if (bMeshExtended) return;

	FVector CurrentScale3D = MeshComponent->GetRelativeScale3D();
	// Extend by 3.5" per end (BoardHeight) to cover perpendicular framing at corners
	float ExtensionCm = 2.0f * BoardHeight; // 2 * 8.89 = 17.78cm (3.5" per end)
	float Ratio = (BoardLength + ExtensionCm) / BoardLength;

	MeshComponent->SetRelativeScale3D(FVector(
		CurrentScale3D.X * Ratio,
		CurrentScale3D.Y,
		CurrentScale3D.Z
	));

	bMeshExtended = true;

	UE_LOG(LogTemp, Log, TEXT("TopPlate [%s]: ExtendMesh ratio=%.4f scale X: %.4f -> %.4f (ext=%.2fcm)"),
		*GetName(), Ratio, CurrentScale3D.X, CurrentScale3D.X * Ratio, ExtensionCm);
}
