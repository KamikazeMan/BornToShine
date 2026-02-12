// Born To Shine - Base class for all buildable construction pieces

#include "BuildablePiece.h"
#include "RimBoard.h"
#include "CornerPost.h"
#include "BottomPlate.h"
#include "SocketManager.h"
#include "ConstructionPhaseManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
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

	// Ensure mesh responds to line traces (visibility channel).
	// QueryOnly is enough for traces (no physics needed for building pieces).
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);

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
	PreviewMaterialBase = nullptr;  // Set in Blueprint for translucent preview; falls back to engine default
	OriginalMeshMaterial = nullptr;
	bAutoNailOnPlace = false;   // Override to true for foundation blocks
}

void ABuildablePiece::BeginPlay()
{
	Super::BeginPlay();

	InitializeSockets();

	// Save the "real" material for restoration when placed/nailed.
	// If NailedMaterial is set in Blueprint, use that — the mesh may start
	// with M_PreviewPiece (the ghost material) so GetMaterial(0) would be wrong.
	if (NailedMaterial)
	{
		OriginalMeshMaterial = NailedMaterial;
	}
	else if (MeshComponent && MeshComponent->GetMaterial(0))
	{
		OriginalMeshMaterial = MeshComponent->GetMaterial(0);
	}

	// Create a preview Material Instance Dynamic for ghost preview (green/red colored).
	// Use PreviewMaterialBase if set (should be a translucent material for best results).
	// Falls back to engine default material (opaque, but responds to BaseColor parameter).
	UMaterialInterface* PreviewBase = PreviewMaterialBase;
	if (!PreviewBase)
	{
		PreviewBase = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	if (PreviewBase && MeshComponent)
	{
		// Guard: never create MID from another MID (Unreal rejects MID-from-MID parent chains)
		if (Cast<UMaterialInstanceDynamic>(PreviewBase))
		{
			DynamicMaterial = Cast<UMaterialInstanceDynamic>(PreviewBase);
		}
		else
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(PreviewBase, this);
		}

		// Pieces start in Preview state — apply the preview material
		MeshComponent->SetMaterial(0, DynamicMaterial);

		if (!PreviewMaterialBase)
		{
			UE_LOG(LogTemp, Log, TEXT("%s: Using engine default for preview. Set PreviewMaterialBase in Blueprint for translucent ghost."), *GetName());
		}
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

			// Swap to preview material (green/red ghost)
			if (DynamicMaterial)
			{
				MeshComponent->SetMaterial(0, DynamicMaterial);
			}
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

			// Restore original material (piece is now placed, show real appearance)
			if (OriginalMeshMaterial)
			{
				MeshComponent->SetMaterial(0, OriginalMeshMaterial);
			}
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
		// Use the plywood's current yaw to decide which board direction to align to.
		// This lets the player rotate the plywood to choose between the two
		// perpendicular frame directions (e.g. 0° vs 90°).
		float CurrentYaw = GetActorRotation().Yaw;
		float NormCurrentYaw = FMath::Fmod(CurrentYaw, 180.0f);
		if (NormCurrentYaw < 0.0f) NormCurrentYaw += 180.0f;

		float BestYawDiff = 360.0f;

		for (ABuildablePiece* P : NearbyPieces)
		{
			if (!P || P->GetPieceType() != EPieceType::RimBoard) continue;

			float Y = P->GetActorRotation().Yaw;
			// Normalize to [0, 180) — boards at 0 and 180 are the same direction
			float NormY = FMath::Fmod(Y, 180.0f);
			if (NormY < 0.0f) NormY += 180.0f;

			// Pick the direction closest to the plywood's current rotation
			float YawDiff = FMath::Abs(NormCurrentYaw - NormY);
			if (YawDiff > 90.0f) YawDiff = 180.0f - YawDiff;

			if (YawDiff < BestYawDiff)
			{
				BestYawDiff = YawDiff;
				PlywoodRefYaw = NormY;
				bHavePlywoodRefYaw = true;
			}
		}
	}

	// ============================================================
	// PRE-COMPUTE PLYWOOD SLOT POSITION (once for all candidates)
	// Uses the FRAME CENTROID as origin so all candidates get the
	// same tile/span ranges and the same slot selection.
	// ============================================================
	FVector PlywoodSlotXY = FVector::ZeroVector;
	bool bHavePlywoodSlot = false;

	if (PieceType == EPieceType::Plywood && bHavePlywoodRefYaw)
	{
		FRotator PlywoodRot(0.0f, PlywoodRefYaw, 0.0f);
		FVector RefFwd = PlywoodRot.RotateVector(FVector::ForwardVector);
		FVector RefRgt = PlywoodRot.RotateVector(FVector::RightVector);

		// Compute frame centroid as a fixed reference origin
		FVector FrameCenter = FVector::ZeroVector;
		int32 FramePieceCount = 0;
		for (ABuildablePiece* P : NearbyPieces)
		{
			if (P && P->GetPieceType() == EPieceType::RimBoard)
			{
				FrameCenter += P->GetActorLocation();
				FramePieceCount++;
			}
		}
		if (FramePieceCount > 0) FrameCenter /= FramePieceCount;

		const float HalfW = 3.81f / 2.0f; // Rim board half-width

		float TileMin = FLT_MAX, TileMax = -FLT_MAX;
		float SpanMin = FLT_MAX, SpanMax = -FLT_MAX;
		int32 ParallelCount = 0, PerpCount = 0;

		for (ABuildablePiece* P : NearbyPieces)
		{
			if (!P || P->GetPieceType() != EPieceType::RimBoard) continue;

			FVector BoardFwd = P->GetActorRotation().RotateVector(FVector::ForwardVector);
			float Dot = FMath::Abs(FVector::DotProduct(BoardFwd, RefFwd));
			FVector LocalCenter = P->GetActorLocation() - FrameCenter;

			if (Dot > 0.7f)
			{
				ParallelCount++;
				float CenterRgt = FVector::DotProduct(LocalCenter, RefRgt);
				TileMin = FMath::Min(TileMin, CenterRgt - HalfW);
				TileMax = FMath::Max(TileMax, CenterRgt + HalfW);
			}
			else
			{
				PerpCount++;
				float CenterFwd = FVector::DotProduct(LocalCenter, RefFwd);
				SpanMin = FMath::Min(SpanMin, CenterFwd - HalfW);
				SpanMax = FMath::Max(SpanMax, CenterFwd + HalfW);
			}
		}

		if (ParallelCount >= 2 && PerpCount >= 2)
		{
			const float SheetShort = 121.92f; // 4ft
			const float SheetLong = 243.84f;  // 8ft

			// Tiling: divide frame width into 4ft columns
			float FrameWidth = TileMax - TileMin;
			int32 NumCols = FMath::Max(1, FMath::RoundToInt(FrameWidth / SheetShort));
			float ColWidth = FrameWidth / NumCols;

			// Spanning: divide frame depth into 8ft rows
			float FrameDepth = SpanMax - SpanMin;
			int32 NumRows = FMath::Max(1, FMath::RoundToInt(FrameDepth / SheetLong));
			float RowDepth = FrameDepth / NumRows;

			// Use player's aimed position (actor location = crosshair hit)
			FVector AimedOffset = GetActorLocation() - FrameCenter;
			float AimedRgt = FVector::DotProduct(AimedOffset, RefRgt);
			float AimedFwd = FVector::DotProduct(AimedOffset, RefFwd);

			// Pick closest column
			float BestCol = TileMin + ColWidth * 0.5f;
			float BestColDist = FLT_MAX;
			for (int32 ci = 0; ci < NumCols; ci++)
			{
				float ColCenter = TileMin + ColWidth * (ci + 0.5f);
				float D = FMath::Abs(AimedRgt - ColCenter);
				if (D < BestColDist) { BestColDist = D; BestCol = ColCenter; }
			}

			// Pick closest row
			float BestRow = SpanMin + RowDepth * 0.5f;
			float BestRowDist = FLT_MAX;
			for (int32 ri = 0; ri < NumRows; ri++)
			{
				float RowCenter = SpanMin + RowDepth * (ri + 0.5f);
				float D = FMath::Abs(AimedFwd - RowCenter);
				if (D < BestRowDist) { BestRowDist = D; BestRow = RowCenter; }
			}

			PlywoodSlotXY = FrameCenter + RefFwd * BestRow + RefRgt * BestCol;
			bHavePlywoodSlot = true;

			int32 ChosenCol = (int32)((BestCol - TileMin) / ColWidth) + 1;
			int32 ChosenRow = (int32)((BestRow - SpanMin) / RowDepth) + 1;
			UE_LOG(LogTemp, Warning, TEXT("PLYWOOD SLOT: Col=%d/%d Row=%d/%d FrameW=%.1f FrameD=%.1f Aim=(%.1f,%.1f) Pos=(%.1f,%.1f)"),
				ChosenCol, NumCols, ChosenRow, NumRows,
				FrameWidth, FrameDepth,
				AimedRgt, AimedFwd,
				PlywoodSlotXY.X, PlywoodSlotXY.Y);
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

			// Special handling for wall stud-to-bottom plate snaps
			// Stud matches plate yaw (1.5" edge along wall), forced vertical (no pitch/roll)
			if (Socket.SocketType == EConstructionSocketType::Wall_Stud_Bottom &&
				TgtSocketType == EConstructionSocketType::Wall_Bottom_Plate &&
				TargetPiece)
			{
				CandidateRotation.Pitch = 0.0f;
				CandidateRotation.Roll = 0.0f;
				CandidateRotation.Yaw = TargetPiece->GetActorRotation().Yaw;
			}

			// Special handling for corner post-to-plate snaps
			// Orient the inside corner of the L-shaped post toward the building center.
			// Calculate frame center from rim boards, then set yaw so the L's concave
			// face (inside corner at 45° from forward) points at the center.
			if (Socket.SocketType == EConstructionSocketType::CornerPost_Bottom &&
				TgtSocketType == EConstructionSocketType::CornerPost_Seat &&
				TargetPiece)
			{
				CandidateRotation.Pitch = 0.0f;
				CandidateRotation.Roll = 0.0f;

				FVector FrameCenterRot = FVector::ZeroVector;
				int32 RimCountRot = 0;
				for (ABuildablePiece* P : NearbyPieces)
				{
					if (P && P->GetPieceType() == EPieceType::RimBoard)
					{
						FrameCenterRot += P->GetActorLocation();
						RimCountRot++;
					}
				}
				if (RimCountRot > 0)
				{
					FrameCenterRot /= RimCountRot;
					FVector ToCenter = FrameCenterRot - SnapLoc;
					ToCenter.Z = 0.0f;

					// Angle from corner toward frame center
					float ToCenterYaw = FMath::RadiansToDegrees(FMath::Atan2(ToCenter.Y, ToCenter.X));

					// The L-shaped post's inside corner bisects its two stud faces at 45°
					// from the forward axis.  Snap to the nearest 90° so the post aligns
					// cleanly with the rectangular building walls.
					CandidateRotation.Yaw = FMath::RoundToFloat((ToCenterYaw - 45.0f) / 90.0f) * 90.0f;
				}
				else
				{
					// Fallback when no rim boards found: match plate yaw
					CandidateRotation.Yaw = TargetPiece->GetActorRotation().Yaw;
				}
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

			// Corner post snap diagnostics
			if (Socket.SocketType == EConstructionSocketType::CornerPost_Bottom &&
				TgtSocketType == EConstructionSocketType::CornerPost_Seat)
			{
				UE_LOG(LogTemp, Log,
					TEXT("CornerPost SNAP: SocketLocal=(%.2f,%.2f,%.2f) SnapLoc=(%.2f,%.2f,%.2f) "
					     "→ ActorZ=%.2f  MeshBottomZ=%.2f (should match plate top)"),
					SocketLocalOffset.X, SocketLocalOffset.Y, SocketLocalOffset.Z,
					SnapLoc.X, SnapLoc.Y, SnapLoc.Z,
					CandidateLocation.Z,
					CandidateLocation.Z + SocketLocalOffset.Z);
			}

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
			// top of the rim board. Socket alignment already accounts for:
			//   - RimBoard_Top_Face socket at BoardHeight/2 above rim center
			//   - BottomPlate_Bottom socket at -PlateHeight/2 below plate center
			// So the actor center is already at: rimTop + plateHalfHeight.
			// We only need to add the plywood thickness gap between rim top and plate bottom.
			if (Socket.SocketType == EConstructionSocketType::BottomPlate_Bottom &&
				TgtSocketType == EConstructionSocketType::RimBoard_Top_Face)
			{
				const float PlywoodThickness = 1.905f; // 3/4"
				CandidateLocation.Z += PlywoodThickness;

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

			// Corner post Z correction + flush alignment.
			// Z: read the plate mesh's actual top surface so the post lands
			// on the real visual top (not the socket approximation).
			// XY: offset 1.905cm toward the frame center on BOTH world axes
			// (corner post sits where two plates meet, needs flush on both).
			if (Socket.SocketType == EConstructionSocketType::CornerPost_Bottom &&
				TgtSocketType == EConstructionSocketType::CornerPost_Seat &&
				TargetPiece)
			{
				// --- Z correction: land on the plate's real top surface ---
				if (const ABottomPlate* Plate = Cast<const ABottomPlate>(TargetPiece))
				{
					UStaticMeshComponent* PlateMesh = Plate->GetMeshComponent();
					if (PlateMesh && PlateMesh->GetStaticMesh())
					{
						FBoxSphereBounds PB = PlateMesh->GetStaticMesh()->GetBounds();
						float PlateRelZ = PlateMesh->GetRelativeLocation().Z;
						float PlateActualTopZ = (PB.Origin.Z + PB.BoxExtent.Z) + PlateRelZ;
						float SocketTopZ = Plate->BoardHeight / 2.0f;
						float ZFix = PlateActualTopZ - SocketTopZ;
						CandidateLocation.Z += ZFix;

						UE_LOG(LogTemp, Log,
							TEXT("CornerPost Z-fix: PlateTopMesh=%.2f SocketTop=%.2f → correction=%.2f"),
							PlateActualTopZ, SocketTopZ, ZFix);
					}
				}

				// --- Flush: offset inward by half a stud width on BOTH axes ---
				// The corner post sits where two perpendicular plates meet, so it
				// must shift 1.905cm toward the frame center on X AND on Y.
				// This keeps the stud's outer face flush with each plate end.
				{
					const float HalfStudWidth = 3.81f / 2.0f; // 1.905cm

					FVector FrameCenterFlush = FVector::ZeroVector;
					int32 RimCountFlush = 0;
					for (ABuildablePiece* P : NearbyPieces)
					{
						if (P && P->GetPieceType() == EPieceType::RimBoard)
						{
							FrameCenterFlush += P->GetActorLocation();
							RimCountFlush++;
						}
					}
					if (RimCountFlush > 0)
					{
						FrameCenterFlush /= RimCountFlush;
						FVector ToCenter = FrameCenterFlush - CandidateLocation;
						ToCenter.Z = 0.0f;

						// Shift 1.905cm toward center on each world axis independently
						if (FMath::Abs(ToCenter.X) > KINDA_SMALL_NUMBER)
						{
							CandidateLocation.X += FMath::Sign(ToCenter.X) * HalfStudWidth;
						}
						if (FMath::Abs(ToCenter.Y) > KINDA_SMALL_NUMBER)
						{
							CandidateLocation.Y += FMath::Sign(ToCenter.Y) * HalfStudWidth;
						}

						UE_LOG(LogTemp, Log,
							TEXT("CornerPost flush: shifted %.2fcm on X (sign=%.0f) and Y (sign=%.0f) toward center (%.1f,%.1f)"),
							HalfStudWidth,
							FMath::Sign(ToCenter.X), FMath::Sign(ToCenter.Y),
							FrameCenterFlush.X, FrameCenterFlush.Y);
					}
				}
			}

			// Plywood XY alignment: apply pre-computed slot position
			// (computed once before the loop using frame centroid as origin)
			if (bHavePlywoodSlot &&
				(Socket.SocketType == EConstructionSocketType::Plywood_Edge ||
				 Socket.SocketType == EConstructionSocketType::Plywood_Corner))
			{
				float SavedZ = CandidateLocation.Z;
				CandidateLocation.X = PlywoodSlotXY.X;
				CandidateLocation.Y = PlywoodSlotXY.Y;
				CandidateLocation.Z = SavedZ;
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

	// Force plywood, wall plates, and wall studs vertical — only yaw varies
	if (PieceType == EPieceType::Plywood || PieceType == EPieceType::WallPlate || PieceType == EPieceType::WallStud)
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

	// For dual-end snaps, also occupy the second target socket AND
	// the second source socket (the other end of this piece).
	// Without this, the unused corner socket stays "open" and
	// attracts new boards from adjacent sections.
	if (CurrentSnapCandidate.bIsDualEndSnap && CurrentSnapCandidate.SecondTargetPiece != nullptr)
	{
		CurrentSnapCandidate.SecondTargetPiece->OccupySocket(
			CurrentSnapCandidate.SecondTargetSocketName, this);

		// Find and occupy this piece's second corner socket (the one NOT used as SourceSocketName)
		for (const FConstructionSocket& S : Sockets)
		{
			if (S.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
				S.SocketName != CurrentSnapCandidate.SourceSocketName &&
				!S.bIsOccupied)
			{
				OccupySocket(S.SocketName, CurrentSnapCandidate.SecondTargetPiece);
				break;
			}
		}
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

	// Wall stud bottom to bottom plate top (stud sits on plate)
	if ((SocketA == EConstructionSocketType::Wall_Stud_Bottom &&
		 SocketB == EConstructionSocketType::Wall_Bottom_Plate) ||
		(SocketA == EConstructionSocketType::Wall_Bottom_Plate &&
		 SocketB == EConstructionSocketType::Wall_Stud_Bottom))
	{
		return 750;
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

	// Free the target socket we originally snapped to
	if (bIsSnapped && SnappedToPiece != nullptr)
	{
		SnappedToPiece->FreeSocket(SnappedToSocketName);
	}

	// Free sockets on ALL connected pieces that reference this piece
	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.bIsOccupied && Socket.ConnectedPiece.IsValid())
		{
			ABuildablePiece* Connected = Socket.ConnectedPiece.Get();
			if (Connected)
			{
				// Find and free the socket on the connected piece that points back to us
				TArray<FConstructionSocket> ConnectedSockets = Connected->GetAllSockets();
				for (const FConstructionSocket& CS : ConnectedSockets)
				{
					if (CS.bIsOccupied && CS.ConnectedPiece.Get() == this)
					{
						Connected->FreeSocket(CS.SocketName);
					}
				}
			}
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

	switch (PieceState)
	{
	case EPieceState::Preview:
	{
		if (!DynamicMaterial) return;

		FLinearColor TargetColor = IsPlacementValid() ? ValidPlacementColor : InvalidPlacementColor;

		// Set color on preview MID — engine default material responds to "BaseColor"
		// Translucent user materials may use other parameter names too
		DynamicMaterial->SetVectorParameterValue(FName("BaseColor"), TargetColor);
		DynamicMaterial->SetVectorParameterValue(FName("Base Color"), TargetColor);
		DynamicMaterial->SetVectorParameterValue(FName("Color"), TargetColor);
		// Try setting opacity for translucent preview materials
		DynamicMaterial->SetScalarParameterValue(FName("Opacity"), TargetColor.A);

		MeshComponent->SetRenderCustomDepth(true);
		break;
	}

	case EPieceState::Placed:
	{
		// Placed: restore original material, show with custom depth outline
		if (OriginalMeshMaterial)
		{
			MeshComponent->SetMaterial(0, OriginalMeshMaterial);
		}
		MeshComponent->SetRenderCustomDepth(true);
		break;
	}

	case EPieceState::Nailed:
	{
		// Nailed: swap to final material (NailedMaterial or original)
		if (NailedMaterial)
		{
			MeshComponent->SetMaterial(0, NailedMaterial);
		}
		else if (OriginalMeshMaterial)
		{
			MeshComponent->SetMaterial(0, OriginalMeshMaterial);
		}
		DynamicMaterial = nullptr;
		MeshComponent->SetRenderCustomDepth(false);
		break;
	}

	default:
		break;
	}
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

void ABuildablePiece::SetHighlighted(bool bHighlight)
{
	if (bIsHighlighted == bHighlight) return;
	bIsHighlighted = bHighlight;

	if (!MeshComponent) return;

	if (bHighlight)
	{
		// Save whatever material is currently on the mesh
		PreHighlightMaterial = MeshComponent->GetMaterial(0);

		// Walk up the parent chain to find a Material or MaterialInstanceConstant.
		// Unreal rejects MID-from-MID parent chains (causes "not a valid parent" error).
		UMaterialInterface* BaseMat = PreHighlightMaterial;
		while (UMaterialInstanceDynamic* ParentMID = Cast<UMaterialInstanceDynamic>(BaseMat))
		{
			BaseMat = ParentMID->Parent;
		}
		if (!BaseMat) BaseMat = UMaterial::GetDefaultMaterial(MD_Surface);

		UMaterialInstanceDynamic* HighlightMID = UMaterialInstanceDynamic::Create(BaseMat, this);

		// Try all common vector parameter names — at least one should match the user's material
		FLinearColor Red(1.0f, 0.15f, 0.15f, 1.0f);
		HighlightMID->SetVectorParameterValue(FName("BaseColor"), Red);
		HighlightMID->SetVectorParameterValue(FName("Base Color"), Red);
		HighlightMID->SetVectorParameterValue(FName("Color"), Red);
		HighlightMID->SetVectorParameterValue(FName("Tint"), Red);
		HighlightMID->SetVectorParameterValue(FName("DiffuseColor"), Red);

		// Also try emissive for a glow that works regardless of lighting
		FLinearColor EmissiveRed(3.0f, 0.3f, 0.3f, 1.0f);
		HighlightMID->SetVectorParameterValue(FName("EmissiveColor"), EmissiveRed);
		HighlightMID->SetVectorParameterValue(FName("Emissive Color"), EmissiveRed);
		HighlightMID->SetVectorParameterValue(FName("Emissive"), EmissiveRed);

		MeshComponent->SetMaterial(0, HighlightMID);
		MeshComponent->SetRenderCustomDepth(true);
		MeshComponent->SetCustomDepthStencilValue(1);
	}
	else
	{
		// Restore the exact material that was on the mesh before highlighting
		if (PreHighlightMaterial)
		{
			MeshComponent->SetMaterial(0, PreHighlightMaterial);
			PreHighlightMaterial = nullptr;
		}
		MeshComponent->SetRenderCustomDepth(false);
	}
}
