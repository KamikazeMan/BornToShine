// Born To Shine - Top Plate (2x4 top plate for wall framing)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "TopPlate.generated.h"

/**
 * Top Plate — 2x4 lumber that caps the top of the wall studs/corner posts.
 *
 * Actual Dimensions: 1.5" x 3.5" x (variable length, default 8ft)
 * Metric: 3.81cm x 8.89cm x 243.84cm
 *
 * Sits on TOP of the wall studs and corner posts.
 * Uses the same 2x4 mesh as BottomPlate.
 * Bridges continuously over door headers (no cut like bottom plate).
 *
 * Socket Layout:
 *   - Bottom: Snaps to Wall_Stud_Top and CornerPost_Top
 *   - End (2): Plate-to-plate corner connections
 *   - Top Face: At intervals for DoubleTopPlate attachment
 */
UCLASS()
class BORNTOSHINE_API ATopPlate : public ABuildablePiece
{
	GENERATED_BODY()

public:
	ATopPlate();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardWidth;  // 1.5" = 3.81 cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardHeight; // 3.5" = 8.89 cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardLength; // Default 8ft = 243.84 cm

	float GetEffectiveLength() const { return BoardLength; }
	float GetBoardHalfWidth() const { return BoardWidth / 2.0f; }

	void ExtendMeshForFlushCorners();

	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetBoardLengthFeet(int32 LengthInFeet);

	void SetBoardLengthCm(float LengthCm);

	UFUNCTION(BlueprintCallable, Category = "Construction")
	int32 GetBoardLengthFeet() const;

	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetLengthDisplayString() const;

	virtual void ScalePiece(float ScaleDelta) override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateBottomSockets();
	void CreateEndSockets();
	void CreateTopFaceSockets();
	void RegenerateSockets();

	int32 CurrentLengthFeet;
	static constexpr int32 MinLengthFeet = 1;
	static constexpr int32 MaxLengthFeet = 16;

	bool bMeshExtended = false;

public:
	bool IsMeshExtended() const { return bMeshExtended; }
};
