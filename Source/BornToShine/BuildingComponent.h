// Born To Shine - Building System Component

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ConstructionTypes.h"
#include "BuildingComponent.generated.h"

/**
 * Component that handles all building system functionality
 * Attach to any character to give them building abilities
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BORNTOSHINE_API UBuildingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Building mode control
	UFUNCTION(BlueprintCallable, Category = "Building")
	void ToggleBuildMode();

	UFUNCTION(BlueprintCallable, Category = "Building")
	bool IsInBuildMode() const { return bIsInBuildMode; }

	// Piece management
	UFUNCTION(BlueprintCallable, Category = "Building")
	void CyclePieceType();

	UFUNCTION(BlueprintCallable, Category = "Building")
	void PlaceCurrentPiece();

	UFUNCTION(BlueprintCallable, Category = "Building")
	void NailLastPlacedPiece();

	UFUNCTION(BlueprintCallable, Category = "Building")
	void RemoveLastPlacedPiece();

	// Piece manipulation
	UFUNCTION(BlueprintCallable, Category = "Building")
	void RotatePreviewLeft();

	UFUNCTION(BlueprintCallable, Category = "Building")
	void RotatePreviewRight();

	UFUNCTION(BlueprintCallable, Category = "Building")
	void RotatePreviewPitch(float Value);

	UFUNCTION(BlueprintCallable, Category = "Building")
	void RotatePreviewRoll(float Value);

	UFUNCTION(BlueprintCallable, Category = "Building")
	void ScalePreview(float ScaleDelta);

	// Get current piece info
	UFUNCTION(BlueprintCallable, Category = "Building")
	EPieceType GetCurrentPieceType() const;

	UFUNCTION(BlueprintCallable, Category = "Building")
	FString GetCurrentPieceName() const;

	UFUNCTION(BlueprintCallable, Category = "Building")
	int32 GetPlacedPieceCount() const { return PlacedPieces.Num(); }

protected:
	virtual void BeginPlay() override;

	// Spawn/destroy preview piece
	void SpawnPreviewPiece();
	void DestroyPreviewPiece();

	// Update preview piece position based on camera
	void UpdatePreviewPosition();

	// Get placement location from raycast
	bool GetPlacementLocation(FVector& OutLocation, FVector& OutNormal);

	// Get owner's camera
	class UCameraComponent* GetOwnerCamera() const;

	// Building mode state
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	bool bIsInBuildMode;

	// Available piece types to build
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	TArray<TSubclassOf<class ABuildablePiece>> AvailablePieceTypes;

	// Current preview piece
	UPROPERTY()
	class ABuildablePiece* CurrentPreviewPiece;

	// Current piece type index
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	int32 CurrentPieceTypeIndex;

	// All placed pieces (not yet nailed)
	UPROPERTY()
	TArray<class ABuildablePiece*> PlacedPieces;

	// Last placed piece (for nailing/removing)
	UPROPERTY()
	class ABuildablePiece* LastPlacedPiece;

	// Preview piece rotation (preserved across frames)
	FRotator PreviewRotation;

	// Raycast settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	float BuildRaycastDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	float PreviewDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	float SnapSearchRadius;
};
