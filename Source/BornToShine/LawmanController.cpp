// Born To Shine - Lawman AI controller (Increment 2: investigate, search, line-of-sight bust)

#include "LawmanController.h"
#include "LawmanCharacter.h"
#include "MoonshineCharacter_Simple.h"
#include "StillPartActor.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h" // FPathFollowingRequestResult/Result, EPathFollowingRequestResult
#include "NavigationSystem.h"
#include "EngineUtils.h" // TActorIterator
#include "GameFramework/Pawn.h"
#include "Engine/World.h"          // UWorld::LineTraceSingleByChannel
#include "CollisionQueryParams.h"  // FCollisionQueryParams / ECC_Visibility
#include "DrawDebugHelpers.h"       // DrawDebugLine / DrawDebugCone / DrawDebugPoint
#include "Components/PrimitiveComponent.h"

ALawmanController::ALawmanController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ALawmanController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	State = ELawmanState::Investigating;
	bPausing = false;
	BeginInvestigation();
}

FVector ALawmanController::GetOperationCenter() const
{
	// The heat source / operation the lawman was dispatched to. Prefer the explicit patrol target
	// set by the spawner; otherwise the local player pawn. (MP-ready: replace with the busted
	// player's heat centroid when per-player heat exists.)
	if (const ALawmanCharacter* Lawman = Cast<ALawmanCharacter>(GetPawn()))
	{
		if (Lawman->PatrolTarget.IsValid())
		{
			return Lawman->PatrolTarget->GetActorLocation();
		}
	}
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return Player->GetActorLocation();
	}
	return GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
}

void ALawmanController::BeginInvestigation()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector OpCenter = GetOperationCenter();

	// A point ~InvestigateAreaRadius from the operation (a random direction), projected to nav —
	// he arrives in the vicinity, not on the still.
	const float AngleRad = FMath::DegreesToRadians(FMath::FRandRange(0.0f, 360.0f));
	FVector Candidate = OpCenter + FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0f) * InvestigateAreaRadius;

	AreaCenter = OpCenter; // default; refined to a nav point below
	if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World))
	{
		FNavLocation Projected;
		const FVector Extent(InvestigateAreaRadius, InvestigateAreaRadius, 1000.0f);
		if (Nav->ProjectPointToNavigation(Candidate, Projected, Extent))
		{
			AreaCenter = Projected.Location;
		}
	}

	FAIMoveRequest Req;
	Req.SetGoalLocation(AreaCenter);
	Req.SetAcceptanceRadius(ArriveRadius);
	Req.SetUsePathfinding(true);

	const FPathFollowingRequestResult Result = MoveTo(Req);
	if (Result.Code == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogTemp, Warning, TEXT("Lawman: no nav path to investigation area"));
	}
	else if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		BeginSearch();
	}
	UE_LOG(LogTemp, Log, TEXT("Lawman investigating area near %s"), *AreaCenter.ToString());
}

void ALawmanController::BeginSearch()
{
	State = ELawmanState::Searching;
	SearchElapsed = 0.0f; // start the give-up clock when active searching begins
	SearchPoints.Reset();

	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;

	const int32 Count = FMath::Max(1, SearchPointCount);
	for (int32 i = 0; i < Count; ++i)
	{
		FNavLocation Point;
		if (Nav && Nav->GetRandomReachablePointInRadius(AreaCenter, SearchRadius, Point))
		{
			SearchPoints.Add(Point.Location);
		}
	}
	if (SearchPoints.Num() == 0)
	{
		SearchPoints.Add(AreaCenter); // degenerate: just stand and look around
	}

	CurrentSearchIndex = 0;
	GotoSearchPoint(CurrentSearchIndex);
}

void ALawmanController::GotoSearchPoint(int32 Index)
{
	if (SearchPoints.Num() == 0) return;
	CurrentSearchIndex = ((Index % SearchPoints.Num()) + SearchPoints.Num()) % SearchPoints.Num();

	FAIMoveRequest Req;
	Req.SetGoalLocation(SearchPoints[CurrentSearchIndex]);
	Req.SetAcceptanceRadius(ArriveRadius);
	Req.SetUsePathfinding(true);

	const FPathFollowingRequestResult Result = MoveTo(Req);
	if (Result.Code == EPathFollowingRequestResult::Failed)
	{
		// Can't reach this point — skip to the next so he doesn't get stuck.
		UE_LOG(LogTemp, Warning, TEXT("Lawman: no nav path to search point, skipping"));
		GotoSearchPoint(CurrentSearchIndex + 1);
	}
	else if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		// Already here: start the look-around pause immediately.
		bPausing = true;
		PauseTimer = SearchPointPauseTime;
		SweepBaseYaw = GetPawn() ? GetPawn()->GetActorRotation().Yaw : 0.0f;
	}
}

void ALawmanController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (State == ELawmanState::Busted) return;

	if (State == ELawmanState::Investigating)
	{
		// Arrived in the vicinity (or gave up pathing) — start actively searching.
		BeginSearch();
		return;
	}

	if (State == ELawmanState::Searching)
	{
		// Reached a search point: pause and look around (sweep the vision cone) before moving on.
		bPausing = true;
		PauseTimer = SearchPointPauseTime;
		SweepBaseYaw = GetPawn() ? GetPawn()->GetActorRotation().Yaw : 0.0f;
	}
}

void ALawmanController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State == ELawmanState::Busted || State == ELawmanState::Leaving) return;

	// Line-of-sight detection on its own cadence.
	DetectAccumulator += DeltaTime;
	if (DetectAccumulator >= DetectionInterval)
	{
		DetectAccumulator = 0.0f;
		RunDetection();
		if (State == ELawmanState::Busted) return;
	}

	// Search timeout: if he's hunted the area for too long without spotting a still, he gives up.
	if (State == ELawmanState::Searching)
	{
		SearchElapsed += DeltaTime;
		if (SearchElapsed >= SearchGiveUpTime)
		{
			GiveUpSearch();
			return;
		}
	}

	// Look-around pause at a search point: sweep the pawn's yaw so the vision cone scans the area.
	if (bPausing && State == ELawmanState::Searching)
	{
		PauseTimer -= DeltaTime;

		if (APawn* P = GetPawn())
		{
			const float Phase = (SearchPointPauseTime > 0.0f) ? (1.0f - PauseTimer / SearchPointPauseTime) : 1.0f;
			const float Offset = FMath::Sin(Phase * 2.0f * PI) * (LookAroundSweepDegrees * 0.5f);
			FRotator Rot = P->GetActorRotation();
			Rot.Yaw = SweepBaseYaw + Offset;
			P->SetActorRotation(Rot);
		}

		if (PauseTimer <= 0.0f)
		{
			bPausing = false;
			GotoSearchPoint(CurrentSearchIndex + 1); // move to the next point
		}
	}
}

bool ALawmanController::HasLineOfSightToStill(AStillPartActor* Part) const
{
	APawn* LawmanPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!IsValid(Part) || !LawmanPawn || !World) return false;

	const FVector EyeLoc = LawmanPawn->GetPawnViewLocation();
	const FVector Facing = LawmanPawn->GetActorForwardVector();
	const float HalfConeCos = FMath::Cos(FMath::DegreesToRadians(SightConeAngle * 0.5f));
	const float RangeSq = SightRange * SightRange;

	// Aim at the part's visual center.
	FVector TargetLoc = Part->GetComponentsBoundingBox(true).GetCenter();
	if (TargetLoc.ContainsNaN() || TargetLoc.IsNearlyZero())
	{
		TargetLoc = Part->GetActorLocation();
	}

	const FVector ToTarget = TargetLoc - EyeLoc;
	const float DistSq = ToTarget.SizeSquared();
	if (DistSq > RangeSq || DistSq < KINDA_SMALL_NUMBER) return false; // out of range

	const FVector Dir = ToTarget * FMath::InvSqrt(DistSq);
	if (FVector::DotProduct(Facing, Dir) < HalfConeCos) return false; // outside the vision cone

	// Clear line of sight: blocked only by world geometry (terrain/trees/rocks), not by the still
	// itself. Trace on WorldStatic (not Visibility) so movement-blocking cover — e.g. Brushify
	// trees that block WorldStatic but not Visibility — also blocks the lawman's sight. Ignore the
	// lawman and the target part so only terrain/trees/rocks between them can occlude.
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(LawmanPawn);
	Params.AddIgnoredActor(Part);
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, EyeLoc, TargetLoc, ECC_WorldStatic, Params);

	if (bDebugDrawSight)
	{
		// Green line = clear LOS (spotted), red = something occluded it. Persist ~1s to see in PIE.
		DrawDebugLine(World, EyeLoc, TargetLoc, bBlocked ? FColor::Red : FColor::Green,
			/*bPersistent*/ false, /*LifeTime*/ 1.0f, /*DepthPriority*/ 0, /*Thickness*/ 2.0f);

		if (bBlocked)
		{
			const AActor* Blocker = Hit.GetActor();
			const UPrimitiveComponent* BlockComp = Hit.GetComponent();
			UE_LOG(LogTemp, Log, TEXT("Lawman LOS to %s BLOCKED by actor '%s' (component '%s') at %s"),
				*Part->GetName(),
				Blocker ? *Blocker->GetName() : TEXT("<none>"),
				BlockComp ? *BlockComp->GetName() : TEXT("<none>"),
				*Hit.ImpactPoint.ToString());
			// Mark the blocking point.
			DrawDebugPoint(World, Hit.ImpactPoint, 12.0f, FColor::Yellow, false, 1.0f);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Lawman clear LOS to %s, distance %.0f"),
				*Part->GetName(), FMath::Sqrt(DistSq));
		}
	}

	return !bBlocked;
}

int32 ALawmanController::CountSpottedStills(APawn* StillOwner) const
{
	UWorld* World = GetWorld();
	if (!StillOwner || !World) return 0;

	// Group visible parts by their still (owning stand; a stand groups to itself) so a still with
	// several visible parts counts once.
	TSet<AStillPartActor*> SpottedStills;
	for (TActorIterator<AStillPartActor> It(World); It; ++It)
	{
		AStillPartActor* Part = *It;
		if (!IsValid(Part) || Part->OwnerPawn.Get() != StillOwner) continue;
		if (!HasLineOfSightToStill(Part)) continue;

		AStillPartActor* Still = Part->OwningStand.IsValid() ? Part->OwningStand.Get() : Part;
		SpottedStills.Add(Still);
	}
	return SpottedStills.Num();
}

void ALawmanController::RunDetection()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Visualize the vision cone (range + half-angle) from the lawman's eye.
	if (bDebugDrawSight)
	{
		if (const APawn* LawmanPawn = GetPawn())
		{
			const FVector EyeLoc = LawmanPawn->GetPawnViewLocation();
			const FVector Facing = LawmanPawn->GetActorForwardVector();
			const float HalfAngleRad = FMath::DegreesToRadians(SightConeAngle * 0.5f);
			DrawDebugCone(World, EyeLoc, Facing, SightRange, HalfAngleRad, HalfAngleRad,
				24, FColor::Cyan, /*bPersistent*/ false, /*LifeTime*/ 1.0f, /*DepthPriority*/ 0, /*Thickness*/ 1.0f);
		}
	}

	for (TActorIterator<AStillPartActor> It(World); It; ++It)
	{
		AStillPartActor* Part = *It;
		if (!IsValid(Part)) continue;

		// Only owned, placed parts can be busted — this also skips the player's ghost preview
		// (a transient AStillPartActor with no OwnerPawn).
		if (!Part->OwnerPawn.IsValid()) continue;

		if (HasLineOfSightToStill(Part))
		{
			// Spotted with clear LOS → bust immediately (no window).
			BustStill(Part);
			return;
		}
	}
}

void ALawmanController::BustStill(AStillPartActor* SeenPart)
{
	State = ELawmanState::Busted;
	StopMovement();
	bPausing = false;

	APawn* StillOwner = SeenPart ? SeenPart->OwnerPawn.Get() : nullptr;
	const FString OwnerName = StillOwner ? StillOwner->GetName() : TEXT("unknown");
	const FVector StillLoc = SeenPart ? SeenPart->GetActorLocation() : FVector::ZeroVector;

	// How many of THIS owner's stills the lawman can see right now (the spotted set). At least the
	// one that triggered the bust.
	const int32 SpottedCount = FMath::Max(1, CountSpottedStills(StillOwner));

	UE_LOG(LogTemp, Warning, TEXT("Lawman SPOTTED %d still(s) owned by %s — BUSTED (nearest at %s)"),
		SpottedCount, *OwnerName, *StillLoc.ToString());

	// Bust the still's OWNER specifically (multiplayer-ready — only that player is affected).
	if (AMoonshineCharacter_Simple* OwnerPlayer = Cast<AMoonshineCharacter_Simple>(StillOwner))
	{
		OwnerPlayer->ApplyBust(SpottedCount);
	}
}

void ALawmanController::GiveUpSearch()
{
	if (State == ELawmanState::Busted || State == ELawmanState::Leaving) return;
	State = ELawmanState::Leaving;
	StopMovement();
	bPausing = false;

	UE_LOG(LogTemp, Log, TEXT("Lawman gave up searching (no still found in %.0fs) — leaving."), SearchGiveUpTime);

	// Leave the same way a heat-drop despawn does: remove the pawn. The player's lawman tracker
	// prunes the destroyed pawn from its active list on its next check.
	if (APawn* P = GetPawn())
	{
		P->Destroy();
	}
}
