// Born To Shine - Construction Phase Manager

#include "ConstructionPhaseManager.h"
#include "BuildablePiece.h"

AConstructionPhaseManager* AConstructionPhaseManager::Instance = nullptr;

AConstructionPhaseManager::AConstructionPhaseManager()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentPhase = EConstructionPhase::Foundation;
	Instance = this;
}

void AConstructionPhaseManager::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("ConstructionPhaseManager: Starting in Foundation phase"));
}

bool AConstructionPhaseManager::CanPlacePieceType(EPieceType PieceType) const
{
	switch (CurrentPhase)
	{
		case EConstructionPhase::Foundation:
			// Only foundation blocks in foundation phase
			return PieceType == EPieceType::Foundation;

		case EConstructionPhase::FloorFrame:
			// Can place rim boards and joists
			return PieceType == EPieceType::RimBoard || PieceType == EPieceType::FloorJoist;

		case EConstructionPhase::FloorSheathing:
			// Can place plywood
			return PieceType == EPieceType::Plywood;

		case EConstructionPhase::WallFrame:
			// Can place wall studs, plates, headers
			return PieceType == EPieceType::WallStud ||
				   PieceType == EPieceType::WallPlate ||
				   PieceType == EPieceType::Header;

		case EConstructionPhase::WallSheathing:
			// Can place wall sheathing
			return PieceType == EPieceType::Plywood;

		default:
			return false;
	}
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
	switch (PieceType)
	{
		case EPieceType::Foundation:
			// Foundation can always be placed (first piece)
			return true;

		case EPieceType::RimBoard:
			// Rim boards require foundation blocks nearby
			{
				TArray<ABuildablePiece*> FoundationBlocks = GetPiecesOfType(EPieceType::Foundation);
				if (FoundationBlocks.Num() == 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("Cannot place rim board - no foundation blocks exist"));
					return false;
				}

				// Check if there's a foundation block within reasonable distance (500cm = 5m)
				TArray<ABuildablePiece*> NearbyFoundations = GetNearbyPieces(ProposedLocation, 500.0f);
				bool bHasNearbyFoundation = false;
				for (ABuildablePiece* Piece : NearbyFoundations)
				{
					if (Piece->GetPieceType() == EPieceType::Foundation)
					{
						bHasNearbyFoundation = true;
						break;
					}
				}

				if (!bHasNearbyFoundation)
				{
					UE_LOG(LogTemp, Warning, TEXT("Cannot place rim board - no foundation blocks nearby"));
					return false;
				}
			}
			return true;

		case EPieceType::FloorJoist:
			// Joists require rim boards
			{
				TArray<ABuildablePiece*> RimBoards = GetPiecesOfType(EPieceType::RimBoard);
				if (RimBoards.Num() < 2)
				{
					UE_LOG(LogTemp, Warning, TEXT("Cannot place joist - need at least 2 rim boards"));
					return false;
				}
			}
			return true;

		case EPieceType::Plywood:
			// Plywood requires rim boards and joists
			{
				TArray<ABuildablePiece*> RimBoards = GetPiecesOfType(EPieceType::RimBoard);
				TArray<ABuildablePiece*> Joists = GetPiecesOfType(EPieceType::FloorJoist);

				if (RimBoards.Num() < 4)
				{
					UE_LOG(LogTemp, Warning, TEXT("Cannot place plywood - need complete rim board perimeter (4+ pieces)"));
					return false;
				}

				if (Joists.Num() < 1)
				{
					UE_LOG(LogTemp, Warning, TEXT("Cannot place plywood - need joists for support"));
					return false;
				}
			}
			return true;

		default:
			return true;
	}
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
