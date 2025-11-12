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
	// Free building mode - allow any piece type
	// Logical order is enforced by CheckPrerequisites() instead
	return true;
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

	PlacedPieces[PieceType].Add(Piece);

	UE_LOG(LogTemp, Log, TEXT("Registered piece: %s (Total of this type: %d)"),
		*UEnum::GetValueAsString(PieceType),
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
