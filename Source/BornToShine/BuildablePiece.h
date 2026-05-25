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

	// Get mutable reference to sockets array for direct modification
	TArray<FConstructionSocket>& GetSocketsMutable() { return Sockets; }

	// Get a specific socket by name (C++ only - returns pointer to socket or nullptr)
	FConstructionSocket* GetSocketByName(FName SocketName);

	// Get a specific socket by name (Blueprint-safe version)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool GetSocketByNameSafe(FName SocketName, FConstructionSocket& OutSocket);

	// Set piece to preview mode (ghost)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	virtual void SetPreviewMode(bool bIsPreview);

	// Attempt to place the piece (snaps to valid sockets)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	virtual bool TryPlace();

	// Nail the piece in place (locks it)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void NailInPlace();

	// Remove/demolish the piece
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void Remove();

	// Update piece position for preview (handles snapping)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	virtual void UpdatePreviewPosition(const FVector& NewLocation, const FRotator& NewRotation);

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
	virtual void ScalePiece(float ScaleDelta);

	// Check if placement is valid
	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool IsPlacementValid() const;

	// Occupy a socket (when another piece connects to it)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void OccupySocket(FName SocketName, ABuildablePiece* ConnectingPiece);

	// Free a socket
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void FreeSocket(FName SocketName);

	// Mark piece as snapped (used by suggestion system to indicate valid placement
	// without running the full snap pipeline)
	void MarkSnapped(bool bSnapped) { bIsSnapped = bSnapped; }

	// Highlight / unhighlight for delete-target feedback
	void SetHighlighted(bool bHighlight);
	bool IsHighlighted() const { return bIsHighlighted; }

	// Force the preview ghost to a specific color (used for overlap feedback)
	void SetPreviewColor(const FLinearColor& Color);

	// Get the mesh component (for line trace hit detection)
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	// Should this piece auto-nail when placed?
	bool ShouldAutoNail() const { return bAutoNailOnPlace; }

protected:
	virtual void BeginPlay() override;

	// Initialize sockets for this piece (override in child classes)
	virtual void InitializeSockets();

	// Update visual feedback (color, transparency)
	virtual void UpdateVisualFeedback();

	// ============================================================
	// SNAP PIPELINE: Phase-separated detection -> selection -> application
	// ============================================================

	// Phase 1: Pure detection (const, no state mutation)
	// Returns ALL valid snap candidates for this piece's current position
	TArray<FSnapCandidate> DetectSnapCandidates() const;

	// Phase 2: Selection (const, no state mutation)
	// Picks the best candidate using priority + distance as tiebreaker
	FSnapCandidate SelectBestCandidate(const TArray<FSnapCandidate>& Candidates) const;

	// Phase 3: Apply snap (mutates state: sets position, rotation, handles resize)
	void ApplySnap(const FSnapCandidate& Candidate);

	// Phase 4: Commit placement (called when player confirms: occupies sockets, registers piece)
	void CommitPlacement();

	// Determine socket connection priority (higher = preferred)
	int32 GetSocketConnectionPriority(EConstructionSocketType SocketA, EConstructionSocketType SocketB) const;

	// Get socket type from a target piece by socket name
	EConstructionSocketType GetTargetSocketType(ABuildablePiece* TargetPiece, FName SocketName) const;

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

	// Current snap candidate (result of the latest snap pipeline run)
	UPROPERTY()
	FSnapCandidate CurrentSnapCandidate;

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

	// Final material to use when piece is nailed/permanently placed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Materials")
	class UMaterialInterface* NailedMaterial;

	// Translucent material for preview ghost (green/red translucent)
	// Set in Blueprint for best results (use a translucent material with BaseColor + Opacity params)
	// If not set, falls back to engine default material (opaque colored preview)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Materials")
	class UMaterialInterface* PreviewMaterialBase;

	// Saved original mesh material (restored when piece is placed/nailed)
	UPROPERTY()
	class UMaterialInterface* OriginalMeshMaterial;

	// Should this piece auto-nail on placement? (true for foundation blocks)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	bool bAutoNailOnPlace;

	// Rotation step in degrees
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	float RotationStep;

	// Highlight state for delete-target feedback
	bool bIsHighlighted = false;

	// Cached material before highlight (to restore nailed material)
	UPROPERTY()
	class UMaterialInterface* PreHighlightMaterial;
};
