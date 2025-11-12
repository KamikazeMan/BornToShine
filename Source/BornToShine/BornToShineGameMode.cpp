// Born To Shine - Game Mode

#include "BornToShineGameMode.h"
#include "MoonshineCharacter.h"
#include "SocketManager.h"
#include "ConstructionPhaseManager.h"
#include "UObject/ConstructorHelpers.h"

ABornToShineGameMode::ABornToShineGameMode()
{
	// Set default pawn class to our character
	DefaultPawnClass = AMoonshineCharacter::StaticClass();

	SocketManager = nullptr;
	PhaseManager = nullptr;
}

void ABornToShineGameMode::BeginPlay()
{
	Super::BeginPlay();

	InitializeConstructionSystem();
}

void ABornToShineGameMode::InitializeConstructionSystem()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Spawn Socket Manager
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName("SocketManager");

	SocketManager = World->SpawnActor<ASocketManager>(ASocketManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (SocketManager)
	{
		UE_LOG(LogTemp, Log, TEXT("GameMode: Socket Manager initialized"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to spawn Socket Manager"));
	}

	// Spawn Construction Phase Manager
	SpawnParams.Name = FName("ConstructionPhaseManager");

	PhaseManager = World->SpawnActor<AConstructionPhaseManager>(AConstructionPhaseManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (PhaseManager)
	{
		UE_LOG(LogTemp, Log, TEXT("GameMode: Construction Phase Manager initialized"));
		UE_LOG(LogTemp, Log, TEXT("Current Phase: %s"), *PhaseManager->GetPhaseRequirements());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to spawn Construction Phase Manager"));
	}
}
