// Born To Shine - Construction Phase Manager

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConstructionTypes.h"
#include "ConstructionPhaseManager.generated.h"

/**
 * Manages construction phases and enforces build order
 * Tracks what pieces have been placed and determines when phases can advance
 */
UCLASS()
class BORNTOSHINE_API AConstructionPhaseManager : public AActor
{
	GENERATED_BODY()

public:
	AConstructionPhaseManager();

	// Singleton instance
	static AConstructionPhaseManager* Instance;

	// Get current construction phase
	UFUNCTION(BlueprintCallable, Category = "Construction")
	EConstructionPhase GetCurrentPhase() const { return CurrentPhase; }

	// Check if a piece type can be placed in current phase
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool CanPlacePieceType(EPieceType PieceType) const;

	// Check if we can advance to the next phase
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool CanAdvancePhase() const;

	// Advance to the next construction phase
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool AdvanceToNextPhase();

	// Register a piece as placed
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void RegisterPlacedPiece(class ABuildablePiece* Piece);

	// Unregister a piece (if removed)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void UnregisterPiece(class ABuildablePiece* Piece);

	// Get all pieces of a specific type
	UFUNCTION(BlueprintCallable, Category = "Construction")
	TArray<class ABuildablePiece*> GetPiecesOfType(EPieceType PieceType) const;

	// Get all pieces in a radius
	UFUNCTION(BlueprintCallable, Category = "Construction")
	TArray<class ABuildablePiece*> GetNearbyPieces(const FVector& Location, float Radius) const;

	// Check prerequisites for a piece type
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool CheckPrerequisites(EPieceType PieceType, const FVector& ProposedLocation) const;

	// Get phase requirements description
	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetPhaseRequirements() const;

	// Get current phase name
	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetCurrentPhaseName() const;

	// Get a user-friendly message explaining what's needed before this piece type can be placed.
	// Returns empty string if the piece is already available.
	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetPrerequisiteMessage(EPieceType PieceType) const;

	// Get the number of placed pieces of a given type
	UFUNCTION(BlueprintCallable, Category = "Construction")
	int32 GetPieceCount(EPieceType PieceType) const;

	// Get piece count for the CURRENT build cycle only (resets when starting new section)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	int32 GetCyclePieceCount(EPieceType PieceType) const;

	// Reset the build cycle — clears cycle counts so gating restarts from Foundation.
	// Called when the player selects Foundation to start a new section.
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void ResetBuildCycle();

	// Increment cycle count for a piece type without registering a placed piece.
	// Used when a piece is SKIPPED (overlap with existing) — the slot was handled
	// even though no new piece was spawned, so it should count toward gating.
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void IncrementCyclePieceCount(EPieceType PieceType);

	// Enable/disable automatic phase advancement (default: true)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	bool bAutoAdvancePhases;

protected:
	virtual void BeginPlay() override;

	// Current construction phase
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	EConstructionPhase CurrentPhase;

	// All placed pieces tracked by type (not replicated - internal tracking only)
	TMap<EPieceType, TArray<class ABuildablePiece*>> PlacedPieces;

	// Per-cycle piece counts: resets each time the player starts a new section (selects Foundation).
	// CanPlacePieceType uses these for gating, not the global PlacedPieces.
	TMap<EPieceType, int32> CyclePieceCounts;

	// Phase advancement requirements
	struct FPhaseRequirement
	{
		EPieceType RequiredPieceType;
		int32 MinimumCount;
		bool bMustFormClosedPerimeter;
	};

	// Check if foundation is complete (for phase advancement)
	bool IsFoundationComplete() const;

	// Check if floor frame is complete
	bool IsFloorFrameComplete() const;

	// Check if rim boards form a closed perimeter
	bool DoRimBoardsFormPerimeter() const;

	// Helper to get phase name from phase enum
	FString GetPhaseName(EConstructionPhase Phase) const;
};
