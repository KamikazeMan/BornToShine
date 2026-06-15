// Born To Shine - Lightweight actor for placed moonshine still parts

#include "StillPartActor.h"
#include "Components/StaticMeshComponent.h"
#include "InventoryComponent.h"

AStillPartActor::AStillPartActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetMobility(EComponentMobility::Movable);
	SetRootComponent(MeshComponent);

	// Every part carries a (usually empty) storage container; only the CinderBlockStand uses it.
	StillStorage = CreateDefaultSubobject<UInventoryComponent>(TEXT("StillStorage"));
}

void AStillPartActor::InitFromItemData(FName InPartID, UStaticMesh* InMesh)
{
	PartID = InPartID;
	if (InMesh)
	{
		MeshComponent->SetStaticMesh(InMesh);
	}
}
