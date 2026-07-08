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

void ALawmanController::BuildStillClusters(APawn* StillOwner, TArray<FStillCluster>& OutClusters) const
{
	OutClusters.Reset();
	UWorld* World = GetWorld();
	if (!StillOwner || !World) return;

	// Gather this owner's placed parts (ghost previews have no OwnerPawn, so they're excluded).
	TArray<AStillPartActor*> Parts;
	for (TActorIterator<AStillPartActor> It(World); It; ++It)
	{
		AStillPartActor* Part = *It;
		if (IsValid(Part) && Part->OwnerPawn.Get() == StillOwner)
		{
			Parts.Add(Part);
		}
	}
	if (Parts.Num() == 0) return;

	// Connected-components (single-linkage) clustering by StillClusterRadius so an assembly's parts
	// become one still, and separate stills stay separate.
	const float RadiusSq = StillClusterRadius * StillClusterRadius;
	TArray<bool> Visited;
	Visited.Init(false, Parts.Num());

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		if (Visited[i]) continue;

		FStillCluster Cluster;
		Cluster.Owner = StillOwner;

		TArray<int32> Queue;
		Queue.Add(i);
		Visited[i] = true;
		while (Queue.Num() > 0)
		{
			const int32 J = Queue.Pop(EAllowShrinking::No);
			Cluster.Parts.Add(Parts[J]);
			const FVector LocJ = Parts[J]->GetActorLocation();
			for (int32 k = 0; k < Parts.Num(); ++k)
			{
				if (!Visited[k] && FVector::DistSquared(LocJ, Parts[k]->GetActorLocation()) <= RadiusSq)
				{
					Visited[k] = true;
					Queue.Add(k);
				}
			}
		}

		// Base-center: average of the LOWER parts (Z within the bottom StillBaseLowerFraction of the
		// cluster's height). This ignores tall parts (cap arm) and small high pipes, so the lawman
		// must see the main body/base — not a protruding part — to detect the still.
		float MinZ = TNumericLimits<float>::Max();
		float MaxZ = -TNumericLimits<float>::Max();
		for (const AStillPartActor* P : Cluster.Parts)
		{
			const float Z = P->GetActorLocation().Z;
			MinZ = FMath::Min(MinZ, Z);
			MaxZ = FMath::Max(MaxZ, Z);
		}
		const float Threshold = MinZ + (MaxZ - MinZ) * FMath::Clamp(StillBaseLowerFraction, 0.0f, 1.0f);

		FVector Sum = FVector::ZeroVector;
		int32 Num = 0;
		for (const AStillPartActor* P : Cluster.Parts)
		{
			const FVector L = P->GetActorLocation();
			if (L.Z <= Threshold + KINDA_SMALL_NUMBER)
			{
				Sum += L;
				++Num;
			}
		}
		Cluster.BaseCenter = (Num > 0) ? (Sum / Num) : Parts[i]->GetActorLocation();

		OutClusters.Add(MoveTemp(Cluster));
	}
}

bool ALawmanController::HasLineOfSightToPoint(const FVector& TargetLoc, const TArray<AStillPartActor*>& IgnoreParts) const
{
	APawn* LawmanPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!LawmanPawn || !World) return false;

	const FVector EyeLoc = LawmanPawn->GetPawnViewLocation();
	const FVector Facing = LawmanPawn->GetActorForwardVector();
	const float HalfConeCos = FMath::Cos(FMath::DegreesToRadians(SightConeAngle * 0.5f));
	const float RangeSq = SightRange * SightRange;

	const FVector ToTarget = TargetLoc - EyeLoc;
	const float DistSq = ToTarget.SizeSquared();
	if (DistSq > RangeSq || DistSq < KINDA_SMALL_NUMBER) return false; // out of range

	const FVector Dir = ToTarget * FMath::InvSqrt(DistSq);
	if (FVector::DotProduct(Facing, Dir) < HalfConeCos) return false; // outside the vision cone

	// Clear line of sight to the base center: blocked only by world geometry (terrain/trees/rocks),
	// not by the still's OWN parts. Trace on WorldStatic (Brushify trees block WorldStatic). Ignore
	// the lawman and every part of the cluster so only external cover can occlude.
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(LawmanPawn);
	for (AStillPartActor* P : IgnoreParts)
	{
		if (IsValid(P)) Params.AddIgnoredActor(P);
	}
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, EyeLoc, TargetLoc, ECC_WorldStatic, Params);

	if (bDebugDrawSight)
	{
		// Green line = clear LOS to the base center (detectable), red = occluded. Persist ~1s.
		DrawDebugLine(World, EyeLoc, TargetLoc, bBlocked ? FColor::Red : FColor::Green,
			/*bPersistent*/ false, /*LifeTime*/ 1.0f, /*DepthPriority*/ 0, /*Thickness*/ 2.0f);

		if (bBlocked)
		{
			const AActor* Blocker = Hit.GetActor();
			const UPrimitiveComponent* BlockComp = Hit.GetComponent();
			UE_LOG(LogTemp, Log, TEXT("Lawman LOS to still base %s BLOCKED by actor '%s' (component '%s') at %s"),
				*TargetLoc.ToString(),
				Blocker ? *Blocker->GetName() : TEXT("<none>"),
				BlockComp ? *BlockComp->GetName() : TEXT("<none>"),
				*Hit.ImpactPoint.ToString());
			DrawDebugPoint(World, Hit.ImpactPoint, 12.0f, FColor::Yellow, false, 1.0f);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Lawman clear LOS to still base %s, distance %.0f"),
				*TargetLoc.ToString(), FMath::Sqrt(DistSq));
		}
	}

	return !bBlocked;
}

int32 ALawmanController::CountSpottedStills(APawn* StillOwner) const
{
	if (!StillOwner) return 0;

	TArray<FStillCluster> Clusters;
	BuildStillClusters(StillOwner, Clusters);

	int32 Count = 0;
	for (const FStillCluster& Cluster : Clusters)
	{
		if (HasLineOfSightToPoint(Cluster.BaseCenter, Cluster.Parts))
		{
			++Count;
		}
	}
	return Count;
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

	// Gather the distinct still owners present in the world.
	TSet<APawn*> Owners;
	for (TActorIterator<AStillPartActor> It(World); It; ++It)
	{
		AStillPartActor* Part = *It;
		if (IsValid(Part) && Part->OwnerPawn.IsValid())
		{
			Owners.Add(Part->OwnerPawn.Get());
		}
	}

	// Cluster each owner's parts into stills and detect by LOS to the base-center (not any part).
	for (APawn* StillOwner : Owners)
	{
		TArray<FStillCluster> Clusters;
		BuildStillClusters(StillOwner, Clusters);
		for (const FStillCluster& Cluster : Clusters)
		{
			if (HasLineOfSightToPoint(Cluster.BaseCenter, Cluster.Parts))
			{
				// Spotted the still's main body with clear LOS → bust immediately (no window).
				BustStill(StillOwner, Cluster.BaseCenter);
				return;
			}
		}
	}
}

void ALawmanController::BustStill(APawn* StillOwner, const FVector& BaseCenter)
{
	State = ELawmanState::Busted;
	StopMovement();
	bPausing = false;

	const FString OwnerName = StillOwner ? StillOwner->GetName() : TEXT("unknown");

	// How many of THIS owner's stills (as clustered units) the lawman can see right now. At least
	// the one that triggered the bust.
	const int32 SpottedCount = FMath::Max(1, CountSpottedStills(StillOwner));

	UE_LOG(LogTemp, Warning, TEXT("Lawman SPOTTED %d still(s) owned by %s — BUSTED (main body at %s)"),
		SpottedCount, *OwnerName, *BaseCenter.ToString());

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
