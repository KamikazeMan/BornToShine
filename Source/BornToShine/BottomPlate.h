// Born To Shine - Bottom Plate (2x4 sole plate for wall framing)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "BottomPlate.generated.h"

/**
 * Bottom Plate (Sole Plate) - 2x4 lumber that forms the base of wall framing.
 *
 * Actual Dimensions: 1.5" x 3.5" x (variable length, default 8ft)
 * Metric: 3.81cm x 8.89cm x 243.84cm
 *
 * Sits on TOP of the plywood subfloor, directly above each rim board.
 * Wall studs will later attach to the top face sockets.
 *
 * Socket Layout:
 * - Bottom Sockets: Along the bottom face, snap to RimBoard_Top_Face below
 * - End Sockets (2): At each end, for plate-to-plate corner connections
 * - Top Face Sockets: At 16" OC for future wall stud attachment
 */
UCLASS()
class BORNTOSHINE_API ABottomPlate : public ABuildablePiece
{
	GENERATED_BODY()

public:
	ABottomPlate();

	// 2x4 lumber dimensions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardWidth;  // 1.5" = 3.81 cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardHeight; // 3.5" = 8.89 cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardLength; // Default 8ft = 243.84 cm

	// Get the effective length (no inside/outside concept for plates)
	float GetEffectiveLength() const { return BoardLength; }

	// Get half board width for flush offset calculations
	float GetBoardHalfWidth() const { return BoardWidth / 2.0f; }

	// Extend the visual mesh by HalfWidth on each end for flush corner joints.
	// Actor position and sockets are unchanged — only the mesh scale increases.
	void ExtendMeshForFlushCorners();

	// Length scaling (1ft to 16ft)
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetBoardLengthFeet(int32 LengthInFeet);

	UFUNCTION(BlueprintCallable, Category = "Construction")
	int32 GetBoardLengthFeet() const;

	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetLengthDisplayString() const;

	// Override scale to change length instead of visual scale
	virtual void ScalePiece(float ScaleDelta) override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	// Scene root to decouple mesh scale from actor transform
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateBottomSockets();
	void CreateEndSockets();
	void CreateTopFaceSockets();

	// Regenerate all sockets when board length changes
	void RegenerateSockets();

	// Current length in feet
	int32 CurrentLengthFeet;

	static constexpr int32 MinLengthFeet = 1;
	static constexpr int32 MaxLengthFeet = 16;
};
