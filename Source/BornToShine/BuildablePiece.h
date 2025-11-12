// Born To Shine - Base class for all buildable construction pieces

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConstructionTypes.h"
#include "BuildablePiece.generated.h"

/**
 * Base class for all construction pieces (Foundation, Rim Boards, Joists, Plywood, etc.)
 * Handles sockets, snapping, placement validation, and visual feedback
 */
UCLASS()
class BORNTOSHINE_API ABuildablePiece : public AActor
{
	GENERATED_BODY()

public:
	ABuildablePiece();

	virtual void Tick(float DeltaTime) override;

	// Get the type of this piece
	UFUNCTION(BlueprintCallable, Category = "Construction")
	EPieceType GetPieceType() const { return PieceType; }

	// Get the current state of this piece
	UFUNCTION(BlueprintCallable, Category = "Construction")
	EPieceState GetPieceState() const { return PieceState; }

	// Get all sockets on this piece
	UFUNCTION(BlueprintCallable, Category = "Construction")
	TArray<FConstructionSocket> GetAllSockets() const { return Sockets; }

	// Get a specific socket by name
	UFUNCTION(BlueprintCallable, Category = "Construction")
	FConstructionSocket* GetSocketByName(FName SocketName);

	// Set piece to preview mode (ghost)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetPreviewMode(bool bIsPreview);

	// Attempt to place the piece (snaps to valid sockets)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool TryPlace();

	// Nail the piece in place (locks it)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void NailInPlace();

	// Remove/demolish the piece
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void Remove();

	// Update piece position for preview (handles snapping)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void UpdatePreviewPosition(const FVector& NewLocation, const FRotator& NewRotation);

	// Rotate piece in different directions
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void RotateLeft();

	UFUNCTION(BlueprintCallable, Category = "Construction")
	void RotateRight();

	UFUNCTION(BlueprintCallable, Category = "Construction")
	void RotateFront();

	UFUNCTION(BlueprintCallable, Category = "Construction")
	void RotateBack();

	UFUNCTION(BlueprintCallable, Category = "Construction")
	void RotateRoll(float Angle);

	// Scale piece with mouse wheel
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void ScalePiece(float ScaleDelta);

	// Check if placement is valid
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool IsPlacementValid() const;

	// Occupy a socket (when another piece connects to it)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void OccupySocket(FName SocketName, ABuildablePiece* ConnectingPiece);

	// Free a socket
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void FreeSocket(FName SocketName);

protected:
	virtual void BeginPlay() override;

	// Initialize sockets for this piece (override in child classes)
	virtual void InitializeSockets();

	// Update visual feedback (color, transparency)
	virtual void UpdateVisualFeedback();

	// Find best snap point near current location
	bool FindSnapPoint(FVector& OutSnapLocation, FRotator& OutSnapRotation);

	// Check if piece is properly supported (gravity check)
	bool IsSupported() const;

	// Type of construction piece
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	EPieceType PieceType;

	// Current state of the piece
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	EPieceState PieceState;

	// All sockets on this piece
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	TArray<FConstructionSocket> Sockets;

	// Static mesh component for visual representation
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* MeshComponent;

	// Material instance for dynamic color changes
	UPROPERTY()
	class UMaterialInstanceDynamic* DynamicMaterial;

	// Is this piece currently snapped to a valid socket?
	UPROPERTY(BlueprintReadOnly, Category = "Construction")
	bool bIsSnapped;

	// Target piece this is snapped to
	UPROPERTY(BlueprintReadOnly, Category = "Construction")
	ABuildablePiece* SnappedToPiece;

	// Target socket name this is snapped to
	UPROPERTY(BlueprintReadOnly, Category = "Construction")
	FName SnappedToSocketName;

	// Snap search radius
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	float SnapSearchRadius;

	// Current scale of the piece
	UPROPERTY(BlueprintReadOnly, Category = "Construction")
	FVector CurrentScale;

	// Minimum and maximum scale
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	float MinScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	float MaxScale;

	// Material colors for different states
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	FLinearColor ValidPlacementColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	FLinearColor InvalidPlacementColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	FLinearColor PlacedColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	FLinearColor NailedColor;

	// Rotation step in degrees
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	float RotationStep;
};
