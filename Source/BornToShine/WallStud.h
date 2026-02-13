// Born To Shine - Wall Stud (2x4 vertical framing member)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "WallStud.generated.h"

/**
 * Wall Stud - 2x4 lumber that stands vertically on the bottom plate.
 *
 * Cross-Section (actual dimensions):
 *   1.5" x 3.5"  (3.81cm x 8.89cm)
 *
 * Orientation when placed:
 *   - 1.5" edge runs along the wall length (local X)
 *   - 3.5" face is parallel to the wall, facing in/out (local Y)
 *   - Height is vertical (local Z)
 *
 * Default height: 92-5/8" (235.27cm) for standard 8ft walls.
 * Configurable via SetStudHeightInches().
 *
 * Socket Layout:
 *   - Bottom Socket (1): Snaps to Wall_Bottom_Plate sockets on the bottom plate
 *   - Top Socket (1): For future top plate attachment
 */
UCLASS()
class BORNTOSHINE_API AWallStud : public ABuildablePiece
{
	GENERATED_BODY()

public:
	AWallStud();

	// 2x4 cross-section dimensions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float StudWidth;   // 1.5" = 3.81cm (along wall length)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float StudDepth;   // 3.5" = 8.89cm (perpendicular to wall, facing in/out)

	// Vertical height of the stud
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float StudHeight;  // Default 92-5/8" = 235.27cm

	// Set stud height in inches (standard: 92.625 for 8ft walls)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetStudHeightInches(float HeightInInches);

	// Get stud height in inches
	UFUNCTION(BlueprintCallable, Category = "Construction")
	float GetStudHeightInches() const;

	// Get stud height in cm
	UFUNCTION(BlueprintCallable, Category = "Construction")
	float GetStudHeightCm() const { return StudHeight; }

	// Display string for HUD
	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetHeightDisplayString() const;

	// Override scale to disable scroll wheel scaling
	virtual void ScalePiece(float ScaleDelta) override;

	/** Scale this stud's mesh Z to match a target height (door frame reference). */
	void ScaleToReferenceHeight(float TargetHeightCm);

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	// Scene root to decouple mesh scale from actor transform
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateBottomSocket();
	void CreateTopSocket();

	// After sockets are created with default StudHeight, re-align them
	// to the actual mesh bounds so the StudTop socket matches the real
	// mesh top regardless of Rhino export pivot.
	void AdjustSocketsToMeshBounds();

	// Regenerate sockets when height changes
	void RegenerateSockets();

	// Update mesh scale to match dimensions
	void UpdateMeshScale();
};
