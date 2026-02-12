// Born To Shine - Rim Board (2x6 lumber for floor frame perimeter)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "SnapRuleTable.h"
#include "RimBoard.generated.h"

/**
 * Rim Board - 2x6 lumber that forms the perimeter of the floor frame
 *
 * Actual Dimensions: 1.5" x 5.5" x (variable length, default 8ft)
 * Metric: 3.81cm x 13.97cm x 243.84cm
 *
 * Socket Layout:
 * - Bottom End Sockets (2): Connect to foundation corners
 * - Top Face Sockets: At 16" intervals for floor joists
 * - Side Face Sockets: For perpendicular joists that butt into rim
 * - End Corner Sockets (4): For 90-degree rim-to-rim connections
 */
UCLASS()
class BORNTOSHINE_API ARimBoard : public ABuildablePiece
{
	GENERATED_BODY()

public:
	ARimBoard();

protected:
	virtual void BeginPlay() override;

	// Initialize sockets (called when piece is spawned)
	virtual void InitializeSockets() override;

public:
	// Rim board dimensions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardWidth;  // 1.5" = 3.81 cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardHeight; // 5.5" = 13.97 cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardLength; // Default 8ft = 243.84 cm

	// Joist spacing options
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Spacing")
	float JoistSpacing; // 16" OC = 40.64 cm (or 24" = 60.96 cm)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Spacing")
	bool bUse24InchSpacing; // False = 16" OC, True = 24" OC

	// Outside vs Inside board system for proper corner joints
	// Outside boards: Full length, extend to outer corner edge
	// Inside boards: 3" shorter, butt against the outside boards
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Corner")
	bool bIsOutsideBoard; // True = outside board (full length), False = inside board (3" shorter)

	// Toggle between outside and inside board mode
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void ToggleBoardType();

	// Get the effective length accounting for outside/inside type
	UFUNCTION(BlueprintCallable, Category = "Construction")
	float GetEffectiveLength() const;

	// Get half board width for flush offset calculations
	float GetBoardHalfWidth() const { return BoardWidth / 2.0f; }

	// Extend the visual mesh by HalfWidth on each end for flush corner joints.
	// Actor position and sockets are unchanged — only the mesh scale increases.
	void ExtendMeshForFlushCorners();

	// Socket generation
	void CreateBottomEndSockets();
	void CreateTopFaceSockets();
	void CreateSideFaceSockets();
	void CreateEndCornerSockets();

	// Override snapping for rim board alignment
	virtual void UpdatePreviewPosition(const FVector& NewLocation, const FRotator& NewRotation) override;

	// Length scaling (1ft to 16ft)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetBoardLengthFeet(int32 LengthInFeet);

	UFUNCTION(BlueprintCallable, Category = "Construction")
	int32 GetBoardLengthFeet() const;

	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetLengthDisplayString() const;

	// Override scale to change length instead of visual scale
	virtual void ScalePiece(float ScaleDelta) override;

private:
	// Scene root component — keeps actor transform at scale (1,1,1)
	// so GetActorTransform().TransformPosition() doesn't scale socket positions.
	// MeshComponent is a child of this and can be scaled independently for visuals.
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	// Helper to calculate socket positions along the board
	TArray<FVector> CalculateJoistSocketPositions() const;

	// Regenerate all sockets when board length changes
	void RegenerateSockets();

	// Current length in feet (for easy tracking)
	int32 CurrentLengthFeet;

	// Min and max length in feet
	static constexpr int32 MinLengthFeet = 1;
	static constexpr int32 MaxLengthFeet = 16;

	// Flag to prevent manual scaling when auto-scaling is active (closing rectangle)
	bool bAutoScalingActive = false;

	// Guard against double-extending mesh (e.g. on save/load)
	bool bMeshExtended = false;
};
