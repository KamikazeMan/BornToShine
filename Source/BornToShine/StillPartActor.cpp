// Born To Shine - Lightweight actor for placed moonshine still parts

#include "StillPartActor.h"
#include "Components/StaticMeshComponent.h"

AStillPartActor::AStillPartActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetMobility(EComponentMobility::Movable);
	SetRootComponent(MeshComponent);
}

int32 AStillPartActor::GetStored(FName Ingredient) const
{
	if (Ingredient == FName(TEXT("Water")))    return StoredWater;
	if (Ingredient == FName(TEXT("Mash")))     return StoredMash;
	if (Ingredient == FName(TEXT("Firewood"))) return StoredFirewood;
	return 0;
}

void AStillPartActor::AddStored(FName Ingredient, int32 Delta)
{
	if (Ingredient == FName(TEXT("Water")))    StoredWater    = FMath::Max(0, StoredWater + Delta);
	else if (Ingredient == FName(TEXT("Mash")))     StoredMash     = FMath::Max(0, StoredMash + Delta);
	else if (Ingredient == FName(TEXT("Firewood"))) StoredFirewood = FMath::Max(0, StoredFirewood + Delta);
}

void AStillPartActor::InitFromItemData(FName InPartID, UStaticMesh* InMesh)
{
	PartID = InPartID;
	if (InMesh)
	{
		MeshComponent->SetStaticMesh(InMesh);
	}
}
