// Born To Shine - Window Frame (single pre-modeled mesh from Rhino)
// Includes stud extension system to fill gap between king stud tops and top plate.

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "WindowFrame.generated.h"

/**
 * Window Frame — single mesh containing king studs, header, sill,
 * cripple studs, and trimmer studs.  Assigned in Blueprint (BP_WindowFrame).
 *
 * Problem: The king studs built into the Rhino mesh are shorter than
 * the full wall stud height, leaving a gap to the top plate.
 *
 * Solution: On placement, this class spawns two stud extension
 * components (standard 2x4 lumber) on top of the king studs,
 * scaled vertically to fill the gap up to the top plate bottom.
 *
 * Placement:
 *   - Snaps to bottom plate top face (Wall_Bottom_Plate sockets).
 *   - On placement, auto-deletes wall studs within the frame width (5 studs).
 *   - Spawns stud extensions above king studs to reach top plate.
 *
 * Socket Layout:
 *   - Bottom (1): WindowFrame_Bottom — snaps to Wall_Bottom_Plate
 *   - Top (1): WindowFrame_Top — for top plate attachment
 */
UCLASS()
class BORNTOSHINE_API AWindowFrame : public ABuildablePiece
{
	GENERATED_BODY()

public:
	AWindowFrame();

	// ---- Window Dimensions (matching the Rhino mesh geometry) ----

	/** Total width of the window frame assembly including king studs (65.52") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float FrameTotalWidth;

	/** Width of the rough opening (27.5") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RoughOpeningWidth;

	/** Height of the rough opening (34.77") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RoughOpeningHeight;

	/** Height from bottom plate top to rough sill bottom (11.81") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float RoughSillHeight;

	/** Height of the window frame assembly (auto-calculated from mesh bounds). */
	UPROPERTY(VisibleAnywhere, Category = "Construction|Dimensions")
	float FrameHeight;

	/** Full height of wall studs: bottom plate top to top plate bottom (92-5/8") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float WallStudHeight;

	// ---- Stud Extension System ----

	/** Left king stud extension mesh component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction|Extensions")
	UStaticMeshComponent* LeftStudExtension;

	/** Right king stud extension mesh component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction|Extensions")
	UStaticMeshComponent* RightStudExtension;

	/** Static mesh for 2x4 stud extensions (same as wall studs — assign in Blueprint) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Extensions")
	UStaticMesh* StudExtensionMesh;

	/** Whether stud extensions are currently active */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction|Extensions")
	bool bHasStudExtensions;

	// ---- Public Functions ----

	/** Spawn stud extension pieces above king studs to reach the top plate */
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void SpawnStudExtensions();

	/** Remove stud extension pieces */
	UFUNCTION(BlueprintCallable, Category = "Construction")
	void RemoveStudExtensions();

	/** Calculate the height needed for stud extensions (gap size) */
	UFUNCTION(BlueprintPure, Category = "Construction")
	float CalculateExtensionHeight() const;

	/** Get the Z height of the king stud tops (relative to actor origin) */
	UFUNCTION(BlueprintPure, Category = "Construction")
	float GetKingStudTopZ() const;

	/** Disable scroll-wheel scaling */
	virtual void ScalePiece(float ScaleDelta) override;

	/** Override placement to trigger auto-delete and stud extensions */
	virtual bool TryPlace() override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	/** Scene root to decouple mesh scale from actor transform */
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateBottomSocket();
	void CreateTopSocket();
	void AdjustSocketsToMeshBounds();

	/** Calculate king stud height from the FrameMesh bounding box */
	float CalculateKingStudHeightFromMesh() const;

	/** Get local X offset for the left king stud */
	float GetLeftKingStudOffsetX() const;

	/** Get local X offset for the right king stud */
	float GetRightKingStudOffsetX() const;

	/** Configure a stud extension mesh component with position and scale */
	void ConfigureExtensionMesh(UStaticMeshComponent* ExtComp, float OffsetX, float ExtensionHeight);

	/** Delete wall studs that overlap with the window frame width */
	void AutoDeleteOverlappingStuds();

	/** Prevents auto-delete from running more than once */
	bool bHasAutoDeleted = false;

	/** Override: hook into placement to trigger auto-delete + extensions */
	virtual void SetPreviewMode(bool bIsPreview) override;
};
