// Born To Shine - Lawman foot-patrol pawn (Increment 1: spawn + approach only)

#include "LawmanCharacter.h"
#include "LawmanController.h"
#include "GameFramework/CharacterMovementComponent.h"

ALawmanCharacter::ALawmanCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// Spawned at runtime, so possess on spawn; the AI controller drives navigation.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = ALawmanController::StaticClass();

	// Face the direction of travel (CharacterMovement orients toward velocity).
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
		Move->MaxWalkSpeed = WalkSpeed;
	}
}

void ALawmanCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
	}
}
