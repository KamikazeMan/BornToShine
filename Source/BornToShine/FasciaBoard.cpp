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

	// Trim rafter tails that extend past the fascia board face
	if (AConstructionPhaseManager::Instance)
	{
		TArray<ABuildablePiece*> Rafters =
			AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::Rafter);

		FVector FasciaLoc = GetActorLocation();
		FVector FasciaFwd = GetActorRotation().RotateVector(FVector::ForwardVector);

		int32 TrimCount = 0;

		for (ABuildablePiece* Piece : Rafters)
		{
			ARafter* Raft = Cast<ARafter>(Piece);
			if (!Raft) continue;

			// Check if this rafter's tail end is near the fascia using
			// along-fascia projection + perpendicular distance.
			FVector TailPos = Raft->GetTailEndWorldPosition();
			FVector ToTail = TailPos - FasciaLoc;
			float AlongFascia = FMath::Abs(FVector::DotProduct(ToTail, FasciaFwd));
			FVector Perp = ToTail - FasciaFwd * FVector::DotProduct(ToTail, FasciaFwd);
			float Dist = Perp.Size();

			if (Dist < 50.0f && AlongFascia < BoardLength / 2.0f + 20.0f)
			{
				// Calculate the fascia back face plane (the face toward the ridge).
				// Fascia thickness = 3.81cm (2x); back face is half-thickness behind center.
				const float FasciaThickness = 3.81f;
				const float FasciaHalfThickness = FasciaThickness * 0.5f;

				// FasciaFwd here is the LENGTH direction of the fascia (along the eave).
				// We need the THICKNESS direction (perpendicular to fascia face).
				// The fascia's RIGHT vector points toward/away from the building.
				FVector FasciaRight = GetActorRotation().RotateVector(FVector::RightVector);

				// Determine which way is "toward the ridge" (back face direction).
				// The rafter origin (ridge end) is on the ridge side.
				FVector RafterOrigin = Raft->GetActorLocation();
				FVector ToRidgeFromFascia = RafterOrigin - FasciaLoc;
				float DotRight = FVector::DotProduct(ToRidgeFromFascia, FasciaRight);
				FVector BackFaceDir = (DotRight > 0.0f) ? FasciaRight : -FasciaRight;

				// Back face point and normal in world space
				FVector BackFacePoint = FasciaLoc + BackFaceDir * FasciaHalfThickness;
				FVector BackFaceNormal = -BackFaceDir; // Normal points outward (away from ridge)

				// Compute cut station by projecting the fascia back face point onto
				// the rafter's forward direction. This matches what the plywood
				// diagnostic uses (FasciaAlongSlope) and works correctly with rotation.
				FVector RafterFwd = Raft->GetActorRotation().RotateVector(FVector::ForwardVector);
				FVector ToBackFace = BackFacePoint - Raft->GetActorLocation();
				float CutStation = FVector::DotProduct(ToBackFace, RafterFwd);

				{
					float CurrentSlope = Raft->GetSlopeLengthCm();
					if (CutStation > 10.0f && CutStation < CurrentSlope + 50.0f)
					{
						// Trigger the procedural plumb cut
						Raft->ReplaceWithProceduralPlumbCutRafter(
							CutStation,
							Raft->GetPitchAngleDegrees(),
							Raft->RafterWidth,
							Raft->RafterDepth);

						// Update the rafter's stored slope length so downstream systems
						// (like roof sheathing trim) see the new trimmed length.
						Raft->TrimmedSlopeLength = CutStation;

						// Update the RafterTail socket position to match the new tail end
						for (FConstructionSocket& Socket : Raft->GetSocketsMutable())
						{
							if (Socket.SocketName == FName("RafterTail"))
								Socket.LocalPosition.X = CutStation;
						}

						UE_LOG(LogTemp, Warning, TEXT("Fascia trim: Rafter %s CutStation=%.1f (was %.1f slope)"),
							*Raft->GetName(), CutStation, CurrentSlope);
						TrimCount++;

						// Shift the entire rafter down-slope to close the fascia gap at the eave
						// and pull the rafter top down so it doesn't stick through the ridge board.
						const float DownSlopeShiftCm = 1.5f; // Tune this value to taste
						FVector RafterSlopeDir = Raft->GetActorRotation().RotateVector(FVector::ForwardVector);
						FVector NewRafterLoc = Raft->GetActorLocation() + RafterSlopeDir * DownSlopeShiftCm;
						Raft->SetActorLocation(NewRafterLoc);

						UE_LOG(LogTemp, Warning, TEXT("Fascia: Shifted rafter %s down-slope by %.1fcm"),
							*Raft->GetName(), DownSlopeShiftCm);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("Fascia trim: Rafter %s CutStation=%.1f out of range (slope=%.1f)"),
							*Raft->GetName(), CutStation, CurrentSlope);
					}
				}
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("Fascia: Trimmed %d rafter tails"), TrimCount);
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
