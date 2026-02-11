// Born To Shine - Base class for all buildable construction pieces

#include "BuildablePiece.h"
#include "RimBoard.h"
#include "SocketManager.h"
#include "ConstructionPhaseManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/PlayerController.h"
#include "SnapRuleTable.h"

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
	// DUAL-END CORNER DETECTION (for closing board only — board 4)
	// Only fires when 3+ rim boards are already placed (U-shape).
	// With only 2 boards (L-shape), single-end corner snap handles board 3.
	// NOTE: No mutation here -- AutoResizeLengthFeet is recorded, not applied.
	// ============================================================
	int32 PlacedRimBoardCount = 0;
	for (ABuildablePiece* Piece : NearbyPieces)
	{
		if (Piece && Piece->GetPieceType() == EPieceType::RimBoard)
		{
			PlacedRimBoardCount++;
		}
	}

	if (PieceType == EPieceType::RimBoard && PlacedRimBoardCount >= 3)
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

						// CRITICAL: Validate that the two target corners form a roughly straight span.
						// For dual-end to work (board 4 closing a rectangle), the span direction
						// between the two open corners must be roughly parallel to one of the target boards.
						// If it's diagonal (~45°), this is an L-shape where single-end snap should handle it.
						FVector SpanDir = (RC.TargetWorldPos - LC.TargetWorldPos).GetSafeNormal();

						// Check alignment against both target boards
						FVector LC_Forward = LC.TargetPiece->GetActorRotation().RotateVector(FVector::ForwardVector);
						FVector RC_Forward = RC.TargetPiece->GetActorRotation().RotateVector(FVector::ForwardVector);

						float DotLC = FMath::Abs(FVector::DotProduct(SpanDir, LC_Forward));
						float DotRC = FMath::Abs(FVector::DotProduct(SpanDir, RC_Forward));

						// The span should be roughly parallel to at least one target board
						// (dot product near 1.0 = parallel, near 0.0 = perpendicular, 0.707 = 45° diagonal)
						// Require at least 0.8 alignment (~37° tolerance) with one of the boards
						float BestAlignment = FMath::Max(DotLC, DotRC);
						if (BestAlignment < 0.8f)
						{
							// Span is diagonal — skip this pair (let single-end corner snap handle it)
							continue;
						}

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

					Candidates.Add(DualCandidate);
				}
			}
		}
	}

	// ============================================================
	// PRE-COMPUTE STABLE PLYWOOD REFERENCE YAW
	// Plywood must use a single consistent yaw for ALL candidates
	// to prevent rotation flipping as different snap targets win.
	// Pick the rim board direction closest to world X-axis (yaw=0).
	// ============================================================
	float PlywoodRefYaw = 0.0f;
	bool bHavePlywoodRefYaw = false;

	if (PieceType == EPieceType::Plywood)
	{
		float BestNormYaw = 360.0f;

		for (ABuildablePiece* P : NearbyPieces)
		{
			if (!P || P->GetPieceType() != EPieceType::RimBoard) continue;

			float Y = P->GetActorRotation().Yaw;
			// Normalize to [0, 180) — boards at 0 and 180 are the same direction
			float NormY = FMath::Fmod(Y, 180.0f);
			if (NormY < 0.0f) NormY += 180.0f;

			if (NormY < BestNormYaw)
			{
				BestNormYaw = NormY;
				PlywoodRefYaw = NormY;
				bHavePlywoodRefYaw = true;
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

			// Special handling for joist-to-rim-board top face snaps
			if (Socket.SocketType == EConstructionSocketType::Joist_End &&
				TgtSocketType == EConstructionSocketType::RimBoard_Top_Face &&
				TargetPiece)
			{
				// Joist runs perpendicular to the rim board
				FRotator TargetRotation = TargetPiece->GetActorRotation();
				CandidateRotation.Yaw = TargetRotation.Yaw + 90.0f;
			}

			// Special handling for bottom plate-to-rim board top face snaps
			// Plate aligns WITH the rim board below (same yaw direction)
			if (Socket.SocketType == EConstructionSocketType::BottomPlate_Bottom &&
				TgtSocketType == EConstructionSocketType::RimBoard_Top_Face &&
				TargetPiece)
			{
				CandidateRotation.Pitch = 0.0f;
				CandidateRotation.Roll = 0.0f;
				CandidateRotation.Yaw = TargetPiece->GetActorRotation().Yaw;
			}

			// Special handling for bottom plate end-to-end snaps (corners/inline)
			if (Socket.SocketType == EConstructionSocketType::BottomPlate_End &&
				TgtSocketType == EConstructionSocketType::BottomPlate_End &&
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
					// INLINE: Right->Left or Left->Right
					CandidateRotation.Yaw = TargetRotation.Yaw;
					bCandidateIsInline = true;
				}
				else
				{
					// CORNER: Left->Left or Right->Right
					FVector TargetRight = TargetRotation.RotateVector(FVector::RightVector);
					FVector PlayerLookDir = FVector::ZeroVector;
					if (UWorld* World = GetWorld())
					{
						APlayerController* PC = World->GetFirstPlayerController();
						if (PC)
						{
							FVector CamLoc;
							FRotator CamRot;
							PC->GetPlayerViewPoint(CamLoc, CamRot);
							PlayerLookDir = CamRot.Vector();
						}
					}
					float DotResult = FVector::DotProduct(PlayerLookDir, TargetRight);
					float RotationSign = (DotResult > 0) ? 90.0f : -90.0f;
					CandidateRotation.Yaw = TargetRotation.Yaw + RotationSign;
					bCandidateIsCorner = true;
				}
			}

			// Special handling for plywood-to-framing top face snaps
			// Use pre-computed stable reference yaw to prevent rotation
			// flipping as different target pieces win the snap contest.
			if ((Socket.SocketType == EConstructionSocketType::Plywood_Edge ||
				 Socket.SocketType == EConstructionSocketType::Plywood_Corner) &&
				(TgtSocketType == EConstructionSocketType::Joist_Top_Face ||
				 TgtSocketType == EConstructionSocketType::RimBoard_Top_Face) &&
				bHavePlywoodRefYaw)
			{
				CandidateRotation.Pitch = 0.0f;
				CandidateRotation.Roll = 0.0f;
				CandidateRotation.Yaw = PlywoodRefYaw;
			}

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
				}
			}

			// Calculate final actor position from socket alignment
			FVector SocketLocalOffset = Socket.LocalPosition;
			FVector SocketWorldOffset = CandidateRotation.RotateVector(SocketLocalOffset);
			FVector CandidateLocation = SnapLoc - SocketWorldOffset;

			// Joist top-face snap: lower joist so its top is flush with rim board top.
			// Snap location is at top-face socket (rim center Z + halfHeight).
			// Joist center should be at rim center Z, so subtract halfHeight.
			if (Socket.SocketType == EConstructionSocketType::Joist_End &&
				TgtSocketType == EConstructionSocketType::RimBoard_Top_Face)
			{
				const float BoardHeight = 13.97f; // 5.5"
				CandidateLocation.Z -= BoardHeight / 2.0f;
			}

			// Plywood Z offset: TopFace/JoistTop sockets are at BoardHeight/2
			// (midpoint of the framing mesh), but the actual mesh top is at
			// BoardHeight above the actor origin. Push plywood UP by BoardHeight/2
			// so its bottom sits on the real framing top surface.
			if (Socket.SocketType == EConstructionSocketType::Plywood_Edge ||
				Socket.SocketType == EConstructionSocketType::Plywood_Corner)
			{
				if (TgtSocketType == EConstructionSocketType::Joist_Top_Face ||
					TgtSocketType == EConstructionSocketType::RimBoard_Top_Face)
				{
					const float BoardHalfHeight = 13.97f / 2.0f; // 6.985cm
					CandidateLocation.Z += BoardHalfHeight;
				}

				UE_LOG(LogTemp, Warning, TEXT("PLYWOOD SNAP Z: src=%s tgt=%s SnapLoc.Z=%.2f SocketOffset.Z=%.2f ActorZ=%.2f"),
					*Socket.SocketName.ToString(),
					*TargetSocketName.ToString(),
					SnapLoc.Z,
					SocketWorldOffset.Z,
					CandidateLocation.Z);
			}

			// Bottom plate Z offset: plate sits on top of plywood, which sits on
			// top of the rim board. From the TopFace socket (at rim board midpoint):
			//   + RimBoardHalfHeight (6.985cm) to reach actual mesh top
			//   + PlywoodThickness (1.905cm) for plywood sheet on top
			// Total = 8.89cm above the TopFace socket position
			if (Socket.SocketType == EConstructionSocketType::BottomPlate_Bottom &&
				TgtSocketType == EConstructionSocketType::RimBoard_Top_Face)
			{
				const float RimBoardHalfHeight = 13.97f / 2.0f; // 6.985cm
				const float PlywoodThickness = 1.905f; // 3/4"
				CandidateLocation.Z += RimBoardHalfHeight + PlywoodThickness; // 8.89cm

				UE_LOG(LogTemp, Warning, TEXT("BOTTOM PLATE SNAP Z: src=%s tgt=%s SnapLoc.Z=%.2f SocketOffset.Z=%.2f ActorZ=%.2f"),
					*Socket.SocketName.ToString(),
					*TargetSocketName.ToString(),
					SnapLoc.Z,
					SocketWorldOffset.Z,
					CandidateLocation.Z);
			}

			// Bottom plate Y offset: shift inward so outside face is flush with
			// the plywood edge. Uses nearby rim boards to find the building center
			// and determine the inward direction for each wall.
			if (Socket.SocketType == EConstructionSocketType::BottomPlate_Bottom &&
				TgtSocketType == EConstructionSocketType::RimBoard_Top_Face &&
				TargetPiece)
			{
				const float PlateHalfWidth = 3.81f / 2.0f; // 1.905cm
				const float FlushTweak = 0.47625f;          // 3/16"
				const float InwardOffset = PlateHalfWidth + FlushTweak;

				FVector FrameCenter = FVector::ZeroVector;
				int32 RimCount = 0;
				for (ABuildablePiece* P : NearbyPieces)
				{
					if (P && P->GetPieceType() == EPieceType::RimBoard)
					{
						FrameCenter += P->GetActorLocation();
						RimCount++;
					}
				}
				if (RimCount > 0)
				{
					FrameCenter /= RimCount;
					FVector ToCenter = FrameCenter - TargetPiece->GetActorLocation();
					ToCenter.Z = 0.0f;
					FVector PlateRight = CandidateRotation.RotateVector(FVector::RightVector);
					float DotY = FVector::DotProduct(ToCenter, PlateRight);
					if (FMath::Abs(DotY) > KINDA_SMALL_NUMBER)
					{
						CandidateLocation += PlateRight * FMath::Sign(DotY) * InwardOffset;
					}
				}
			}

			// Plywood XY alignment: compute position from frame rectangle
			// Classify rim boards as parallel or perpendicular to plywood,
			// then use each group's WIDTH (not length) in the cross-axis
			if ((Socket.SocketType == EConstructionSocketType::Plywood_Edge ||
				 Socket.SocketType == EConstructionSocketType::Plywood_Corner) &&
				(TgtSocketType == EConstructionSocketType::RimBoard_Top_Face ||
				 TgtSocketType == EConstructionSocketType::Joist_Top_Face) &&
				TargetPiece)
			{
				FVector RefFwd = CandidateRotation.RotateVector(FVector::ForwardVector);
				FVector RefRgt = CandidateRotation.RotateVector(FVector::RightVector);
				FVector RefOrigin = TargetPiece->GetActorLocation();
				const float HalfW = 3.81f / 2.0f; // Rim board half-width

				// Parallel boards define tiling range (right axis, 4ft sheets)
				// Perpendicular boards define spanning range (forward axis, 8ft sheet)
				float TileMin = FLT_MAX, TileMax = -FLT_MAX;
				float SpanMin = FLT_MAX, SpanMax = -FLT_MAX;
				int32 ParallelCount = 0, PerpCount = 0;

				for (ABuildablePiece* P : NearbyPieces)
				{
					if (!P || P->GetPieceType() != EPieceType::RimBoard) continue;

					FVector BoardFwd = P->GetActorRotation().RotateVector(FVector::ForwardVector);
					float Dot = FMath::Abs(FVector::DotProduct(BoardFwd, RefFwd));
					FVector LocalCenter = P->GetActorLocation() - RefOrigin;

					if (Dot > 0.7f)
					{
						// Parallel board — its outer faces (width) define tiling range
						ParallelCount++;
						float CenterRgt = FVector::DotProduct(LocalCenter, RefRgt);
						TileMin = FMath::Min(TileMin, CenterRgt - HalfW);
						TileMax = FMath::Max(TileMax, CenterRgt + HalfW);
					}
					else
					{
						// Perpendicular board — its outer faces (width) define span range
						PerpCount++;
						float CenterFwd = FVector::DotProduct(LocalCenter, RefFwd);
						SpanMin = FMath::Min(SpanMin, CenterFwd - HalfW);
						SpanMax = FMath::Max(SpanMax, CenterFwd + HalfW);
					}
				}

				if (ParallelCount >= 2 && PerpCount >= 2)
				{
					const float SheetShort = 121.92f; // 4ft nominal

					// Center plywood along forward axis to span between perp boards
					float PlywoodFwd = (SpanMin + SpanMax) / 2.0f;

					// Frame-fitted slot spacing: divide actual frame width by number
					// of sheets so each slot covers its portion of the frame exactly.
					// The mesh is pre-scaled to match this effective width.
					float FrameWidth = TileMax - TileMin;
					int32 NumSheets = FMath::Max(1, FMath::RoundToInt(FrameWidth / SheetShort));
					float EffSlotWidth = FrameWidth / NumSheets;

					// Pick the slot closest to the player's current placement
					FVector CurrentOffset = CandidateLocation - RefOrigin;
					float CurrentRgt = FVector::DotProduct(CurrentOffset, RefRgt);

					float BestSlot = TileMin + EffSlotWidth * 0.5f;
					float BestDist = FLT_MAX;
					for (int32 si = 0; si < NumSheets; si++)
					{
						float SlotCenter = TileMin + EffSlotWidth * (si + 0.5f);
						float D = FMath::Abs(CurrentRgt - SlotCenter);
						if (D < BestDist)
						{
							BestDist = D;
							BestSlot = SlotCenter;
						}
					}

					float SavedZ = CandidateLocation.Z;
					CandidateLocation = RefOrigin + RefFwd * PlywoodFwd + RefRgt * BestSlot;
					CandidateLocation.Z = SavedZ;

					UE_LOG(LogTemp, Warning, TEXT("PLYWOOD FRAME: Tile=[%.1f,%.1f] Span=[%.1f,%.1f] Slot=%d/%d EffW=%.1f Ctr=(%.1f,%.1f)"),
						TileMin, TileMax, SpanMin, SpanMax,
						(int32)((BestSlot - TileMin) / EffSlotWidth) + 1, NumSheets,
						EffSlotWidth,
						CandidateLocation.X, CandidateLocation.Y);
				}
			}

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
	FVector FinalLocation = Candidate.SnapLocation;
	FRotator FinalRotation = Candidate.SnapRotation;

	// Force plywood and wall plates perfectly flat — only yaw varies
	if (PieceType == EPieceType::Plywood || PieceType == EPieceType::WallPlate)
	{
		FinalRotation.Pitch = 0.0f;
		FinalRotation.Roll = 0.0f;
	}

	SetActorRotation(FinalRotation);

	// Corner joints: boards stay at centerline positions (centered on foundation).
	// Small overlap at corners is acceptable — boards sit centered on their foundations.
	SetActorLocation(FinalLocation);

	// Debug: Log final Z for plywood placements
	if (PieceType == EPieceType::Plywood)
	{
		UE_LOG(LogTemp, Warning, TEXT("PLYWOOD FINAL POS: Actor Z=%.2f  SnapTo=%s (socket=%s) Priority=%d"),
			FinalLocation.Z,
			Candidate.TargetPiece ? *Candidate.TargetPiece->GetName() : TEXT("null"),
			*Candidate.TargetSocketName.ToString(),
			Candidate.Priority);
	}

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

		// Also mark THIS piece's source socket as occupied (bidirectional)
		if (CurrentSnapCandidate.SourceSocketName != NAME_None)
		{
			OccupySocket(CurrentSnapCandidate.SourceSocketName, SnappedToPiece);
		}
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

	// Plywood edge to Joist/Rim top face (MEDIUM PRIORITY)
	if ((SocketA == EConstructionSocketType::Plywood_Edge &&
		 (SocketB == EConstructionSocketType::Joist_Top_Face ||
		  SocketB == EConstructionSocketType::RimBoard_Top_Face)) ||
		((SocketA == EConstructionSocketType::Joist_Top_Face ||
		  SocketA == EConstructionSocketType::RimBoard_Top_Face) &&
		 SocketB == EConstructionSocketType::Plywood_Edge))
	{
		return 600;
	}

	// Plywood edge to Plywood edge (sheet-to-sheet, MEDIUM PRIORITY)
	if (SocketA == EConstructionSocketType::Plywood_Edge &&
		SocketB == EConstructionSocketType::Plywood_Edge)
	{
		return 500;
	}

	// Bottom plate end-to-end (corner/inline connections)
	if (SocketA == EConstructionSocketType::BottomPlate_End &&
		SocketB == EConstructionSocketType::BottomPlate_End)
	{
		return 900;
	}

	// Bottom plate bottom to Rim board top face
	if ((SocketA == EConstructionSocketType::BottomPlate_Bottom &&
		 SocketB == EConstructionSocketType::RimBoard_Top_Face) ||
		(SocketA == EConstructionSocketType::RimBoard_Top_Face &&
		 SocketB == EConstructionSocketType::BottomPlate_Bottom))
	{
		return 700;
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
