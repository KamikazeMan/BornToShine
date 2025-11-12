// Born To Shine - Base class for all buildable construction pieces

#include "BuildablePiece.h"
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

		// Enable collision only for tracing
		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
	}
	else
	{
		PieceState = EPieceState::Placed;

		// Enable full collision
		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
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
		SetActorRotation(SnapRotation);
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
			if (Distance < BestDistance)
			{
				BestDistance = Distance;

				// Calculate the offset from socket to actor origin
				FVector SocketOffset = Socket.LocalPosition;
				OutSnapLocation = SnapLoc - GetActorRotation().RotateVector(SocketOffset);
				OutSnapRotation = SnapRot - Socket.LocalRotation;

				SnappedToPiece = TargetPiece;
				SnappedToSocketName = TargetSocketName;
				bFoundSnap = true;
			}
		}
	}

	return bFoundSnap;
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
