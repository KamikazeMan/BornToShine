// Born To Shine - Top Plate Implementation

#include "TopPlate.h"
#include "CornerPost.h"
#include "WallStud.h"
#include "ConstructionPhaseManager.h"
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

	// Re-align sockets to the actual mesh extents so the bottom/top
	// sockets match the real mesh surfaces regardless of Rhino export pivot.
	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("TopPlate: BeginPlay - %d ft (%.1f cm), BoardHeight=%.2f, Total sockets: %d"),
		CurrentLengthFeet, BoardLength, BoardHeight, Sockets.Num());
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

		// Reset extension flag — mesh is being rescaled to a new base length
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

	// Early return if length hasn't changed — avoids resetting bMeshExtended
	if (FMath::IsNearlyEqual(LengthCm, BoardLength, 0.01f))
	{
		return;
	}

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

	// Re-align to actual mesh bounds (same as BeginPlay path)
	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("TopPlate: Regenerated %d sockets for %d ft plate"), Sockets.Num(), CurrentLengthFeet);
}

bool ATopPlate::TryPlace()
{
	if (!Super::TryPlace()) return false;

	// Extend mesh for flush corners after snap placement
	ExtendMeshForFlushCorners();

	// --- Comprehensive diagnostic logging ---
	FVector PlatePos = GetActorLocation();
	FRotator PlateRot = GetActorRotation();

	// Use the actual bottom socket Z (adjusted to mesh bounds) for accurate reporting
	float PlateBotZ = PlatePos.Z - BoardHeight / 2.0f;
	for (const FConstructionSocket& S : Sockets)
	{
		if (S.SocketType == EConstructionSocketType::TopPlate_Bottom)
		{
			PlateBotZ = PlatePos.Z + S.LocalPosition.Z;
			break;
		}
	}

	// Log snap target details
	FString TargetName = SnappedToPiece ? SnappedToPiece->GetName() : TEXT("null");
	FString TargetType = SnappedToPiece ? UEnum::GetValueAsString(SnappedToPiece->GetPieceType()) : TEXT("None");
	FVector TargetPos = SnappedToPiece ? SnappedToPiece->GetActorLocation() : FVector::ZeroVector;
	FRotator TargetRot = SnappedToPiece ? SnappedToPiece->GetActorRotation() : FRotator::ZeroRotator;

	UE_LOG(LogTemp, Warning,
		TEXT("=== TOP PLATE PLACED === [%s] pos=(%.1f, %.1f, %.1f) yaw=%.1f meshBottom=%.2f len=%dft"),
		*GetName(), PlatePos.X, PlatePos.Y, PlatePos.Z, PlateRot.Yaw, PlateBotZ, CurrentLengthFeet);

	UE_LOG(LogTemp, Warning,
		TEXT("  SnapTarget: %s (%s) socket=%s pos=(%.1f, %.1f, %.1f) yaw=%.1f"),
		*TargetName, *TargetType, *SnappedToSocketName.ToString(),
		TargetPos.X, TargetPos.Y, TargetPos.Z, TargetRot.Yaw);

	UE_LOG(LogTemp, Warning,
		TEXT("  SnapCandidate: src=%s tgt=%s prio=%d dist=%.1f corner=%d inline=%d"),
		*CurrentSnapCandidate.SourceSocketName.ToString(),
		*CurrentSnapCandidate.TargetSocketName.ToString(),
		CurrentSnapCandidate.Priority, CurrentSnapCandidate.Distance,
		CurrentSnapCandidate.bIsCornerSnap ? 1 : 0,
		CurrentSnapCandidate.bIsInlineSnap ? 1 : 0);

	return true;
}

void ATopPlate::AdjustSocketsToMeshBounds()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	FVector MeshRelLoc = MeshComponent->GetRelativeLocation();

	// Mesh bottom/top in actor-local space (accounts for any BP mesh offset)
	float MeshBottomZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) + MeshRelLoc.Z;
	float MeshTopZ    = (Bounds.Origin.Z + Bounds.BoxExtent.Z) + MeshRelLoc.Z;
	float MeshCenterZ = (MeshBottomZ + MeshTopZ) / 2.0f;
	float ActualHeight = MeshTopZ - MeshBottomZ;

	if (ActualHeight < 0.1f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TopPlate: Mesh height too small (%.2f cm) — skipping socket adjustment"),
			ActualHeight);
		return;
	}

	float OldHeight = BoardHeight;

	// Update BoardHeight to reflect the real mesh vertical extent
	BoardHeight = ActualHeight;

	// Adjust all sockets to match the real mesh surfaces
	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketType == EConstructionSocketType::TopPlate_Bottom)
		{
			Socket.LocalPosition.Z = MeshBottomZ;
		}
		else if (Socket.SocketType == EConstructionSocketType::TopPlate_End)
		{
			Socket.LocalPosition.Z = MeshCenterZ;
		}
		else if (Socket.SocketType == EConstructionSocketType::TopPlate_Top)
		{
			Socket.LocalPosition.Z = MeshTopZ;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("TopPlate: AdjustSockets — MeshZ=[%.2f, %.2f] height=%.2fcm (was %.2fcm) center=%.2f MeshRelZ=%.2f"),
		MeshBottomZ, MeshTopZ, ActualHeight, OldHeight, MeshCenterZ, MeshRelLoc.Z);
}

void ATopPlate::ExtendMeshForFlushCorners()
{
	if (!MeshComponent) return;
	if (bMeshExtended) return;

	FVector CurrentScale3D = MeshComponent->GetRelativeScale3D();
	// Same formula as BottomPlate: extend by BoardWidth total (HalfWidth per end)
	float Ratio = (BoardLength + BoardWidth) / BoardLength;

	MeshComponent->SetRelativeScale3D(FVector(
		CurrentScale3D.X * Ratio,
		CurrentScale3D.Y,
		CurrentScale3D.Z
	));

	// Re-center the mesh after scaling. If the mesh asset origin isn't at the
	// geometric center, scaling shifts the visual center. Compensate so the
	// extension is symmetric on both ends (flush at all 4 corners, not just 2).
	if (MeshComponent->GetStaticMesh())
	{
		FBoxSphereBounds AssetBounds = MeshComponent->GetStaticMesh()->GetBounds();
		float CenterShift = AssetBounds.Origin.X * (Ratio - 1.0f) * CurrentScale3D.X;
		if (FMath::Abs(CenterShift) > 0.001f)
		{
			FVector RelLoc = MeshComponent->GetRelativeLocation();
			RelLoc.X -= CenterShift;
			MeshComponent->SetRelativeLocation(RelLoc);
			UE_LOG(LogTemp, Warning, TEXT("TopPlate [%s]: Re-centered mesh X by %.3f (asset origin X=%.2f)"),
				*GetName(), -CenterShift, AssetBounds.Origin.X);
		}
	}

	bMeshExtended = true;

	UE_LOG(LogTemp, Warning, TEXT("TopPlate [%s]: ExtendMesh ratio=%.4f scale X: %.4f -> %.4f (ext=%.2fcm, %.2fcm/end)"),
		*GetName(), Ratio, CurrentScale3D.X, CurrentScale3D.X * Ratio, BoardWidth, BoardWidth / 2.0f);
}
