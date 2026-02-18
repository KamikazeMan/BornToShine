// Born To Shine - Rafter (static mesh with dynamic pitch-based placement)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "Rafter.generated.h"

/**
 * Rafter — 2x6 lumber that forms the sloped roof structure.
 *
 * Uses a static mesh (straight 2x6 board) that gets X-scaled to the
 * correct slope length. Roof pitch is applied via actor rotation (Pitch
 * component of FRotator), not baked into mesh vertices.
 *
 * Cross-Section: 1.5" x 5.5" (3.81cm x 13.97cm)
 *
 * Socket Layout (all along the straight board's local X axis):
 *   - Ridge End: at -SlopeLength/2 (attaches to RidgeBoard_Side)
 *   - Birdsmouth: at MainSlope distance from ridge end
 *   - Tail End: at +SlopeLength/2 (for fascia board attachment)
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

	// Set pitch and rebuild sockets + scale
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

	// Original unscaled length of the mesh along X (captured once in BeginPlay)
	float MeshDefaultLength;

	void CreateRidgeEndSocket();
	void CreateBirdsmouthSocket();
	void CreateTailEndSocket();
	void RegenerateSockets();

	// Scale the mesh along X to match SlopeLength
	void UpdateRafterLength();
};
