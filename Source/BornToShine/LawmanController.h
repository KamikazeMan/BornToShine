// Born To Shine - Lawman AI controller (Increment 2: investigate, search, line-of-sight bust)

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "LawmanController.generated.h"

/** What the lawman is currently doing. */
UENUM()
enum class ELawmanState : uint8
{
	Investigating, // heading to the general area of the player's operation
	Searching,     // wandering nav points in the area, pausing to look around
	Busted         // spotted a still and made the bust; idle
};

/**
 * Drives the lawman to INVESTIGATE the area of high heat, SEARCH it, and BUST a still only when he
 * gets clear line-of-sight to it (range + vision cone + unobstructed trace). He does not know the
 * still's exact location — the player can hide it behind terrain/trees/cover.
 */
UCLASS()
class BORNTOSHINE_API ALawmanController : public AAIController
{
	GENERATED_BODY()

public:
	ALawmanController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	virtual void Tick(float DeltaTime) override;

	// --- Investigation ---

	// The lawman heads to a point this far (cm) from the player's operation — NOT the exact still,
	// so he arrives in the vicinity and has to find it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman|Search")
	float InvestigateAreaRadius = 1500.0f;

	// --- Search ---

	// How many random nav points he visits while hunting the area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman|Search")
	int32 SearchPointCount = 5;

	// Radius (cm) around the investigation point that search points are drawn from.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman|Search")
	float SearchRadius = 1500.0f;

	// Distance (cm) at which a search/investigation move counts as "arrived".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman|Search")
	float ArriveRadius = 200.0f;

	// Seconds spent "looking around" at each search point before moving on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman|Search")
	float SearchPointPauseTime = 2.5f;

	// Total yaw sweep (deg) while looking around at a search point (turns the vision cone).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman|Search")
	float LookAroundSweepDegrees = 140.0f;

	// --- Detection (line of sight) ---

	// Max distance (cm) a still can be spotted.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman|Sight")
	float SightRange = 2000.0f;

	// Full vision-cone angle (deg); a still outside this cone (behind him) is not seen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman|Sight")
	float SightConeAngle = 90.0f;

	// How often (s) the line-of-sight check runs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lawman|Sight")
	float DetectionInterval = 0.2f;

protected:
	void BeginInvestigation();
	void BeginSearch();
	void GotoSearchPoint(int32 Index);

	// The core mechanic: returns true (and busts) if a still is within range+cone with clear LOS.
	void RunDetection();

	// Fires the bust on the still's OwnerPawn (multiplayer-ready).
	void BustStill(class AStillPartActor* SeenPart);

	// The player's operation center (heat source) this lawman was sent to investigate.
	FVector GetOperationCenter() const;

	ELawmanState State = ELawmanState::Investigating;

	FVector AreaCenter = FVector::ZeroVector;
	TArray<FVector> SearchPoints;
	int32 CurrentSearchIndex = 0;

	bool bPausing = false;
	float PauseTimer = 0.0f;
	float SweepBaseYaw = 0.0f;

	float DetectAccumulator = 0.0f;
};
