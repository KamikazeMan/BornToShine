// Born To Shine - Building System Component

#include "BuildingComponent.h"
#include "BuildablePiece.h"
#include "RimBoard.h"
#include "FloorJoist.h"
#include "RectangleBuilder.h"
#include "BornToShineHUD.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
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

	// Find or create the RectangleBuilder component on the owning actor
	RectangleBuilder = GetOwner()->FindComponentByClass<URectangleBuilderComponent>();
	if (!RectangleBuilder)
	{
		RectangleBuilder = NewObject<URectangleBuilderComponent>(GetOwner());
		RectangleBuilder->RegisterComponent();
	}
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

	// Toggle crosshair visibility
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC)
		{
			ABornToShineHUD* HUD = Cast<ABornToShineHUD>(PC->GetHUD());
			if (HUD)
			{
				HUD->SetCrosshairVisible(bIsInBuildMode);
			}
		}
	}

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

	// JOIST SUGGESTION OVERRIDE: When placing a floor joist and the RectangleBuilder
	// has joist suggestions, position the preview at the next joist location.
	if (RectangleBuilder && RectangleBuilder->HasJoistSuggestions() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::FloorJoist)
	{
		FJoistSuggestion JoistSug = RectangleBuilder->GetNextJoistSuggestion();
		if (JoistSug.bIsValid)
		{
			AFloorJoist* PreviewJoist = Cast<AFloorJoist>(CurrentPreviewPiece);
			if (PreviewJoist && JoistSug.LengthFeet > 0 && JoistSug.LengthFeet != PreviewJoist->GetBoardLengthFeet())
			{
				PreviewJoist->SetBoardLengthFeet(JoistSug.LengthFeet);
			}

			CurrentPreviewPiece->SetActorLocation(JoistSug.Position);
			CurrentPreviewPiece->SetActorRotation(JoistSug.Rotation);
			return;
		}
	}

	// RECTANGLE BUILDER OVERRIDE: When placing a rim board and the RectangleBuilder
	// has an active suggestion (L-shape or U-shape detected), bypass all normal snap
	// detection. Position the preview exactly where the suggestion says.
	if (RectangleBuilder && RectangleBuilder->HasActiveSuggestion() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::RimBoard)
	{
		FBoardSuggestion Suggestion = RectangleBuilder->GetActiveSuggestion();

		// Resize the preview board to match the suggestion
		ARimBoard* PreviewRim = Cast<ARimBoard>(CurrentPreviewPiece);
		if (PreviewRim && Suggestion.LengthFeet > 0 && Suggestion.LengthFeet != PreviewRim->GetBoardLengthFeet())
		{
			PreviewRim->SetBoardLengthFeet(Suggestion.LengthFeet);
		}

		// Set the preview directly at the suggested transform — no snap detection
		CurrentPreviewPiece->SetActorLocation(Suggestion.Position);
		CurrentPreviewPiece->SetActorRotation(Suggestion.Rotation);
		return;
	}

	// Normal path: raycast + snap detection
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

	// JOIST SUGGESTION PATH: When joist suggestions exist and we're placing a joist,
	// bypass TryPlace and use the calculated position.
	if (RectangleBuilder && RectangleBuilder->HasJoistSuggestions() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::FloorJoist)
	{
		AFloorJoist* Joist = Cast<AFloorJoist>(CurrentPreviewPiece);
		if (Joist && RectangleBuilder->ApplyJoistSuggestion(Joist))
		{
			PlacedPieces.Add(CurrentPreviewPiece);
			LastPlacedPiece = CurrentPreviewPiece;

			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();

			UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Joist placed via suggestion (Total: %d)"), PlacedPieces.Num());
			return;
		}
	}

	// RECTANGLE BUILDER PATH: When a suggestion is active, bypass TryPlace entirely.
	// Place the board exactly where the RectangleBuilder calculated, with correct length.
	if (RectangleBuilder && RectangleBuilder->HasActiveSuggestion() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::RimBoard)
	{
		ARimBoard* RimBoard = Cast<ARimBoard>(CurrentPreviewPiece);
		if (RimBoard && RectangleBuilder->ApplySuggestionToBoard(RimBoard))
		{
			PlacedPieces.Add(CurrentPreviewPiece);
			LastPlacedPiece = CurrentPreviewPiece;

			// Notify RectangleBuilder — this will advance the state (LShape -> UShape -> Complete)
			RectangleBuilder->OnRimBoardPlaced(RimBoard);

			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();

			UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Board placed via RectangleBuilder suggestion (Total: %d)"), PlacedPieces.Num());
			return;
		}
	}

	// Normal path: snap-based placement
	if (CurrentPreviewPiece->TryPlace())
	{
		PlacedPieces.Add(CurrentPreviewPiece);
		LastPlacedPiece = CurrentPreviewPiece;

		// Notify RectangleBuilder if this is a rim board
		if (RectangleBuilder && CurrentPreviewPiece->GetPieceType() == EPieceType::RimBoard)
		{
			RectangleBuilder->OnRimBoardPlaced(Cast<ARimBoard>(LastPlacedPiece));
		}

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
		// Notify RectangleBuilder before removing if this is a rim board
		if (RectangleBuilder && LastPlacedPiece->GetPieceType() == EPieceType::RimBoard)
		{
			RectangleBuilder->OnRimBoardRemoved(Cast<ARimBoard>(LastPlacedPiece));
		}

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

	// Show joist suggestion info
	if (CurrentPreviewPiece && CurrentPreviewPiece->GetPieceType() == EPieceType::FloorJoist)
	{
		if (RectangleBuilder && RectangleBuilder->HasJoistSuggestions())
		{
			FJoistSuggestion NextJoist = RectangleBuilder->GetNextJoistSuggestion();
			UE_LOG(LogTemp, Log, TEXT("Floor Joist selected: %d/%d joists to place, %dft span"),
				RectangleBuilder->GetPlacedJoistCount(),
				RectangleBuilder->GetJoistSuggestions().Num(),
				NextJoist.LengthFeet);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Floor Joist selected: No joist layout available (build a rectangle first)"));
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
