#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "WallSheathing.generated.h"

/**
 * Wall Sheathing — 4x8 plywood sheet for wall covering.
 *
 * Mesh: /Game/Assets/WallSheathing (121x1x243 cm, oriented vertically)
 * Snaps to wall studs on exterior or interior face.
 * Auto-detects which side based on player position relative to building center.
 * Bottom-aligned to bottom plate top with 3.82cm Z extension to fill wall cavity.
 * Auto-tiles at 4ft (121.92cm) intervals along the wall.
 *
 * Socket Layout:
 *   - Wall_Stud_Face sockets along the back face for stud attachment
 */
UCLASS()
class BORNTOSHINE_API AWallSheathing : public ABuildablePiece
{
	GENERATED_BODY()

public:
	AWallSheathing();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float SheetWidth;      // 4ft = 121.92cm (along wall)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float SheetHeight;     // 8ft = 243.84cm (vertical)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float SheetThickness;  // 0.50" = 1.27cm

	/** Disable scroll-wheel scaling. */
	virtual void ScalePiece(float ScaleDelta) override;

	/** Override placement to handle window cutouts. */
	virtual bool TryPlace() override;

	/**
	 * Shift the corner extension to the correct side.
	 * Side: -1 = extend left (-X), +1 = extend right (+X), 0 = centered (no corner)
	 */
	void SetCornerExtensionSide(int32 Side);

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateFaceSockets();
	void AdjustSocketsToMeshBounds();
};
