// Born To Shine - Placeholder moonshine buyer

#include "BuyerActor.h"
#include "Components/StaticMeshComponent.h"

ABuyerActor::ABuyerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Same Visibility-blocking defaults as AStillPartActor so the E-interaction trace hits it.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetMobility(EComponentMobility::Movable);
	SetRootComponent(MeshComponent);
}
