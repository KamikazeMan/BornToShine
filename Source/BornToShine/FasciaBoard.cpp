// Born To Shine - Fascia Board Implementation

#include "FasciaBoard.h"
#include "ConstructionPhaseManager.h"
#include "Rafter.h"
#include "Components/StaticMeshComponent.h"

AFasciaBoard::AFasciaBoard()
{
	PieceType = EPieceType::FasciaBoard;

	// 1x6 actual dimensions (3/4" x 5.5")
	BoardWidth = 1.905f;   // 3/4 inch
	BoardHeight = 13.97f;  // 5.5 inches

	// Default 8ft length
	CurrentLengthFeet = 8;
	BoardLength = CurrentLengthFeet * 30.48f;

	// Scene root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void AFasciaBoard::BeginPlay()
{
	Super::BeginPlay();

	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("FasciaBoard: BeginPlay - %d ft (%.1f cm), Height=%.2fcm, Sockets=%d"),
		CurrentLengthFeet, BoardLength, BoardHeight, Sockets.Num());
}

void AFasciaBoard::InitializeSockets()
{
	Sockets.Empty();

	CreateEndSockets();
	CreateRafterTailSockets();

	UE_LOG(LogTemp, Log, TEXT("FasciaBoard: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void AFasciaBoard::CreateEndSockets()
{
	float HalfLen = BoardLength / 2.0f;

	FConstructionSocket LeftEnd;
	LeftEnd.SocketName = FName(TEXT("FasciaEnd_Left"));
	LeftEnd.SocketType = EConstructionSocketType::Fascia_End;
	LeftEnd.LocalPosition = FVector(-HalfLen, 0.0f, 0.0f);
	LeftEnd.LocalRotation = FRotator(0.0f, 180.0f, 0.0f);
	LeftEnd.Orientation = ESocketOrientation::Horizontal;
	LeftEnd.bIsOccupied = false;
	Sockets.Add(LeftEnd);

	FConstructionSocket RightEnd;
	RightEnd.SocketName = FName(TEXT("FasciaEnd_Right"));
	RightEnd.SocketType = EConstructionSocketType::Fascia_End;
	RightEnd.LocalPosition = FVector(HalfLen, 0.0f, 0.0f);
	RightEnd.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RightEnd.Orientation = ESocketOrientation::Horizontal;
	RightEnd.bIsOccupied = false;
	Sockets.Add(RightEnd);
}

void AFasciaBoard::CreateRafterTailSockets()
{
	float HalfLen = BoardLength / 2.0f;

	// Create sockets at 16" OC (40.64cm) for rafter tail attachment
	const float Spacing = 40.64f;
	int32 NumSpaces = FMath::FloorToInt(BoardLength / Spacing);
	float StartOffset = -HalfLen + Spacing;

	for (int32 i = 0; i < NumSpaces; i++)
	{
		float XPos = StartOffset + i * Spacing;
		if (XPos > HalfLen - Spacing * 0.5f) break;

		FConstructionSocket TailSocket;
		TailSocket.SocketName = FName(*FString::Printf(TEXT("FasciaRafterTail_%d"), i));
		TailSocket.SocketType = EConstructionSocketType::Fascia_RafterTail;
		TailSocket.LocalPosition = FVector(XPos, -BoardWidth / 2.0f, 0.0f);
		TailSocket.LocalRotation = FRotator(0.0f, -90.0f, 0.0f);
		TailSocket.Orientation = ESocketOrientation::Horizontal;
		TailSocket.bIsOccupied = false;
		Sockets.Add(TailSocket);
	}
}

void AFasciaBoard::SetBoardLengthCm(float LengthCm)
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

	UE_LOG(LogTemp, Log, TEXT("FasciaBoard: Length set to %.1f cm (~%d ft)"),
		BoardLength, CurrentLengthFeet);
}

void AFasciaBoard::SetBoardLengthFeet(int32 LengthInFeet)
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
	}
}

int32 AFasciaBoard::GetBoardLengthFeet() const
{
	return CurrentLengthFeet;
}

FString AFasciaBoard::GetLengthDisplayString() const
{
	return FString::Printf(TEXT("%d ft (%.1f cm)"), CurrentLengthFeet, BoardLength);
}

void AFasciaBoard::ScalePiece(float ScaleDelta)
{
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

bool AFasciaBoard::TryPlace()
{
	if (!Super::TryPlace()) return false;

	// Trim rafter tails that extend past the fascia board face.
	// Use distance-to-line (along fascia board axis) instead of distance-to-point
	// so tails at the far ends of the building are still found.
	if (AConstructionPhaseManager::Instance)
	{
		TArray<ABuildablePiece*> Rafters =
			AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::Rafter);

		FVector FasciaLoc = GetActorLocation();
		FVector FasciaFwd = GetActorRotation().RotateVector(FVector::ForwardVector);
		FVector FasciaNormal = GetActorRotation().RotateVector(FVector::RightVector);
		float FasciaHalfLen = BoardLength / 2.0f;

		int32 TrimCount = 0;

		for (ABuildablePiece* Piece : Rafters)
		{
			ARafter* Raft = Cast<ARafter>(Piece);
			if (!Raft) continue;

			// Check proximity using distance to the fascia LINE (not center point).
			// Project tail onto fascia forward axis to check if within board span,
			// then check perpendicular distance to the fascia line.
			FVector TailPos = Raft->GetTailEndWorldPosition();
			FVector DeltaFromFascia = TailPos - FasciaLoc;
			float AlongFascia = FMath::Abs(FVector::DotProduct(DeltaFromFascia, FasciaFwd));
			float PerpToFascia = FMath::Abs(FVector::DotProduct(DeltaFromFascia, FasciaNormal));
			float VertDist = FMath::Abs(DeltaFromFascia.Z);

			// Tail must be within the board span (+margin) and close perpendicular
			if (AlongFascia > FasciaHalfLen + 50.0f) continue;
			if (PerpToFascia > 60.0f) continue; // Within ~2ft perpendicular
			if (VertDist > 60.0f) continue;

			// Get rafter direction and find where it intersects fascia plane
			FVector RafterOrigin = Raft->GetActorLocation();
			FVector RafterDir = Raft->GetActorRotation().RotateVector(FVector::ForwardVector);

			// Project fascia location onto rafter line to find trim distance
			FVector ToFascia = FasciaLoc - RafterOrigin;
			float TrimDist = FVector::DotProduct(ToFascia, RafterDir);

			if (TrimDist > 10.0f) // Sanity: rafter must extend at least 10cm
			{
				// Add fascia thickness so rafter ends at fascia back face
				const float FasciaThickness = BoardWidth; // 1.905cm = 3/4"
				TrimDist += FasciaThickness;

				float CurrentSlope = Raft->GetSlopeLengthCm();
				if (TrimDist < CurrentSlope) // Only trim if actually shorter
				{
					UStaticMeshComponent* RaftMesh = Raft->GetMeshComponent();
					if (RaftMesh && RaftMesh->GetStaticMesh())
					{
						FBoxSphereBounds RBounds = RaftMesh->GetStaticMesh()->GetBounds();
						float MeshDefaultLen = RBounds.BoxExtent.X * 2.0f;
						if (MeshDefaultLen > 1.0f)
						{
							float NewXScale = TrimDist / MeshDefaultLen;
							FVector CurScale = RaftMesh->GetRelativeScale3D();
							RaftMesh->SetRelativeScale3D(FVector(NewXScale, CurScale.Y, CurScale.Z));
							TrimCount++;

							UE_LOG(LogTemp, Log,
								TEXT("Fascia: Trimmed rafter %s from %.1f to %.1f cm (XScale=%.3f)"),
								*Raft->GetName(), CurrentSlope, TrimDist, NewXScale);
						}
					}
				}
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("Fascia: Trimmed %d rafter tails (BoardLen=%.1f)"), TrimCount, BoardLength);
	}

	return true;
}

void AFasciaBoard::RegenerateSockets()
{
	Sockets.Empty();

	CreateEndSockets();
	CreateRafterTailSockets();

	AdjustSocketsToMeshBounds();

	UE_LOG(LogTemp, Log, TEXT("FasciaBoard: Regenerated %d sockets for %d ft board"), Sockets.Num(), CurrentLengthFeet);
}

void AFasciaBoard::AdjustSocketsToMeshBounds()
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

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketType == EConstructionSocketType::Fascia_End ||
			Socket.SocketType == EConstructionSocketType::Fascia_RafterTail)
		{
			Socket.LocalPosition.Z = MeshCenterZ;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("FasciaBoard: AdjustSockets - MeshZ=[%.2f, %.2f] height=%.2fcm center=%.2f"),
		MeshBottomZ, MeshTopZ, ActualHeight, MeshCenterZ);
}
