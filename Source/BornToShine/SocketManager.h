// Born To Shine - Socket Compatibility Manager

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConstructionTypes.h"
#include "SnapRuleTable.h"
#include "SocketManager.generated.h"

/**
 * Manages socket compatibility rules and snap detection
 * This is a singleton that all buildable pieces reference
 */
UCLASS()
class BORNTOSHINE_API ASocketManager : public AActor
{
	GENERATED_BODY()

public:
	ASocketManager();

	// Singleton instance
	static ASocketManager* Instance;

	// Initialize compatibility rules
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void InitializeCompatibilityRules();

	// Check if two socket types are compatible
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool AreSocketsCompatible(EConstructionSocketType SourceSocket, EConstructionSocketType TargetSocket, EConstructionPhase CurrentPhase) const;

	// Get the compatibility rule for a socket type
	UFUNCTION(BlueprintCallable, Category = "Construction")
	FSocketCompatibilityRule GetCompatibilityRule(EConstructionSocketType SocketType) const;

	// Find the best snap point within range
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool FindBestSnapPoint(
		const FConstructionSocket& SourceSocket,
		const FVector& WorldLocation,
		const FRotator& WorldRotation,
		TArray<class ABuildablePiece*> NearbyPieces,
		EConstructionPhase CurrentPhase,
		FVector& OutSnapLocation,
		FRotator& OutSnapRotation,
		ABuildablePiece*& OutTargetPiece,
		FName& OutTargetSocketName
	);

	// Check if a socket position is valid (not floating, aligned, etc.)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool IsPlacementValid(
		const FVector& Location,
		const FRotator& Rotation,
		class ABuildablePiece* Piece,
		EConstructionPhase CurrentPhase
	);

protected:
	virtual void BeginPlay() override;

	// All compatibility rules defined here
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	TArray<FSocketCompatibilityRule> CompatibilityRules;

	// Create default compatibility rules
	void CreateFoundationRules();
	void CreateRimBoardRules();
	void CreateJoistRules();
	void CreatePlywoodRules();
	void CreateBottomPlateRules();
	void CreateWallStudRules();
	void CreateCornerPostRules();
	void CreateDoorFrameRules();
	void CreateTopPlateRules();
	void CreateDoubleTopPlateRules();
	void CreateRidgePostRules();
	void CreateRidgeBoardRules();
	void CreateRafterRules();
	void CreateFasciaBoardRules();

	// Helper functions
	bool CheckSocketAlignment(
		const FVector& SourceLocation,
		const FRotator& SourceRotation,
		const FVector& TargetLocation,
		const FRotator& TargetRotation,
		float MaxAngleDiff
	) const;

	float CalculateSnapScore(
		const FVector& SourceLocation,
		const FVector& TargetLocation,
		const FRotator& SourceRotation,
		const FRotator& TargetRotation
	) const;
};
