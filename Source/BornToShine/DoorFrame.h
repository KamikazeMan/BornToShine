// Born To Shine - Door Frame (single pre-modeled mesh: king studs, trimmers, header, cripples)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "Components/BoxComponent.h"
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

	/** Rough opening width (gap between trimmers, used for plate cut). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RoughOpeningWidth;

	/** Overall frame width including king studs (used for stud overlap deletion). */
	UPROPERTY(VisibleAnywhere, Category = "Construction|Dimensions")
	float FrameOverallWidth;

	/** Height of the door opening from the frame bottom to the header bottom.
	    Default 205.74cm = 81" (standard 6'8" door trimmer height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RoughOpeningHeight;

	float GetFrameHeightCm() const { return FrameHeight; }

	/** Mark overlap deletion as already done (used during save/load to prevent
	    re-splitting remnant plates that were already restored). */
	void SetHasAutoDeleted(bool bValue) { bHasAutoDeleted = bValue; }

	/** Disable scroll-wheel scaling. */
	virtual void ScalePiece(float ScaleDelta) override;

	/** Override placement to trigger auto-delete of overlapping studs/plates. */
	virtual bool TryPlace() override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateBottomSocket();
	void CreateTopSocket();
	void AdjustSocketsToMeshBounds();
	void SetupCollisionBoxes();
	void EnableDoorCollision(bool bEnable);

	/** Aggressively disable ALL collision on MeshComponent.
	 *  The mesh's convex hull covers the door opening; if any collision
	 *  remains on it, the opening is blocked by an invisible wall. */
	void KillMeshCollision();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Collision")
	UBoxComponent* LeftPostCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Collision")
	UBoxComponent* RightPostCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Collision")
	UBoxComponent* HeaderCollision;

	/** When true (default), BeginPlay auto-sizes boxes from mesh bounds.
	 *  Uncheck after fine-tuning in the Blueprint viewport so your edits stick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Collision")
	bool bAutoSizeCollisionBoxes;

	/**
	 * Called once after the door frame is placed (exits preview mode).
	 * Finds and destroys overlapping wall studs and the bottom plate
	 * section under the door opening.
	 */
	void AutoDeleteOverlappingPieces();

	/** Prevents AutoDeleteOverlappingPieces from running more than once. */
	bool bHasAutoDeleted = false;

	/** Override: hook into placement to trigger auto-delete. */
	virtual void SetPreviewMode(bool bIsPreview) override;
};
