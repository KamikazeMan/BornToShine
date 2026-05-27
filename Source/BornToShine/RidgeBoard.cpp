// Born To Shine - Ridge Board Implementation

#include "RidgeBoard.h"
#include "Components/StaticMeshComponent.h"
#include "RoofGridComponent.h"
#include "ConstructionPhaseManager.h"
#include "Rafter.h"
#include "FasciaBoard.h"

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

	RoofGrid = CreateDefaultSubobject<URoofGridComponent>(TEXT("RoofGrid"));
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
	const float Spacing = 40.64f; // 16" OC
	const float GableOverhangCm = 30.48f; // 12" per side
	float HalfLen = BoardLength / 2.0f;

	// Building span between ridge posts (exclude overhang)
	float BuildingSpan = BoardLength - (GableOverhangCm * 2.0f);
	float BuildingStartX = -HalfLen + GableOverhangCm;

	// If board doesn't have overhangs, use full length
	if (BuildingSpan < Spacing)
	{
		BuildingStartX = -HalfLen;
		BuildingSpan = BoardLength;
	}

	// Center sockets within the building span (matching how wall studs are centered)
	int32 NumSockets = FMath::Max(2, FMath::FloorToInt(BuildingSpan / Spacing) + 1);
	float TotalSpan = (NumSockets - 1) * Spacing;
	float SpanCenterX = BuildingStartX + BuildingSpan / 2.0f;
	float StartX = SpanCenterX - TotalSpan / 2.0f;

	float SideSocketLocalZ = BoardHeight / 2.0f;

	for (int32 i = 0; i < NumSockets; i++)
	{
		float XPos = StartX + i * Spacing;

		// Left side (negative Y)
		FConstructionSocket LeftSide;
		LeftSide.SocketName = FName(*FString::Printf(TEXT("RidgeBoardSide_L%d"), i));
		LeftSide.SocketType = EConstructionSocketType::RidgeBoard_Side;
		LeftSide.LocalPosition = FVector(XPos, -BoardWidth / 2.0f, SideSocketLocalZ);
		LeftSide.LocalRotation = FRotator(0.0f, -90.0f, 0.0f);
		LeftSide.Orientation = ESocketOrientation::Horizontal;
		LeftSide.bIsOccupied = false;
		Sockets.Add(LeftSide);

		// Right side (positive Y)
		FConstructionSocket RightSide;
		RightSide.SocketName = FName(*FString::Printf(TEXT("RidgeBoardSide_R%d"), i));
		RightSide.SocketType = EConstructionSocketType::RidgeBoard_Side;
		RightSide.LocalPosition = FVector(XPos, BoardWidth / 2.0f, SideSocketLocalZ);
		RightSide.LocalRotation = FRotator(0.0f, 90.0f, 0.0f);
		RightSide.Orientation = ESocketOrientation::Horizontal;
		RightSide.bIsOccupied = false;
		Sockets.Add(RightSide);
	}

	// --- END SOCKETS for gable-end rafters ---
	// Add sockets at the very ends of the ridge board (at the overhang tips)
	// so rafters can attach at the gable ends.
	float EndLeftX = -HalfLen;
	float EndRightX = HalfLen;

	{
		FConstructionSocket LeftSideEnd;
		LeftSideEnd.SocketName = FName(TEXT("RidgeBoardSide_EndL_Left"));
		LeftSideEnd.SocketType = EConstructionSocketType::RidgeBoard_Side;
		LeftSideEnd.LocalPosition = FVector(EndLeftX, -BoardWidth / 2.0f, SideSocketLocalZ);
		LeftSideEnd.LocalRotation = FRotator(0.0f, -90.0f, 0.0f);
		LeftSideEnd.Orientation = ESocketOrientation::Horizontal;
		LeftSideEnd.bIsOccupied = false;
		Sockets.Add(LeftSideEnd);

		FConstructionSocket RightSideEnd;
		RightSideEnd.SocketName = FName(TEXT("RidgeBoardSide_EndL_Right"));
		RightSideEnd.SocketType = EConstructionSocketType::RidgeBoard_Side;
		RightSideEnd.LocalPosition = FVector(EndLeftX, BoardWidth / 2.0f, SideSocketLocalZ);
		RightSideEnd.LocalRotation = FRotator(0.0f, 90.0f, 0.0f);
		RightSideEnd.Orientation = ESocketOrientation::Horizontal;
		RightSideEnd.bIsOccupied = false;
		Sockets.Add(RightSideEnd);
	}

	{
		FConstructionSocket LeftSideEnd;
		LeftSideEnd.SocketName = FName(TEXT("RidgeBoardSide_EndR_Left"));
		LeftSideEnd.SocketType = EConstructionSocketType::RidgeBoard_Side;
		LeftSideEnd.LocalPosition = FVector(EndRightX, -BoardWidth / 2.0f, SideSocketLocalZ);
		LeftSideEnd.LocalRotation = FRotator(0.0f, -90.0f, 0.0f);
		LeftSideEnd.Orientation = ESocketOrientation::Horizontal;
		LeftSideEnd.bIsOccupied = false;
		Sockets.Add(LeftSideEnd);

		FConstructionSocket RightSideEnd;
		RightSideEnd.SocketName = FName(TEXT("RidgeBoardSide_EndR_Right"));
		RightSideEnd.SocketType = EConstructionSocketType::RidgeBoard_Side;
		RightSideEnd.LocalPosition = FVector(EndRightX, BoardWidth / 2.0f, SideSocketLocalZ);
		RightSideEnd.LocalRotation = FRotator(0.0f, 90.0f, 0.0f);
		RightSideEnd.Orientation = ESocketOrientation::Horizontal;
		RightSideEnd.bIsOccupied = false;
		Sockets.Add(RightSideEnd);
	}

	UE_LOG(LogTemp, Log, TEXT("RidgeBoard: Created %d OC socket pairs + 4 end sockets, BuildingSpan=%.1f, BoardLen=%.1f"),
		NumSockets, BuildingSpan, BoardLength);
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
			Socket.LocalPosition.Z = MeshTopZ;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("RidgeBoard: AdjustSockets - MeshZ=[%.2f, %.2f] height=%.2fcm center=%.2f"),
		MeshBottomZ, MeshTopZ, ActualHeight, MeshCenterZ);
}

void ARidgeBoard::RebuildRoofGrids()
{
	if (!RoofGrid) return;
	if (!AConstructionPhaseManager::Instance) return;

	TArray<ABuildablePiece*> AllRafters = AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::Rafter);
	TArray<ABuildablePiece*> AllFascia = AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::FasciaBoard);

	// Find left and right fascia by checking which side they're on
	ABuildablePiece* LeftFascia = nullptr;
	ABuildablePiece* RightFascia = nullptr;
	FVector RidgeForward = GetActorRotation().RotateVector(FVector::ForwardVector);
	FVector RidgeRight = GetActorRotation().RotateVector(FVector::RightVector);

	for (ABuildablePiece* F : AllFascia)
	{
		if (!F) continue;
		FVector ToFascia = F->GetActorLocation() - GetActorLocation();
		float DotRight = FVector::DotProduct(ToFascia, RidgeRight);
		if (DotRight < 0.0f && !LeftFascia) LeftFascia = F;
		else if (DotRight > 0.0f && !RightFascia) RightFascia = F;
	}

	RoofGrid->RebuildRoofSideGrid(ERoofSide::Left, AllRafters, this, LeftFascia);
	RoofGrid->RebuildRoofSideGrid(ERoofSide::Right, AllRafters, this, RightFascia);
}
