// Born To Shine - Physical world pickup

#include "WorldPickupActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "MoonshineCharacter_Simple.h"

AWorldPickupActor::AWorldPickupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetMobility(EComponentMobility::Movable);
	// Block Visibility (so the interaction sphere-sweep hits it) and world (so it settles), but only
	// OVERLAP the pawn so the player can't shove it around or get blocked by it. Physics starts in Init.
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	MeshComponent->SetSimulatePhysics(false);
	SetRootComponent(MeshComponent);

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(MeshComponent);
	PickupSphere->SetSphereRadius(120.0f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);
}

void AWorldPickupActor::BeginPlay()
{
	Super::BeginPlay();
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AWorldPickupActor::OnSphereBeginOverlap);
}

void AWorldPickupActor::Init(FName InItemId, int32 InCount, UStaticMesh* InMesh, bool bSimulate)
{
	ItemId = InItemId;
	Count = InCount;
	if (InMesh)
	{
		MeshComponent->SetStaticMesh(InMesh);
	}
	MeshComponent->SetSimulatePhysics(bSimulate);
}

void AWorldPickupActor::TossForward(const FVector& Direction, float Strength)
{
	if (MeshComponent->IsSimulatingPhysics())
	{
		MeshComponent->AddImpulse(Direction.GetSafeNormal() * Strength, NAME_None, true);
	}
}

void AWorldPickupActor::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	// The character decides whether walk-over auto-pickup is enabled.
	if (AMoonshineCharacter_Simple* C = Cast<AMoonshineCharacter_Simple>(OtherActor))
	{
		C->NotifyPickupOverlap(this);
	}
}
