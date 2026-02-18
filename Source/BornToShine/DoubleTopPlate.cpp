// Born To Shine - Double Top Plate Implementation

#include "DoubleTopPlate.h"
#include "Components/StaticMeshComponent.h"

ADoubleTopPlate::ADoubleTopPlate()
{
	PieceType = EPieceType::DoubleTopPlate;

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

void ADoubleTopPlate::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("DoubleTopPlate: BeginPlay - %d ft (%.1f cm), Total sockets: %d"),
		CurrentLengthFeet, BoardLength, Sockets.Num());
}

void ADoubleTopPlate::InitializeSockets()
{
	Sockets.Empty();

	CreateBottomSockets();
	CreateEndSockets();

	UE_LOG(LogTemp, Log, TEXT("DoubleTopPlate: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ADoubleTopPlate::CreateBottomSockets()
{
	float HalfLen = BoardLength / 2.0f;
	float SocketZ = -BoardHeight / 2.0f;

	FConstructionSocket LeftBottom;
	LeftBottom.SocketName = FName(TEXT("DblTopBottom_Left"));
	LeftBottom.SocketType = EConstructionSocketType::DoubleTopPlate_Bottom;
	LeftBottom.LocalPosition = FVector(-HalfLen, 0.0f, SocketZ);
	LeftBottom.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
	LeftBottom.bIsOccupied = false;
	Sockets.Add(LeftBottom);

	FConstructionSocket RightBottom;
	RightBottom.SocketName = FName(TEXT("DblTopBottom_Right"));
	RightBottom.SocketType = EConstructionSocketType::DoubleTopPlate_Bottom;
	RightBottom.LocalPosition = FVector(HalfLen, 0.0f, SocketZ);
	RightBottom.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
	RightBottom.bIsOccupied = false;
	Sockets.Add(RightBottom);

	FConstructionSocket CenterBottom;
	CenterBottom.SocketName = FName(TEXT("DblTopBottom_Center"));
	CenterBottom.SocketType = EConstructionSocketType::DoubleTopPlate_Bottom;
	CenterBottom.LocalPosition = FVector(0.0f, 0.0f, SocketZ);
	CenterBottom.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
	CenterBottom.bIsOccupied = false;
	Sockets.Add(CenterBottom);
}

void ADoubleTopPlate::CreateEndSockets()
{
	float HalfLen = BoardLength / 2.0f;

	FConstructionSocket LeftEnd;
	LeftEnd.SocketName = FName(TEXT("DblTopEnd_Left"));
	LeftEnd.SocketType = EConstructionSocketType::DoubleTopPlate_End;
	LeftEnd.LocalPosition = FVector(-HalfLen, 0.0f, 0.0f);
	LeftEnd.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	LeftEnd.bIsOccupied = false;
	Sockets.Add(LeftEnd);

	FConstructionSocket RightEnd;
	RightEnd.SocketName = FName(TEXT("DblTopEnd_Right"));
	RightEnd.SocketType = EConstructionSocketType::DoubleTopPlate_End;
	RightEnd.LocalPosition = FVector(HalfLen, 0.0f, 0.0f);
	RightEnd.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RightEnd.bIsOccupied = false;
	Sockets.Add(RightEnd);
}

void ADoubleTopPlate::SetBoardLengthFeet(int32 LengthInFeet)
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

		UE_LOG(LogTemp, Log, TEXT("DoubleTopPlate: Length changed to %s"),
			*GetLengthDisplayString());
	}
}

void ADoubleTopPlate::SetBoardLengthCm(float LengthCm)
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

	UE_LOG(LogTemp, Log, TEXT("DoubleTopPlate: Length set to %.1f cm (~%d ft)"),
		BoardLength, CurrentLengthFeet);
}

int32 ADoubleTopPlate::GetBoardLengthFeet() const
{
	return CurrentLengthFeet;
}

FString ADoubleTopPlate::GetLengthDisplayString() const
{
	return FString::Printf(TEXT("%d ft (%.1f cm)"), CurrentLengthFeet, BoardLength);
}

void ADoubleTopPlate::ScalePiece(float ScaleDelta)
{
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ADoubleTopPlate::RegenerateSockets()
{
	Sockets.Empty();

	CreateBottomSockets();
	CreateEndSockets();

	UE_LOG(LogTemp, Log, TEXT("DoubleTopPlate: Regenerated %d sockets for %d ft plate"), Sockets.Num(), CurrentLengthFeet);
}
