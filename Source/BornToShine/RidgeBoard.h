// Born To Shine - Ridge Board (2x8 horizontal beam at roof peak)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "RoofGridComponent.h"
#include "RidgeBoard.generated.h"

/**
 * Ridge Board — 2x8 lumber that sits horizontally at the peak of the roof.
 *
 * Actual Dimensions: 1.5" x 7.25" x (variable length)
 * Metric: 3.81cm x 18.415cm x variable
 *
 * Sits in the pockets of two ridge posts, spanning the length of the building.
 * Rafters attach to both sides of this board.
 *
 * Socket Layout:
 *   - End (2): Snap into RidgePost_Pocket sockets
 *   - Side: Along the length at intervals for rafter attachment
 */
UCLASS()
class BORNTOSHINE_API ARidgeBoard : public ABuildablePiece
{
	GENERATED_BODY()

public:
	ARidgeBoard();

	// 2x8 lumber dimensions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardWidth; // 1.5" = 3.81cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardHeight; // 7.25" = 18.415cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float BoardLength; // Variable, matches building length

	float GetEffectiveLength() const { return BoardLength; }

	// Set board length in cm
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetBoardLengthCm(float LengthCm);

	// Set board length in feet
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SetBoardLengthFeet(int32 LengthInFeet);

	// Get board length in feet
	UFUNCTION(BlueprintCallable, Category = "Construction")
	int32 GetBoardLengthFeet() const;

	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetLengthDisplayString() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roof Grid")
	URoofGridComponent* RoofGrid;

	// Called when roof framing changes (rafters placed, fascia placed, etc.)
	void RebuildRoofGrids();

	// Override scale to disable scroll wheel scaling
	virtual void ScalePiece(float ScaleDelta) override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateEndSockets();
	void CreateSideSockets();
	void RegenerateSockets();
	void AdjustSocketsToMeshBounds();

	int32 CurrentLengthFeet;
	static constexpr int32 MinLengthFeet = 1;
	static constexpr int32 MaxLengthFeet = 20;
};
