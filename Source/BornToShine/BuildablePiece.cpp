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
#include "RectangleBuilder.h"
#include "RidgeBoard.h"
#include "RidgePost.h"
#include "Rafter.h"
#include "WindowFrame.h"
#include "DoubleTopPlate.h"
#include "FasciaBoard.h"
#include "WallSheathing.h"
#include "RoofSheathing.h"
#include "Kismet/GameplayStatics.h"

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

	// Only allow dual-end snap when RectangleBuilder is in UShape state
	// (i.e., actually waiting for a closing board 4). Without this gate,
	// completed sections' open corner sockets cause bogus diagonal snaps.
	bool bRectangleNeedsClosingBoard = false;
	if (UWorld* World = GetWorld())
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC && PC->GetPawn())
		{
			URectangleBuilderComponent* RectBuilder = PC->GetPawn()->FindComponentByClass<URectangleBuilderComponent>();
			if (RectBuilder && RectBuilder->GetRectangleState() == ERectangleState::UShape)
			{
				bRectangleNeedsClosingBoard = true;
			}
		}
	}

	if (PieceType == EPieceType::RimBoard && PlacedRimBoardCount >= 3 && bRectangleNeedsClosingBoard)
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
					// Allow multiple sheets on the same rafter — Rafter_Top_Face can be shared
					if (TargetSocket.bIsOccupied && TargetSocket.SocketType != EConstructionSocketType::Rafter_Top_Face) continue;
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
			// Door/window frames ONLY snap to bottom plates — ignore top plates,
			// studs, and everything else so the frame always anchors to
			// the correct plate for the Z correction and plate split.
			if (PieceType == EPieceType::DoorFrame && TargetPiece &&
				TargetPiece->GetPieceType() != EPieceType::WallPlate)
			{
				UE_LOG(LogTemp, Log,
					TEXT("DoorFrame snap filter: REJECTED target %s (type %d) — only WallPlate accepted"),
					*TargetPiece->GetName(), (int32)TargetPiece->GetPieceType());
				continue;
			}
			if (PieceType == EPieceType::WindowFrame && TargetPiece &&
				TargetPiece->GetPieceType() != EPieceType::WallPlate)
			{
				continue;
			}

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

			// Door frame snaps to bottom plate the same way as wall studs
			if (Socket.SocketType == EConstructionSocketType::DoorFrame_Bottom &&
				TgtSocketType == EConstructionSocketType::Wall_Bottom_Plate &&
				TargetPiece)
			{
				CandidateRotation.Pitch = 0.0f;
				CandidateRotation.Roll = 0.0f;
				CandidateRotation.Yaw = TargetPiece->GetActorRotation().Yaw;
			}

			// Window frame snaps to bottom plate the same way as wall studs
			if (Socket.SocketType == EConstructionSocketType::WindowFrame_Bottom &&
				TgtSocketType == EConstructionSocketType::Wall_Bottom_Plate &&
				TargetPiece)
			{
				CandidateRotation.Pitch = 0.0f;
				CandidateRotation.Roll = 0.0f;
				CandidateRotation.Yaw = TargetPiece->GetActorRotation().Yaw;
			}

			// Top plate snaps to wall stud/corner post/door frame/window frame tops.
			// Plate matches the target's yaw (runs along the wall), forced flat.
			if (Socket.SocketType == EConstructionSocketType::TopPlate_Bottom &&
				(TgtSocketType == EConstructionSocketType::Wall_Stud_Top ||
				 TgtSocketType == EConstructionSocketType::CornerPost_Top ||
				 TgtSocketType == EConstructionSocketType::DoorFrame_Top ||
				 TgtSocketType == EConstructionSocketType::WindowFrame_Top) &&
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

				// Find rim boards belonging to the SAME building by flood-filling
				// from rim board corners nearest to the snap location.
				// This prevents rim boards from adjacent buildings from
				// pulling the frame center to the wrong position.
				TSet<ABuildablePiece*> SameBuildingRims;
				TArray<ABuildablePiece*> FloodQueue;

				// Seed: rim boards with corner sockets near the snap point
				for (ABuildablePiece* P : NearbyPieces)
				{
					if (!P || P->GetPieceType() != EPieceType::RimBoard) continue;
					TArray<FConstructionSocket> PSocks = P->GetAllSockets();
					for (const FConstructionSocket& PS : PSocks)
					{
						if (PS.SocketType == EConstructionSocketType::RimBoard_End_Corner)
						{
							FVector CornerWorld = P->GetActorTransform().TransformPosition(PS.LocalPosition);
							if (FVector::Dist2D(CornerWorld, SnapLoc) < 50.0f)
							{
								if (!SameBuildingRims.Contains(P))
								{
									SameBuildingRims.Add(P);
									FloodQueue.Add(P);
								}
							}
						}
					}
				}

				// Flood-fill: add rim boards connected via matching corner sockets
				while (FloodQueue.Num() > 0)
				{
					ABuildablePiece* Cur = FloodQueue.Pop();
					TArray<FConstructionSocket> CurSocks = Cur->GetAllSockets();
					for (const FConstructionSocket& CS : CurSocks)
					{
						if (CS.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;
						FVector CurCorner = Cur->GetActorTransform().TransformPosition(CS.LocalPosition);

						for (ABuildablePiece* P : NearbyPieces)
						{
							if (!P || P->GetPieceType() != EPieceType::RimBoard || SameBuildingRims.Contains(P)) continue;
							TArray<FConstructionSocket> PSocks = P->GetAllSockets();
							for (const FConstructionSocket& PS : PSocks)
							{
								if (PS.SocketType == EConstructionSocketType::RimBoard_End_Corner)
								{
									FVector PCorner = P->GetActorTransform().TransformPosition(PS.LocalPosition);
									if (FVector::Dist(CurCorner, PCorner) < 20.0f)
									{
										SameBuildingRims.Add(P);
										FloodQueue.Add(P);
									}
								}
							}
						}
					}
				}

				FVector FrameCenterRot = FVector::ZeroVector;
				int32 RimCountRot = 0;
				for (ABuildablePiece* P : SameBuildingRims)
				{
					FrameCenterRot += P->GetActorLocation();
					RimCountRot++;
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

			// Special handling for top plate end-to-end snaps (corners/inline)
			// Same pattern as bottom plate
			if (Socket.SocketType == EConstructionSocketType::TopPlate_End &&
				TgtSocketType == EConstructionSocketType::TopPlate_End &&
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

			// Rafter ridge end snaps to ridge board side:
			// The RidgeBoard_Side socket already encodes which side the rafter faces:
			//   Left sockets (RidgeBoardSide_L*) have LocalRotation.Yaw = -90
			//   Right sockets (RidgeBoardSide_R*) have LocalRotation.Yaw = +90
			// Pitch is set from the rafter's pitch angle (slopes downward from ridge to tail).
			if (Socket.SocketType == EConstructionSocketType::Rafter_Ridge &&
				TgtSocketType == EConstructionSocketType::RidgeBoard_Side &&
				TargetPiece)
			{
				FRotator TargetActorRotation = TargetPiece->GetActorRotation();
				CandidateRotation.Roll = 0.0f;

				CandidateRotation.Pitch = -24.5f;
				FConstructionSocket TgtSocket;
				float SocketYawOffset = 90.0f; // fallback
				if (TargetPiece->GetSocketByNameSafe(TargetSocketName, TgtSocket))
				{
					SocketYawOffset = TgtSocket.LocalRotation.Yaw;
				}

				// Rafter +X = along slope toward tail (from ridge toward wall).
				// Socket yaw -90 = rafter faces left, +90 = rafter faces right.
				CandidateRotation.Yaw = TargetActorRotation.Yaw + SocketYawOffset;

				UE_LOG(LogTemp, Log,
					TEXT("Rafter snap: RidgeYaw=%.1f SocketYaw=%.1f → RafterYaw=%.1f Pitch=%.1f (socket=%s)"),
					TargetActorRotation.Yaw, SocketYawOffset, CandidateRotation.Yaw, CandidateRotation.Pitch,
					*TargetSocketName.ToString());
			}

			// Rafter birdsmouth snaps to top plate / double top plate top.
			// Rafter runs PERPENDICULAR to the wall plate. The side of the plate
			// determines which direction the rafter faces — look for the nearest
			// ridge board to determine which way is "toward the ridge."
			if (Socket.SocketType == EConstructionSocketType::Rafter_BirdsMouth &&
				(TgtSocketType == EConstructionSocketType::TopPlate_Top ||
				 TgtSocketType == EConstructionSocketType::DoubleTopPlate_End) &&
				TargetPiece)
			{
				CandidateRotation.Roll = 0.0f;

				CandidateRotation.Pitch = -24.5f;
				// Find the nearest ridge board to determine direction.
				float PlateYaw = TargetPiece->GetActorRotation().Yaw;
				FVector PlateRight = FRotator(0, PlateYaw, 0).RotateVector(FVector::RightVector);

				FVector BestRidgeDir = FVector::ZeroVector;
				float BestRidgeDist = FLT_MAX;
				for (ABuildablePiece* P : NearbyPieces)
				{
					if (P && P->GetPieceType() == EPieceType::RidgeBoard)
					{
						FVector ToRidge = P->GetActorLocation() - SnapLoc;
						ToRidge.Z = 0.0f;
						float D = ToRidge.Size();
						if (D < BestRidgeDist && D > 1.0f)
						{
							BestRidgeDist = D;
							BestRidgeDir = ToRidge.GetSafeNormal();
						}
					}
				}

				if (BestRidgeDist < FLT_MAX)
				{
					// Rafter +X points from ridge TOWARD tail (downslope).
					// So rafter faces AWAY from the ridge → yaw = opposite of toward-ridge.
					float ToRidgeYaw = FMath::RadiansToDegrees(FMath::Atan2(BestRidgeDir.Y, BestRidgeDir.X));
					CandidateRotation.Yaw = ToRidgeYaw + 180.0f; // +X = away from ridge
				}
				else
				{
					// Fallback: use plate's right vector perpendicular
					CandidateRotation.Yaw = PlateYaw + 90.0f;
				}

				UE_LOG(LogTemp, Log,
					TEXT("Rafter BirdsMouth snap: PlateYaw=%.1f → RafterYaw=%.1f Pitch=%.1f (socket=%s)"),
					PlateYaw, CandidateRotation.Yaw, CandidateRotation.Pitch,
					*TargetSocketName.ToString());
			}

			// Rafter tail snaps to fascia board rafter tail sockets.
			// Rafter runs perpendicular to the fascia board (which runs along the eave).
			// Socket LocalRotation.Yaw (-90) encodes which side the rafter faces.
			if (Socket.SocketType == EConstructionSocketType::Rafter_Tail &&
				TgtSocketType == EConstructionSocketType::Fascia_RafterTail &&
				TargetPiece)
			{
				FRotator TargetActorRotation = TargetPiece->GetActorRotation();
				CandidateRotation.Roll = 0.0f;

				CandidateRotation.Pitch = -24.5f;
				FConstructionSocket TgtSocket;
				float SocketYawOffset = -90.0f; // fallback
				if (TargetPiece->GetSocketByNameSafe(TargetSocketName, TgtSocket))
				{
					SocketYawOffset = TgtSocket.LocalRotation.Yaw;
				}

				// Rafter tail points AWAY from the building.
				// The rafter's +X goes from ridge to tail.
				// Fascia socket yaw points inward, rafter +X = outward = opposite.
				CandidateRotation.Yaw = TargetActorRotation.Yaw + SocketYawOffset + 180.0f;

				UE_LOG(LogTemp, Log,
					TEXT("Rafter Tail snap: FasciaYaw=%.1f SocketYaw=%.1f → RafterYaw=%.1f Pitch=%.1f"),
					TargetActorRotation.Yaw, SocketYawOffset, CandidateRotation.Yaw, CandidateRotation.Pitch);
			}

			// Ridge board end snaps to ridge post pocket.
			// Board must be horizontal (no pitch/roll) and oriented to face
			// the OTHER ridge post so the board spans between both.
			if (Socket.SocketType == EConstructionSocketType::RidgeBoard_End &&
				TgtSocketType == EConstructionSocketType::RidgePost_Pocket &&
				TargetPiece)
			{
				CandidateRotation.Pitch = 0.0f;
				CandidateRotation.Roll = 0.0f;

				// Find the closest other ridge post to determine board direction
				// (closest ensures we pair posts on the same building, not across buildings)
				FVector ThisPostLoc = TargetPiece->GetActorLocation();
				FVector OtherPostLoc = FVector::ZeroVector;
				bool bFoundOtherPost = false;
				float BestPostDist = FLT_MAX;

				for (ABuildablePiece* P : NearbyPieces)
				{
					if (P && P != TargetPiece && P->GetPieceType() == EPieceType::RidgePost)
					{
						float D = FVector::Dist(ThisPostLoc, P->GetActorLocation());
						if (D < BestPostDist)
						{
							BestPostDist = D;
							OtherPostLoc = P->GetActorLocation();
							bFoundOtherPost = true;
						}
					}
				}

				if (bFoundOtherPost)
				{
					// Board direction: from this post toward the other post
					FVector PostDir = (OtherPostLoc - ThisPostLoc).GetSafeNormal2D();
					float BoardYaw = FMath::RadiansToDegrees(FMath::Atan2(PostDir.Y, PostDir.X));

					// Which end of the board is snapping determines direction.
					// If the LEFT end (negative X) snaps, board +X should point AWAY from this post.
					// If the RIGHT end (positive X) snaps, board +X should point TOWARD this post.
					bool bIsLeftEnd = Socket.SocketName.ToString().Contains(TEXT("Left"));
					if (bIsLeftEnd)
					{
						// Left end at this post → +X (right end) toward other post
						CandidateRotation.Yaw = BoardYaw;
					}
					else
					{
						// Right end at this post → +X toward this post = opposite direction
						CandidateRotation.Yaw = BoardYaw + 180.0f;
					}
				}
				else
				{
					// Only one post found — use the post's yaw + 90 (perpendicular to gable wall)
					CandidateRotation.Yaw = TargetPiece->GetActorRotation().Yaw + 90.0f;
				}

				UE_LOG(LogTemp, Log,
					TEXT("RidgeBoard snap: PostPos=%s OtherPost=%s → Yaw=%.1f (socket=%s)"),
					*ThisPostLoc.ToString(),
					bFoundOtherPost ? *OtherPostLoc.ToString() : TEXT("none"),
					CandidateRotation.Yaw, *Socket.SocketName.ToString());
			}

			// Ridge post snaps to double top plate — orient along wall, upright
			if (Socket.SocketType == EConstructionSocketType::RidgePost_Bottom &&
				(TgtSocketType == EConstructionSocketType::DoubleTopPlate_End ||
				 TgtSocketType == EConstructionSocketType::TopPlate_Top) &&
				TargetPiece)
			{
				CandidateRotation.Pitch = 0.0f;
				CandidateRotation.Roll = 0.0f;
				CandidateRotation.Yaw = TargetPiece->GetActorRotation().Yaw;
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
			// XY: only shift 1.905cm along the plate toward its center.
			// No perpendicular shift — the re-centered origin (first stud
			// center) naturally sits centered on the plate width.
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

				// --- Flush: 1.905cm along plate toward plate center only ---
				{
					const float HalfStudWidth = 3.81f / 2.0f; // 1.905cm

					FVector PlateForward = TargetPiece->GetActorRotation().RotateVector(FVector::ForwardVector);

					FVector SeatToPlateCenter = TargetPiece->GetActorLocation() - SnapLoc;
					SeatToPlateCenter.Z = 0.0f;

					float DotAlong = FVector::DotProduct(SeatToPlateCenter, PlateForward);
					if (FMath::Abs(DotAlong) > KINDA_SMALL_NUMBER)
					{
						CandidateLocation += PlateForward * FMath::Sign(DotAlong) * HalfStudWidth;
					}

					UE_LOG(LogTemp, Log,
						TEXT("CornerPost flush: shifted %.2fcm along plate (DotAlong=%.2f, PlateYaw=%.1f) FinalPos=(%.2f,%.2f,%.2f)"),
						HalfStudWidth, DotAlong, TargetPiece->GetActorRotation().Yaw,
						CandidateLocation.X, CandidateLocation.Y, CandidateLocation.Z);
				}
			}

			// Door frame Z correction.
			// The door frame sits on the PLYWOOD, not on top of the plate,
			// because the plate section under the door gets removed.
			// The plate lies flat (2x4: 3.81cm vertical). Its center Z was
			// calculated as PlywoodTopZ + PlateHalfHeight in CalculatePlateLayout,
			// so PlywoodTopZ = PlateCenter.Z - PlateHalfHeight.
			if (Socket.SocketType == EConstructionSocketType::DoorFrame_Bottom &&
				TgtSocketType == EConstructionSocketType::Wall_Bottom_Plate &&
				TargetPiece)
			{
				const float PlateHalfHeight = 3.81f / 2.0f; // 1.905cm — 2x4 lies flat
				float PlywoodTopZ = TargetPiece->GetActorLocation().Z - PlateHalfHeight;
				CandidateLocation.Z = PlywoodTopZ - Socket.LocalPosition.Z;

				UE_LOG(LogTemp, Log,
					TEXT("DoorFrame Z-fix: PlateCenter=%.2f PlateHalfH=%.2f PlywoodTop=%.2f FrameSocketZ=%.2f -> ActorZ=%.2f"),
					TargetPiece->GetActorLocation().Z, PlateHalfHeight,
					PlywoodTopZ, Socket.LocalPosition.Z, CandidateLocation.Z);
			}

			// Door frame centering: override XY to center on the plate's midpoint.
			// The door frame should always be centered on whatever wall it snaps to.
			if (Socket.SocketType == EConstructionSocketType::DoorFrame_Bottom &&
				TgtSocketType == EConstructionSocketType::Wall_Bottom_Plate &&
				TargetPiece)
			{
				FVector PlateCenter = TargetPiece->GetActorLocation();
				float SavedZ = CandidateLocation.Z; // Keep the Z from snap + corrections
				CandidateLocation.X = PlateCenter.X;
				CandidateLocation.Y = PlateCenter.Y;
				CandidateLocation.Z = SavedZ;

				UE_LOG(LogTemp, Log,
					TEXT("DoorFrame CENTER: Centered on plate [%s] at (%.1f, %.1f) Z=%.1f"),
					*TargetPiece->GetName(), PlateCenter.X, PlateCenter.Y, SavedZ);
			}

			// Window frame Z correction — same as door frame.
			// Window frame king studs sit on the bottom plate (not on plywood),
			// so place the frame bottom at the plate top surface.
			if (Socket.SocketType == EConstructionSocketType::WindowFrame_Bottom &&
				TgtSocketType == EConstructionSocketType::Wall_Bottom_Plate &&
				TargetPiece)
			{
				const float PlateHalfHeight = 3.81f / 2.0f; // 1.905cm
				float PlateTopZ = TargetPiece->GetActorLocation().Z + PlateHalfHeight;
				CandidateLocation.Z = PlateTopZ - Socket.LocalPosition.Z;

				UE_LOG(LogTemp, Log,
					TEXT("WindowFrame Z-fix: PlateCenter=%.2f PlateHalfH=%.2f PlateTop=%.2f FrameSocketZ=%.2f -> ActorZ=%.2f"),
					TargetPiece->GetActorLocation().Z, PlateHalfHeight,
					PlateTopZ, Socket.LocalPosition.Z, CandidateLocation.Z);
			}

			// Window frame centering: same as door frame — center on the plate.
			if (Socket.SocketType == EConstructionSocketType::WindowFrame_Bottom &&
				TgtSocketType == EConstructionSocketType::Wall_Bottom_Plate &&
				TargetPiece)
			{
				FVector PlateCenter = TargetPiece->GetActorLocation();
				float SavedZ = CandidateLocation.Z;
				CandidateLocation.X = PlateCenter.X;
				CandidateLocation.Y = PlateCenter.Y;
				CandidateLocation.Z = SavedZ;
			}

			// Wall sheathing — snaps to bottom plate, grid-locks at 4ft intervals along the wall.
			// Single socket on sheet bottom snaps to plate top face.
			if (Socket.SocketType == EConstructionSocketType::WallSheathing_Face &&
				TgtSocketType == EConstructionSocketType::Wall_Bottom_Plate &&
				TargetPiece)
			{
				// Match wall yaw
				CandidateRotation.Pitch = 0.0f;
				CandidateRotation.Roll = 0.0f;
				CandidateRotation.Yaw = TargetPiece->GetActorRotation().Yaw;

				// Z: bottom of sheet = top of bottom plate
				const float PlateHalfHeight = 3.81f / 2.0f; // 1.905cm
				float PlateTopZ = TargetPiece->GetActorLocation().Z + PlateHalfHeight;
				const AWallSheathing* WS = Cast<const AWallSheathing>(this);
				float WSHeight = WS ? WS->SheetHeight : 247.66f;
				CandidateLocation.Z = PlateTopZ + WSHeight / 2.0f;

				// Find the wall's starting corner by looking for the furthest bottom plate
				// endpoint in the negative wall direction. Sheets tile from this corner.
				FVector WallDir = FRotator(0, CandidateRotation.Yaw, 0).RotateVector(FVector::ForwardVector);
				FVector PlateOrigin = TargetPiece->GetActorLocation();

				// Find all bottom plates on this wall (same yaw) to determine wall extents
				float MinAlong = 0.0f;
				float MaxAlong = 0.0f;
				for (ABuildablePiece* P : NearbyPieces)
				{
					if (P && P->GetPieceType() == EPieceType::WallPlate)
					{
						// Check if same wall (similar yaw)
						float YawDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(
							P->GetActorRotation().Yaw, CandidateRotation.Yaw));
						if (YawDiff < 5.0f || FMath::Abs(YawDiff - 180.0f) < 5.0f)
						{
							FVector ToPlate = P->GetActorLocation() - PlateOrigin;
							float Along = FVector::DotProduct(ToPlate, WallDir);

							// Get plate half-length to find its endpoints
							ABottomPlate* BP = Cast<ABottomPlate>(P);
							float PlateHalfLen = BP ? BP->GetEffectiveLength() / 2.0f : 0.0f;
							MinAlong = FMath::Min(MinAlong, Along - PlateHalfLen);
							MaxAlong = FMath::Max(MaxAlong, Along + PlateHalfLen);
						}
					}
				}

				// Also check corner posts for wall start/end
				for (ABuildablePiece* P : NearbyPieces)
				{
					if (P && P->GetPieceType() == EPieceType::CornerPost)
					{
						FVector ToPost = P->GetActorLocation() - PlateOrigin;
						float Along = FVector::DotProduct(ToPost, WallDir);
						MinAlong = FMath::Min(MinAlong, Along);
						MaxAlong = FMath::Max(MaxAlong, Along);
					}
				}

				// Wall start = the minimum extent (left corner when facing the wall)
				float WallStartAlong = MinAlong;
				const float GridSize = 121.92f; // 4ft sheet width

				// Project candidate position onto wall direction relative to wall start
				FVector Delta = CandidateLocation - PlateOrigin;
				float AlongWall = FVector::DotProduct(Delta, WallDir);

				// Snap to nearest grid position, with sheet CENTER at WallStart + (N * GridSize) + GridSize/2
				// This puts sheet EDGES at WallStart + (N * GridSize)
				float RelativeToStart = AlongWall - WallStartAlong;
				float SheetIndex = FMath::RoundToFloat((RelativeToStart - GridSize / 2.0f) / GridSize);
				float SnappedAlong = WallStartAlong + SheetIndex * GridSize + GridSize / 2.0f;

				// Clamp so sheet doesn't extend past wall end
				float WallEndAlong = MaxAlong;
				float SheetLeftEdge = SnappedAlong - GridSize / 2.0f;
				float SheetRightEdge = SnappedAlong + GridSize / 2.0f;

				// If sheet would extend past the wall end, pull it back
				if (SheetRightEdge > WallEndAlong + 1.0f)
				{
					SnappedAlong = WallEndAlong - GridSize / 2.0f;
				}
				// If sheet would extend past the wall start, push it forward
				if (SheetLeftEdge < WallStartAlong - 1.0f)
				{
					SnappedAlong = WallStartAlong + GridSize / 2.0f;
				}

				// Apply snapped position along wall
				CandidateLocation = PlateOrigin + WallDir * SnappedAlong;
				CandidateLocation.Z = PlateTopZ + WSHeight / 2.0f;

				UE_LOG(LogTemp, Log, TEXT("WallSheathing: WallStart=%.1f, AlongWall=%.1f, SheetIdx=%.0f, SnappedAlong=%.1f"),
					WallStartAlong, AlongWall, SheetIndex, SnappedAlong);

				// Interior/exterior detection based on player camera position
				FVector FrameCenter = FVector::ZeroVector;
				int32 FrameCount = 0;
				for (ABuildablePiece* P : NearbyPieces)
				{
					if (P && (P->GetPieceType() == EPieceType::RimBoard ||
					          P->GetPieceType() == EPieceType::WallPlate))
					{
						FrameCenter += P->GetActorLocation();
						FrameCount++;
					}
				}

				if (FrameCount > 0)
				{
					FrameCenter /= FrameCount;

					FVector CamLoc = FVector::ZeroVector;
					if (UWorld* World = TargetPiece->GetWorld())
					{
						APlayerController* PC = World->GetFirstPlayerController();
						if (PC)
						{
							FRotator CamRot;
							PC->GetPlayerViewPoint(CamLoc, CamRot);
						}
					}

					// Wall's perpendicular directions
					FVector WallRight = FRotator(0, CandidateRotation.Yaw, 0).RotateVector(FVector::RightVector);
					FVector WallToCenter = FrameCenter - TargetPiece->GetActorLocation();
					WallToCenter.Z = 0.0f;
					float DotToCenter = FVector::DotProduct(WallToCenter, WallRight);

					FVector InwardDir = WallRight * FMath::Sign(DotToCenter);
					FVector OutwardDir = -InwardDir;

					FVector PlayerToWall = CamLoc - TargetPiece->GetActorLocation();
					PlayerToWall.Z = 0.0f;
					float PlayerDot = FVector::DotProduct(PlayerToWall, OutwardDir);

					bool bExterior = (PlayerDot > 0.0f);

					// Offset: stud half-depth + sheet half-thickness
					const float StudHalfDepth = 8.89f / 2.0f;  // 4.445cm
					const float SheetHalfThick = 1.27f / 2.0f;  // 0.635cm
					float FaceOffset = StudHalfDepth + SheetHalfThick;

					if (bExterior)
					{
						CandidateLocation += OutwardDir * FaceOffset;
					}
					else
					{
						CandidateLocation += InwardDir * FaceOffset;
					}

					UE_LOG(LogTemp, Log, TEXT("WallSheathing: %s face, offset=%.2f, Yaw=%.1f, GridSnap=%.1f along wall"),
						bExterior ? TEXT("Exterior") : TEXT("Interior"), FaceOffset, CandidateRotation.Yaw, SnappedAlong);
				}
			}

			// ── Roof sheathing → rafter top face ──
			if (Socket.SocketType == EConstructionSocketType::RoofSheathing_Face &&
				TgtSocketType == EConstructionSocketType::Rafter_Top_Face &&
				TargetPiece)
			{
				const ARoofSheathing* RoofSheet = Cast<const ARoofSheathing>(this);
				if (RoofSheet)
				{
					// Get rafter orientation
					FRotator RafterRot = TargetPiece->GetActorRotation();
					float RafterPitch = RafterRot.Pitch;

					// The sheet lays on the rafter with:
					// - Sheet X (8ft/243.84cm) along the RIDGE direction (perpendicular to rafter slope)
					// - Sheet Y (4ft/121.92cm) along the SLOPE direction (same as rafter run)
					// - Sheet Z (thickness) perpendicular to roof surface (normal to slope)

					// Use the rafter's actual transform vectors instead of
					// reconstructing from Euler components (which produces crooked results).
					FVector RafterForward = RafterRot.RotateVector(FVector::ForwardVector);
					FVector RafterUp = RafterRot.RotateVector(FVector::UpVector);

					// Roof normal = rafter's local up vector (perpendicular to slope)
					FVector RoofNormal = RafterUp;

					// Ridge direction = perpendicular to rafter slope, lying in the roof plane
					FVector RidgeDirVec = FVector::CrossProduct(RoofNormal, RafterForward).GetSafeNormal();

					CandidateRotation = FRotationMatrix::MakeFromXZ(RidgeDirVec, RoofNormal).Rotator();
					float RidgeYaw = CandidateRotation.Yaw;

					// Offset sheet so it sits ON TOP of the rafter (not centered on it)
					// Rafter depth = 13.97cm (2x6). Sheet sits on the top face.
					const float RafterHalfDepth = 13.97f / 2.0f; // 6.985cm
					const float SheetHalfThick = 1.27f / 2.0f;    // 0.635cm

					CandidateLocation += RoofNormal * (RafterHalfDepth + SheetHalfThick);

					// === GRID SNAP: RIDGE DIRECTION (along ridge, 8ft sheets) ===
					FVector RidgeDir = RidgeDirVec;
					FVector RafterLoc = TargetPiece->GetActorLocation();

					// Use player's aimed position (pre-snap actor location) for column selection
					// so aiming along the ridge picks different columns.
					// CandidateLocation is derived from the rafter socket which has the same
					// ridge-direction position for all rafters on the same side.
					float AlongRidge = FVector::DotProduct(GetActorLocation() - RafterLoc, RidgeDir);

					// Find end rafter positions along ridge to determine roof width
					float MinRidgeProj = 0.0f;
					float MaxRidgeProj = 0.0f;
					bool bFirstRafter = true;
					float MaxSlopeLen = 0.0f;

					for (ABuildablePiece* P : NearbyPieces)
					{
						if (!P || P->GetPieceType() != EPieceType::Rafter) continue;
						float PPitch = P->GetActorRotation().Pitch;
						if (FMath::Sign(PPitch) != FMath::Sign(RafterRot.Pitch)) continue;

						float Proj = FVector::DotProduct(P->GetActorLocation() - RafterLoc, RidgeDir);
						if (bFirstRafter)
						{
							MinRidgeProj = Proj;
							MaxRidgeProj = Proj;
							bFirstRafter = false;
						}
						else
						{
							MinRidgeProj = FMath::Min(MinRidgeProj, Proj);
							MaxRidgeProj = FMath::Max(MaxRidgeProj, Proj);
						}

						ARafter* R = Cast<ARafter>(P);
						if (R) MaxSlopeLen = FMath::Max(MaxSlopeLen, R->GetSlopeLengthCm());
					}

					// Roof edges flush with end rafters (add half rafter width so
					// sheathing covers the outer face of end rafters)
					const float RafterHalfWidth = 1.905f; // half of 3.81cm (2x4 width)
					float RidgeStart = MinRidgeProj - RafterHalfWidth;
					float RidgeEnd = MaxRidgeProj + RafterHalfWidth;
					float RidgeSpan = RidgeEnd - RidgeStart;

					// === SLOPE DIRECTION (fascia to ridge, 4ft sheets) ===
					FVector SlopeDir = RafterForward;
					float AlongSlope = FVector::DotProduct(CandidateLocation - RafterLoc, SlopeDir);

					// Snap at 4ft intervals from the ridge (rafter origin = slope distance 0)
					const float SlopeGridSize = 121.92f; // 4ft
					int32 MaxSlopeIdx = FMath::Max(0, FMath::CeilToInt(MaxSlopeLen / SlopeGridSize) - 1);
					int32 SlopeIdx = FMath::RoundToInt(AlongSlope / SlopeGridSize);
					SlopeIdx = FMath::Clamp(SlopeIdx, 0, MaxSlopeIdx);
					float SnappedAlongSlope;
					if (SlopeIdx == MaxSlopeIdx)
					{
						float RowStart = SlopeIdx * SlopeGridSize;
						float RowEnd = MaxSlopeLen;
						float Remaining = RowEnd - RowStart;
						if (Remaining < SlopeGridSize && Remaining > 1.0f)
						{
							// Last row: center on remaining space
							SnappedAlongSlope = (RowStart + RowEnd) / 2.0f;
						}
						else
						{
							SnappedAlongSlope = (SlopeIdx * SlopeGridSize) + SlopeGridSize / 2.0f;
						}
					}
					else
					{
						// Normal row: center at grid position
						SnappedAlongSlope = (SlopeIdx * SlopeGridSize) + SlopeGridSize / 2.0f;
					}

					// Clamp so sheet doesn't extend past slope length
					// No clamping — TryPlace() will trim sheets that extend past roof edges

					float StaggerOffset = 0.0f; // No stagger — clean grid on small buildings

					// === RIDGE GRID: tile 8ft sheets from RidgeStart ===
					const float RidgeGridSize = 243.84f; // 8ft per sheet
					float SheetHalfLen = RoofSheet->SheetLength / 2.0f;

					// How many sheets needed to cover the ridge span
					int32 MaxRidgeIdx = FMath::Max(0, FMath::CeilToInt(RidgeSpan / RidgeGridSize) - 1);

					// Sheets start from RidgeStart + stagger offset
					float EffectiveStart = RidgeStart + StaggerOffset;
					float RelAlongRidge = AlongRidge - EffectiveStart;
					int32 RidgeIdx = FMath::RoundToInt((RelAlongRidge - SheetHalfLen) / RidgeGridSize);
					RidgeIdx = FMath::Clamp(RidgeIdx, 0, MaxRidgeIdx);

					// Sheet center position along ridge
					float SnappedAlongRidge;
					if (RidgeIdx == MaxRidgeIdx)
					{
						float ColStart = EffectiveStart + (RidgeIdx * RidgeGridSize);
						float ColEnd = RidgeEnd;
						float Remaining = ColEnd - ColStart;
						if (Remaining < RidgeGridSize && Remaining > 1.0f)
						{
							// Last sheet: center on remaining space
							SnappedAlongRidge = (ColStart + ColEnd) / 2.0f;
						}
						else
						{
							SnappedAlongRidge = EffectiveStart + (RidgeIdx * RidgeGridSize) + SheetHalfLen;
						}
					}
					else
					{
						// Normal sheet: center at grid position
						SnappedAlongRidge = EffectiveStart + (RidgeIdx * RidgeGridSize) + SheetHalfLen;
					}

					// Sheathing stops at the fascia board, not the rafter tail tip.
					const float RafterTailOverhang = 20.32f; // 8 inches
					if (MaxSlopeLen > RafterTailOverhang)
					{
						MaxSlopeLen -= RafterTailOverhang;
					}

					// Store roof boundaries on the sheet for TryPlace() trimming
					ARoofSheathing* MutableSheet = const_cast<ARoofSheathing*>(RoofSheet);
					if (MutableSheet)
					{
						MutableSheet->RoofRidgeStart = RidgeStart;
						MutableSheet->RoofRidgeEnd = RidgeEnd;
						MutableSheet->RoofSlopeMax = MaxSlopeLen;
						MutableSheet->RoofRidgeDir = RidgeDir;
						MutableSheet->RoofSlopeDir = SlopeDir;
						MutableSheet->RoofRafterOrigin = RafterLoc;
					}

					// No clamping — TryPlace() will trim sheets that extend past roof edges

					// Apply final position
					CandidateLocation += RidgeDir * (SnappedAlongRidge - AlongRidge);
					CandidateLocation += SlopeDir * (SnappedAlongSlope - AlongSlope);

					// Skip if a roof sheathing sheet already exists at this grid position
					if (AConstructionPhaseManager::Instance)
					{
						TArray<ABuildablePiece*> ExistingSheets =
							AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::RoofSheathing);
						bool bSlotOccupied = false;
						for (ABuildablePiece* Existing : ExistingSheets)
						{
							if (Existing && Existing != this)
							{
								float DistSq = FVector::DistSquared(Existing->GetActorLocation(), CandidateLocation);
								if (DistSq < 900.0f) // 30cm tolerance
								{
									bSlotOccupied = true;
									break;
								}
							}
						}
						if (bSlotOccupied)
						{
							// Try next rafter — this grid slot is taken
							continue;
						}
					}

					{
						static float LastRoofLog = 0.0f;
						float Now = GetWorld()->GetTimeSeconds();
						if (Now - LastRoofLog > 2.0f)
						{
							UE_LOG(LogTemp, Warning, TEXT("RoofSheathing: RidgeSpan=%.1f SlopeLen=%.1f RidgeIdx=%d/%d SlopeIdx=%d/%d"),
								RidgeSpan, MaxSlopeLen, RidgeIdx, MaxRidgeIdx, SlopeIdx, MaxSlopeIdx);
							LastRoofLog = Now;
						}
					}
				}
			}

			// Fascia board position correction: move inward toward ridge and up slightly.
			// PIE-tested values from David's manual adjustment.
			if (Socket.SocketType == EConstructionSocketType::Fascia_RafterTail &&
				TgtSocketType == EConstructionSocketType::Rafter_Tail &&
				TargetPiece)
			{
				// Find building center from ridge posts to determine inward direction
				FVector InwardDir = FVector::ZeroVector;
				float BestRidgeDist = FLT_MAX;
				for (ABuildablePiece* P : NearbyPieces)
				{
					if (P && P->GetPieceType() == EPieceType::RidgePost)
					{
						FVector ToRidge = P->GetActorLocation() - CandidateLocation;
						ToRidge.Z = 0.0f;
						float D = ToRidge.Size();
						if (D < BestRidgeDist && D > 1.0f)
						{
							BestRidgeDist = D;
							InwardDir = ToRidge.GetSafeNormal();
						}
					}
				}

				if (BestRidgeDist < FLT_MAX)
				{
					CandidateLocation += InwardDir * 2.754373f; // PIE-tested inward toward ridge
				}
				CandidateLocation.Z += 1.737422f; // PIE-tested Z correction (upward)

				UE_LOG(LogTemp, Log,
					TEXT("Fascia snap correction: Inward=2.754cm toward ridge, Z+=1.737cm, FinalPos=(%.1f,%.1f,%.1f)"),
					CandidateLocation.X, CandidateLocation.Y, CandidateLocation.Z);
			}

			// Ridge post flush alignment: offset inward so outer face aligns
			// with the double top plate outer face (post is wider than plate).
			// PostWidth=11.43cm (3x1.5"), PlateWidth=8.89cm (3.5") → 1.27cm offset.
			// The offset must be PERPENDICULAR to the gable wall (across the wall thickness),
			// pushing the post toward the building interior.
			if (Socket.SocketType == EConstructionSocketType::RidgePost_Bottom &&
				(TgtSocketType == EConstructionSocketType::DoubleTopPlate_End ||
				 TgtSocketType == EConstructionSocketType::TopPlate_Top) &&
				TargetPiece)
			{
				// The DTP's RIGHT vector is perpendicular to the wall surface.
				// We need to move the post to the building center along this axis.
				FVector WallRight = TargetPiece->GetActorRotation().RotateVector(FVector::RightVector);

				// Find building center from same-building rim boards only.
				// Flood-fill from rim boards near the snap point to avoid
				// including rim boards from adjacent buildings.
				TSet<ABuildablePiece*> PostBuildingRims;
				TArray<ABuildablePiece*> PostFloodQueue;
				for (ABuildablePiece* P : NearbyPieces)
				{
					if (!P || P->GetPieceType() != EPieceType::RimBoard) continue;
					TArray<FConstructionSocket> PSocks = P->GetAllSockets();
					for (const FConstructionSocket& PS : PSocks)
					{
						if (PS.SocketType == EConstructionSocketType::RimBoard_End_Corner)
						{
							FVector CW = P->GetActorTransform().TransformPosition(PS.LocalPosition);
							if (FVector::Dist2D(CW, SnapLoc) < 50.0f && !PostBuildingRims.Contains(P))
							{
								PostBuildingRims.Add(P);
								PostFloodQueue.Add(P);
							}
						}
					}
				}
				while (PostFloodQueue.Num() > 0)
				{
					ABuildablePiece* Cur = PostFloodQueue.Pop();
					TArray<FConstructionSocket> CS2 = Cur->GetAllSockets();
					for (const FConstructionSocket& CS : CS2)
					{
						if (CS.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;
						FVector CC = Cur->GetActorTransform().TransformPosition(CS.LocalPosition);
						for (ABuildablePiece* P : NearbyPieces)
						{
							if (!P || P->GetPieceType() != EPieceType::RimBoard || PostBuildingRims.Contains(P)) continue;
							TArray<FConstructionSocket> PSocks = P->GetAllSockets();
							for (const FConstructionSocket& PS : PSocks)
							{
								if (PS.SocketType == EConstructionSocketType::RimBoard_End_Corner)
								{
									FVector PC = P->GetActorTransform().TransformPosition(PS.LocalPosition);
									if (FVector::Dist(CC, PC) < 20.0f)
									{
										PostBuildingRims.Add(P);
										PostFloodQueue.Add(P);
									}
								}
							}
						}
					}
				}
				FVector FrameCenter = FVector::ZeroVector;
				int32 RimCount = 0;
				for (ABuildablePiece* P : PostBuildingRims)
				{
					FrameCenter += P->GetActorLocation();
					RimCount++;
				}
				if (RimCount > 0)
				{
					FrameCenter /= RimCount;

					// Direction from post to building center
					FVector ToCenter = FrameCenter - CandidateLocation;
					ToCenter.Z = 0.0f;

					// Project onto wall's right vector to find which side is "inward"
					float DotPerp = FVector::DotProduct(ToCenter, WallRight);

					FVector BeforeFlush = CandidateLocation;

					if (FMath::Abs(DotPerp) > KINDA_SMALL_NUMBER)
					{
						// Shift post toward building center (inward) along wall perpendicular
						CandidateLocation += WallRight * FMath::Sign(DotPerp) * 2.03f;
					}

					UE_LOG(LogTemp, Warning,
						TEXT("RidgePost flush: DTP=[%s] pos=(%.1f,%.1f,%.1f) | Post BEFORE=(%.1f,%.1f,%.1f) AFTER=(%.1f,%.1f,%.1f) | "
						     "FrameCenter=(%.1f,%.1f) WallRight=(%.2f,%.2f) DotPerp=%.2f FlushOffset=2.03 RimCount=%d"),
						*TargetPiece->GetName(),
						TargetPiece->GetActorLocation().X, TargetPiece->GetActorLocation().Y, TargetPiece->GetActorLocation().Z,
						BeforeFlush.X, BeforeFlush.Y, BeforeFlush.Z,
						CandidateLocation.X, CandidateLocation.Y, CandidateLocation.Z,
						FrameCenter.X, FrameCenter.Y,
						WallRight.X, WallRight.Y,
						DotPerp, RimCount);
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

			// Top plate snap candidate diagnostic
			if (Socket.SocketType == EConstructionSocketType::TopPlate_Bottom ||
				Socket.SocketType == EConstructionSocketType::TopPlate_End)
			{
				UE_LOG(LogTemp, Log,
					TEXT("  TopPlate CANDIDATE: src=%s → tgt=%s on [%s] snapZ=%.2f actorZ=%.2f prio=%d dist=%.1f corner=%d inline=%d yaw=%.1f"),
					*Socket.SocketName.ToString(),
					*TargetSocketName.ToString(),
					TargetPiece ? *TargetPiece->GetName() : TEXT("null"),
					SnapLoc.Z,
					CandidateLocation.Z,
					Prio, Dist,
					bCandidateIsCorner ? 1 : 0,
					bCandidateIsInline ? 1 : 0,
					CandidateRotation.Yaw);
			}

			// --- Duplicate overlap check ---
			// When building adjacent to an existing structure, skip candidates
			// where a piece of the same type already exists at the snap location.
			// Prevents doubling on shared walls (plates, studs, posts, rim boards).
			if (AConstructionPhaseManager::Instance &&
				(PieceType == EPieceType::RimBoard ||
				 PieceType == EPieceType::WallPlate ||
				 PieceType == EPieceType::WallStud ||
				 PieceType == EPieceType::CornerPost ||
				 PieceType == EPieceType::TopPlate ||
				 PieceType == EPieceType::DoubleTopPlate))
			{
				// 15cm tolerance — wide enough to catch flush-offset duplicates
				// but narrow enough to not skip adjacent studs (16" = 40.6cm apart)
				const float OverlapTolerance = 15.0f; // cm
				float OverlapToleranceSq = OverlapTolerance * OverlapTolerance;
				bool bOverlapsExisting = false;

				TArray<ABuildablePiece*> SameTypePieces =
					AConstructionPhaseManager::Instance->GetPiecesOfType(PieceType);

				for (ABuildablePiece* Existing : SameTypePieces)
				{
					if (Existing && Existing != this)
					{
						float DistSq = FVector::DistSquared(Existing->GetActorLocation(), CandidateLocation);
						if (DistSq < OverlapToleranceSq)
						{
							bOverlapsExisting = true;
							UE_LOG(LogTemp, Log,
								TEXT("Overlap skip: %s at (%.1f,%.1f,%.1f) blocked by existing %s at (%.1f,%.1f,%.1f) dist=%.1fcm"),
								*UEnum::GetValueAsString(PieceType),
								CandidateLocation.X, CandidateLocation.Y, CandidateLocation.Z,
								*Existing->GetName(),
								Existing->GetActorLocation().X, Existing->GetActorLocation().Y, Existing->GetActorLocation().Z,
								FMath::Sqrt(DistSq));
							break;
						}
					}
				}

				if (bOverlapsExisting)
				{
					continue; // Skip this candidate — a piece already exists here
				}
			}

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

	// Force flat/vertical — only yaw varies
	if (PieceType == EPieceType::Plywood || PieceType == EPieceType::WallPlate ||
		PieceType == EPieceType::WallStud || PieceType == EPieceType::TopPlate ||
		PieceType == EPieceType::DoubleTopPlate)
	{
		FinalRotation.Pitch = 0.0f;
		FinalRotation.Roll = 0.0f;
	}

	// Ridge post: force upright, match plate yaw
	if (PieceType == EPieceType::RidgePost)
	{
		FinalRotation.Pitch = 0.0f;
		FinalRotation.Roll = 0.0f;
		// Yaw comes from DetectSnapCandidates (matches target plate yaw).
		// Enforce it here as well in case the candidate rotation was lost.
		if (Candidate.TargetPiece)
		{
			FinalRotation.Yaw = Candidate.TargetPiece->GetActorRotation().Yaw;
		}
	}

	// Ridge board alignment — PIE-tuned coordinates applied relative to the ridge post.
	// Original PIE values: Pos=(366.078746, 121.919998, 346.759027) Rot=(0, 0, 0)
	// These encode: X = PostX + BoardHalfLen, Y = PostY, Z = PostZ + PocketCenterZ + 8.9065
	// The 8.9065 cm is the mesh pivot correction (distance from actor origin to mesh center).
	// By computing from the post's current transform, the board follows the post dynamically.
	if (PieceType == EPieceType::RidgeBoard)
	{
		FinalRotation = FRotator(0.0f, FinalRotation.Yaw, 0.0f);

		ARidgePost* Post = Cast<ARidgePost>(Candidate.TargetPiece);
		if (Post)
		{
			FVector PostLoc = Post->GetActorLocation();
			float PocketCenterZ = Post->PostHeight - (Post->PocketDepth / 2.0f);

			// Z = post base + pocket center, adjusted for the ridge post mesh
			// being shifted down 3.76cm from actor origin for base alignment.
			FinalLocation.Z = PostLoc.Z + PocketCenterZ - 3.759796f;
			FinalLocation.Y = PostLoc.Y;

			// Auto-resize ridge board to span between the two ridge posts
			// and center it at the midpoint between them.
			ARidgeBoard* RBoard = Cast<ARidgeBoard>(this);
			if (RBoard)
			{
				TArray<AActor*> AllPosts;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARidgePost::StaticClass(), AllPosts);

				ARidgePost* OtherPost = nullptr;
				float BestDist = FLT_MAX;
				for (AActor* A : AllPosts)
				{
					ARidgePost* RP = Cast<ARidgePost>(A);
					if (RP && RP != Post)
					{
						float D = FVector::Dist(PostLoc, RP->GetActorLocation());
						if (D < BestDist)
						{
							BestDist = D;
							OtherPost = RP;
						}
					}
				}

				if (OtherPost)
				{
					FVector OtherLoc = OtherPost->GetActorLocation();
					float PostDist = FVector::Dist2D(PostLoc, OtherLoc);
					// Board spans pocket-to-pocket plus one PostWidth so ends are
					// flush with the outer faces of both posts.
					// Add 12" (30.48cm) overhang on each end for gable overhang
					const float GableOverhangCm = 30.48f; // 12 inches per side
					float NeededLengthCm = PostDist + Post->PostWidth + (GableOverhangCm * 2.0f);

					RBoard->SetBoardLengthCm(NeededLengthCm);

					// Center the board at the midpoint between the two posts.
					FVector Midpoint = (PostLoc + OtherLoc) / 2.0f;
					FinalLocation.X = Midpoint.X;
					FinalLocation.Y = Midpoint.Y;
					// Z already set from pocket calculation above

					// Board direction: along the line between the two posts
					FVector PostDir = (OtherLoc - PostLoc).GetSafeNormal2D();
					float BoardYaw = FMath::RadiansToDegrees(FMath::Atan2(PostDir.Y, PostDir.X));
					FinalRotation = FRotator(0.0f, BoardYaw, 0.0f);

					UE_LOG(LogTemp, Log,
						TEXT("RidgeBoard auto-resize: PostDist=%.1f + PostWidth=%.1f → Length=%.1fcm, Center=(%.1f, %.1f) Yaw=%.1f"),
						PostDist, Post->PostWidth, NeededLengthCm,
						FinalLocation.X, FinalLocation.Y, BoardYaw);
				}
			}

			UE_LOG(LogTemp, Log,
				TEXT("RidgeBoard aligned: Pos=(%.3f, %.3f, %.3f) PostZ=%.1f PocketCenterZ=%.2f"),
				FinalLocation.X, FinalLocation.Y, FinalLocation.Z,
				PostLoc.Z, PocketCenterZ);
		}
	}

	// Fascia board auto-resize: measure rafter tail span along the fascia direction
	// and resize the board so it is flush with the outermost rafter tails.
	if (PieceType == EPieceType::FasciaBoard)
	{
		AFasciaBoard* FasciaSelf = Cast<AFasciaBoard>(this);
		if (FasciaSelf && AConstructionPhaseManager::Instance)
		{
			TArray<ABuildablePiece*> Rafters =
				AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::Rafter);

			if (Rafters.Num() > 0)
			{
				// Fascia direction vector (along the board length = actor +X)
				FVector FasciaDir = FinalRotation.RotateVector(FVector::ForwardVector);

				// Collect rafter tail positions projected onto fascia direction
				TArray<float> TailProjections;
				FVector FasciaCenter = FinalLocation;

				for (ABuildablePiece* Piece : Rafters)
				{
					ARafter* Raft = Cast<ARafter>(Piece);
					if (!Raft) continue;

					FVector TailPos = Raft->GetTailEndWorldPosition();
					FVector ToTail = TailPos - FasciaCenter;

					// Perpendicular distance from fascia line — filter out far-side rafters
					float AlongFascia = FVector::DotProduct(ToTail, FasciaDir);
					FVector Perp = ToTail - FasciaDir * AlongFascia;
					float PerpDist = Perp.Size();

					if (PerpDist < 50.0f) // Within 50cm of fascia line
					{
						TailProjections.Add(AlongFascia);
					}
				}

				if (TailProjections.Num() >= 2)
				{
					float MinProj = FLT_MAX, MaxProj = -FLT_MAX;
					for (float Proj : TailProjections)
					{
						MinProj = FMath::Min(MinProj, Proj);
						MaxProj = FMath::Max(MaxProj, Proj);
					}

					float Span = MaxProj - MinProj;
					// Add one rafter width (1.5" = 3.81cm) margin per end so fascia
					// is flush with the outer face of the end rafters.
					float Margin = 3.81f;
					float NeededLength = Span + Margin * 2.0f - (1.905f * 2.0f);

					FasciaSelf->SetBoardLengthCm(NeededLength);

					// Re-center fascia at the midpoint of the tail span
					float MidProj = (MinProj + MaxProj) / 2.0f;
					FinalLocation += FasciaDir * MidProj;

					UE_LOG(LogTemp, Warning,
						TEXT("Fascia auto-resize: %d tails, Span=%.1fcm, Length=%.1fcm, CenterOffset=%.1f"),
						TailProjections.Num(), Span, NeededLength, MidProj);
				}
			}
		}
	}

	// Rafter alignment — PIE-tuned rotations applied relative to the ridge board.
	// Original PIE values:
	//   Right side: Pos=(363.888735, 120.824998, 349.0) Rot=(-23.840288, 90, 0)
	//   Left side:  Pos=(363.888735, 123.824998, 349.0) Rot=(-23.840288, -90, 0)
	// Pitch and yaw are applied as overrides; position uses the snap-computed values
	// with Y/Z corrections from the ridge board's side socket geometry.
	if (PieceType == EPieceType::Rafter)
	{
		FinalRotation.Roll = 0.0f;

		UE_LOG(LogTemp, Error, TEXT(">>> RAFTER APPLYSNAP ENTERED: TargetPiece=%s TargetSocket=%s SourceSocket=%s Rot=P%.1f Y%.1f"),
			Candidate.TargetPiece ? *Candidate.TargetPiece->GetName() : TEXT("null"),
			*Candidate.TargetSocketName.ToString(),
			*Candidate.SourceSocketName.ToString(),
			FinalRotation.Pitch, FinalRotation.Yaw);

		// Set RunDistanceCm and PitchRatio from the actual building geometry
		// (ridge post stores BuildingHalfWidthCm from the suggestion system).
		// Without this, RunDistanceCm stays hardcoded at 121.92cm (4ft default).
		ARafter* RafterSelf = Cast<ARafter>(this);
		if (RafterSelf && Candidate.TargetPiece)
		{
			// Find nearest placed ridge post to read building dimensions
			TArray<AActor*> FoundPosts;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARidgePost::StaticClass(), FoundPosts);
			float BestDist = FLT_MAX;
			ARidgePost* NearestPost = nullptr;
			for (AActor* A : FoundPosts)
			{
				ARidgePost* Post = Cast<ARidgePost>(A);
				if (!Post) continue;
				float Dist = FVector::Dist(Candidate.TargetPiece->GetActorLocation(), Post->GetActorLocation());
				if (Dist < BestDist)
				{
					BestDist = Dist;
					NearestPost = Post;
				}
			}

			UE_LOG(LogTemp, Error, TEXT(">>> RAFTER RIDGE POST SEARCH: Found %d posts, NearestPost=%s, BestDist=%.1f"),
				FoundPosts.Num(), NearestPost ? *NearestPost->GetName() : TEXT("null"), BestDist);

			if (NearestPost && NearestPost->BuildingHalfWidthCm > 0.0f)
			{
				float ActualRun = NearestPost->BuildingHalfWidthCm;
				float ActualPitch = NearestPost->GetPitchRatio();
				UE_LOG(LogTemp, Error, TEXT(">>> RAFTER SETTING RunDistanceCm=%.1f from RidgePost HalfWidth=%.1f, PitchRatio=%.1f"),
					ActualRun, NearestPost->BuildingHalfWidthCm, ActualPitch);
				if (!FMath::IsNearlyEqual(ActualRun, RafterSelf->RunDistanceCm, 0.1f) ||
					!FMath::IsNearlyEqual(ActualPitch, RafterSelf->PitchRatio, 0.01f))
				{
					UE_LOG(LogTemp, Warning, TEXT("Rafter: Updating from ridge post — Run: %.1f→%.1fcm (%.1f→%.1fft), Pitch: %.1f→%.1f/12"),
						RafterSelf->RunDistanceCm, ActualRun,
						RafterSelf->RunDistanceCm / 30.48f, ActualRun / 30.48f,
						RafterSelf->PitchRatio, ActualPitch);
					RafterSelf->SetPitch(ActualPitch, ActualRun);

					// Recompute FinalLocation with updated sockets
					// (SetPitch regenerates sockets, so ridge socket may have changed)
					FVector SocketLocalOffset(0.0f, 0.0f, 0.0f); // Ridge socket still at origin
					FVector SocketWorldOffset = FinalRotation.RotateVector(SocketLocalOffset);
					FinalLocation = Candidate.SnapLocation - SocketWorldOffset;
				}
			}

			UE_LOG(LogTemp, Warning, TEXT("Rafter: RunDistanceCm=%.1f (%.1fft), SlopeLen=%.1f, PitchRatio=%.1f/12"),
				RafterSelf->RunDistanceCm, RafterSelf->RunDistanceCm / 30.48f,
				RafterSelf->GetSlopeLengthCm(), RafterSelf->PitchRatio);
		}

		float ActualPitchDeg = RafterSelf ? RafterSelf->GetPitchAngleDegrees() : 26.57f;
		FinalRotation.Pitch = -ActualPitchDeg;

		// Z offset: lower rafter center so rafter TOP aligns with ridge board top.
		// At the ridge the rafter is nearly vertical in cross-section, so the full
		// half-depth is the correct drop.  At the birdsmouth (wall plate) the
		// cross-section is tilted by the pitch angle, so the vertical component
		// of the half-depth is RafterHalfDepth * cos(pitch).
		float RafterHalfDepth = RafterSelf ? (RafterSelf->RafterDepth / 2.0f) : 6.985f;
		float PitchRad = FMath::DegreesToRadians(ActualPitchDeg);
		bool bIsRidgeSnap = (Candidate.SourceSocketName == FName("RafterRidge"));
		float ZOffset = bIsRidgeSnap ? RafterHalfDepth : (RafterHalfDepth * FMath::Cos(PitchRad));

		float RafterHalfWidth = RafterSelf ? (RafterSelf->RafterWidth / 2.0f) : 1.905f;
		float InwardShift = 1.905f + RafterHalfWidth;

		UE_LOG(LogTemp, Warning, TEXT("Rafter: Calculated pitch=%.2f deg (PitchRatio=%.2f/12, Run=%.1fcm) ZOffset=%.2f InwardShift=%.2f"),
			ActualPitchDeg, RafterSelf ? RafterSelf->PitchRatio : 0.0f, RafterSelf ? RafterSelf->RunDistanceCm : 0.0f, ZOffset, InwardShift);

		// Ridge board side sockets: additional position and yaw overrides.
		// RidgeBoardSide_R* = right side, _L* = left side.
		FString TargetSocketStr = Candidate.TargetSocketName.ToString();
		if (TargetSocketStr.Contains(TEXT("_R")))
		{
			FinalLocation.Y += InwardShift;
			FinalLocation.Y -= 3.81f; // PIE-tested inward correction
			FinalLocation.Z -= ZOffset;
			FinalLocation.Z += 0.3f; // PIE-tested upward correction
			FinalRotation.Yaw = 90.0f;
			FinalRotation.Roll = 0.0f;
		}
		else if (TargetSocketStr.Contains(TEXT("_L")))
		{
			FinalLocation.Y -= InwardShift;
			FinalLocation.Y += 3.81f; // PIE-tested inward correction
			FinalLocation.Z -= ZOffset;
			FinalLocation.Z += 0.3f; // PIE-tested upward correction
			FinalRotation.Yaw = -90.0f;
			FinalRotation.Roll = 0.0f;
		}

		if (TargetSocketStr.Contains(TEXT("End")))
		{
			FVector RidgeBoardFwd = Candidate.TargetPiece->GetActorRotation().RotateVector(FVector::ForwardVector);
			float RafterHalfW = RafterSelf ? (RafterSelf->RafterWidth / 2.0f) : 1.905f;

			if (TargetSocketStr.Contains(TEXT("EndL")))
			{
				FinalLocation += RidgeBoardFwd * RafterHalfW;
			}
			else if (TargetSocketStr.Contains(TEXT("EndR")))
			{
				FinalLocation -= RidgeBoardFwd * RafterHalfW;
			}

			UE_LOG(LogTemp, Log, TEXT("Rafter end flush: shifted %.2fcm inward along ridge board (socket=%s)"),
				RafterHalfW, *TargetSocketStr);
		}

		UE_LOG(LogTemp, Log, TEXT("Rafter ApplySnap: Pos=%s Rot=%s (Socket=%s)"),
			*FinalLocation.ToString(), *FinalRotation.ToString(),
			*Candidate.TargetSocketName.ToString());
	}

	SetActorRotation(FinalRotation);

	// Corner joints: boards stay at centerline positions (centered on foundation).
	// Small overlap at corners is acceptable — boards sit centered on their foundations.
	SetActorLocation(FinalLocation);

	// Roof sheathing: preview-time trimming so ghost shows actual trimmed size
	if (PieceType == EPieceType::RoofSheathing)
	{
		ARoofSheathing* RoofSheet = Cast<ARoofSheathing>(this);
		if (RoofSheet && MeshComponent && MeshComponent->GetStaticMesh() &&
			!RoofSheet->RoofRidgeDir.IsNearlyZero() && !RoofSheet->RoofSlopeDir.IsNearlyZero())
		{
			FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
			float UnscaledX = Bounds.BoxExtent.X * 2.0f;
			float UnscaledY = Bounds.BoxExtent.Y * 2.0f;
			float SheetHalfX = UnscaledX / 2.0f;
			float SheetHalfY = UnscaledY / 2.0f;
			FVector ToSheet = FinalLocation - RoofSheet->RoofRafterOrigin;
			float SheetAlongRidge = FVector::DotProduct(ToSheet, RoofSheet->RoofRidgeDir);
			float SheetAlongSlope = FVector::DotProduct(ToSheet, RoofSheet->RoofSlopeDir);
			float SheetRidgeLeft = SheetAlongRidge - SheetHalfX;
			float SheetRidgeRight = SheetAlongRidge + SheetHalfX;
			float SheetSlopeBottom = SheetAlongSlope - SheetHalfY;
			float SheetSlopeTop = SheetAlongSlope + SheetHalfY;
			float TrimLeft = FMath::Max(SheetRidgeLeft, RoofSheet->RoofRidgeStart);
			float TrimRight = FMath::Min(SheetRidgeRight, RoofSheet->RoofRidgeEnd);
			float TrimBottom = FMath::Max(SheetSlopeBottom, 0.0f);
			float TrimTop = FMath::Min(SheetSlopeTop, RoofSheet->RoofSlopeMax);
			bool bNeedsTrim = (TrimLeft > SheetRidgeLeft + 0.1f || TrimRight < SheetRidgeRight - 0.1f ||
							   TrimBottom > SheetSlopeBottom + 0.1f || TrimTop < SheetSlopeTop - 0.1f);
			if (bNeedsTrim)
			{
				float NewWidth = TrimRight - TrimLeft;
				float NewHeight = TrimTop - TrimBottom;
				if (NewWidth > 5.0f && NewHeight > 5.0f)
				{
					float ScaleX = NewWidth / UnscaledX;
					float ScaleY = NewHeight / UnscaledY;
					FVector CurScale = MeshComponent->GetRelativeScale3D();
					MeshComponent->SetRelativeScale3D(FVector(ScaleX, ScaleY, CurScale.Z));
					float TrimCenterRidge = ((TrimLeft + TrimRight) / 2.0f) - SheetAlongRidge;
					float TrimCenterSlope = ((TrimBottom + TrimTop) / 2.0f) - SheetAlongSlope;
					MeshComponent->SetRelativeLocation(FVector(TrimCenterRidge, TrimCenterSlope, 0.0f));
				}
			}
			else
			{
				MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
				MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
			}
		}
	}

	// Wall sheathing: detect which edge is at a corner and shift the extension to that side
	if (PieceType == EPieceType::WallSheathing && Candidate.TargetPiece)
	{
		AWallSheathing* WS = Cast<AWallSheathing>(this);
		if (WS)
		{
			FVector WallDir = FRotator(0, FinalRotation.Yaw, 0).RotateVector(FVector::ForwardVector);
			FVector PlateOrigin = Candidate.TargetPiece->GetActorLocation();

			// Find wall extents from plates and corner posts
			float MinAlong = 0.0f;
			float MaxAlong = 0.0f;
			TArray<AActor*> AllPieces;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABuildablePiece::StaticClass(), AllPieces);

			for (AActor* A : AllPieces)
			{
				ABuildablePiece* P = Cast<ABuildablePiece>(A);
				if (!P) continue;

				if (P->GetPieceType() == EPieceType::WallPlate)
				{
					float YawDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(
						P->GetActorRotation().Yaw, FinalRotation.Yaw));
					if (YawDiff < 5.0f || FMath::Abs(YawDiff - 180.0f) < 5.0f)
					{
						FVector ToPlate = P->GetActorLocation() - PlateOrigin;
						float Along = FVector::DotProduct(ToPlate, WallDir);
						ABottomPlate* BP = Cast<ABottomPlate>(P);
						float PlateHalfLen = BP ? BP->GetEffectiveLength() / 2.0f : 0.0f;
						MinAlong = FMath::Min(MinAlong, Along - PlateHalfLen);
						MaxAlong = FMath::Max(MaxAlong, Along + PlateHalfLen);
					}
				}
				else if (P->GetPieceType() == EPieceType::CornerPost)
				{
					FVector ToPost = P->GetActorLocation() - PlateOrigin;
					float Along = FVector::DotProduct(ToPost, WallDir);
					MinAlong = FMath::Min(MinAlong, Along);
					MaxAlong = FMath::Max(MaxAlong, Along);
				}
			}

			// Determine which edge is at a corner
			const float GridSize = 121.92f;
			FVector SheetToPlate = FinalLocation - PlateOrigin;
			float SheetAlong = FVector::DotProduct(SheetToPlate, WallDir);
			float SheetLeftEdge = SheetAlong - GridSize / 2.0f;
			float SheetRightEdge = SheetAlong + GridSize / 2.0f;

			const float CornerTolerance = 5.0f; // cm
			bool bLeftAtCorner = FMath::Abs(SheetLeftEdge - MinAlong) < CornerTolerance;
			bool bRightAtCorner = FMath::Abs(SheetRightEdge - MaxAlong) < CornerTolerance;

			if (bLeftAtCorner && !bRightAtCorner)
			{
				// Corner is on the left edge — extend mesh in -X direction
				WS->SetCornerExtensionSide(-1);
			}
			else if (bRightAtCorner && !bLeftAtCorner)
			{
				// Corner is on the right edge — extend mesh in +X direction
				WS->SetCornerExtensionSide(1);
			}
			else
			{
				// Middle sheet or both edges at corners — center extension
				WS->SetCornerExtensionSide(0);
			}

			UE_LOG(LogTemp, Log, TEXT("WallSheathing ApplySnap: SheetAlong=%.1f, WallExtent=[%.1f, %.1f], LeftAtCorner=%d, RightAtCorner=%d"),
				SheetAlong, MinAlong, MaxAlong, bLeftAtCorner, bRightAtCorner);
		}
	}

	// Ridge board Z chain diagnostic: trace DTP → post → pocket → ridge board
	if (PieceType == EPieceType::RidgeBoard && Candidate.TargetPiece)
	{
		ARidgePost* RidgePost = Cast<ARidgePost>(Candidate.TargetPiece);
		if (RidgePost)
		{
			// Find the pocket socket local Z from the post's sockets
			float PocketSocketLocalZ = 0.0f;
			for (const FConstructionSocket& Sock : RidgePost->GetAllSockets())
			{
				if (Sock.SocketType == EConstructionSocketType::RidgePost_Pocket)
				{
					PocketSocketLocalZ = Sock.LocalPosition.Z;
					break;
				}
			}

			ARidgeBoard* RidgeBd = Cast<ARidgeBoard>(this);
			float RidgeBoardTopZ = GetActorLocation().Z + (RidgeBd ? RidgeBd->BoardHeight / 2.0f : 0.0f);

			UE_LOG(LogTemp, Error, TEXT(">>> Z CHAIN: DTP_top=%.2f, PostActorZ=%.2f, PostHeight=%.2f, "
				"PocketSocketZ=%.2f, RidgeBoardActorZ=%.2f, RidgeBoardTopZ=%.2f, "
				"ExpectedRidgeBoardZ(post+pocket)=%.2f, diff=%.2f"),
				286.1f,
				RidgePost->GetActorLocation().Z,
				RidgePost->PostHeight,
				PocketSocketLocalZ,
				GetActorLocation().Z,
				RidgeBoardTopZ,
				RidgePost->GetActorLocation().Z + PocketSocketLocalZ,
				GetActorLocation().Z - (RidgePost->GetActorLocation().Z + PocketSocketLocalZ));
		}
	}

	// Rafter placement diagnostics
	if (PieceType == EPieceType::Rafter)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rafter placed at Pos=%s Rot=%s"),
			*FinalLocation.ToString(), *FinalRotation.ToString());

		// Geometry check: log actor position, ridge board, and tail end
		UE_LOG(LogTemp, Error, TEXT(">>> RAFTER GEOMETRY CHECK:"));
		UE_LOG(LogTemp, Error, TEXT("  Actor pos: %s"), *GetActorLocation().ToString());
		if (Candidate.TargetPiece)
		{
			UE_LOG(LogTemp, Error, TEXT("  Ridge board pos: %s"), *Candidate.TargetPiece->GetActorLocation().ToString());
		}
		ARafter* RafterGeom = Cast<ARafter>(this);
		if (RafterGeom)
		{
			UE_LOG(LogTemp, Error, TEXT("  Rafter Yaw=%.1f, mesh extends %.1fcm from origin along +X rotated by yaw"),
				FinalRotation.Yaw, RafterGeom->GetSlopeLengthCm());

			// Calculate where the tail end is in world space
			FVector TailLocal(RafterGeom->GetSlopeLengthCm(), 0, 0);
			FVector TailWorld = GetActorTransform().TransformPosition(TailLocal);
			UE_LOG(LogTemp, Error, TEXT("  Tail end world pos: %s"), *TailWorld.ToString());
		}

		// Verify rafter origin = ridge board top surface (ridge socket at actor origin)
		if (Candidate.TargetPiece)
		{
			ARidgeBoard* RidgeBd = Cast<ARidgeBoard>(Candidate.TargetPiece);
			if (RidgeBd)
			{
				// Compute the actual mesh top Z from the ridge board's mesh bounds
				// (don't use ActorZ + BoardHeight/2 — mesh origin may not be centered)
				float RidgeBoardActorZ = RidgeBd->GetActorLocation().Z;
				float RidgeBoardTopZ_Formula = RidgeBoardActorZ + RidgeBd->BoardHeight / 2.0f;

				// Get the ACTUAL side socket Z to see what SnapLoc.Z should be
				float SideSocketLocalZ = 0.0f;
				for (const FConstructionSocket& RBSock : RidgeBd->GetAllSockets())
				{
					if (RBSock.SocketType == EConstructionSocketType::RidgeBoard_Side)
					{
						SideSocketLocalZ = RBSock.LocalPosition.Z;
						break;
					}
				}
				float SideSocketWorldZ = RidgeBoardActorZ + SideSocketLocalZ;

				UE_LOG(LogTemp, Warning,
					TEXT("Ridge board: ActorZ=%.1f BoardHeight=%.1f FormulaTopZ=%.1f | SideSocketLocalZ=%.1f SideSocketWorldZ=%.1f | Rafter Z=%.1f | diff(formula)=%.1f diff(socket)=%.1f"),
					RidgeBoardActorZ, RidgeBd->BoardHeight, RidgeBoardTopZ_Formula,
					SideSocketLocalZ, SideSocketWorldZ,
					FinalLocation.Z,
					FinalLocation.Z - RidgeBoardTopZ_Formula,
					FinalLocation.Z - SideSocketWorldZ);
			}
		}

		// Verify birdsmouth lands on double top plate
		FVector BirdsmouthWorld = FinalLocation;
		for (const FConstructionSocket& S : Sockets)
		{
			if (S.SocketName == FName("RafterBirdsmouth"))
			{
				BirdsmouthWorld = GetActorTransform().TransformPosition(S.LocalPosition);
				break;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("Birdsmouth world pos = %s (Z=%.1f)"),
			*BirdsmouthWorld.ToString(), BirdsmouthWorld.Z);

		// Log double top plate top Z for verification
		TArray<AActor*> FoundPlates;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADoubleTopPlate::StaticClass(), FoundPlates);
		for (AActor* A : FoundPlates)
		{
			ADoubleTopPlate* DTP = Cast<ADoubleTopPlate>(A);
			if (DTP)
			{
				float PlateTopZ = DTP->GetActorLocation().Z + DTP->BoardHeight / 2.0f;
				float BirdsmouthGap = BirdsmouthWorld.Z - PlateTopZ;
				UE_LOG(LogTemp, Warning, TEXT("DoubleTopPlate '%s' top Z = %.1f, birdsmouth gap = %.1f cm"),
					*DTP->GetName(), PlateTopZ, BirdsmouthGap);
			}
		}
	}

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

	// Door frame bottom to bottom plate top (same priority as wall studs)
	if ((SocketA == EConstructionSocketType::DoorFrame_Bottom &&
		 SocketB == EConstructionSocketType::Wall_Bottom_Plate) ||
		(SocketA == EConstructionSocketType::Wall_Bottom_Plate &&
		 SocketB == EConstructionSocketType::DoorFrame_Bottom))
	{
		return 750;
	}

	// Window frame bottom to bottom plate top (same priority as wall studs)
	if ((SocketA == EConstructionSocketType::WindowFrame_Bottom &&
		 SocketB == EConstructionSocketType::Wall_Bottom_Plate) ||
		(SocketA == EConstructionSocketType::Wall_Bottom_Plate &&
		 SocketB == EConstructionSocketType::WindowFrame_Bottom))
	{
		return 750;
	}

	// Top plate bottom to wall stud/corner post/door frame/window frame tops
	// HIGHEST priority for top plates — each plate sits on its wall's studs
	if (SocketA == EConstructionSocketType::TopPlate_Bottom &&
		(SocketB == EConstructionSocketType::Wall_Stud_Top ||
		 SocketB == EConstructionSocketType::CornerPost_Top ||
		 SocketB == EConstructionSocketType::DoorFrame_Top ||
		 SocketB == EConstructionSocketType::WindowFrame_Top))
	{
		return 950;
	}
	if ((SocketA == EConstructionSocketType::Wall_Stud_Top ||
		 SocketA == EConstructionSocketType::CornerPost_Top ||
		 SocketA == EConstructionSocketType::DoorFrame_Top ||
		 SocketA == EConstructionSocketType::WindowFrame_Top) &&
		SocketB == EConstructionSocketType::TopPlate_Bottom)
	{
		return 950;
	}

	// Top plate end-to-end (corner/inline connections) — secondary to stud snaps
	if (SocketA == EConstructionSocketType::TopPlate_End &&
		SocketB == EConstructionSocketType::TopPlate_End)
	{
		return 700;
	}

	// Rafter ridge end to ridge board side (HIGHEST for rafters — must always beat birdsmouth snaps)
	if ((SocketA == EConstructionSocketType::Rafter_Ridge &&
		 SocketB == EConstructionSocketType::RidgeBoard_Side) ||
		(SocketA == EConstructionSocketType::RidgeBoard_Side &&
		 SocketB == EConstructionSocketType::Rafter_Ridge))
	{
		return 900;
	}

	// Rafter birdsmouth to plate (LOW — only used for validation, never for placement)
	if ((SocketA == EConstructionSocketType::Rafter_BirdsMouth &&
		 (SocketB == EConstructionSocketType::TopPlate_Top ||
		  SocketB == EConstructionSocketType::DoubleTopPlate_End)) ||
		((SocketA == EConstructionSocketType::TopPlate_Top ||
		  SocketA == EConstructionSocketType::DoubleTopPlate_End) &&
		 SocketB == EConstructionSocketType::Rafter_BirdsMouth))
	{
		return 100;
	}

	// Ridge board end to ridge post pocket (PRIMARY ridge board snap)
	if ((SocketA == EConstructionSocketType::RidgeBoard_End &&
		 SocketB == EConstructionSocketType::RidgePost_Pocket) ||
		(SocketA == EConstructionSocketType::RidgePost_Pocket &&
		 SocketB == EConstructionSocketType::RidgeBoard_End))
	{
		return 850;
	}

	// Ridge post bottom to double top plate / top plate top
	if ((SocketA == EConstructionSocketType::RidgePost_Bottom &&
		 (SocketB == EConstructionSocketType::DoubleTopPlate_End ||
		  SocketB == EConstructionSocketType::TopPlate_Top)) ||
		((SocketA == EConstructionSocketType::DoubleTopPlate_End ||
		  SocketA == EConstructionSocketType::TopPlate_Top) &&
		 SocketB == EConstructionSocketType::RidgePost_Bottom))
	{
		return 800;
	}

	// Wall sheathing face to bottom plate (MEDIUM priority)
	if ((SocketA == EConstructionSocketType::WallSheathing_Face &&
		 SocketB == EConstructionSocketType::Wall_Bottom_Plate) ||
		(SocketA == EConstructionSocketType::Wall_Bottom_Plate &&
		 SocketB == EConstructionSocketType::WallSheathing_Face))
	{
		return 600;
	}

	// Roof sheathing face to rafter top (MEDIUM priority)
	if ((SocketA == EConstructionSocketType::RoofSheathing_Face &&
		 SocketB == EConstructionSocketType::Rafter_Top_Face) ||
		(SocketA == EConstructionSocketType::Rafter_Top_Face &&
		 SocketB == EConstructionSocketType::RoofSheathing_Face))
	{
		return 600;
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

	// Rafters can snap to either ridge board side or birdsmouth (top plate).
	// The PIE-tuned pitch override in ApplySnap ensures correct pitch for all snap types.
	// (Previously blocked birdsmouth snaps, but pitch override now handles that case.)

	return true;
}

bool ABuildablePiece::IsSupported() const
{
	if (PieceType == EPieceType::Foundation)
		return true;

	return bIsSnapped;
}

void ABuildablePiece::SetPreviewColor(const FLinearColor& Color)
{
	if (!DynamicMaterial) return;
	DynamicMaterial->SetVectorParameterValue(FName("BaseColor"), Color);
	DynamicMaterial->SetVectorParameterValue(FName("Base Color"), Color);
	DynamicMaterial->SetVectorParameterValue(FName("Color"), Color);
	DynamicMaterial->SetScalarParameterValue(FName("Opacity"), Color.A);
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
		DynamicMaterial->SetVectorParameterValue(FName("BaseColor"), TargetColor);
		DynamicMaterial->SetVectorParameterValue(FName("Base Color"), TargetColor);
		DynamicMaterial->SetVectorParameterValue(FName("Color"), TargetColor);
		DynamicMaterial->SetScalarParameterValue(FName("Opacity"), TargetColor.A);

		if (MeshComponent) MeshComponent->SetRenderCustomDepth(true);
		break;
	}

	case EPieceState::Placed:
	{
		if (MeshComponent)
		{
			if (OriginalMeshMaterial) MeshComponent->SetMaterial(0, OriginalMeshMaterial);
			MeshComponent->SetRenderCustomDepth(true);
		}
		break;
	}

	case EPieceState::Nailed:
	{
		UMaterialInterface* FinalMat = NailedMaterial ? NailedMaterial : OriginalMeshMaterial;
		if (MeshComponent)
		{
			if (FinalMat) MeshComponent->SetMaterial(0, FinalMat);
			MeshComponent->SetRenderCustomDepth(false);
		}
		DynamicMaterial = nullptr;
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
