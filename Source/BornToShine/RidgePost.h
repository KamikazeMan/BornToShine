// Born To Shine - Ridge Post (3 laminated 2x6 with ridge beam pocket)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "RidgePost.generated.h"

/**
 * Ridge Post — 3 laminated 2x6 boards with a pocket at the top for the ridge beam.
 *
 * Construction:
 *   Three 2x6 boards nailed together. The middle board is cut short
 *   to create a notch/pocket for the 2x8 ridge beam to sit in.
 *   The two outer 2x6s sandwich the beam on both sides.
 *
 * Post Footprint (top view):
 *   4.5" x 5.5" (11.43cm x 13.97cm)
 *   Width = 3 x 1.5" = 4.5" (across laminated boards)
 *   Depth = 5.5" (face of each 2x6)
 *
 * Pocket Dimensions:
 *   Width: 1.5" (3.81cm) — middle board thickness
 *   Depth: 7.25" (18.415cm) — height of 2x8 ridge beam
 *
 * Placement:
 *   - Sits on double top plate at CENTER of building width
 *   - One at each gable end wall
 *   - Height adjustable via scroll wheel (determines roof pitch)
 *   - Default height for 6/12 pitch
 *
 * Socket Layout:
 *   - Bottom (1): Snaps to DoubleTopPlate top face
 *   - Pocket (1): At the top, for ridge board attachment
 */
UCLASS()
class BORNTOSHINE_API ARidgePost : public ABuildablePiece
{
	GENERATED_BODY()

public:
	ARidgePost();

	// Individual 2x6 board dimensions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardThickness; // 1.5" = 3.81cm (each board)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardFaceWidth; // 5.5" = 13.97cm (face of 2x6)

	// Total post assembly dimensions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float PostWidth; // 3 x 1.5" = 4.5" = 11.43cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float PostDepth; // 5.5" = 13.97cm

	// Pocket dimensions (for 2x8 ridge beam)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float PocketWidth; // 1.5" = 3.81cm (middle board thickness)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float PocketDepth; // 7.25" = 18.415cm (2x8 height)

	// Post height above double top plate (adjustable via scroll wheel)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float PostHeight;

	// Building half-width (set by suggestion system, used for pitch calculation)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Pitch")
	float BuildingHalfWidthCm;

	// Height adjustment per scroll tick (in cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Pitch")
	float HeightIncrementCm;

	// Minimum/maximum post height
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Pitch")
	float MinPostHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Pitch")
	float MaxPostHeight;

	// Get current roof pitch as rise/12 (e.g., returns 6.0 for 6/12 pitch)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	float GetPitchRatio() const;

	// Get pitch display string (e.g., "6/12 pitch")
	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetPitchDisplayString() const;

	// Get post height in inches
	UFUNCTION(BlueprintCallable, Category = "Construction")
	float GetPostHeightInches() const;

	// Set post height in cm
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetPostHeightCm(float HeightCm);

	// Set building half-width (called by suggestion system)
	void SetBuildingHalfWidth(float HalfWidthCm);

	// Override scroll wheel to adjust height instead of scale
	virtual void ScalePiece(float ScaleDelta) override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	// Dual-mesh architecture: PocketMesh (top, never scaled) + PostMesh (lower column, scaled on Z)
	// Until David provides split meshes, both are nullptr and we use MeshComponent at scale 1,1,1.
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* PocketMesh;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* PostMesh;

	// Original unscaled height of the mesh from Rhino (captured once in BeginPlay)
	float OriginalMeshHeight;

	// Height of the pocket portion (top part with notch - never scaled)
	float PocketPortionHeight;

	void CreateBottomSocket();
	void CreatePocketSocket();
	void RegenerateSockets();
	void UpdateMeshScale();
	void AdjustSocketsToMeshBounds();

	// Show pitch on screen
	void DisplayPitchInfo() const;
};
