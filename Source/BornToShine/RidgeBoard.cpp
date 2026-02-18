// Born To Shine - Ridge Board Implementation

#include "RidgeBoard.h"
#include "Components/StaticMeshComponent.h"

ARidgeBoard::ARidgeBoard()
{
	PieceType = EPieceType::RidgeBoard;

	// 2x8 lumber actual dimensions
	BoardWidth = 3.81f;    // 1.5 inches
	BoardHeight = 18.415f; // 7.25 inches

	// Default length: 8ft (matching typical building span)
	CurrentLengthFeet = 8;
	BoardLength = CurrentLengthFeet * 30.48f;

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

void ARidgeBoard::BeginPlay()
{
	Super::BeginPlay();

	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("RidgeBoard: BeginPlay - %d ft (%.1f cm), Height=%.2fcm, Sockets=%d"),
		CurrentLengthFeet, BoardLength, BoardHeight, Sockets.Num());
}

void ARidgeBoard::InitializeSockets()
{
	Sockets.Empty();

	CreateEndSockets();
	CreateSideSockets();

	UE_LOG(LogTemp, Log, TEXT("RidgeBoard: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void ARidgeBoard::CreateEndSockets()
{
	float HalfLen = BoardLength / 2.0f;

	// Left end socket — snaps into ridge post pocket
	FConstructionSocket LeftEnd;
	LeftEnd.SocketName = FName(TEXT("RidgeBoardEnd_Left"));
	LeftEnd.SocketType = EConstructionSocketType::RidgeBoard_End;
	LeftEnd.LocalPosition = FVector(-HalfLen, 0.0f, 0.0f);
	LeftEnd.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	LeftEnd.Orientation = ESocketOrientation::Horizontal;
	LeftEnd.bIsOccupied = false;
	Sockets.Add(LeftEnd);

	// Right end socket
	FConstructionSocket RightEnd;
	RightEnd.SocketName = FName(TEXT("RidgeBoardEnd_Right"));
	RightEnd.SocketType = EConstructionSocketType::RidgeBoard_End;
	RightEnd.LocalPosition = FVector(HalfLen, 0.0f, 0.0f);
	RightEnd.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RightEnd.Orientation = ESocketOrientation::Horizontal;
	RightEnd.bIsOccupied = false;
	Sockets.Add(RightEnd);
}

void ARidgeBoard::CreateSideSockets()
{
	// Create side sockets at 16" OC (40.64cm) intervals for rafter attachment.
	// Socket count scales with board length — works for any size building.
	const float Spacing = 40.64f; // 16" OC
	int32 NumSockets = FMath::Max(2, FMath::FloorToInt(BoardLength / Spacing) + 1);

	// Center the socket positions on the board
	float TotalSpan = (NumSockets - 1) * Spacing;
	float StartX = -TotalSpan / 2.0f;

	// Rafter mesh bottom is at actor origin (Z=0), top at +RafterDepth.
	// We want rafter top flush with ridge board top, so:
	//   SnapZ + RafterDepth = RidgeBoardHalfHeight  →  SnapZ = HalfHeight - RafterDepth
	const float RafterDepth = 13.97f; // 5.5" = 13.97cm
	float SideSocketLocalZ = (BoardHeight / 2.0f) - RafterDepth; // 9.21 - 13.97 = -4.76cm

	for (int32 i = 0; i < NumSockets; i++)
	{
		float XPos = StartX + i * Spacing;

		// Left side (negative Y)
		FConstructionSocket LeftSide;
		LeftSide.SocketName = FName(*FString::Printf(TEXT("RidgeBoardSide_L%d"), i));
		LeftSide.SocketType = EConstructionSocketType::RidgeBoard_Side;
		LeftSide.LocalPosition = FVector(XPos, -BoardWidth / 2.0f, SideSocketLocalZ);
		LeftSide.LocalRotation = FRotator(0.0f, -90.0f, 0.0f); // Facing left
		LeftSide.Orientation = ESocketOrientation::Horizontal;
		LeftSide.bIsOccupied = false;
		Sockets.Add(LeftSide);

		// Right side (positive Y)
		FConstructionSocket RightSide;
		RightSide.SocketName = FName(*FString::Printf(TEXT("RidgeBoardSide_R%d"), i));
		RightSide.SocketType = EConstructionSocketType::RidgeBoard_Side;
		RightSide.LocalPosition = FVector(XPos, BoardWidth / 2.0f, SideSocketLocalZ);
		RightSide.LocalRotation = FRotator(0.0f, 90.0f, 0.0f); // Facing right
		RightSide.Orientation = ESocketOrientation::Horizontal;
		RightSide.bIsOccupied = false;
		Sockets.Add(RightSide);
	}
}

void ARidgeBoard::SetBoardLengthCm(float LengthCm)
{
	const float MinCm = MinLengthFeet * 30.48f;
	const float MaxCm = MaxLengthFeet * 30.48f;
	LengthCm = FMath::Clamp(LengthCm, MinCm, MaxCm);

	if (FMath::IsNearlyEqual(LengthCm, BoardLength, 0.01f))
		return;

	float OldLength = BoardLength;
	BoardLength = LengthCm;
	CurrentLengthFeet = FMath::RoundToInt(LengthCm / 30.48f);
	if (CurrentLengthFeet < MinLengthFeet) CurrentLengthFeet = MinLengthFeet;

	if (MeshComponent && OldLength > 0.0f)
	{
		FVector S = MeshComponent->GetRelativeScale3D();
		float Ratio = BoardLength / OldLength;
		MeshComponent->SetRelativeScale3D(FVector(S.X * Ratio, S.Y, S.Z));
	}

	RegenerateSockets();

	UE_LOG(LogTemp, Log, TEXT("RidgeBoard: Length set to %.1f cm (~%d ft)"),
		BoardLength, CurrentLengthFeet);
}

void ARidgeBoard::SetBoardLengthFeet(int32 LengthInFeet)
{
	LengthInFeet = FMath::Clamp(LengthInFeet, MinLengthFeet, MaxLengthFeet);

	if (LengthInFeet != CurrentLengthFeet)
	{
		float OldLength = BoardLength;
		CurrentLengthFeet = LengthInFeet;
		BoardLength = CurrentLengthFeet * 30.48f;

		if (MeshComponent && OldLength > 0.0f)
		{
			FVector S = MeshComponent->GetRelativeScale3D();
			float Ratio = BoardLength / OldLength;
			MeshComponent->SetRelativeScale3D(FVector(S.X * Ratio, S.Y, S.Z));
		}

		RegenerateSockets();

		UE_LOG(LogTemp, Log, TEXT("RidgeBoard: Length changed to %s"),
			*GetLengthDisplayString());
	}
}

int32 ARidgeBoard::GetBoardLengthFeet() const
{
	return CurrentLengthFeet;
}

FString ARidgeBoard::GetLengthDisplayString() const
{
	return FString::Printf(TEXT("%d ft (%.1f cm)"), CurrentLengthFeet, BoardLength);
}

void ARidgeBoard::ScalePiece(float ScaleDelta)
{
	// Ridge board: scroll wheel scaling disabled
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void ARidgeBoard::RegenerateSockets()
{
	Sockets.Empty();

	CreateEndSockets();
	CreateSideSockets();

	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("RidgeBoard: Regenerated %d sockets for %d ft board"), Sockets.Num(), CurrentLengthFeet);
}

void ARidgeBoard::AdjustSocketsToMeshBounds()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	FVector MeshRelLoc = MeshComponent->GetRelativeLocation();
	FVector MeshScale = MeshComponent->GetRelativeScale3D();

	float MeshBottomZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * MeshScale.Z + MeshRelLoc.Z;
	float MeshTopZ    = (Bounds.Origin.Z + Bounds.BoxExtent.Z) * MeshScale.Z + MeshRelLoc.Z;
	float MeshCenterZ = (MeshBottomZ + MeshTopZ) / 2.0f;
	float ActualHeight = MeshTopZ - MeshBottomZ;

	if (ActualHeight < 0.1f) return;

	BoardHeight = ActualHeight;

	// Adjust end sockets to mesh center Z (they sit in ridge post pockets)
	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketType == EConstructionSocketType::RidgeBoard_End)
		{
			Socket.LocalPosition.Z = MeshCenterZ;
		}
		else if (Socket.SocketType == EConstructionSocketType::RidgeBoard_Side)
		{
			// Rafter mesh bottom is at actor origin, top at +RafterDepth.
			// Place snap point so rafter top aligns with ridge board top:
			//   SnapZ + RafterDepth = MeshTopZ  →  SnapZ = MeshTopZ - RafterDepth
			const float RafterDepth = 13.97f; // 5.5" = 13.97cm
			Socket.LocalPosition.Z = MeshTopZ - RafterDepth;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("RidgeBoard: AdjustSockets - MeshZ=[%.2f, %.2f] height=%.2fcm center=%.2f"),
		MeshBottomZ, MeshTopZ, ActualHeight, MeshCenterZ);
}
