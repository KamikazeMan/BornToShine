// Born To Shine - Window Frame (single pre-modeled mesh: king studs, trimmers, header, rough sill, cripples)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "Components/BoxComponent.h"
#include "WindowFrame.generated.h"

/**
 * Window Frame -- single mesh containing king studs, trimmers, header,
 * rough sill, and cripple studs.  Assigned in Blueprint (BP_WindowFrame).
 *
 * Placement:
 *   - Snaps to bottom plate top face (Wall_Bottom_Plate sockets),
 *     same as door frame / wall studs.
 *   - On placement, auto-deletes any wall studs whose positions
 *     fall within the window frame's width on the same wall.
 *
 * Socket Layout:
 *   - Bottom (1): WindowFrame_Bottom -- snaps to Wall_Bottom_Plate
 *   - Top (1): WindowFrame_Top -- for top plate attachment
 */
UCLASS()
class BORNTOSHINE_API AWindowFrame : public ABuildablePiece
{
	GENERATED_BODY()

public:
	AWindowFrame();

	/** Height of the window frame assembly (same as wall studs by default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float FrameHeight;

	/** Rough opening width (gap between trimmers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RoughOpeningWidth;

	/** Overall frame width including all studs (used for stud overlap deletion). */
	UPROPERTY(VisibleAnywhere, Category = "Construction|Dimensions")
	float FrameOverallWidth;

	/** Rough opening height. Default 88.304cm = 34.77" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RoughOpeningHeight;

	/** Rough sill height from bottom plate. Default 30.0cm = 11.81" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RoughSillHeight;

	float GetFrameHeightCm() const { return FrameHeight; }

	/** Mark overlap deletion as already done (used during save/load to prevent
	    re-deleting studs that were already removed). */
	void SetHasAutoDeleted(bool bValue) { bHasAutoDeleted = bValue; }

	/** Disable scroll-wheel scaling. */
	virtual void ScalePiece(float ScaleDelta) override;

	/** Override placement to trigger auto-delete of overlapping studs. */
	virtual bool TryPlace() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Collision")
	UBoxComponent* LeftPostCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Collision")
	UBoxComponent* RightPostCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Collision")
	UBoxComponent* HeaderCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Collision")
	UBoxComponent* SillCollision;

	/** When true (default), BeginPlay auto-sizes boxes from mesh bounds.
	 *  Uncheck after fine-tuning in the Blueprint viewport so your edits stick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Collision")
	bool bAutoSizeCollisionBoxes;

	// --- Extension Studs (bottom cripples + king studs, top cripples + king studs) ---

	/** Static mesh for all extension studs (assign a 2x4 stud mesh in Blueprint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Extensions")
	UStaticMesh* ExtensionMesh;

	/** Enable auto-generated extension studs (bottom cripples, top cripples,
	 *  and king stud extensions) that fill the gaps the pre-modeled mesh
	 *  leaves at the bottom plate and top plate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Extensions")
	bool bEnableExtensions;

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
	void EnableWindowCollision(bool bEnable);

	/** Aggressively disable ALL collision on MeshComponent.
	 *  The mesh's convex hull covers the window opening; if any collision
	 *  remains on it, the opening is blocked by an invisible wall. */
	void KillMeshCollision();

	/**
	 * Called once after the window frame is placed (exits preview mode).
	 * Finds and destroys overlapping wall studs within the frame width.
	 */
	void AutoDeleteOverlappingStuds();

	/** Prevents AutoDeleteOverlappingStuds from running more than once. */
	bool bHasAutoDeleted = false;

	/** Dynamically created bottom extension stud components (cripples + king studs). */
	UPROPERTY()
	TArray<UStaticMeshComponent*> BottomExtensions;

	/** Dynamically created top extension stud components (cripples + king studs). */
	UPROPERTY()
	TArray<UStaticMeshComponent*> TopExtensions;

	/** Spawn bottom cripple stud + king stud extensions from bottom plate to rough sill. */
	void CreateBottomExtensions();

	/** Spawn top cripple stud + king stud extensions from header top to top plate. */
	void CreateTopExtensions();

	/** Update extension visibility/material for preview vs placed state. */
	void SetExtensionPreviewMode(bool bIsPreview);

	/** Override: hook into placement to trigger auto-delete. */
	virtual void SetPreviewMode(bool bIsPreview) override;
};
