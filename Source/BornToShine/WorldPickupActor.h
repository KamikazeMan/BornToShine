// Born To Shine - Physical, recoverable world pickup dropped from the inventory.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldPickupActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USphereComponent;

/**
 * A dropped inventory item sitting in the world: a physics static mesh holding an item id + count.
 * Pick it up by aiming + interact (default) or by walking over it (if the player opts in).
 */
UCLASS()
class BORNTOSHINE_API AWorldPickupActor : public AActor
{
	GENERATED_BODY()

public:
	AWorldPickupActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pickup")
	UStaticMeshComponent* MeshComponent;

	// Proximity volume for optional walk-over pickup.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pickup")
	USphereComponent* PickupSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pickup")
	FName ItemId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pickup")
	int32 Count = 0;

	// Assign identity + mesh and (optionally) begin physics simulation.
	void Init(FName InItemId, int32 InCount, UStaticMesh* InMesh, bool bSimulate = true);

	// Small forward toss so a freshly dropped pickup clears the player.
	void TossForward(const FVector& Direction, float Strength);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);
};
