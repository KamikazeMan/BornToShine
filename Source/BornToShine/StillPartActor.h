// Born To Shine - Lightweight actor for placed moonshine still parts

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StillPartActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** Operating state of a completed still. Linear progression Empty -> ... -> Done. */
UENUM(BlueprintType)
enum class EStillState : uint8
{
	Empty   UMETA(DisplayName="Empty"),
	Water   UMETA(DisplayName="Water Added"),
	Mash    UMETA(DisplayName="Mash Added"),
	Lit     UMETA(DisplayName="Fire Lit"),
	Running UMETA(DisplayName="Distilling"),
	Done    UMETA(DisplayName="Batch Complete")
};

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

	// Batch output state (meaningful on the MasonJar catch vessel only).
	// Full: a distilling run finished and the jar holds moonshine, awaiting the lid.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="StillPart")
	bool bIsFull = false;

	// Sealed: the lid was placed on a full jar; the moonshine can be collected with E.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="StillPart")
	bool bIsSealed = false;

	// The CinderBlockStand this part belongs to (null for stands themselves). Set at placement:
	// vessels record the stand they snapped to; cap-like parts inherit it from their snap target.
	UPROPERTY(VisibleAnywhere, Category="StillPart")
	TWeakObjectPtr<AStillPartActor> OwningStand;

	// --- Per-assembly operating state (meaningful on the CinderBlockStand only — the assembly
	// root). Each still runs its own state machine and batch timer. ---
	UPROPERTY(VisibleAnywhere, Category="StillPart")
	EStillState StillState = EStillState::Empty;

	// Seconds elapsed in the current distilling run (counts up while bBatchRunning).
	float BatchElapsed = 0.0f;

	// True while this stand's batch timer is ticking (Running state).
	bool bBatchRunning = false;

	// Jars still waiting in THIS jar after a partial collection (MasonJar only). While > 0 the
	// jar stays sealed and each E press collects as much as fits.
	UPROPERTY(VisibleAnywhere, Category="StillPart")
	int32 RemainingJars = 0;

	// Assign the part identity and mesh after spawning.
	void InitFromItemData(FName InPartID, UStaticMesh* InMesh);
};
