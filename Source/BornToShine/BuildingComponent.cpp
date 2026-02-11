// Born To Shine - Building System Component

#include "BuildingComponent.h"
#include "BuildablePiece.h"
#include "RimBoard.h"
#include "FloorJoist.h"
#include "PlywoodSheet.h"
#include "BottomPlate.h"
#include "RectangleBuilder.h"
#include "ConstructionPhaseManager.h"
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

	// Auto-register piece types that are missing from AvailablePieceTypes.
	// The C++ classes have default cube meshes so they render without Blueprint setup.
	auto HasPieceType = [this](EPieceType TypeToFind) -> bool
	{
		for (const TSubclassOf<ABuildablePiece>& PieceClass : AvailablePieceTypes)
		{
			if (PieceClass)
			{
				ABuildablePiece* CDO = PieceClass->GetDefaultObject<ABuildablePiece>();
				if (CDO && CDO->GetPieceType() == TypeToFind) return true;
			}
		}
		return false;
	};

	if (!HasPieceType(EPieceType::Plywood))
	{
		AvailablePieceTypes.Add(APlywoodSheet::StaticClass());
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Auto-added PlywoodSheet to AvailablePieceTypes"));
	}
	if (!HasPieceType(EPieceType::WallPlate))
	{
		AvailablePieceTypes.Add(ABottomPlate::StaticClass());
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Auto-added BottomPlate to AvailablePieceTypes"));
	}

	UE_LOG(LogTemp, Log, TEXT("BuildingComponent: %d piece types available"), AvailablePieceTypes.Num());
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

	TSubclassOf<ABuildablePiece> PieceClass = AvailablePieceTypes[CurrentPieceTypeIndex];
	if (!PieceClass)
	{
		UE_LOG(LogTemp, Error, TEXT("BuildingComponent: AvailablePieceTypes[%d] is NULL — skip"),
			CurrentPieceTypeIndex);
		return;
	}

	FVector SpawnLocation = Owner->GetActorLocation() + Owner->GetActorForwardVector() * PreviewDistance;

	CurrentPreviewPiece = GetWorld()->SpawnActor<ABuildablePiece>(
		PieceClass,
		SpawnLocation,
		PreviewRotation
	);

	if (CurrentPreviewPiece)
	{
		CurrentPreviewPiece->SetPreviewMode(true);

		// Diagnostic: verify the mesh loaded
		UStaticMeshComponent* Mesh = CurrentPreviewPiece->GetMeshComponent();
		bool bHasMesh = Mesh && Mesh->GetStaticMesh();
		bool bHasMaterial = Mesh && Mesh->GetMaterial(0);

		UE_LOG(LogTemp, Warning, TEXT("SpawnPreviewPiece[%d]: Class=%s  Type=%s  HasMesh=%d  HasMaterial=%d  Visible=%d"),
			CurrentPieceTypeIndex,
			*PieceClass->GetName(),
			*UEnum::GetDisplayValueAsText(CurrentPreviewPiece->GetPieceType()).ToString(),
			bHasMesh, bHasMaterial,
			Mesh ? Mesh->IsVisible() : -1);

		if (!bHasMesh)
		{
			UE_LOG(LogTemp, Error, TEXT("  >> NO MESH on preview! If using raw C++ class, create a BP_ wrapper with a static mesh assigned."));
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
				FString::Printf(TEXT("WARNING: %s has no mesh — create a Blueprint with a static mesh"), *PieceClass->GetName()));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnPreviewPiece: SpawnActor FAILED for class %s"), *PieceClass->GetName());
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

	// PLATE SUGGESTION OVERRIDE: When placing a bottom plate and the RectangleBuilder
	// has plate suggestions (from a completed rim board rectangle), position the preview
	// at the next plate location on top of the plywood.
	if (RectangleBuilder && RectangleBuilder->HasPlateSuggestions() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::WallPlate)
	{
		FPlateSuggestion PlateSug = RectangleBuilder->GetNextPlateSuggestion();
		if (PlateSug.bIsValid)
		{
			ABottomPlate* PreviewPlate = Cast<ABottomPlate>(CurrentPreviewPiece);
			if (PreviewPlate && PlateSug.LengthFeet > 0 && PlateSug.LengthFeet != PreviewPlate->GetBoardLengthFeet())
			{
				PreviewPlate->SetBoardLengthFeet(PlateSug.LengthFeet);
			}

			// Recalculate Z from actual plywood top surface (suggestion Z was estimated before plywood existed)
			FVector PreviewPos = PlateSug.Position;
			const float PlateHalfHeight = 8.89f / 2.0f;
			if (AConstructionPhaseManager::Instance)
			{
				TArray<ABuildablePiece*> PlywoodPieces =
					AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::Plywood);
				float BestPlywoodTopZ = 0.0f;
				bool bFoundPly = false;
				for (ABuildablePiece* P : PlywoodPieces)
				{
					APlywoodSheet* Ply = Cast<APlywoodSheet>(P);
					if (!Ply) continue;
					float TopZ = Ply->GetActorLocation().Z + Ply->SheetThickness / 2.0f;
					if (!bFoundPly || TopZ > BestPlywoodTopZ)
					{
						BestPlywoodTopZ = TopZ;
					}
					bFoundPly = true;
				}
				if (bFoundPly)
				{
					PreviewPos.Z = BestPlywoodTopZ + PlateHalfHeight;
				}
			}

			CurrentPreviewPiece->SetActorLocation(PreviewPos);
			CurrentPreviewPiece->SetActorRotation(PlateSug.Rotation);
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

	// PLATE SUGGESTION PATH: When plate suggestions exist and we're placing a wall plate,
	// bypass TryPlace and use the calculated position above the rim board.
	if (RectangleBuilder && RectangleBuilder->HasPlateSuggestions() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::WallPlate)
	{
		ABottomPlate* Plate = Cast<ABottomPlate>(CurrentPreviewPiece);
		if (Plate && RectangleBuilder->ApplyPlateSuggestion(Plate))
		{
			PlacedPieces.Add(CurrentPreviewPiece);
			LastPlacedPiece = CurrentPreviewPiece;

			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();

			UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Bottom plate placed via suggestion (Total: %d)"), PlacedPieces.Num());
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

	// Show plywood info
	if (CurrentPreviewPiece && CurrentPreviewPiece->GetPieceType() == EPieceType::Plywood)
	{
		UE_LOG(LogTemp, Log, TEXT("Plywood Sheet selected: 4x8ft panel — snap to joist/rim board top faces"));
	}

	// Show bottom plate info
	if (CurrentPreviewPiece && CurrentPreviewPiece->GetPieceType() == EPieceType::WallPlate)
	{
		if (RectangleBuilder && RectangleBuilder->HasPlateSuggestions())
		{
			FPlateSuggestion NextPlate = RectangleBuilder->GetNextPlateSuggestion();
			UE_LOG(LogTemp, Log, TEXT("Bottom Plate selected: %d/%d plates to place, %dft length"),
				RectangleBuilder->GetPlacedPlateCount(),
				RectangleBuilder->GetPlateSuggestions().Num(),
				NextPlate.LengthFeet);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Bottom Plate selected: No plate layout available (build a rim board rectangle first)"));
		}
	}
}

void UBuildingComponent::RotatePreviewLeft()
{
	if (CurrentPreviewPiece)
	{
		// Plywood snaps to rim board directions — use 90° steps so one press
		// toggles between the two perpendicular frame orientations.
		float Step = (CurrentPreviewPiece->GetPieceType() == EPieceType::Plywood) ? 90.0f : 15.0f;
		PreviewRotation.Yaw -= Step;
		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: RotateLeft - PreviewRotation.Yaw=%.1f (step=%.0f)"), PreviewRotation.Yaw, Step);
	}
}

void UBuildingComponent::RotatePreviewRight()
{
	if (CurrentPreviewPiece)
	{
		float Step = (CurrentPreviewPiece->GetPieceType() == EPieceType::Plywood) ? 90.0f : 15.0f;
		PreviewRotation.Yaw += Step;
		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: RotateRight - PreviewRotation.Yaw=%.1f (step=%.0f)"), PreviewRotation.Yaw, Step);
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
