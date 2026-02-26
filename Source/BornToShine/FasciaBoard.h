// Born To Shine - Fascia Board (attaches to rafter tail ends)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "FasciaBoard.generated.h"

/**
 * Fascia Board — horizontal board nailed to the rafter tail ends.
 *
 * Actual Dimensions: 1" x 6" (2.54cm x 15.24cm) nominal
 * (actual 3/4" x 5.5" = 1.905cm x 13.97cm)
 *
 * Runs along the eave (bottom edge of the roof overhang),
 * connecting all rafter tails on one side. Provides a finished
 * edge and mounting surface for gutters.
 *
 * Socket Layout:
 *   - End (2): Board-to-board connections
 *   - Rafter Tail (N): At 16" OC intervals to connect to rafter tails
 */
UCLASS()
class BORNTOSHINE_API AFasciaBoard : public ABuildablePiece
{
	GENERATED_BODY()

public:
	AFasciaBoard();

	// Board dimensions (1x6 actual)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardWidth; // 3/4" = 1.905cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardHeight; // 5.5" = 13.97cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardLength; // Variable length

	float GetEffectiveLength() const { return BoardLength; }

	// Set board length in cm
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetBoardLengthCm(float LengthCm);

	// Set board length in feet
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetBoardLengthFeet(int32 LengthInFeet);

	UFUNCTION(BlueprintCallable, Category = "Construction")
	int32 GetBoardLengthFeet() const;

	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetLengthDisplayString() const;

	// Override scale to disable scroll wheel
	virtual void ScalePiece(float ScaleDelta) override;

	/** Override placement to trim rafter tails that extend past the fascia face. */
	virtual bool TryPlace() override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateEndSockets();
	void CreateRafterTailSockets();
	void RegenerateSockets();
	void AdjustSocketsToMeshBounds();

	int32 CurrentLengthFeet;
	static constexpr int32 MinLengthFeet = 1;
	static constexpr int32 MaxLengthFeet = 20;
};
