// Born To Shine - Construction Phase Manager

#include "ConstructionPhaseManager.h"
#include "BuildablePiece.h"

AConstructionPhaseManager* AConstructionPhaseManager::Instance = nullptr;

AConstructionPhaseManager::AConstructionPhaseManager()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentPhase = EConstructionPhase::Foundation;
	bAutoAdvancePhases = false; // Disabled - using free build with prerequisites instead
	Instance = this;
}

void AConstructionPhaseManager::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("ConstructionPhaseManager: Starting in Foundation phase"));
}

bool AConstructionPhaseManager::CanPlacePieceType(EPieceType PieceType) const
{
	switch (PieceType)
	{
	case EPieceType::Foundation:
		return true; // Always available

	case EPieceType::RimBoard:
		return GetCyclePieceCount(EPieceType::Foundation) >= 4;

	case EPieceType::FloorJoist:
		return GetCyclePieceCount(EPieceType::RimBoard) >= 4;

	case EPieceType::Plywood:
		return GetCyclePieceCount(EPieceType::FloorJoist) >= 5;

	case EPieceType::WallPlate: // Bottom Plate
		return GetCyclePieceCount(EPieceType::Plywood) >= 2;

	case EPieceType::CornerPost:
		return GetCyclePieceCount(EPieceType::WallPlate) >= 4;

	case EPieceType::WallStud:
		return GetCyclePieceCount(EPieceType::CornerPost) >= 4;

	case EPieceType::TopPlate:
		return GetCyclePieceCount(EPieceType::WallStud) >= 20;

	case EPieceType::DoubleTopPlate:
		return GetCyclePieceCount(EPieceType::TopPlate) >= 4;

	case EPieceType::DoorFrame:
		return GetCyclePieceCount(EPieceType::DoubleTopPlate) >= 4;

	case EPieceType::WindowFrame:
		return GetCyclePieceCount(EPieceType::DoubleTopPlate) >= 4; // Unlocks with door frame

	case EPieceType::Header:
		return GetCyclePieceCount(EPieceType::DoubleTopPlate) >= 4; // Unlocks with door frame

	// Everything below unlocks after door frame is placed
	case EPieceType::RidgePost:
	case EPieceType::RidgeBoard:
	case EPieceType::Rafter:
	case EPieceType::FasciaBoard:
	case EPieceType::WallSheathing:
	case EPieceType::RoofSheathing:
		return GetCyclePieceCount(EPieceType::DoorFrame) >= 1;

	default:
		return true;
	}
}

FString AConstructionPhaseManager::GetPrerequisiteMessage(EPieceType PieceType) const
{
	if (CanPlacePieceType(PieceType)) return FString();

	switch (PieceType)
	{
	case EPieceType::RimBoard:
		return FString::Printf(TEXT("Place 4 foundations first (%d/4)"),
			GetCyclePieceCount(EPieceType::Foundation));

	case EPieceType::FloorJoist:
		return FString::Printf(TEXT("Place 4 rim boards first (%d/4)"),
			GetCyclePieceCount(EPieceType::RimBoard));

	case EPieceType::Plywood:
		return FString::Printf(TEXT("Place 5 floor joists first (%d/5)"),
			GetCyclePieceCount(EPieceType::FloorJoist));

	case EPieceType::WallPlate:
		return FString::Printf(TEXT("Place 2 plywood sheets first (%d/2)"),
			GetCyclePieceCount(EPieceType::Plywood));

	case EPieceType::CornerPost:
		return FString::Printf(TEXT("Place 4 bottom plates first (%d/4)"),
			GetCyclePieceCount(EPieceType::WallPlate));

	case EPieceType::WallStud:
		return FString::Printf(TEXT("Place 4 corner posts first (%d/4)"),
			GetCyclePieceCount(EPieceType::CornerPost));

	case EPieceType::TopPlate:
		return FString::Printf(TEXT("Place 20 wall studs first (%d/20)"),
			GetCyclePieceCount(EPieceType::WallStud));

	case EPieceType::DoubleTopPlate:
		return FString::Printf(TEXT("Place 4 top plates first (%d/4)"),
			GetCyclePieceCount(EPieceType::TopPlate));

	case EPieceType::DoorFrame:
	case EPieceType::WindowFrame:
	case EPieceType::Header:
		return FString::Printf(TEXT("Place 4 double top plates first (%d/4)"),
			GetCyclePieceCount(EPieceType::DoubleTopPlate));

	case EPieceType::RidgePost:
	case EPieceType::RidgeBoard:
	case EPieceType::Rafter:
	case EPieceType::FasciaBoard:
	case EPieceType::WallSheathing:
	case EPieceType::RoofSheathing:
		return TEXT("Place a door frame first");

	default:
		return TEXT("Prerequisites not met");
	}
}

int32 AConstructionPhaseManager::GetPieceCount(EPieceType PieceType) const
{
	if (PlacedPieces.Contains(PieceType))
	{
		return PlacedPieces[PieceType].Num();
	}
	return 0;
}

int32 AConstructionPhaseManager::GetCyclePieceCount(EPieceType PieceType) const
{
	if (const int32* Count = CyclePieceCounts.Find(PieceType))
	{
		return *Count;
	}
	return 0;
}

void AConstructionPhaseManager::ResetBuildCycle()
{
	CyclePieceCounts.Empty();
	UE_LOG(LogTemp, Log, TEXT("ConstructionPhaseManager: Build cycle RESET — all piece types locked until prerequisites met again"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
			TEXT("New section — build cycle reset"));
	}
}

void AConstructionPhaseManager::IncrementCyclePieceCount(EPieceType PieceType)
{
	if (!CyclePieceCounts.Contains(PieceType))
	{
		CyclePieceCounts.Add(PieceType, 0);
	}
	CyclePieceCounts[PieceType]++;
	UE_LOG(LogTemp, Log, TEXT("Cycle count incremented (skip): type=%s (Cycle: %d)"),
		*UEnum::GetValueAsString(PieceType),
		CyclePieceCounts[PieceType]);
}

bool AConstructionPhaseManager::CanAdvancePhase() const
{
	switch (CurrentPhase)
	{
		case EConstructionPhase::Foundation:
			return IsFoundationComplete();

		case EConstructionPhase::FloorFrame:
			return IsFloorFrameComplete();

		case EConstructionPhase::FloorSheathing:
			// Check if floor is fully sheathed
			// TODO: Implement plywood coverage check
			return true;

		default:
			return false;
	}
}

bool AConstructionPhaseManager::AdvanceToNextPhase()
{
	if (!CanAdvancePhase())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot advance phase - requirements not met"));
		return false;
	}

	switch (CurrentPhase)
	{
		case EConstructionPhase::Foundation:
			CurrentPhase = EConstructionPhase::FloorFrame;
			UE_LOG(LogTemp, Log, TEXT("Advanced to Floor Frame phase"));
			return true;

		case EConstructionPhase::FloorFrame:
			CurrentPhase = EConstructionPhase::FloorSheathing;
			UE_LOG(LogTemp, Log, TEXT("Advanced to Floor Sheathing phase"));
			return true;

		case EConstructionPhase::FloorSheathing:
			CurrentPhase = EConstructionPhase::WallFrame;
			UE_LOG(LogTemp, Log, TEXT("Advanced to Wall Frame phase"));
			return true;

		case EConstructionPhase::WallFrame:
			CurrentPhase = EConstructionPhase::WallSheathing;
			UE_LOG(LogTemp, Log, TEXT("Advanced to Wall Sheathing phase"));
			return true;

		case EConstructionPhase::WallSheathing:
			CurrentPhase = EConstructionPhase::RoofFrame;
			UE_LOG(LogTemp, Log, TEXT("Advanced to Roof Frame phase"));
			return true;

		case EConstructionPhase::RoofFrame:
			CurrentPhase = EConstructionPhase::RoofSheathing;
			UE_LOG(LogTemp, Log, TEXT("Advanced to Roof Sheathing phase"));
			return true;

		default:
			return false;
	}
}

void AConstructionPhaseManager::RegisterPlacedPiece(ABuildablePiece* Piece)
{
	if (!Piece) return;

	EPieceType PieceType = Piece->GetPieceType();

	if (!PlacedPieces.Contains(PieceType))
	{
		PlacedPieces.Add(PieceType, TArray<ABuildablePiece*>());
	}

	// Idempotent: skip if already registered
	if (PlacedPieces[PieceType].Contains(Piece))
	{
		return;
	}

	PlacedPieces[PieceType].Add(Piece);

	// Increment cycle count (used for phase gating)
	if (!CyclePieceCounts.Contains(PieceType))
	{
		CyclePieceCounts.Add(PieceType, 0);
	}
	CyclePieceCounts[PieceType]++;

	UE_LOG(LogTemp, Log, TEXT("Registered piece: %s type=%s (Cycle: %d, Total: %d)"),
		*Piece->GetName(),
		*UEnum::GetValueAsString(PieceType),
		CyclePieceCounts[PieceType],
		PlacedPieces[PieceType].Num());

	// Auto-advance phase if enabled and requirements are met
	if (bAutoAdvancePhases && CanAdvancePhase())
	{
		EConstructionPhase OldPhase = CurrentPhase;
		if (AdvanceToNextPhase())
		{
			// Show on-screen message about phase advancement
			if (GEngine)
			{
				FString PhaseName = GetCurrentPhaseName();
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
					FString::Printf(TEXT("✓ Phase Advanced: %s"), *PhaseName));
			}
			UE_LOG(LogTemp, Log, TEXT("Auto-advanced from %s to %s"),
				*GetPhaseName(OldPhase), *GetCurrentPhaseName());
		}
	}
}

void AConstructionPhaseManager::UnregisterPiece(ABuildablePiece* Piece)
{
	if (!Piece) return;

	EPieceType PieceType = Piece->GetPieceType();

	if (PlacedPieces.Contains(PieceType))
	{
		PlacedPieces[PieceType].Remove(Piece);
		UE_LOG(LogTemp, Log, TEXT("Unregistered piece: %s"), *UEnum::GetValueAsString(PieceType));
	}
}

TArray<ABuildablePiece*> AConstructionPhaseManager::GetPiecesOfType(EPieceType PieceType) const
{
	if (PlacedPieces.Contains(PieceType))
	{
		return PlacedPieces[PieceType];
	}
	return TArray<ABuildablePiece*>();
}

TArray<ABuildablePiece*> AConstructionPhaseManager::GetNearbyPieces(const FVector& Location, float Radius) const
{
	TArray<ABuildablePiece*> NearbyPieces;
	float RadiusSquared = Radius * Radius;

	// Check all placed pieces
	for (const auto& TypePair : PlacedPieces)
	{
		for (ABuildablePiece* Piece : TypePair.Value)
		{
			if (Piece && FVector::DistSquared(Piece->GetActorLocation(), Location) <= RadiusSquared)
			{
				NearbyPieces.Add(Piece);
			}
		}
	}

	return NearbyPieces;
}

bool AConstructionPhaseManager::CheckPrerequisites(EPieceType PieceType, const FVector& ProposedLocation) const
{
	// Free building mode - no prerequisites enforced
	// Socket snapping naturally handles logical building order
	// If there's no socket nearby, piece won't snap but can still be placed
	return true;
}

FString AConstructionPhaseManager::GetPhaseRequirements() const
{
	switch (CurrentPhase)
	{
		case EConstructionPhase::Foundation:
			return TEXT("Place foundation blocks at 8ft intervals to establish building footprint");

		case EConstructionPhase::FloorFrame:
			return TEXT("Install rim boards on foundation corners, then add floor joists");

		case EConstructionPhase::FloorSheathing:
			return TEXT("Install plywood sheathing starting from corners");

		case EConstructionPhase::WallFrame:
			return TEXT("Frame walls with studs and plates");

		default:
			return TEXT("Unknown phase");
	}
}

bool AConstructionPhaseManager::IsFoundationComplete() const
{
	// Foundation is complete when we have at least 4 foundation blocks
	// forming a rectangular base
	TArray<ABuildablePiece*> Foundations = GetPiecesOfType(EPieceType::Foundation);

	// Minimum 4 corners required
	if (Foundations.Num() < 4)
	{
		return false;
	}

	// TODO: Add more sophisticated checking:
	// - Verify they form a rectangle
	// - Check spacing is consistent
	// - Ensure proper grid alignment

	return true;
}

bool AConstructionPhaseManager::IsFloorFrameComplete() const
{
	// Floor frame is complete when:
	// 1. Rim boards form a closed perimeter
	// 2. Joists are installed at proper spacing

	if (!DoRimBoardsFormPerimeter())
	{
		return false;
	}

	TArray<ABuildablePiece*> Joists = GetPiecesOfType(EPieceType::FloorJoist);

	// Need at least 2 joists for a minimal floor
	if (Joists.Num() < 2)
	{
		return false;
	}

	// TODO: Add more sophisticated checking:
	// - Verify joist spacing (16" or 24" OC)
	// - Check all joists are properly connected
	// - Ensure no gaps in coverage

	return true;
}

bool AConstructionPhaseManager::DoRimBoardsFormPerimeter() const
{
	TArray<ABuildablePiece*> RimBoards = GetPiecesOfType(EPieceType::RimBoard);

	// Need at least 4 rim boards to form a rectangle
	if (RimBoards.Num() < 4)
	{
		return false;
	}

	// TODO: Implement perimeter detection:
	// - Check each rim board connects to exactly 2 others
	// - Verify they form a closed loop
	// - Ensure corners are at 90 degrees

	return true;
}

FString AConstructionPhaseManager::GetCurrentPhaseName() const
{
	return GetPhaseName(CurrentPhase);
}

FString AConstructionPhaseManager::GetPhaseName(EConstructionPhase Phase) const
{
	switch (Phase)
	{
		case EConstructionPhase::Foundation:
			return TEXT("Foundation Phase");
		case EConstructionPhase::FloorFrame:
			return TEXT("Floor Frame Phase");
		case EConstructionPhase::FloorSheathing:
			return TEXT("Floor Sheathing Phase");
		case EConstructionPhase::WallFrame:
			return TEXT("Wall Frame Phase");
		case EConstructionPhase::WallSheathing:
			return TEXT("Wall Sheathing Phase");
		case EConstructionPhase::RoofFrame:
			return TEXT("Roof Frame Phase");
		case EConstructionPhase::RoofSheathing:
			return TEXT("Roof Sheathing Phase");
		default:
			return TEXT("Unknown Phase");
	}
}
