// Born To Shine - Rafter (procedural mesh with dynamic pitch-based cuts)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "Rafter.generated.h"

class UProceduralMeshComponent;

/**
 * Rafter — 2x6 lumber that forms the sloped roof structure.
 *
 * Uses ProceduralMeshComponent to generate mesh dynamically based on roof pitch.
 * All cut angles are auto-calculated from the pitch.
 *
 * Cross-Section: 1.5" x 5.5" (3.81cm x 13.97cm)
 *
 * Three Dynamic Cuts:
 *   1. Plumb Cut (Ridge End) — vertical cut at the ridge board, angle = pitch angle
 *   2. Birdsmouth Cut (Top Plate) — notch where rafter sits on the double top plate
 *      - Seat cut (horizontal) + heel cut (vertical)
 *      - Seat depth = 2/3 of rafter depth
 *   3. Tail Cut (Overhang End) — angled or plumb cut at the fascia end
 *
 * Geometry:
 *   - Rafter runs from ridge board down to top plate, continuing past for overhang
 *   - Length auto-calculated from pitch and building half-width
 *   - All angles auto-calculated from pitch ratio
 *
 * Socket Layout:
 *   - Ridge End (1): Attaches to RidgeBoard_Side socket
 *   - Birdsmouth (1): Sits on top plate
 *   - Tail End (1): For fascia board attachment
 */
UCLASS()
class BORNTOSHINE_API ARafter : public ABuildablePiece
{
	GENERATED_BODY()

public:
	ARafter();

	// 2x6 lumber cross-section
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RafterWidth; // 1.5" = 3.81cm (thickness)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RafterDepth; // 5.5" = 13.97cm (face height)

	// Roof pitch (rise per 12 units of run, e.g. 6.0 = 6/12)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Pitch")
	float PitchRatio;

	// Building half-width (run distance from ridge to wall, in cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Pitch")
	float RunDistanceCm;

	// Overhang past the wall (in cm), default 12" = 30.48cm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float OverhangCm;

	// Birdsmouth seat cut depth (fraction of rafter depth, default 2/3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BirdsmouthSeatFraction;

	// Set pitch and rebuild mesh
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetPitch(float NewPitchRatio, float NewRunCm);

	// Get the pitch angle in degrees (from horizontal)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	float GetPitchAngleDegrees() const;

	// Get the total rafter length along the slope (ridge to tail)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	float GetSlopeLengthCm() const;

	// Get display string
	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetRafterDisplayString() const;

	// Override scale to disable scroll wheel
	virtual void ScalePiece(float ScaleDelta) override;

	// Get tail end position in world space (for fascia board attachment)
	FVector GetTailEndWorldPosition() const;

	// Get the pitch angle in radians
	float GetPitchAngleRadians() const;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	// Procedural mesh for the rafter shape
	UPROPERTY(VisibleAnywhere)
	UProceduralMeshComponent* ProceduralMesh;

	// Generate the rafter mesh with all cuts
	void GenerateRafterMesh();

	// Helper: add a rectangular cross-section extruded along a path
	void BuildRafterGeometry(
		TArray<FVector>& Vertices,
		TArray<int32>& Triangles,
		TArray<FVector>& Normals,
		TArray<FVector2D>& UVs
	);

	void CreateRidgeEndSocket();
	void CreateBirdsmouthSocket();
	void CreateTailEndSocket();
	void RegenerateSockets();

	// Material for the procedural mesh
	UPROPERTY(EditAnywhere, Category = "Construction|Materials")
	class UMaterialInterface* RafterMaterial;
};
