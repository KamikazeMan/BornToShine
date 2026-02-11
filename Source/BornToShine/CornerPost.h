// Born To Shine - Corner Post (4-stud outside corner assembly, single mesh)

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "CornerPost.generated.h"

/**
 * Corner Post - A single-mesh 4-stud outside corner assembly.
 *
 * This is a pre-modeled piece (created in Rhino) representing the full
 * corner post assembly: 2 vertical studs + 2 flat blocking studs between them.
 * The mesh is assigned in the Blueprint (BP_CornerPost).
 *
 * Placement:
 *   - Snaps to bottom plate ends via CornerPost_Bottom <-> CornerPost_Seat sockets
 *   - Stands vertically, matching the plate's yaw
 *   - Height matches wall studs (92-5/8" = 235.27cm default)
 *
 * Socket Layout:
 *   - Bottom (1): CornerPost_Bottom — snaps to plate end seat sockets
 *   - Top (1): CornerPost_Top — for future top plate attachment
 */
UCLASS()
class BORNTOSHINE_API ACornerPost : public ABuildablePiece
{
	GENERATED_BODY()

public:
	ACornerPost();

	// Height of the corner post (same as wall studs)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float PostHeight; // Default 235.27cm (92-5/8")

	// Lateral offset (cm) applied along the plate's local Y axis when snapping.
	// Moves the corner post inward so its outer stud face is flush with the
	// bottom plate's outer face.  Tune in Blueprint to match your Rhino mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Alignment")
	float FlushInwardOffset; // Default 2.54cm (1")

	// Get height in cm
	UFUNCTION(BlueprintCallable, Category = "Construction")
	float GetPostHeightCm() const { return PostHeight; }

	// Override scale to disable scroll wheel scaling
	virtual void ScalePiece(float ScaleDelta) override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateBottomSocket();
	void CreateTopSocket();
};
