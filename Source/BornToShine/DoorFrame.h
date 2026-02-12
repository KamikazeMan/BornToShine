// Born To Shine - Door Frame (single pre-modeled mesh: king studs, trimmers, header, cripples)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "DoorFrame.generated.h"

/**
 * Door Frame — single mesh containing king studs, trimmers, header,
 * and cripple studs above the header.  Assigned in Blueprint (BP_DoorFrame).
 *
 * Placement:
 *   - Snaps to bottom plate top face (Wall_Bottom_Plate sockets),
 *     same as regular wall studs.
 *   - On placement, auto-deletes any wall studs whose bounding boxes
 *     overlap the door frame's bounding box.
 *   - Also auto-deletes the bottom plate section under the door opening
 *     (the plate between the two trimmers must be removed for a real door).
 *
 * Socket Layout:
 *   - Bottom (1): DoorFrame_Bottom — snaps to Wall_Bottom_Plate
 *   - Top (1): DoorFrame_Top — for future top plate attachment
 */
UCLASS()
class BORNTOSHINE_API ADoorFrame : public ABuildablePiece
{
	GENERATED_BODY()

public:
	ADoorFrame();

	/** Height of the door frame assembly (same as wall studs by default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float FrameHeight;

	/** Rough opening width of the door (used for overlap detection). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RoughOpeningWidth;

	float GetFrameHeightCm() const { return FrameHeight; }

	/** Disable scroll-wheel scaling. */
	virtual void ScalePiece(float ScaleDelta) override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateBottomSocket();
	void CreateTopSocket();
	void AdjustSocketsToMeshBounds();

	/**
	 * Called once after the door frame is placed (exits preview mode).
	 * Finds and destroys overlapping wall studs and the bottom plate
	 * section under the door opening.
	 */
	void AutoDeleteOverlappingPieces();

	/** Override: hook into placement to trigger auto-delete. */
	virtual void SetPreviewMode(bool bIsPreview) override;
};
