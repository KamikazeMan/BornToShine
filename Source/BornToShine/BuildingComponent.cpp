// Born To Shine - Building System Component

#include "BuildingComponent.h"
#include "BuildablePiece.h"
#include "RimBoard.h"
#include "FloorJoist.h"
#include "PlywoodSheet.h"
#include "BottomPlate.h"
#include "WallStud.h"
#include "TopPlate.h"
#include "DoorFrame.h"
#include "RidgePost.h"
#include "RidgeBoard.h"
#include "Rafter.h"
#include "FasciaBoard.h"
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
	if (!HasPieceType(EPieceType::WallStud))
	{
		AvailablePieceTypes.Add(AWallStud::StaticClass());
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Auto-added WallStud to AvailablePieceTypes"));
	}
	if (!HasPieceType(EPieceType::DoorFrame))
	{
		AvailablePieceTypes.Add(ADoorFrame::StaticClass());
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Auto-added DoorFrame to AvailablePieceTypes"));
	}
	if (!HasPieceType(EPieceType::TopPlate))
	{
		AvailablePieceTypes.Add(ATopPlate::StaticClass());
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Auto-added TopPlate to AvailablePieceTypes"));
	}
	if (!HasPieceType(EPieceType::RidgePost))
	{
		AvailablePieceTypes.Add(ARidgePost::StaticClass());
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Auto-added RidgePost to AvailablePieceTypes"));
	}
	if (!HasPieceType(EPieceType::RidgeBoard))
	{
		AvailablePieceTypes.Add(ARidgeBoard::StaticClass());
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Auto-added RidgeBoard to AvailablePieceTypes"));
	}
	if (!HasPieceType(EPieceType::Rafter))
	{
		AvailablePieceTypes.Add(ARafter::StaticClass());
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Auto-added Rafter to AvailablePieceTypes"));
	}
	if (!HasPieceType(EPieceType::FasciaBoard))
	{
		AvailablePieceTypes.Add(AFasciaBoard::StaticClass());
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Auto-added FasciaBoard to AvailablePieceTypes"));
	}

	// Also scan PieceTypeInfos for any piece types configured in the editor
	// that still aren't in AvailablePieceTypes (handles the case where the user
	// added a PieceTypeInfos entry but forgot to add the BP to AvailablePieceTypes).
	for (const FPieceTypeInfo& EditorInfo : PieceTypeInfos)
	{
		if (EditorInfo.PieceType == EPieceType::None) continue;
		if (HasPieceType(EditorInfo.PieceType)) continue;

		TSubclassOf<ABuildablePiece> AutoClass = nullptr;
		switch (EditorInfo.PieceType)
		{
		case EPieceType::DoorFrame:       AutoClass = ADoorFrame::StaticClass(); break;
		case EPieceType::WallStud:        AutoClass = AWallStud::StaticClass(); break;
		case EPieceType::WallPlate:       AutoClass = ABottomPlate::StaticClass(); break;
		case EPieceType::Plywood:         AutoClass = APlywoodSheet::StaticClass(); break;
		case EPieceType::TopPlate:        AutoClass = ATopPlate::StaticClass(); break;
		case EPieceType::RidgePost:       AutoClass = ARidgePost::StaticClass(); break;
		case EPieceType::RidgeBoard:      AutoClass = ARidgeBoard::StaticClass(); break;
		case EPieceType::Rafter:          AutoClass = ARafter::StaticClass(); break;
		case EPieceType::FasciaBoard:     AutoClass = AFasciaBoard::StaticClass(); break;
		default: break;
		}

		if (AutoClass)
		{
			AvailablePieceTypes.Add(AutoClass);
			UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Auto-added class for PieceType %d from PieceTypeInfos"),
				(int32)EditorInfo.PieceType);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BuildingComponent: %d piece types available, %d PieceTypeInfos configured"),
		AvailablePieceTypes.Num(), PieceTypeInfos.Num());
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

		UE_LOG(LogTemp, Log, TEXT("SpawnPreviewPiece[%d]: Class=%s  Type=%s  HasMesh=%d  HasMaterial=%d  Visible=%d"),
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
			CurrentPreviewPiece->MarkSnapped(true);

			// Red ghost when joist already exists at this position
			if (RectangleBuilder->OverlapsExistingPiece(EPieceType::FloorJoist, JoistSug.Position))
			{
				CurrentPreviewPiece->SetPreviewColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
			}
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
			const float PlateHalfHeight = 3.81f / 2.0f; // 1.5" / 2 — 2x4 lies flat
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
			CurrentPreviewPiece->MarkSnapped(true);
			return;
		}
	}

	// WALL STUD SUGGESTION OVERRIDE: When placing a wall stud and the RectangleBuilder
	// has stud suggestions, position the preview at the next stud location on the plate.
	if (RectangleBuilder &&
		RectangleBuilder->HasStudSuggestions() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::WallStud)
	{
		FStudSuggestion StudSug = RectangleBuilder->GetNextStudSuggestion();
		if (StudSug.bIsValid)
		{
			AWallStud* PreviewStud = Cast<AWallStud>(CurrentPreviewPiece);
			if (PreviewStud && !FMath::IsNearlyEqual(StudSug.StudHeightCm, PreviewStud->GetStudHeightCm(), 0.1f))
			{
				PreviewStud->SetStudHeightInches(StudSug.StudHeightCm / 2.54f);
			}

			CurrentPreviewPiece->SetActorLocation(StudSug.Position);
			CurrentPreviewPiece->SetActorRotation(StudSug.Rotation);
			CurrentPreviewPiece->MarkSnapped(true);
			return;
		}
	}

	// TOP PLATE SUGGESTION OVERRIDE: When placing a top plate and the RectangleBuilder
	// has top plate suggestions (first plates 0-3 + double plates 4-7), position the
	// preview at the next location. Both layers use the same TopPlate piece type.
	if (RectangleBuilder &&
		RectangleBuilder->HasTopPlateSuggestions() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::TopPlate)
	{
		FTopPlateSuggestion TopPlateSug = RectangleBuilder->GetNextTopPlateSuggestion();
		if (TopPlateSug.bIsValid)
		{
			ATopPlate* PreviewTopPlate = Cast<ATopPlate>(CurrentPreviewPiece);
			if (PreviewTopPlate)
			{
				// Only resize if length actually changed (avoids resetting bMeshExtended every frame)
				if (!FMath::IsNearlyEqual(TopPlateSug.LengthCm, PreviewTopPlate->GetEffectiveLength(), 0.1f))
				{
					PreviewTopPlate->SetBoardLengthCm(TopPlateSug.LengthCm);
				}
				// NOTE: Do NOT call ExtendMeshForFlushCorners here in preview.
				// Extension is applied once during ApplyTopPlateSuggestion to avoid
				// double-extend (scale compounding from 1.0156 → 1.0315).
			}

			CurrentPreviewPiece->SetActorLocation(TopPlateSug.Position);
			CurrentPreviewPiece->SetActorRotation(TopPlateSug.Rotation);
			CurrentPreviewPiece->MarkSnapped(true);
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
		CurrentPreviewPiece->MarkSnapped(true);

		// Turn preview red when an existing board already occupies this position
		if (RectangleBuilder->OverlapsExistingPiece(EPieceType::RimBoard, Suggestion.Position, 50.0f))
		{
			CurrentPreviewPiece->SetPreviewColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
		}
		else
		{
			CurrentPreviewPiece->SetPreviewColor(FLinearColor(0.0f, 1.0f, 0.0f, 0.5f));
		}
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
		else if (Joist)
		{
			// Overlap skip — count toward cycle gating even though no new piece placed
			if (AConstructionPhaseManager::Instance)
				AConstructionPhaseManager::Instance->IncrementCyclePieceCount(EPieceType::FloorJoist);
			CurrentPreviewPiece->SetPreviewColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
			CurrentPreviewPiece->SetLifeSpan(0.75f);
			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();
			UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Joist skipped (overlap), respawning"));
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
		else if (Plate)
		{
			// Overlap skip — count toward cycle gating
			if (AConstructionPhaseManager::Instance)
				AConstructionPhaseManager::Instance->IncrementCyclePieceCount(EPieceType::WallPlate);
			CurrentPreviewPiece->SetPreviewColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
			CurrentPreviewPiece->SetLifeSpan(0.75f);
			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();
			UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Plate skipped (overlap), respawning"));
			return;
		}
	}

	// WALL STUD SUGGESTION PATH: When stud suggestions exist and we're placing a wall stud,
	// bypass TryPlace and use the calculated 16" OC position on the bottom plate.
	if (RectangleBuilder &&
		RectangleBuilder->HasStudSuggestions() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::WallStud)
	{
		AWallStud* Stud = Cast<AWallStud>(CurrentPreviewPiece);
		if (Stud && RectangleBuilder->ApplyStudSuggestion(Stud))
		{
			PlacedPieces.Add(CurrentPreviewPiece);
			LastPlacedPiece = CurrentPreviewPiece;

			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();

			UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Wall stud placed via suggestion (Total: %d)"), PlacedPieces.Num());
			return;
		}
		else if (Stud)
		{
			// Overlap skip — count toward cycle gating
			if (AConstructionPhaseManager::Instance)
				AConstructionPhaseManager::Instance->IncrementCyclePieceCount(EPieceType::WallStud);
			CurrentPreviewPiece->SetPreviewColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
			CurrentPreviewPiece->SetLifeSpan(0.75f);
			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();
			UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Stud skipped (overlap), respawning"));
			return;
		}
	}

	// TOP PLATE SUGGESTION PATH: When top plate suggestions exist and we're placing a top plate,
	// bypass TryPlace and use the calculated position above the studs.
	if (RectangleBuilder &&
		RectangleBuilder->HasTopPlateSuggestions() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::TopPlate)
	{
		ATopPlate* TopPlateActor = Cast<ATopPlate>(CurrentPreviewPiece);
		if (TopPlateActor && RectangleBuilder->ApplyTopPlateSuggestion(TopPlateActor))
		{
			PlacedPieces.Add(CurrentPreviewPiece);
			LastPlacedPiece = CurrentPreviewPiece;

			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();

			UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Top plate placed via suggestion (Total: %d)"), PlacedPieces.Num());
			return;
		}
		else if (TopPlateActor)
		{
			// Overlap skip — count toward cycle gating
			if (AConstructionPhaseManager::Instance)
				AConstructionPhaseManager::Instance->IncrementCyclePieceCount(EPieceType::TopPlate);
			CurrentPreviewPiece->SetPreviewColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
			CurrentPreviewPiece->SetLifeSpan(0.75f);
			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();
			UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Top plate skipped (overlap), respawning"));
			return;
		}
	}

	// RIDGE POST SUGGESTION PATH: When ridge post suggestions exist,
	// bypass TryPlace and use the calculated center position.
	if (RectangleBuilder &&
		RectangleBuilder->HasRidgePostSuggestions() &&
		CurrentPreviewPiece->GetPieceType() == EPieceType::RidgePost)
	{
		ARidgePost* Post = Cast<ARidgePost>(CurrentPreviewPiece);
		if (Post && RectangleBuilder->ApplyRidgePostSuggestion(Post))
		{
			PlacedPieces.Add(CurrentPreviewPiece);
			LastPlacedPiece = CurrentPreviewPiece;

			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();

			UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Ridge post placed via suggestion (Total: %d)"), PlacedPieces.Num());
			return;
		}
		else if (Post)
		{
			// Overlap skip
			if (AConstructionPhaseManager::Instance)
				AConstructionPhaseManager::Instance->IncrementCyclePieceCount(EPieceType::RidgePost);
			CurrentPreviewPiece->SetPreviewColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
			CurrentPreviewPiece->SetLifeSpan(0.75f);
			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();
			UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Ridge post skipped (overlap), respawning"));
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
		else if (RimBoard)
		{
			// Overlap skip — count toward cycle gating (shared wall counts as handled)
			if (AConstructionPhaseManager::Instance)
				AConstructionPhaseManager::Instance->IncrementCyclePieceCount(EPieceType::RimBoard);
			CurrentPreviewPiece->SetPreviewColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
			CurrentPreviewPiece->SetLifeSpan(0.75f);
			CurrentPreviewPiece = nullptr;
			SpawnPreviewPiece();
			UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Rim board skipped (overlap), state advanced, respawning"));
			return;
		}
	}

	// Normal path: snap-based placement
	if (CurrentPreviewPiece->TryPlace())
	{
		ABuildablePiece* JustPlaced = CurrentPreviewPiece;
		PlacedPieces.Add(CurrentPreviewPiece);
		LastPlacedPiece = CurrentPreviewPiece;

		// Notify RectangleBuilder if this is a rim board
		if (RectangleBuilder && CurrentPreviewPiece->GetPieceType() == EPieceType::RimBoard)
		{
			RectangleBuilder->OnRimBoardPlaced(Cast<ARimBoard>(LastPlacedPiece));
		}

		// Safety registration with PhaseManager (idempotent — skips if already registered by CommitPlacement)
		if (AConstructionPhaseManager::Instance && JustPlaced)
		{
			AConstructionPhaseManager::Instance->RegisterPlacedPiece(JustPlaced);
		}

		CurrentPreviewPiece = nullptr;
		SpawnPreviewPiece();

		UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Piece placed type=%s (Total: %d)"),
			JustPlaced ? *UEnum::GetDisplayValueAsText(JustPlaced->GetPieceType()).ToString() : TEXT("null"),
			PlacedPieces.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Cannot place piece at this location"));
	}

	// --- Self-healing: after EVERY placement, scan all door frames and fix
	// any mesh collision that was re-enabled by shared BodySetup mutation. ---
	if (AConstructionPhaseManager::Instance)
	{
		TArray<ABuildablePiece*> AllDoors =
			AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::DoorFrame);
		for (ABuildablePiece* P : AllDoors)
		{
			if (!P) continue;
			UStaticMeshComponent* M = P->GetMeshComponent();
			if (M && M->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				UE_LOG(LogTemp, Error,
					TEXT("DOORFRAME COLLISION CORRUPTION detected after placement! [%s] CollisionEnabled=%d. Forcing NoCollision."),
					*P->GetName(), (int32)M->GetCollisionEnabled());
				M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				M->SetCollisionResponseToAllChannels(ECR_Ignore);
			}
		}
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

	// Cycle forward, but skip locked piece types (up to one full loop)
	AConstructionPhaseManager* PM = AConstructionPhaseManager::Instance;
	int32 StartIndex = CurrentPieceTypeIndex;
	int32 NextIndex = (CurrentPieceTypeIndex + 1) % AvailablePieceTypes.Num();

	if (PM)
	{
		int32 Attempts = 0;
		while (Attempts < AvailablePieceTypes.Num())
		{
			if (AvailablePieceTypes[NextIndex])
			{
				ABuildablePiece* CDO = AvailablePieceTypes[NextIndex]->GetDefaultObject<ABuildablePiece>();
				if (CDO && PM->CanPlacePieceType(CDO->GetPieceType()))
				{
					break; // Found an available piece
				}
			}
			NextIndex = (NextIndex + 1) % AvailablePieceTypes.Num();
			Attempts++;
		}
		// If no available piece found after full loop, stay at current
		if (Attempts >= AvailablePieceTypes.Num()) return;
	}

	// Selecting Foundation resets the build cycle
	if (PM && AvailablePieceTypes[NextIndex])
	{
		ABuildablePiece* NextCDO = AvailablePieceTypes[NextIndex]->GetDefaultObject<ABuildablePiece>();
		if (NextCDO && NextCDO->GetPieceType() == EPieceType::Foundation)
		{
			PM->ResetBuildCycle();
		}
	}

	CurrentPieceTypeIndex = NextIndex;
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

	// Show wall stud info
	if (CurrentPreviewPiece && CurrentPreviewPiece->GetPieceType() == EPieceType::WallStud)
	{
		if (RectangleBuilder && RectangleBuilder->HasStudSuggestions())
		{
			FStudSuggestion NextStud = RectangleBuilder->GetNextStudSuggestion();
			UE_LOG(LogTemp, Log, TEXT("Wall Stud selected: %d/%d studs to place (16\" OC), height=%.1f\""),
				RectangleBuilder->GetPlacedStudCount(),
				RectangleBuilder->GetStudSuggestions().Num(),
				NextStud.StudHeightCm / 2.54f);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Wall Stud selected: No stud layout available (place all bottom plates first)"));
		}
	}

	// Show top plate info (covers both first plates 1-4 and double plates 5-8)
	if (CurrentPreviewPiece && CurrentPreviewPiece->GetPieceType() == EPieceType::TopPlate)
	{
		if (RectangleBuilder && RectangleBuilder->HasTopPlateSuggestions())
		{
			FTopPlateSuggestion NextTP = RectangleBuilder->GetNextTopPlateSuggestion();
			const TCHAR* Layer = NextTP.bIsDoubleTopPlate ? TEXT("double") : TEXT("first");
			UE_LOG(LogTemp, Log, TEXT("Top Plate selected: %d/%d to place (%s layer), %.1fcm length"),
				RectangleBuilder->GetPlacedTopPlateCount(),
				RectangleBuilder->GetTopPlateSuggestions().Num(),
				Layer, NextTP.LengthCm);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Top Plate selected: No layout available (place all wall studs first)"));
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

void UBuildingComponent::SetPieceTypeIndex(int32 Index)
{
	if (!bIsInBuildMode) return;
	if (Index < 0 || Index >= AvailablePieceTypes.Num()) return;
	if (Index == CurrentPieceTypeIndex) return;

	AConstructionPhaseManager* PM = AConstructionPhaseManager::Instance;

	// Determine the target piece type
	EPieceType TargetType = EPieceType::None;
	if (AvailablePieceTypes[Index])
	{
		ABuildablePiece* CDO = AvailablePieceTypes[Index]->GetDefaultObject<ABuildablePiece>();
		if (CDO) TargetType = CDO->GetPieceType();
	}

	// Selecting Foundation resets the build cycle — everything locks again
	if (TargetType == EPieceType::Foundation && PM)
	{
		PM->ResetBuildCycle();
	}

	// Phase gating: check if the target piece type is available (AFTER potential reset)
	if (PM && TargetType != EPieceType::None && !PM->CanPlacePieceType(TargetType))
	{
		// Blocked — show prerequisite message on screen
		FString Msg = PM->GetPrerequisiteMessage(TargetType);
		if (GEngine && !Msg.IsEmpty())
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
				FString::Printf(TEXT("Locked: %s"), *Msg));
		}
		UE_LOG(LogTemp, Warning, TEXT("BuildingComponent: Piece %s locked — %s"),
			*UEnum::GetDisplayValueAsText(TargetType).ToString(), *Msg);
		return;
	}

	CurrentPieceTypeIndex = Index;
	SpawnPreviewPiece();

	UE_LOG(LogTemp, Log, TEXT("BuildingComponent: Radial select -> %s (index %d)"),
		*GetCurrentPieceName(), Index);
}

TArray<FString> UBuildingComponent::GetPieceTypeNames() const
{
	TArray<FString> Names;
	for (const TSubclassOf<ABuildablePiece>& PieceClass : AvailablePieceTypes)
	{
		if (!PieceClass)
		{
			Names.Add(TEXT("?"));
			continue;
		}
		// Use Blueprint class name: strip BP_ prefix and _C suffix
		FString Name = PieceClass->GetName();
		Name.RemoveFromStart(TEXT("BP_"));
		Name.RemoveFromEnd(TEXT("_C"));

		// Insert spaces before uppercase letters (PascalCase -> words)
		FString Nice;
		for (int32 i = 0; i < Name.Len(); i++)
		{
			if (i > 0 && FChar::IsUpper(Name[i]) && !FChar::IsUpper(Name[i - 1]))
			{
				Nice += TEXT(" ");
			}
			Nice += Name[i];
		}
		Names.Add(Nice);
	}
	return Names;
}

TArray<FPieceTypeInfo> UBuildingComponent::GetPieceTypeInfos() const
{
	// Build infos from AvailablePieceTypes only (source of truth for spawnable
	// pieces).  Overlay editor-configured PieceTypeInfos matched by PieceType
	// (not by index), so the two arrays don't need to be in the same order.
	TArray<FString> Names = GetPieceTypeNames();
	TArray<FPieceTypeInfo> Infos;

	AConstructionPhaseManager* PM = AConstructionPhaseManager::Instance;

	// Diagnostic: log cycle piece counts when building menu infos
	if (PM)
	{
		UE_LOG(LogTemp, Log, TEXT("GetPieceTypeInfos: CycleCounts — Found=%d Rim=%d Joist=%d Ply=%d Plate=%d Stud=%d Top=%d DblTop=%d Ridge=%d"),
			PM->GetCyclePieceCount(EPieceType::Foundation),
			PM->GetCyclePieceCount(EPieceType::RimBoard),
			PM->GetCyclePieceCount(EPieceType::FloorJoist),
			PM->GetCyclePieceCount(EPieceType::Plywood),
			PM->GetCyclePieceCount(EPieceType::WallPlate),
			PM->GetCyclePieceCount(EPieceType::WallStud),
			PM->GetCyclePieceCount(EPieceType::TopPlate),
			PM->GetCyclePieceCount(EPieceType::DoubleTopPlate),
			PM->GetCyclePieceCount(EPieceType::RidgePost));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GetPieceTypeInfos: PhaseManager Instance is NULL — all pieces will show as available"));
	}

	for (int32 i = 0; i < AvailablePieceTypes.Num(); i++)
	{
		FPieceTypeInfo Info;
		Info.DisplayName = Names.IsValidIndex(i) ? Names[i] : TEXT("?");
		Info.bAvailable = true;

		if (AvailablePieceTypes.IsValidIndex(i) && AvailablePieceTypes[i])
		{
			ABuildablePiece* CDO = AvailablePieceTypes[i]->GetDefaultObject<ABuildablePiece>();
			if (CDO)
			{
				Info.PieceType = CDO->GetPieceType();

				// Default subtitles per piece type
				switch (Info.PieceType)
				{
				case EPieceType::Foundation:  Info.Subtitle = TEXT("12x12"); break;
				case EPieceType::RimBoard:    Info.Subtitle = TEXT("2x6 8ft"); break;
				case EPieceType::FloorJoist:  Info.Subtitle = TEXT("16in OC"); break;
				case EPieceType::Plywood:     Info.Subtitle = TEXT("4x8"); break;
				case EPieceType::WallPlate:   Info.Subtitle = TEXT("2x4"); break;
				case EPieceType::WallStud:    Info.Subtitle = TEXT("92-5/8\""); break;
				case EPieceType::CornerPost:      Info.Subtitle = TEXT("4-Stud"); break;
				case EPieceType::DoorFrame:       Info.Subtitle = TEXT("36\""); break;
				case EPieceType::TopPlate:        Info.Subtitle = TEXT("2x4"); break;
				case EPieceType::RidgePost:       Info.Subtitle = TEXT("3-2x6"); break;
				case EPieceType::RidgeBoard:      Info.Subtitle = TEXT("2x8"); break;
				case EPieceType::Rafter:          Info.Subtitle = TEXT("2x6"); break;
				case EPieceType::FasciaBoard:     Info.Subtitle = TEXT("1x6"); break;
				default: break;
				}
			}

			// Find matching editor info by PieceType (decoupled from index order)
			for (const FPieceTypeInfo& EditorInfo : PieceTypeInfos)
			{
				if (EditorInfo.PieceType == Info.PieceType)
				{
					if (!EditorInfo.DisplayName.IsEmpty()) Info.DisplayName = EditorInfo.DisplayName;
					if (!EditorInfo.Subtitle.IsEmpty()) Info.Subtitle = EditorInfo.Subtitle;
					if (!EditorInfo.Icon.IsNull()) Info.Icon = EditorInfo.Icon;
					// Don't override bAvailable from editor — phase gating takes priority
					break;
				}
			}

			// --- Phase gating: set bAvailable from ConstructionPhaseManager ---
			if (PM)
			{
				Info.bAvailable = PM->CanPlacePieceType(Info.PieceType);

				// --- Progress indicators: append placed/total to subtitle (cycle counts) ---
				int32 Placed = PM->GetCyclePieceCount(Info.PieceType);
				int32 Total = 0;

				// Use RectangleBuilder totals for suggestion-driven pieces
				if (RectangleBuilder)
				{
					switch (Info.PieceType)
					{
					case EPieceType::RimBoard:
						Total = 4; // Always 4 per rectangle
						break;
					case EPieceType::FloorJoist:
						if (RectangleBuilder->GetJoistSuggestions().Num() > 0)
							Total = RectangleBuilder->GetJoistSuggestions().Num();
						break;
					case EPieceType::WallPlate:
						if (RectangleBuilder->GetPlateSuggestions().Num() > 0)
							Total = RectangleBuilder->GetPlateSuggestions().Num();
						break;
					case EPieceType::WallStud:
						if (RectangleBuilder->GetStudSuggestions().Num() > 0)
							Total = RectangleBuilder->GetStudSuggestions().Num();
						break;
					case EPieceType::TopPlate:
						if (RectangleBuilder->GetTopPlateSuggestions().Num() > 0)
							Total = RectangleBuilder->GetTopPlateSuggestions().Num();
						break;
					case EPieceType::RidgePost:
						if (RectangleBuilder->GetRidgePostSuggestions().Num() > 0)
							Total = RectangleBuilder->GetRidgePostSuggestions().Num();
						break;
					default:
						break;
					}
				}

				// Append progress to subtitle (e.g. "2x6 8ft  2/4")
				if (Total > 0)
				{
					Info.Subtitle = FString::Printf(TEXT("%s  %d/%d"), *Info.Subtitle, Placed, Total);
				}
				else if (Placed > 0)
				{
					Info.Subtitle = FString::Printf(TEXT("%s  x%d"), *Info.Subtitle, Placed);
				}
			}
		}

		Infos.Add(Info);
	}

	return Infos;
}
