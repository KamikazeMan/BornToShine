// Born To Shine - Rim Board (2x6 lumber for floor frame perimeter)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
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
	// Helper to calculate socket positions along the board
	TArray<FVector> CalculateJoistSocketPositions() const;

	// Regenerate all sockets when board length changes
	void RegenerateSockets();

	// Current length in feet (for easy tracking)
	int32 CurrentLengthFeet;

	// Min and max length in feet
	static constexpr int32 MinLengthFeet = 1;
	static constexpr int32 MaxLengthFeet = 16;
};
