// Born To Shine - Lawman foot-patrol pawn (Increment 1: spawn + approach only)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LawmanCharacter.generated.h"

/**
 * A lawman who spawns at high heat and walks toward the player's still on the NavMesh.
 * Increment 1 is PRESENCE ONLY: no detection, catching, or raid. Assign a skeletal mesh
 * and anim blueprint in a BP subclass (BP_Lawman); the C++ handles movement + navigation.
 */
UCLASS()
class BORNTOSHINE_API ALawmanCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ALawmanCharacter();

	virtual void BeginPlay() override;

	// Ground walk speed (cm/s) pushed into CharacterMovement on spawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman")
	float WalkSpeed = 300.0f;

	// What this lawman is walking toward (a still actor, or the player). Set by the spawner
	// before FinishSpawning so the AI controller can read it on possession.
	UPROPERTY(BlueprintReadOnly, Category = "Lawman")
	TWeakObjectPtr<AActor> PatrolTarget;
};
