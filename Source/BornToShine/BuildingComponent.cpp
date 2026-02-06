// Born To Shine - Building System Component

#include "BuildingComponent.h"
#include "BuildablePiece.h"
#include "RimBoard.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"

UBuildingComponent::UBuildingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	bIsInBuildMode = false;
	CurrentPreviewPiece = nullptr;
	LastPlacedPiece = nullptr;
	CurrentPieceTypeIndex = 0;

	BuildRaycastDistance = 2000.0f; // 20 meters
	PreviewDistance = 500.0f;       // 5 meters (increased for third-person comfort)
	SnapSearchRadius = 500.0f;      // 5 meters

	PreviewRotation = FRotator::ZeroRotator; // Start with no rotation
}

void UBuildingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBuildingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update preview piece position if in build mode
	if (bIsInBuildMode && CurrentPreviewPiece)
	{
		UpdatePreviewPosition();
	}
}

void UBuildingComponent::ToggleBuildMode()
{
	bIsInBuildMode = !bIsInBuildMode;

	if (bIsInBuildMode)
	{
		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Entered build mode"));
		SpawnPreviewPiece();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Exited build mode"));
		DestroyPreviewPiece();
	}
}

void UBuildingComponent::SpawnPreviewPiece()
{
	if (AvailablePieceTypes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: No piece types available"));
		return;
	}

	// Ensure index is valid
	if (CurrentPieceTypeIndex >= AvailablePieceTypes.Num())
	{
		CurrentPieceTypeIndex = 0;
	}

	// Destroy existing preview
	DestroyPreviewPiece();

	// Reset rotation for new piece
	PreviewRotation = FRotator::ZeroRotator;

	// Spawn new preview piece
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FVector SpawnLocation = Owner->GetActorLocation() + Owner->GetActorForwardVector() * PreviewDistance;

	CurrentPreviewPiece = GetWorld()->SpawnActor<ABuildablePiece>(
		AvailablePieceTypes[CurrentPieceTypeIndex],
		SpawnLocation,
		PreviewRotation
	);

	if (CurrentPreviewPiece)
	{
		CurrentPreviewPiece->SetPreviewMode(true);
		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Spawned preview piece"));
	}
}

void UBuildingComponent::DestroyPreviewPiece()
{
	if (CurrentPreviewPiece)
	{
		CurrentPreviewPiece->Destroy();
		CurrentPreviewPiece = nullptr;
	}
}

void UBuildingComponent::UpdatePreviewPosition()
{
	if (!CurrentPreviewPiece) return;

	FVector PlacementLocation;
	FVector PlacementNormal;

	if (GetPlacementLocation(PlacementLocation, PlacementNormal))
	{
		CurrentPreviewPiece->UpdatePreviewPosition(PlacementLocation, PreviewRotation);
	}
	else
	{
		// No valid hit, place in front of camera
		UCameraComponent* Camera = GetOwnerCamera();
		if (Camera)
		{
			FVector CameraLocation = Camera->GetComponentLocation();
			FVector CameraForward = Camera->GetForwardVector();
			FVector DefaultLocation = CameraLocation + (CameraForward * PreviewDistance);
			CurrentPreviewPiece->UpdatePreviewPosition(DefaultLocation, PreviewRotation);
		}
	}
}

bool UBuildingComponent::GetPlacementLocation(FVector& OutLocation, FVector& OutNormal)
{
	UCameraComponent* Camera = GetOwnerCamera();
	if (!Camera) return false;

	FVector Start = Camera->GetComponentLocation();
	FVector Forward = Camera->GetForwardVector();
	FVector End = Start + (Forward * BuildRaycastDistance);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	if (CurrentPreviewPiece)
	{
		QueryParams.AddIgnoredActor(CurrentPreviewPiece);
	}

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	if (bHit)
	{
		OutLocation = HitResult.Location;
		OutNormal = HitResult.Normal;
		return true;
	}

	return false;
}

UCameraComponent* UBuildingComponent::GetOwnerCamera() const
{
	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	// Try to find camera component
	TArray<UCameraComponent*> Cameras;
	Owner->GetComponents<UCameraComponent>(Cameras);

	// Return active camera
	for (UCameraComponent* Camera : Cameras)
	{
		if (Camera && Camera->IsActive())
		{
			return Camera;
		}
	}

	// Return first camera if none active
	return Cameras.Num() > 0 ? Cameras[0] : nullptr;
}

void UBuildingComponent::PlaceCurrentPiece()
{
	if (!bIsInBuildMode || !CurrentPreviewPiece) return;

	// Try to place the piece
	if (CurrentPreviewPiece->TryPlace())
	{
		// Add to placed pieces list
		PlacedPieces.Add(CurrentPreviewPiece);
		LastPlacedPiece = CurrentPreviewPiece;

		// Spawn new preview
		CurrentPreviewPiece = nullptr;
		SpawnPreviewPiece();

		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Piece placed (Total: %d)"), PlacedPieces.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Cannot place piece at this location"));
	}
}

void UBuildingComponent::NailLastPlacedPiece()
{
	if (LastPlacedPiece)
	{
		LastPlacedPiece->NailInPlace();
		PlacedPieces.Remove(LastPlacedPiece);
		LastPlacedPiece = nullptr;
		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Piece nailed in place"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: No piece to nail"));
	}
}

void UBuildingComponent::RemoveLastPlacedPiece()
{
	if (LastPlacedPiece)
	{
		LastPlacedPiece->Remove();
		PlacedPieces.Remove(LastPlacedPiece);
		LastPlacedPiece = nullptr;
		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Last piece removed"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: No piece to remove"));
	}
}

void UBuildingComponent::CyclePieceType()
{
	if (!bIsInBuildMode || AvailablePieceTypes.Num() == 0) return;

	CurrentPieceTypeIndex = (CurrentPieceTypeIndex + 1) % AvailablePieceTypes.Num();
	SpawnPreviewPiece();

	UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Cycled to %s"), *GetCurrentPieceName());

	// Show length info for rim boards (only when cycling, not every frame)
	if (CurrentPreviewPiece && CurrentPreviewPiece->GetPieceType() == EPieceType::RimBoard)
	{
		if (class ARimBoard* RimBoard = Cast<class ARimBoard>(CurrentPreviewPiece))
		{
			FString LengthInfo = RimBoard->GetLengthDisplayString();
			UE_LOG(LogTemp, Log, TEXT("RimBoard selected: %s (scroll wheel disabled)"), *LengthInfo);
		}
	}
}

void UBuildingComponent::RotatePreviewLeft()
{
	if (CurrentPreviewPiece)
	{
		PreviewRotation.Yaw -= 15.0f; // 15 degree rotation step
		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: RotateLeft - PreviewRotation.Yaw=%.1f"), PreviewRotation.Yaw);
	}
}

void UBuildingComponent::RotatePreviewRight()
{
	if (CurrentPreviewPiece)
	{
		PreviewRotation.Yaw += 15.0f; // 15 degree rotation step
		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: RotateRight - PreviewRotation.Yaw=%.1f"), PreviewRotation.Yaw);
	}
}

void UBuildingComponent::RotatePreviewPitch(float Value)
{
	if (CurrentPreviewPiece && FMath::Abs(Value) > 0.1f)
	{
		if (Value > 0)
		{
			PreviewRotation.Pitch += 15.0f;
		}
		else
		{
			PreviewRotation.Pitch -= 15.0f;
		}
	}
}

void UBuildingComponent::RotatePreviewRoll(float Value)
{
	if (CurrentPreviewPiece && FMath::Abs(Value) > 0.1f)
	{
		PreviewRotation.Roll += Value * 15.0f;
	}
}

void UBuildingComponent::ScalePreview(float ScaleDelta)
{
	if (CurrentPreviewPiece && FMath::Abs(ScaleDelta) > 0.01f)
	{
		CurrentPreviewPiece->ScalePiece(ScaleDelta * 0.1f);

		// Show updated length for rim boards
		if (CurrentPreviewPiece->GetPieceType() == EPieceType::RimBoard)
		{
			if (ARimBoard* RimBoard = Cast<ARimBoard>(CurrentPreviewPiece))
			{
				UE_LOG(LogTemp, Warning, TEXT("Rim Board Length: %s"), *RimBoard->GetLengthDisplayString());
			}
		}
	}
}

EPieceType UBuildingComponent::GetCurrentPieceType() const
{
	if (CurrentPreviewPiece)
	{
		return CurrentPreviewPiece->GetPieceType();
	}
	return EPieceType::None;
}

FString UBuildingComponent::GetCurrentPieceName() const
{
	return UEnum::GetDisplayValueAsText(GetCurrentPieceType()).ToString();
}
