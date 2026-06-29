// Born To Shine - Lawman AI controller (Increment 1: MoveTo target, then idle)

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "LawmanController.generated.h"

/**
 * Drives the lawman toward the nearest active player still (else the player pawn) on the NavMesh.
 * On arrival it stops and idles — detection/raid are later increments.
 */
UCLASS()
class BORNTOSHINE_API ALawmanController : public AAIController
{
	GENERATED_BODY()

public:
	ALawmanController();

	// Prefer the nearest complete/running still as the goal; if none, head for the player pawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman")
	bool bTargetStillElsePlayer = true;

	// Distance (cm) at which the lawman counts as "arrived" and stops.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman")
	float ArriveRadius = 300.0f;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:
	// Picks the goal actor and issues an AI MoveTo. Logs on no-nav-path.
	void StartPatrol();

	// Resolve the goal: nearest active still (when bTargetStillElsePlayer) else the player pawn.
	AActor* ResolveTarget() const;

	bool bArrived = false;
};
