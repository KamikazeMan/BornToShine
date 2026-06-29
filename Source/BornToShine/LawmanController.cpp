// Born To Shine - Lawman AI controller (Increment 1: MoveTo target, then idle)

#include "LawmanController.h"
#include "LawmanCharacter.h"
#include "MoonshineCharacter_Simple.h"
#include "Kismet/GameplayStatics.h"

ALawmanController::ALawmanController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ALawmanController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	bArrived = false;
	StartPatrol();
}

AActor* ALawmanController::ResolveTarget() const
{
	// An explicit target chosen by the spawner takes precedence (set before FinishSpawning).
	if (const ALawmanCharacter* Lawman = Cast<ALawmanCharacter>(GetPawn()))
	{
		if (Lawman->PatrolTarget.IsValid())
		{
			return Lawman->PatrolTarget.Get();
		}
	}

	AMoonshineCharacter_Simple* Player =
		Cast<AMoonshineCharacter_Simple>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Player) return nullptr;

	if (bTargetStillElsePlayer)
	{
		const FVector From = GetPawn() ? GetPawn()->GetActorLocation() : Player->GetActorLocation();
		if (AActor* Still = Player->FindNearestActiveStill(From))
		{
			return Still;
		}
	}
	return Player;
}

void ALawmanController::StartPatrol()
{
	AActor* Target = ResolveTarget();
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("Lawman: no target to approach"));
		return;
	}

	FAIMoveRequest Req;
	Req.SetGoalActor(Target);
	Req.SetAcceptanceRadius(ArriveRadius);
	Req.SetUsePathfinding(true);

	const FPathFollowingRequestResult Result = MoveTo(Req);
	if (Result.Code == EPathFollowingRequestResult::Failed)
	{
		// No reachable path on the NavMesh — log instead of crashing; he simply idles.
		UE_LOG(LogTemp, Warning, TEXT("Lawman: no nav path to target"));
	}
	else if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		bArrived = true;
		UE_LOG(LogTemp, Log, TEXT("Lawman reached target."));
	}
}

void ALawmanController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (bArrived) return;

	if (Result.IsSuccess())
	{
		bArrived = true;
		UE_LOG(LogTemp, Log, TEXT("Lawman reached target."));
		// Stop and idle — later increments add detection/raid here.
		StopMovement();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Lawman: move did not reach target (path blocked/aborted)"));
	}
}
