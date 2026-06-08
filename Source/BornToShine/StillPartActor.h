// Born To Shine - Lightweight actor for placed moonshine still parts

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StillPartActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * Minimal mesh-holder actor for still parts (pot, cap, cinder block stands, etc.).
 * Step 1: just holds a mesh and a PartID. Snapping/sockets come in later steps.
 */
UCLASS()
class BORNTOSHINE_API AStillPartActor : public AActor
{
	GENERATED_BODY()

public:
	AStillPartActor();

	// Which still part this actor represents (matches the inventory ItemID).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="StillPart")
	FName PartID;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="StillPart")
	UStaticMeshComponent* MeshComponent;

	// How close the camera ray must pass to this part's visual center to trigger the snap (cm).
	// Per-part so tiny parts (e.g. MasonJarLid) can use a larger aim radius than big vessels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	float SnapRadiusCm = 35.0f;

	// Assign the part identity and mesh after spawning.
	void InitFromItemData(FName InPartID, UStaticMesh* InMesh);
};
