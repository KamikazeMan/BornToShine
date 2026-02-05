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
	// This base implementation does nothing
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

		// Enable collision only for tracing, not physics
		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			// Don't collide with pawns (characters) to prevent pushing them
			MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			// Also ignore physics objects to prevent interference
			MeshComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
		}
	}
	else
	{
		PieceState = EPieceState::Placed;

		// Enable full collision
		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			// Restore collision with pawns and physics
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
			UE_LOG(LogTemp, Verbose, TEXT("Using auto-rotation for corner snap: %.1f°"), SnapRotation.Yaw);
		}
		else
		{
			// Normal snap: Preserve user's Yaw rotation
			// Only use snap rotation for pitch/roll alignment
			FRotator FinalRotation = SnapRotation;
			FinalRotation.Yaw = NewRotation.Yaw; // Keep user's horizontal rotation
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
		// Only log once per second to avoid spam
		static float LastLogTime = 0.0f;
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastLogTime > 1.0f)
		{
			UE_LOG(LogTemp, Verbose, TEXT("%s: No nearby pieces within %.0fcm"), *GetName(), SnapSearchRadius);
			LastLogTime = CurrentTime;
		}
		return false;
	}

	UE_LOG(LogTemp, Verbose, TEXT("%s: Searching %d nearby pieces with %d sockets"),
		*GetName(), NearbyPieces.Num(), Sockets.Num());

	// Try each socket on this piece to find the best snap
	bool bFoundSnap = false;
	float BestDistance = FLT_MAX;
	int32 BestPriority = -1; // Track connection priority

	for (const FConstructionSocket& Socket : Sockets)
	{
		FVector SocketWorldLocation = GetActorTransform().TransformPosition(Socket.LocalPosition);
		FRotator SocketWorldRotation = GetActorRotation() + Socket.LocalRotation;

		// DEBUG: Log when evaluating corner sockets
		if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner)
		{
			UE_LOG(LogTemp, Warning, TEXT("🔵 Evaluating RimBoard_End_Corner socket: %s at world %s"),
				*Socket.SocketName.ToString(),
				*SocketWorldLocation.ToString());
		}

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

			// Get target socket type to determine priority
			EConstructionSocketType TargetSocketType = GetTargetSocketType(TargetPiece, TargetSocketName);

			// Calculate connection priority
			int32 Priority = GetSocketConnectionPriority(Socket.SocketType, TargetSocketType);

			// Choose this snap if:
			// 1. It has higher priority, OR
			// 2. Same priority but closer distance
			bool bIsBetter = (Priority > BestPriority) ||
			                 (Priority == BestPriority && Distance < BestDistance);

			if (bIsBetter)
			{
				BestDistance = Distance;
				BestPriority = Priority;

				// CRITICAL: Calculate where THIS actor's origin should be
				// to align THIS socket with the TARGET socket
				FVector SocketLocalOffset = Socket.LocalPosition;
				FRotator CurrentActorRotation = GetActorRotation();

				// Special handling for rim-to-rim corner snaps: auto-rotate to perpendicular
				if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
					TargetSocketType == EConstructionSocketType::RimBoard_End_Corner &&
					TargetPiece)
				{
					// No socket name filtering - let distance-based scoring pick best match
					// Socket names (Left/Right) are in local board space and don't reflect
					// world-space geometry after rotation. The 4-socket system (Outer/Inner)
					// will naturally pick the correct pairing based on proximity.

					// Get target piece rotation
					FRotator TargetRotation = TargetPiece->GetActorRotation();

					// Calculate relative position to determine rotation direction
					FVector TargetSocketWorldPos = SnapLoc;
					FVector SourceSocketWorldPos = SocketWorldLocation;
					FVector ToTarget = (TargetSocketWorldPos - SourceSocketWorldPos).GetSafeNormal();

					// Get target's forward vector
					FVector TargetForward = TargetRotation.RotateVector(FVector::ForwardVector);

					// Use cross product to determine which way to rotate
					// If cross product Z is positive, rotate +90°, if negative, rotate -90°
					FVector CrossProduct = FVector::CrossProduct(TargetForward, ToTarget);
					float RotationOffset = (CrossProduct.Z > 0) ? 90.0f : -90.0f;

					CurrentActorRotation.Yaw = TargetRotation.Yaw + RotationOffset;

					UE_LOG(LogTemp, Warning, TEXT("  🔄 Auto-rotating for corner snap: Target=%.1f° + Offset=%.1f° = New=%.1f° (Cross.Z=%.2f)"),
						TargetRotation.Yaw, RotationOffset, CurrentActorRotation.Yaw, CrossProduct.Z);
				}

				// Transform the socket offset by the (possibly auto-rotated) rotation
				FVector SocketWorldOffset = CurrentActorRotation.RotateVector(SocketLocalOffset);

				// Actor position = Target socket position - socket offset in world space
				OutSnapLocation = SnapLoc - SocketWorldOffset;
				OutSnapRotation = CurrentActorRotation;

				// OUTSIDE/INSIDE BOARD CORNER SYSTEM:
				// - Outside boards: Full 8ft, extend to outer corner
				// - Inside boards: 3" shorter, butt against outside boards
				// When an inside board connects to an outside board, offset it by board width
				if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
					TargetSocketType == EConstructionSocketType::RimBoard_End_Corner &&
					TargetPiece)
				{
					// Check if either board is an inside board (needs offset)
					ARimBoard* SourceRimBoard = Cast<ARimBoard>(this);
					ARimBoard* TargetRimBoard = Cast<ARimBoard>(TargetPiece);

					if (SourceRimBoard && TargetRimBoard)
					{
						bool bSourceIsInside = !SourceRimBoard->bIsOutsideBoard;
						bool bTargetIsOutside = TargetRimBoard->bIsOutsideBoard;

						// Inside board at 93" should fit naturally between outside boards
						// The 3" shorter length (1.5" each end) accounts for butting against outside board sides
						// NO offset needed - just let the shorter board fit
						if (bSourceIsInside && bTargetIsOutside)
						{
							UE_LOG(LogTemp, Warning, TEXT("  🔧 Inside board (93\") connecting to outside board - no offset (shorter length handles fit)"));
						}
						else if (!bSourceIsInside && !bTargetIsOutside)
						{
							UE_LOG(LogTemp, Warning, TEXT("  🔧 Outside board connecting to inside board - no offset needed"));
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("  🔧 Both boards are %s type"),
								bSourceIsInside ? TEXT("INSIDE") : TEXT("OUTSIDE"));
						}
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("  🧲 Corner snap - boards overlap for flush look (no offset)"));
					}
				}

				SnappedToPiece = TargetPiece;
				SnappedToSocketName = TargetSocketName;
				bFoundSnap = true;

				UE_LOG(LogTemp, Warning, TEXT("SNAP FOUND! Socket: %s -> Target: %s on %s (Priority=%d)"),
					*Socket.SocketName.ToString(),
					*TargetSocketName.ToString(),
					*TargetPiece->GetName(),
					Priority);
				UE_LOG(LogTemp, Warning, TEXT("  Target World Pos (SnapLoc): %s"), *SnapLoc.ToString());
				UE_LOG(LogTemp, Warning, TEXT("  Socket Local Pos: %s"), *Socket.LocalPosition.ToString());
				UE_LOG(LogTemp, Warning, TEXT("  Socket World Offset: %s"), *SocketWorldOffset.ToString());
				UE_LOG(LogTemp, Warning, TEXT("  Final Actor Snap Location: %s, Distance: %.1fcm"),
					*OutSnapLocation.ToString(), Distance);
			}
		}
	}

	return bFoundSnap;
}

int32 ABuildablePiece::GetSocketConnectionPriority(EConstructionSocketType SocketA, EConstructionSocketType SocketB) const
{
	// Rim-to-Rim corner connections (HIGHEST PRIORITY)
	// Must win over foundation to enable auto-rotation and proper corner alignment
	// Z height will be maintained at foundation level separately
	if ((SocketA == EConstructionSocketType::RimBoard_End_Corner &&
		 SocketB == EConstructionSocketType::RimBoard_End_Corner))
	{
		return 1000; // Highest priority - enables auto-rotation and corner snapping
	}

	// Rim-to-Rim side connections (HIGH PRIORITY)
	// For perpendicular joists
	if ((SocketA == EConstructionSocketType::RimBoard_Side_Face &&
		 SocketB == EConstructionSocketType::RimBoard_Side_Face))
	{
		return 900;
	}

	// Joist-to-Rim connections (HIGH PRIORITY)
	// Joists connecting to rim boards
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
	// Only for initial placement when no other rim boards nearby
	if ((SocketA == EConstructionSocketType::RimBoard_Bottom_End &&
		 (SocketB == EConstructionSocketType::Foundation_Corner ||
		  SocketB == EConstructionSocketType::Foundation_Side)) ||
		((SocketA == EConstructionSocketType::Foundation_Corner ||
		  SocketA == EConstructionSocketType::Foundation_Side) &&
		 SocketB == EConstructionSocketType::RimBoard_Bottom_End))
	{
		return 10; // Low priority - corners should always override
	}

	// Default priority for other connections
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

	// Check if placement is valid
	if (!IsPlacementValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot place piece - invalid placement"));
		return false;
	}

	// Change state to placed (or nailed if auto-nail is enabled)
	if (bAutoNailOnPlace)
	{
		PieceState = EPieceState::Nailed;
	}
	else
	{
		PieceState = EPieceState::Placed;
	}

	// Register with construction phase manager
	if (AConstructionPhaseManager::Instance)
	{
		AConstructionPhaseManager::Instance->RegisterPlacedPiece(this);
		UE_LOG(LogTemp, Log, TEXT("Registered %s with PhaseManager"), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ConstructionPhaseManager not found! Add BP_ConstructionPhaseManager to your level!"));
	}

	// If snapped, occupy the target socket
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
	// Unregister from construction phase manager
	if (AConstructionPhaseManager::Instance)
	{
		AConstructionPhaseManager::Instance->UnregisterPiece(this);
	}

	// Free any occupied sockets
	if (bIsSnapped && SnappedToPiece != nullptr)
	{
		SnappedToPiece->FreeSocket(SnappedToSocketName);
	}

	// Free our own sockets
	for (FConstructionSocket& Socket : Sockets)
	{
		if (Socket.bIsOccupied && Socket.ConnectedPiece.IsValid())
		{
			// The connected piece should handle its own cleanup
			Socket.bIsOccupied = false;
			Socket.ConnectedPiece = nullptr;
		}
	}

	Destroy();
}

bool ABuildablePiece::IsPlacementValid() const
{
	if (!AConstructionPhaseManager::Instance) return false;

	// Check if this piece type can be placed in current phase
	if (!AConstructionPhaseManager::Instance->CanPlacePieceType(PieceType))
	{
		return false;
	}

	// Check prerequisites
	if (!AConstructionPhaseManager::Instance->CheckPrerequisites(PieceType, GetActorLocation()))
	{
		return false;
	}

	// Check if piece is supported (not floating)
	if (!IsSupported())
	{
		return false;
	}

	// If we require snapping, check if snapped
	// (For now, all pieces should snap except foundation)
	if (PieceType != EPieceType::Foundation && !bIsSnapped)
	{
		return false;
	}

	return true;
}

bool ABuildablePiece::IsSupported() const
{
	// Foundation blocks don't need support
	if (PieceType == EPieceType::Foundation)
	{
		return true;
	}

	// Other pieces must be snapped to something
	return bIsSnapped;
}

void ABuildablePiece::UpdateVisualFeedback()
{
	if (!MeshComponent) return;

	// If nailed and we have a final material, switch to it
	if (PieceState == EPieceState::Nailed && NailedMaterial)
	{
		MeshComponent->SetMaterial(0, NailedMaterial);
		MeshComponent->SetRenderCustomDepth(false);
		return;
	}

	// Otherwise use dynamic material with color feedback
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

	// Update material color and opacity
	DynamicMaterial->SetVectorParameterValue(FName("BaseColor"), TargetColor);

	// Set transparency for preview mode
	if (PieceState == EPieceState::Preview)
	{
		MeshComponent->SetRenderCustomDepth(true);
	}
	else
	{
		MeshComponent->SetRenderCustomDepth(false);
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

	bIsSnapped = false; // Clear snap state when manually rotating
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

	// Calculate new scale
	float NewScaleValue = CurrentScale.X + ScaleDelta;
	NewScaleValue = FMath::Clamp(NewScaleValue, MinScale, MaxScale);

	CurrentScale = FVector(NewScaleValue, NewScaleValue, NewScaleValue);
	SetActorScale3D(CurrentScale);

	UE_LOG(LogTemp, Log, TEXT("Piece scaled to %f"), NewScaleValue);
}
