// Born To Shine - Base class for all buildable construction pieces

#include "BuildablePiece.h"
#include "RimBoard.h"
#include "SocketManager.h"
#include "ConstructionPhaseManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"

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

	// Try to find a snap point
	FVector SnapLocation;
	FRotator SnapRotation;

	if (FindSnapPoint(SnapLocation, SnapRotation))
	{
		SetActorLocation(SnapLocation);

		// Check if this is a rim-to-rim corner snap (auto-rotation case)
		bool bIsCornerSnap = SnappedToSocketName.ToString().Contains("EndCorner");

		if (bIsCornerSnap)
		{
			// Corner snap: Use the full auto-rotated SnapRotation
			SetActorRotation(SnapRotation);
		}
		else
		{
			// Normal snap: Preserve user's Yaw rotation
			FRotator FinalRotation = SnapRotation;
			FinalRotation.Yaw = NewRotation.Yaw;
			SetActorRotation(FinalRotation);
		}

		bIsSnapped = true;
	}
	else
	{
		bIsSnapped = false;
	}
}

bool ABuildablePiece::FindSnapPoint(FVector& OutSnapLocation, FRotator& OutSnapRotation)
{
	if (!ASocketManager::Instance)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindSnapPoint: SocketManager is null!"));
		return false;
	}
	if (!AConstructionPhaseManager::Instance)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindSnapPoint: PhaseManager is null!"));
		return false;
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
		return false;
	}

	// Debug: Log search info (throttled)
	static float LastSearchLogTime = 0.0f;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastSearchLogTime > 2.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Searching %d nearby pieces with %d sockets"),
			*GetName(), NearbyPieces.Num(), Sockets.Num());
		LastSearchLogTime = CurrentTime;
	}

	// ============================================================
	// DUAL-END CORNER DETECTION (for 3rd/4th rim boards)
	// Before single-socket snapping, check if BOTH ends of this rim board
	// can reach corner sockets on two DIFFERENT placed boards.
	// If so, this is a spanning board and we do two-point alignment.
	// ============================================================
	if (PieceType == EPieceType::RimBoard)
	{
		FVector DualSnapLocation;
		FRotator DualSnapRotation;
		ABuildablePiece* DualSnapPiece;
		FName DualSnapSocketName;

		if (TryDualEndCornerSnap(NearbyPieces, DualSnapLocation, DualSnapRotation, DualSnapPiece, DualSnapSocketName))
		{
			OutSnapLocation = DualSnapLocation;
			OutSnapRotation = DualSnapRotation;
			SnappedToPiece = DualSnapPiece;
			SnappedToSocketName = DualSnapSocketName;
			return true;
		}
	}

	// ============================================================
	// STANDARD SINGLE-SOCKET SNAPPING
	// ============================================================
	bool bFoundSnap = false;
	float BestDistance = FLT_MAX;
	int32 BestPriority = -1;

	for (const FConstructionSocket& Socket : Sockets)
	{
		FVector SocketWorldLocation = GetActorTransform().TransformPosition(Socket.LocalPosition);
		FRotator SocketWorldRotation = GetActorRotation() + Socket.LocalRotation;

		FVector SnapLoc;
		FRotator SnapRot;
		ABuildablePiece* TargetPiece;
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
			float Distance = FVector::Dist(SocketWorldLocation, SnapLoc);

			EConstructionSocketType TargetSocketType = GetTargetSocketType(TargetPiece, TargetSocketName);
			int32 Priority = GetSocketConnectionPriority(Socket.SocketType, TargetSocketType);

			bool bIsBetter = (Priority > BestPriority) ||
			                 (Priority == BestPriority && Distance < BestDistance);

			if (bIsBetter)
			{
				BestDistance = Distance;
				BestPriority = Priority;

				FVector SocketLocalOffset = Socket.LocalPosition;
				FRotator CurrentActorRotation = GetActorRotation();

				// Special handling for rim-to-rim corner snaps (single-end)
				if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
					TargetSocketType == EConstructionSocketType::RimBoard_End_Corner &&
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
						CurrentActorRotation.Yaw = TargetRotation.Yaw;
						UE_LOG(LogTemp, Warning, TEXT("Inline snap: %s -> %s"), *SourceSocketStr, *TargetSocketStr);
					}
					else
					{
						// CORNER JOINT: Left->Left or Right->Right
						FVector TargetForward = TargetRotation.RotateVector(FVector::ForwardVector);
						FVector SocketToTarget = (SnapLoc - SocketWorldLocation).GetSafeNormal();
						FVector CrossProduct = FVector::CrossProduct(TargetForward, SocketToTarget);
						float RotationOffset = (CrossProduct.Z > 0) ? 90.0f : -90.0f;
						CurrentActorRotation.Yaw = TargetRotation.Yaw + RotationOffset;

						UE_LOG(LogTemp, Warning, TEXT("Corner snap: %s -> %s (rot offset %.0f)"),
							*SourceSocketStr, *TargetSocketStr, RotationOffset);
					}
				}

				FVector SocketWorldOffset = CurrentActorRotation.RotateVector(SocketLocalOffset);
				OutSnapLocation = SnapLoc - SocketWorldOffset;
				OutSnapRotation = CurrentActorRotation;

				SnappedToPiece = TargetPiece;
				SnappedToSocketName = TargetSocketName;
				bFoundSnap = true;
			}
		}
	}

	return bFoundSnap;
}

bool ABuildablePiece::TryDualEndCornerSnap(
	const TArray<ABuildablePiece*>& NearbyPieces,
	FVector& OutSnapLocation,
	FRotator& OutSnapRotation,
	ABuildablePiece*& OutTargetPiece,
	FName& OutTargetSocketName)
{
	// Find our Left and Right EndCorner sockets
	FConstructionSocket* LeftCornerSocket = nullptr;
	FConstructionSocket* RightCornerSocket = nullptr;

	for (FConstructionSocket& Socket : Sockets)
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

	if (!LeftCornerSocket || !RightCornerSocket) return false;

	// Get world positions of both corner sockets
	FVector LeftWorldPos = GetActorTransform().TransformPosition(LeftCornerSocket->LocalPosition);
	FVector RightWorldPos = GetActorTransform().TransformPosition(RightCornerSocket->LocalPosition);

	// Search for valid corner targets for EACH end
	struct FCornerCandidate
	{
		ABuildablePiece* TargetPiece;
		FName TargetSocketName;
		FVector TargetWorldPos;
		FRotator TargetWorldRot;
		float Distance;
	};

	// Expanded search radius for dual-end detection
	float DualSearchRadius = SnapSearchRadius * 2.0f;

	TArray<FCornerCandidate> LeftCandidates;
	TArray<FCornerCandidate> RightCandidates;

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

			// Check distance to LEFT end
			float LeftDist = FVector::Dist(LeftWorldPos, TargetWorldPos);
			if (LeftDist < DualSearchRadius)
			{
				FCornerCandidate Candidate;
				Candidate.TargetPiece = Piece;
				Candidate.TargetSocketName = TargetSocket.SocketName;
				Candidate.TargetWorldPos = TargetWorldPos;
				Candidate.TargetWorldRot = TargetWorldRot;
				Candidate.Distance = LeftDist;
				LeftCandidates.Add(Candidate);
			}

			// Check distance to RIGHT end
			float RightDist = FVector::Dist(RightWorldPos, TargetWorldPos);
			if (RightDist < DualSearchRadius)
			{
				FCornerCandidate Candidate;
				Candidate.TargetPiece = Piece;
				Candidate.TargetSocketName = TargetSocket.SocketName;
				Candidate.TargetWorldPos = TargetWorldPos;
				Candidate.TargetWorldRot = TargetWorldRot;
				Candidate.Distance = RightDist;
				RightCandidates.Add(Candidate);
			}
		}
	}

	// Need candidates for each end
	if (LeftCandidates.Num() == 0 || RightCandidates.Num() == 0) return false;

	// Find the best pair where left and right connect to DIFFERENT target pieces
	FCornerCandidate BestLeft;
	FCornerCandidate BestRight;
	bool bFoundValidPair = false;
	float BestPairScore = -1.0f;

	for (const FCornerCandidate& LC : LeftCandidates)
	{
		for (const FCornerCandidate& RC : RightCandidates)
		{
			// Must be different target pieces (spanning between two boards)
			if (LC.TargetPiece == RC.TargetPiece) continue;

			// Score: prefer closer total distance
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

	if (!bFoundValidPair) return false;

	// ============================================================
	// TWO-POINT ALIGNMENT
	// Position and rotate the board so its corner sockets land on both targets.
	// ============================================================

	FVector TargetLeftPos = BestLeft.TargetWorldPos;
	FVector TargetRightPos = BestRight.TargetWorldPos;

	// Direction from left target to right target
	FVector SpanDirection = (TargetRightPos - TargetLeftPos).GetSafeNormal();

	// Board rotation aligns X-axis (length) with span direction
	FRotator SpanRotation = SpanDirection.Rotation();

	// Calculate actor origin from left socket alignment
	FVector LeftLocalOffset = SpanRotation.RotateVector(LeftCornerSocket->LocalPosition);
	FVector ActorPosition = TargetLeftPos - LeftLocalOffset;

	// Check how well the right end lines up
	FVector RightLocalOffset = SpanRotation.RotateVector(RightCornerSocket->LocalPosition);
	FVector PredictedRightPos = ActorPosition + RightLocalOffset;
	float RightError = FVector::Dist(PredictedRightPos, TargetRightPos);

	// Auto-resize the rim board to fit the gap if needed
	ARimBoard* RimBoard = Cast<ARimBoard>(this);
	if (RimBoard && RightError > 5.0f)
	{
		// Calculate the gap distance between the two target sockets
		float GapDistance = FVector::Dist(TargetLeftPos, TargetRightPos);

		// Current socket span
		float CurrentSocketSpan = FVector::Dist(LeftCornerSocket->LocalPosition, RightCornerSocket->LocalPosition);

		// We need socket span ≈ gap distance
		// EffectiveLength ≈ socket span (sockets are near the ends)
		// For outside boards: EffectiveLength = LengthFeet * 30.48
		float NeededLengthCm = GapDistance;
		int32 NeededLengthFeet = FMath::RoundToInt(NeededLengthCm / 30.48f);
		NeededLengthFeet = FMath::Clamp(NeededLengthFeet, 1, 16);

		if (NeededLengthFeet != RimBoard->GetBoardLengthFeet())
		{
			UE_LOG(LogTemp, Warning, TEXT("Dual-end snap: Auto-resizing board from %d ft to %d ft (gap=%.1f cm)"),
				RimBoard->GetBoardLengthFeet(), NeededLengthFeet, GapDistance);
			RimBoard->SetBoardLengthFeet(NeededLengthFeet);

			// Re-find sockets after resize (they moved)
			LeftCornerSocket = nullptr;
			RightCornerSocket = nullptr;
			for (FConstructionSocket& Socket : Sockets)
			{
				if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner)
				{
					if (Socket.SocketName.ToString().Contains(TEXT("Left")))
						LeftCornerSocket = &Socket;
					else if (Socket.SocketName.ToString().Contains(TEXT("Right")))
						RightCornerSocket = &Socket;
				}
			}

			if (LeftCornerSocket && RightCornerSocket)
			{
				// Recalculate position with new socket locations
				LeftLocalOffset = SpanRotation.RotateVector(LeftCornerSocket->LocalPosition);
				ActorPosition = TargetLeftPos - LeftLocalOffset;

				RightLocalOffset = SpanRotation.RotateVector(RightCornerSocket->LocalPosition);
				PredictedRightPos = ActorPosition + RightLocalOffset;
				RightError = FVector::Dist(PredictedRightPos, TargetRightPos);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("DUAL-END SNAP: Left->%s on %s, Right->%s on %s | Yaw=%.1f | Error=%.2f cm"),
		*BestLeft.TargetSocketName.ToString(), *BestLeft.TargetPiece->GetName(),
		*BestRight.TargetSocketName.ToString(), *BestRight.TargetPiece->GetName(),
		SpanRotation.Yaw, RightError);

	OutSnapLocation = ActorPosition;
	OutSnapRotation = SpanRotation;
	OutTargetPiece = BestLeft.TargetPiece;
	OutTargetSocketName = BestLeft.TargetSocketName;

	return true;
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

	if (bAutoNailOnPlace)
	{
		PieceState = EPieceState::Nailed;
	}
	else
	{
		PieceState = EPieceState::Placed;
	}

	if (AConstructionPhaseManager::Instance)
	{
		AConstructionPhaseManager::Instance->RegisterPlacedPiece(this);
		UE_LOG(LogTemp, Log, TEXT("Registered %s with PhaseManager"), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ConstructionPhaseManager not found! Add BP_ConstructionPhaseManager to your level!"));
	}

	if (bIsSnapped && SnappedToPiece != nullptr)
	{
		SnappedToPiece->OccupySocket(SnappedToSocketName, this);
	}

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
