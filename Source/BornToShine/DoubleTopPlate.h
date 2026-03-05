// Born To Shine - Double Top Plate (second 2x4, stacked with corner overlap)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "DoubleTopPlate.generated.h"

/**
 * Double Top Plate — second 2x4 stacked on top of the first top plate.
 *
 * At alternating corners the double plate OVERLAPS onto the adjacent
 * wall's first top plate by 3.5" (8.89cm) to tie the walls together.
 * Uses the same 2x4 mesh as BottomPlate/TopPlate.
 *
 * Socket Layout:
 *   - Bottom: Snaps to TopPlate_Top
 *   - End (2): Plate-to-plate connections
 */
UCLASS()
class BORNTOSHINE_API ADoubleTopPlate : public ABuildablePiece
{
	GENERATED_BODY()

public:
	ADoubleTopPlate();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardWidth;  // 1.5" = 3.81 cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardHeight; // 3.5" = 8.89 cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardLength; // Default 8ft = 243.84 cm

	float GetEffectiveLength() const { return BoardLength; }
	float GetBoardHalfWidth() const { return BoardWidth / 2.0f; }

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
	void RegenerateSockets();

	int32 CurrentLengthFeet;
	static constexpr int32 MinLengthFeet = 1;
	static constexpr int32 MaxLengthFeet = 16;

	bool bMeshExtended = false;
};
