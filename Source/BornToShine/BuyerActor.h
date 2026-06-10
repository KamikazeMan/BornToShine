// Born To Shine - Placeholder moonshine buyer (no dialogue/suspicion/quality yet)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuyerActor.generated.h"

class UStaticMeshComponent;

/**
 * Minimal buyer: stand near it, aim, press E to sell all MoonshineJar at PricePerJar each.
 * Mesh is assigned in BP defaults (placeholder crate).
 */
UCLASS()
class BORNTOSHINE_API ABuyerActor : public AActor
{
	GENERATED_BODY()

public:
	ABuyerActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Selling")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selling")
	int32 PricePerJar = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selling")
	FString BuyerName = TEXT("Backwoods Buyer");
};
