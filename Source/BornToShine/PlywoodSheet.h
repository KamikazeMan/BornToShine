// Born To Shine - Plywood Sheet (4x8 subfloor panel)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "PlywoodSheet.generated.h"

/**
 * Plywood Sheet - 4'x8' panel for floor sheathing.
 *
 * Dimensions: 121.92cm x 243.84cm x 1.5875cm (4' x 8' x 5/8")
 * Lays flat on top of joists and rim boards.
 *
 * Socket Layout:
 * - 4 Corner Sockets: At each corner, snap to RimBoard_End_Corner or Plywood_Corner
 * - Edge Sockets: Along all 4 edges at joist spacing, snap to Joist_Top_Face,
 *   RimBoard_Top_Face, or adjacent Plywood_Edge
 */
UCLASS()
class BORNTOSHINE_API APlywoodSheet : public ABuildablePiece
{
	GENERATED_BODY()

public:
	APlywoodSheet();

	// Sheet dimensions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float SheetWidth;  // 4' = 121.92 cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float SheetLength; // 8' = 243.84 cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float SheetThickness; // 5/8" = 1.5875 cm

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	// Scene root to decouple mesh scale from actor transform
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateCornerSockets();
	void CreateEdgeSockets();
};
