// Born To Shine - Base class for all buildable construction pieces

#include "BuildablePiece.h"
#include "RimBoard.h"
#include "SocketManager.h"
#include "ConstructionPhaseManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/PlayerController.h"

ABuildablePiece::ABuildablePiece()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create mesh component
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	// Default values
	PieceType = EPieceType::None;
	PieceState = EPieceState::Preview;
	bIsSnapped = false;
	SnappedToPiece = nullptr;
	SnappedToSocketName = NAME_None;
	SnapSearchRadius = 500.0f; // 5 meters

	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
	MinScale = 0.5f;
	MaxScale = 3.0f;
	RotationStep = 15.0f; // 15 degrees per rotation

	// Default colors
	ValidPlacementColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.5f);    // Green translucent
	InvalidPlacementColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.5f);  // Red translucent
	PlacedColor = FLinearColor(1.0f, 1.0f, 0.0f, 0.8f);            // Yellow
	NailedColor = FLinearColor(0.8f, 0.6f, 0.4f, 1.0f);            // Wood color

	// Material settings
	NailedMaterial = nullptr;  // Set in Blueprint
	bAutoNailOnPlace = false;   // Override to true for foundation blocks
}

void ABuildablePiece::BeginPlay()
{
	Super::BeginPlay();

	InitializeSockets();

	// Create dynamic material instance for visual feedback
	if (MeshComponent && MeshComponent->GetMaterial(0))
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(MeshComponent->GetMaterial(0), this);
		MeshComponent->SetMaterial(0, DynamicMaterial);
	}

	UpdateVisualFeedback();
}

void ABuildablePiece::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update visual feedback in preview mode
	if (PieceState == EPieceState::Preview)
	{
		UpdateVisualFeedback();
	}
}

void ABuildablePiece::InitializeSockets()
{
	// Override in child classes to add specific sockets
}

FConstructionSocket* ABuildablePiece::GetSocketByName(FName SocketName)
{
	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == SocketName)
		{
			return &Socket;
		}
	}
	return nullptr;
}

bool ABuildablePiece::GetSocketByNameSafe(FName SocketName, FConstructionSocket& OutSocket)
{
	for (const FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == SocketName)
		{
			OutSocket = Socket;
			return true;
		}
	}
	return false;
}

void ABuildablePiece::SetPreviewMode(bool bIsPreview)
{
	if (bIsPreview)
	{
		PieceState = EPieceState::Preview;

		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			MeshComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
		}
	}
	else
	{
		PieceState = EPieceState::Placed;

		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			MeshComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
		}
	}

	UpdateVisualFeedback();
}

void ABuildablePiece::UpdatePreviewPosition(const FVector& NewLocation, const FRotator& NewRotation)
{
	if (PieceState != EPieceState::Preview) return;

	SetActorLocation(NewLocation);
	SetActorRotation(NewRotation);

	// Phase 1: Pure detection (no mutation)
	TArray<FSnapCandidate> Candidates = DetectSnapCandidates();

	// Phase 2: Selection (no mutation)
	FSnapCandidate Best = SelectBestCandidate(Candidates);

	// Phase 3: Apply (mutation happens here, separated from detection)
	if (Best.IsValid())
	{
		ApplySnap(Best);
	}
	else
	{
		bIsSnapped = false;
		CurrentSnapCandidate = FSnapCandidate();
	}
}

TArray<FSnapCandidate> ABuildablePiece::DetectSnapCandidates() const
{
	TArray<FSnapCandidate> Candidates;

	if (!ASocketManager::Instance)
	{
		UE_LOG(LogTemp, Warning, TEXT("DetectSnapCandidates: SocketManager is null!"));
		return Candidates;
	}
	if (!AConstructionPhaseManager::Instance)
	{
		UE_LOG(LogTemp, Warning, TEXT("DetectSnapCandidates: PhaseManager is null!"));
		return Candidates;
	}

	// Get nearby pieces
	TArray<ABuildablePiece*> NearbyPieces = AConstructionPhaseManager::Instance->GetNearbyPieces(
		GetActorLocation(),
		SnapSearchRadius
	);

	if (NearbyPieces.Num() == 0)
	{
		static float LastLogTime = 0.0f;
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastLogTime > 2.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: No nearby pieces within %.0fcm of (%.1f, %.1f, %.1f)"),
				*GetName(), SnapSearchRadius, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
			LastLogTime = CurrentTime;
		}
		return Candidates;
	}

	// Debug: Log search info (throttled)
	static float LastSearchLogTime = 0.0f;
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastSearchLogTime > 2.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: Searching %d nearby pieces with %d sockets"),
				*GetName(), NearbyPieces.Num(), Sockets.Num());
			LastSearchLogTime = CurrentTime;
		}
	}

	// ============================================================
	// DUAL-END CORNER DETECTION (for 3rd/4th rim boards)
	// Check if BOTH ends of this rim board can reach corner sockets
	// on two DIFFERENT placed boards. If so, record as a dual-end candidate.
	// NOTE: No mutation here -- AutoResizeLengthFeet is recorded, not applied.
	// ============================================================
	if (PieceType == EPieceType::RimBoard)
	{
		// Find our Left and Right EndCorner sockets
		const FConstructionSocket* LeftCornerSocket = nullptr;
		const FConstructionSocket* RightCornerSocket = nullptr;

		for (const FConstructionSocket& Socket : Sockets)
		{
			if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner)
			{
				if (Socket.SocketName.ToString().Contains(TEXT("Left")))
				{
					LeftCornerSocket = &Socket;
				}
				else if (Socket.SocketName.ToString().Contains(TEXT("Right")))
				{
					RightCornerSocket = &Socket;
				}
			}
		}

		if (LeftCornerSocket && RightCornerSocket)
		{
			// Get world positions of both corner sockets
			FVector LeftWorldPos = GetActorTransform().TransformPosition(LeftCornerSocket->LocalPosition);
			FVector RightWorldPos = GetActorTransform().TransformPosition(RightCornerSocket->LocalPosition);

			// Expanded search radius for dual-end detection
			float DualSearchRadius = SnapSearchRadius * 2.0f;

			struct FDualCornerCandidate
			{
				ABuildablePiece* TargetPiece;
				FName TargetSocketName;
				FVector TargetWorldPos;
				FRotator TargetWorldRot;
				float Distance;
			};

			TArray<FDualCornerCandidate> LeftCands;
			TArray<FDualCornerCandidate> RightCands;

			for (ABuildablePiece* Piece : NearbyPieces)
			{
				if (!Piece) continue;

				TArray<FConstructionSocket> TargetSockets = Piece->GetAllSockets();

				for (const FConstructionSocket& TargetSocket : TargetSockets)
				{
					if (TargetSocket.bIsOccupied) continue;
					if (TargetSocket.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;

					FVector TargetWorldPos = Piece->GetActorTransform().TransformPosition(TargetSocket.LocalPosition);
					FRotator TargetWorldRot = Piece->GetActorRotation() + TargetSocket.LocalRotation;

					float LeftDist = FVector::Dist(LeftWorldPos, TargetWorldPos);
					if (LeftDist < DualSearchRadius)
					{
						FDualCornerCandidate DC;
						DC.TargetPiece = Piece;
						DC.TargetSocketName = TargetSocket.SocketName;
						DC.TargetWorldPos = TargetWorldPos;
						DC.TargetWorldRot = TargetWorldRot;
						DC.Distance = LeftDist;
						LeftCands.Add(DC);
					}

					float RightDist = FVector::Dist(RightWorldPos, TargetWorldPos);
					if (RightDist < DualSearchRadius)
					{
						FDualCornerCandidate DC;
						DC.TargetPiece = Piece;
						DC.TargetSocketName = TargetSocket.SocketName;
						DC.TargetWorldPos = TargetWorldPos;
						DC.TargetWorldRot = TargetWorldRot;
						DC.Distance = RightDist;
						RightCands.Add(DC);
					}
				}
			}

			// Find the best pair where left and right connect to DIFFERENT target pieces
			if (LeftCands.Num() > 0 && RightCands.Num() > 0)
			{
				FDualCornerCandidate BestLeft;
				FDualCornerCandidate BestRight;
				bool bFoundValidPair = false;
				float BestPairScore = -1.0f;

				for (const FDualCornerCandidate& LC : LeftCands)
				{
					for (const FDualCornerCandidate& RC : RightCands)
					{
						// Must be different target pieces (spanning between two boards)
						if (LC.TargetPiece == RC.TargetPiece) continue;

						float PairScore = 2000.0f / (LC.Distance + RC.Distance + 1.0f);

						if (PairScore > BestPairScore)
						{
							BestPairScore = PairScore;
							BestLeft = LC;
							BestRight = RC;
							bFoundValidPair = true;
						}
					}
				}

				if (bFoundValidPair)
				{
					// TWO-POINT ALIGNMENT
					FVector TargetLeftPos = BestLeft.TargetWorldPos;
					FVector TargetRightPos = BestRight.TargetWorldPos;

					FVector SpanDirection = (TargetRightPos - TargetLeftPos).GetSafeNormal();
					FRotator SpanRotation = SpanDirection.Rotation();

					// Calculate actor origin from left socket alignment
					FVector LeftLocalOffset = SpanRotation.RotateVector(LeftCornerSocket->LocalPosition);
					FVector ActorPosition = TargetLeftPos - LeftLocalOffset;

					// Check how well the right end lines up
					FVector RightLocalOffset = SpanRotation.RotateVector(RightCornerSocket->LocalPosition);
					FVector PredictedRightPos = ActorPosition + RightLocalOffset;
					float RightError = FVector::Dist(PredictedRightPos, TargetRightPos);

					// Determine if auto-resize is needed (record it, don't apply)
					int32 NeededResizeFeet = 0;
					if (RightError > 5.0f)
					{
						float GapDistance = FVector::Dist(TargetLeftPos, TargetRightPos);
						float NeededLengthCm = GapDistance;
						int32 NeededLengthFeet = FMath::RoundToInt(NeededLengthCm / 30.48f);
						NeededLengthFeet = FMath::Clamp(NeededLengthFeet, 1, 16);

						// Check if resize is actually needed by comparing to current length
						const ARimBoard* RimBoard = Cast<const ARimBoard>(this);
						if (RimBoard && NeededLengthFeet != RimBoard->GetBoardLengthFeet())
						{
							NeededResizeFeet = NeededLengthFeet;
						}
					}

					FSnapCandidate DualCandidate;
					DualCandidate.SourceSocketName = LeftCornerSocket->SocketName;
					DualCandidate.SourceSocketType = LeftCornerSocket->SocketType;
					DualCandidate.TargetSocketName = BestLeft.TargetSocketName;
					DualCandidate.TargetSocketType = EConstructionSocketType::RimBoard_End_Corner;
					DualCandidate.TargetPiece = BestLeft.TargetPiece;
					DualCandidate.SnapLocation = ActorPosition;
					DualCandidate.SnapRotation = SpanRotation;
					DualCandidate.Score = BestPairScore;
					DualCandidate.Priority = 2000; // Highest priority -- dual-end snap
					DualCandidate.Distance = BestLeft.Distance + BestRight.Distance;
					DualCandidate.bIsCornerSnap = true;
					DualCandidate.bIsInlineSnap = false;
					DualCandidate.bIsDualEndSnap = true;
					DualCandidate.SecondTargetPiece = BestRight.TargetPiece;
					DualCandidate.SecondTargetSocketName = BestRight.TargetSocketName;
					DualCandidate.AutoResizeLengthFeet = NeededResizeFeet;

					UE_LOG(LogTemp, Warning, TEXT("DUAL-END CANDIDATE: Left->%s on %s, Right->%s on %s | Yaw=%.1f | AutoResize=%d ft"),
						*BestLeft.TargetSocketName.ToString(), *BestLeft.TargetPiece->GetName(),
						*BestRight.TargetSocketName.ToString(), *BestRight.TargetPiece->GetName(),
						SpanRotation.Yaw, NeededResizeFeet);

					Candidates.Add(DualCandidate);
				}
			}
		}
	}

	// ============================================================
	// STANDARD SINGLE-SOCKET SNAP CANDIDATES
	// ============================================================
	for (const FConstructionSocket& Socket : Sockets)
	{
		FVector SocketWorldLocation = GetActorTransform().TransformPosition(Socket.LocalPosition);
		FRotator SocketWorldRotation = GetActorRotation() + Socket.LocalRotation;

		FVector SnapLoc;
		FRotator SnapRot;
		ABuildablePiece* TargetPiece = nullptr;
		FName TargetSocketName;

		if (ASocketManager::Instance->FindBestSnapPoint(
			Socket,
			SocketWorldLocation,
			SocketWorldRotation,
			NearbyPieces,
			AConstructionPhaseManager::Instance->GetCurrentPhase(),
			SnapLoc,
			SnapRot,
			TargetPiece,
			TargetSocketName))
		{
			float Dist = FVector::Dist(SocketWorldLocation, SnapLoc);

			EConstructionSocketType TgtSocketType = GetTargetSocketType(TargetPiece, TargetSocketName);
			int32 Prio = GetSocketConnectionPriority(Socket.SocketType, TgtSocketType);

			// Compute the candidate rotation
			FRotator CandidateRotation = GetActorRotation();
			bool bCandidateIsCorner = false;
			bool bCandidateIsInline = false;

			// Special handling for rim-to-rim corner snaps (single-end)
			if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
				TgtSocketType == EConstructionSocketType::RimBoard_End_Corner &&
				TargetPiece)
			{
				FRotator TargetRotation = TargetPiece->GetActorRotation();

				FString SourceSocketStr = Socket.SocketName.ToString();
				FString TargetSocketStr = TargetSocketName.ToString();
				bool bSourceIsRight = SourceSocketStr.Contains(TEXT("Right"));
				bool bTargetIsRight = TargetSocketStr.Contains(TEXT("Right"));
				bool bOppositeEnds = (bSourceIsRight != bTargetIsRight);

				if (bOppositeEnds)
				{
					// INLINE EXTENSION: Right->Left or Left->Right
					CandidateRotation.Yaw = TargetRotation.Yaw;
					bCandidateIsInline = true;
					UE_LOG(LogTemp, Warning, TEXT("Inline snap candidate: %s -> %s"), *SourceSocketStr, *TargetSocketStr);
				}
				else
				{
					// CORNER JOINT: Left->Left or Right->Right
					// Use dot product of player's look direction against target's right vector
					// to deterministically choose TargetYaw +90 or -90.
					FVector TargetRight = TargetRotation.RotateVector(FVector::RightVector);

					// Get player camera forward direction
					FVector PieceToCamera = FVector::ZeroVector;
					if (UWorld* World = GetWorld())
					{
						APlayerController* PC = World->GetFirstPlayerController();
						if (PC)
						{
							FVector CamLoc;
							FRotator CamRot;
							PC->GetPlayerViewPoint(CamLoc, CamRot);
							PieceToCamera = CamRot.Vector(); // Player look direction
						}
					}

					// Dot product: if player is looking along target's right vector, extend right (+90)
					// If looking against it, extend left (-90)
					float DotResult = FVector::DotProduct(PieceToCamera, TargetRight);
					float RotationSign = (DotResult > 0) ? 90.0f : -90.0f;
					CandidateRotation.Yaw = TargetRotation.Yaw + RotationSign;

					bCandidateIsCorner = true;

					UE_LOG(LogTemp, Warning, TEXT("Corner snap candidate: %s -> %s (dot=%.2f, rot offset %.0f)"),
						*SourceSocketStr, *TargetSocketStr, DotResult, RotationSign);
				}
			}

			// Calculate final actor position from socket alignment
			FVector SocketLocalOffset = Socket.LocalPosition;
			FVector SocketWorldOffset = CandidateRotation.RotateVector(SocketLocalOffset);
			FVector CandidateLocation = SnapLoc - SocketWorldOffset;

			FSnapCandidate Candidate;
			Candidate.SourceSocketName = Socket.SocketName;
			Candidate.SourceSocketType = Socket.SocketType;
			Candidate.TargetSocketName = TargetSocketName;
			Candidate.TargetSocketType = TgtSocketType;
			Candidate.TargetPiece = TargetPiece;
			Candidate.SnapLocation = CandidateLocation;
			Candidate.SnapRotation = CandidateRotation;
			Candidate.Score = 0.f; // Not used for selection; Priority+Distance are used
			Candidate.Priority = Prio;
			Candidate.Distance = Dist;
			Candidate.bIsCornerSnap = bCandidateIsCorner;
			Candidate.bIsInlineSnap = bCandidateIsInline;
			Candidate.bIsDualEndSnap = false;
			Candidate.SecondTargetPiece = nullptr;
			Candidate.AutoResizeLengthFeet = 0;

			Candidates.Add(Candidate);
		}
	}

	return Candidates;
}

FSnapCandidate ABuildablePiece::SelectBestCandidate(const TArray<FSnapCandidate>& Candidates) const
{
	if (Candidates.Num() == 0)
	{
		return FSnapCandidate();
	}

	// Select the best candidate: highest Priority first, then lowest Distance as tiebreaker
	int32 BestIndex = 0;
	int32 BestPriority = Candidates[0].Priority;
	float BestDistance = Candidates[0].Distance;

	for (int32 i = 1; i < Candidates.Num(); ++i)
	{
		const FSnapCandidate& C = Candidates[i];
		bool bIsBetter = (C.Priority > BestPriority) ||
		                 (C.Priority == BestPriority && C.Distance < BestDistance);

		if (bIsBetter)
		{
			BestIndex = i;
			BestPriority = C.Priority;
			BestDistance = C.Distance;
		}
	}

	return Candidates[BestIndex];
}

void ABuildablePiece::ApplySnap(const FSnapCandidate& Candidate)
{
	if (!Candidate.IsValid()) return;

	// If this is a dual-end snap with auto-resize needed, apply the resize first
	if (Candidate.bIsDualEndSnap && Candidate.AutoResizeLengthFeet > 0 && PieceType == EPieceType::RimBoard)
	{
		ARimBoard* RimBoard = Cast<ARimBoard>(this);
		if (RimBoard && Candidate.AutoResizeLengthFeet != RimBoard->GetBoardLengthFeet())
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplySnap: Auto-resizing board from %d ft to %d ft"),
				RimBoard->GetBoardLengthFeet(), Candidate.AutoResizeLengthFeet);
			RimBoard->SetBoardLengthFeet(Candidate.AutoResizeLengthFeet);

			// After resize, re-run detection and selection to get accurate positions
			// with the new socket locations
			TArray<FSnapCandidate> RefreshedCandidates = DetectSnapCandidates();
			FSnapCandidate RefreshedBest = SelectBestCandidate(RefreshedCandidates);

			if (RefreshedBest.IsValid())
			{
				// Apply the refreshed snap instead
				SetActorLocation(RefreshedBest.SnapLocation);
				SetActorRotation(RefreshedBest.SnapRotation);

				bIsSnapped = true;
				SnappedToPiece = RefreshedBest.TargetPiece;
				SnappedToSocketName = RefreshedBest.TargetSocketName;
				CurrentSnapCandidate = RefreshedBest;
				return;
			}
		}
	}

	// Set actor location and rotation from candidate
	SetActorLocation(Candidate.SnapLocation);
	SetActorRotation(Candidate.SnapRotation);

	// Update snap state
	bIsSnapped = true;
	SnappedToPiece = Candidate.TargetPiece;
	SnappedToSocketName = Candidate.TargetSocketName;
	CurrentSnapCandidate = Candidate;
}

void ABuildablePiece::CommitPlacement()
{
	// Occupy socket on the primary target piece
	if (bIsSnapped && SnappedToPiece != nullptr)
	{
		SnappedToPiece->OccupySocket(SnappedToSocketName, this);
	}

	// For dual-end snaps, also occupy the second target socket
	if (CurrentSnapCandidate.bIsDualEndSnap && CurrentSnapCandidate.SecondTargetPiece != nullptr)
	{
		CurrentSnapCandidate.SecondTargetPiece->OccupySocket(
			CurrentSnapCandidate.SecondTargetSocketName, this);
	}

	// Register with PhaseManager
	if (AConstructionPhaseManager::Instance)
	{
		AConstructionPhaseManager::Instance->RegisterPlacedPiece(this);
		UE_LOG(LogTemp, Log, TEXT("Registered %s with PhaseManager"), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ConstructionPhaseManager not found! Add BP_ConstructionPhaseManager to your level!"));
	}
}

int32 ABuildablePiece::GetSocketConnectionPriority(EConstructionSocketType SocketA, EConstructionSocketType SocketB) const
{
	// Rim-to-Rim corner connections (HIGHEST PRIORITY)
	if ((SocketA == EConstructionSocketType::RimBoard_End_Corner &&
		 SocketB == EConstructionSocketType::RimBoard_End_Corner))
	{
		return 1000;
	}

	// Rim-to-Rim side connections (HIGH PRIORITY)
	if ((SocketA == EConstructionSocketType::RimBoard_Side_Face &&
		 SocketB == EConstructionSocketType::RimBoard_Side_Face))
	{
		return 900;
	}

	// Joist-to-Rim connections (HIGH PRIORITY)
	if ((SocketA == EConstructionSocketType::Joist_End &&
		 (SocketB == EConstructionSocketType::RimBoard_Top_Face ||
		  SocketB == EConstructionSocketType::RimBoard_Side_Face)) ||
		((SocketA == EConstructionSocketType::RimBoard_Top_Face ||
		  SocketA == EConstructionSocketType::RimBoard_Side_Face) &&
		 SocketB == EConstructionSocketType::Joist_End))
	{
		return 800;
	}

	// Rim bottom to Foundation (LOW PRIORITY)
	if ((SocketA == EConstructionSocketType::RimBoard_Bottom_End &&
		 (SocketB == EConstructionSocketType::Foundation_Corner ||
		  SocketB == EConstructionSocketType::Foundation_Side)) ||
		((SocketA == EConstructionSocketType::Foundation_Corner ||
		  SocketA == EConstructionSocketType::Foundation_Side) &&
		 SocketB == EConstructionSocketType::RimBoard_Bottom_End))
	{
		return 10;
	}

	return 0;
}

EConstructionSocketType ABuildablePiece::GetTargetSocketType(ABuildablePiece* TargetPiece, FName SocketName) const
{
	if (!TargetPiece) return EConstructionSocketType::None;

	FConstructionSocket TargetSocket;
	if (TargetPiece->GetSocketByNameSafe(SocketName, TargetSocket))
	{
		return TargetSocket.SocketType;
	}

	return EConstructionSocketType::None;
}

bool ABuildablePiece::TryPlace()
{
	if (PieceState != EPieceState::Preview) return false;

	if (!IsPlacementValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot place piece - invalid placement"));
		return false;
	}

	// Commit the snap (occupy sockets, register piece)
	CommitPlacement();

	PieceState = bAutoNailOnPlace ? EPieceState::Nailed : EPieceState::Placed;
	UpdateVisualFeedback();

	UE_LOG(LogTemp, Log, TEXT("Piece placed successfully%s"), bAutoNailOnPlace ? TEXT(" and auto-nailed") : TEXT(""));
	return true;
}

void ABuildablePiece::NailInPlace()
{
	if (PieceState != EPieceState::Placed) return;

	PieceState = EPieceState::Nailed;
	UpdateVisualFeedback();

	UE_LOG(LogTemp, Log, TEXT("Piece nailed in place - now locked"));
}

void ABuildablePiece::Remove()
{
	if (AConstructionPhaseManager::Instance)
	{
		AConstructionPhaseManager::Instance->UnregisterPiece(this);
	}

	if (bIsSnapped && SnappedToPiece != nullptr)
	{
		SnappedToPiece->FreeSocket(SnappedToSocketName);
	}

	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.bIsOccupied && Socket.ConnectedPiece.IsValid())
		{
			Socket.bIsOccupied = false;
			Socket.ConnectedPiece = nullptr;
		}
	}

	Destroy();
}

bool ABuildablePiece::IsPlacementValid() const
{
	if (!AConstructionPhaseManager::Instance) return false;

	if (!AConstructionPhaseManager::Instance->CanPlacePieceType(PieceType))
		return false;

	if (!AConstructionPhaseManager::Instance->CheckPrerequisites(PieceType, GetActorLocation()))
		return false;

	if (!IsSupported())
		return false;

	if (PieceType != EPieceType::Foundation && !bIsSnapped)
		return false;

	return true;
}

bool ABuildablePiece::IsSupported() const
{
	if (PieceType == EPieceType::Foundation)
		return true;

	return bIsSnapped;
}

void ABuildablePiece::UpdateVisualFeedback()
{
	if (!MeshComponent) return;

	if (PieceState == EPieceState::Nailed && NailedMaterial)
	{
		MeshComponent->SetMaterial(0, NailedMaterial);
		MeshComponent->SetRenderCustomDepth(false);
		return;
	}

	if (!DynamicMaterial) return;

	FLinearColor TargetColor;

	switch (PieceState)
	{
		case EPieceState::Preview:
			TargetColor = IsPlacementValid() ? ValidPlacementColor : InvalidPlacementColor;
			break;
		case EPieceState::Placed:
			TargetColor = PlacedColor;
			break;
		case EPieceState::Nailed:
			TargetColor = NailedColor;
			break;
		default:
			TargetColor = FLinearColor::White;
			break;
	}

	DynamicMaterial->SetVectorParameterValue(FName("BaseColor"), TargetColor);

	if (PieceState == EPieceState::Preview)
		MeshComponent->SetRenderCustomDepth(true);
	else
		MeshComponent->SetRenderCustomDepth(false);
}

void ABuildablePiece::OccupySocket(FName SocketName, ABuildablePiece* ConnectingPiece)
{
	FConstructionSocket* Socket = GetSocketByName(SocketName);
	if (Socket)
	{
		Socket->bIsOccupied = true;
		Socket->ConnectedPiece = ConnectingPiece;
		UE_LOG(LogTemp, Log, TEXT("Socket '%s' occupied by %s"), *SocketName.ToString(), *ConnectingPiece->GetName());
	}
}

void ABuildablePiece::FreeSocket(FName SocketName)
{
	FConstructionSocket* Socket = GetSocketByName(SocketName);
	if (Socket)
	{
		Socket->bIsOccupied = false;
		Socket->ConnectedPiece = nullptr;
		UE_LOG(LogTemp, Log, TEXT("Socket '%s' freed"), *SocketName.ToString());
	}
}

void ABuildablePiece::RotateLeft()
{
	if (PieceState == EPieceState::Nailed) return;

	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw -= RotationStep;
	SetActorRotation(NewRotation);
	bIsSnapped = false;
	UE_LOG(LogTemp, Log, TEXT("RotateLeft: New rotation Yaw=%.1f"), NewRotation.Yaw);
}

void ABuildablePiece::RotateRight()
{
	if (PieceState == EPieceState::Nailed) return;

	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw += RotationStep;
	SetActorRotation(NewRotation);
	bIsSnapped = false;
	UE_LOG(LogTemp, Log, TEXT("RotateRight: New rotation Yaw=%.1f"), NewRotation.Yaw);
}

void ABuildablePiece::RotateFront()
{
	if (PieceState == EPieceState::Nailed) return;

	FRotator NewRotation = GetActorRotation();
	NewRotation.Pitch += RotationStep;
	SetActorRotation(NewRotation);
	bIsSnapped = false;
}

void ABuildablePiece::RotateBack()
{
	if (PieceState == EPieceState::Nailed) return;

	FRotator NewRotation = GetActorRotation();
	NewRotation.Pitch -= RotationStep;
	SetActorRotation(NewRotation);
	bIsSnapped = false;
}

void ABuildablePiece::RotateRoll(float Angle)
{
	if (PieceState == EPieceState::Nailed) return;

	FRotator NewRotation = GetActorRotation();
	NewRotation.Roll += Angle;
	SetActorRotation(NewRotation);
	bIsSnapped = false;
}

void ABuildablePiece::ScalePiece(float ScaleDelta)
{
	if (PieceState == EPieceState::Nailed) return;

	float NewScaleValue = CurrentScale.X + ScaleDelta;
	NewScaleValue = FMath::Clamp(NewScaleValue, MinScale, MaxScale);

	CurrentScale = FVector(NewScaleValue, NewScaleValue, NewScaleValue);
	SetActorScale3D(CurrentScale);

	UE_LOG(LogTemp, Log, TEXT("Piece scaled to %f"), NewScaleValue);
}
