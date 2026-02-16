// Born To Shine - Socket Compatibility Manager

#include "SocketManager.h"
#include "SnapRuleTable.h"
#include "BuildablePiece.h"
#include "Kismet/KismetMathLibrary.h"

ASocketManager* ASocketManager::Instance = nullptr;

ASocketManager::ASocketManager()
{
	PrimaryActorTick.bCanEverTick = false;
	Instance = this;
}

void ASocketManager::BeginPlay()
{
	Super::BeginPlay();
	InitializeCompatibilityRules();
}

void ASocketManager::InitializeCompatibilityRules()
{
	CompatibilityRules.Empty();

	CreateFoundationRules();
	CreateRimBoardRules();
	CreateJoistRules();
	CreatePlywoodRules();
	CreateBottomPlateRules();
	CreateWallStudRules();
	CreateCornerPostRules();
	CreateDoorFrameRules();
	CreateTopPlateRules();
	CreateDoubleTopPlateRules();

	UE_LOG(LogTemp, Log, TEXT("SocketManager: Initialized %d compatibility rules"), CompatibilityRules.Num());
}

void ASocketManager::CreateFoundationRules()
{
	// Foundation SIDE sockets are the ONLY connection points for rim boards
	// Side sockets are at the edges, centered on the groove (Y=0 or X=0)
	// This ensures rim boards sit perfectly centered in the foundation groove
	FSocketCompatibilityRule FoundationSideRule;
	FoundationSideRule.SourceSocketType = EConstructionSocketType::Foundation_Side;
	FoundationSideRule.CompatibleSocketTypes.Add(EConstructionSocketType::RimBoard_Bottom_End);
	FoundationSideRule.RequiredPhase = EConstructionPhase::FloorFrame;
	FoundationSideRule.SnapDistance = 100.0f;
	FoundationSideRule.bCheckAlignment = false; // Foundation groove accepts boards at any yaw
	FoundationSideRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(FoundationSideRule);

	// NOTE: Foundation_Corner sockets (at diagonal corners) are NOT used for rim boards
	// because they would place boards off-center from the groove.
}

void ASocketManager::CreateRimBoardRules()
{
	// Rim board bottom ends connect to foundation SIDE sockets (primary)
	// Side sockets ensure rim boards are centered on the foundation groove
	FSocketCompatibilityRule RimBottomRule;
	RimBottomRule.SourceSocketType = EConstructionSocketType::RimBoard_Bottom_End;
	RimBottomRule.CompatibleSocketTypes.Add(EConstructionSocketType::Foundation_Side); // Primary - centered on groove
	RimBottomRule.RequiredPhase = EConstructionPhase::FloorFrame;
	RimBottomRule.SnapDistance = 100.0f;
	RimBottomRule.bCheckAlignment = false; // Foundation groove accepts boards at any yaw
	RimBottomRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(RimBottomRule);

	// Rim board top face accepts joist ends (during floor framing)
	FSocketCompatibilityRule RimTopRule;
	RimTopRule.SourceSocketType = EConstructionSocketType::RimBoard_Top_Face;
	RimTopRule.CompatibleSocketTypes.Add(EConstructionSocketType::Joist_End);
	RimTopRule.RequiredPhase = EConstructionPhase::FloorFrame;
	RimTopRule.SnapDistance = 40.0f;
	RimTopRule.bCheckAlignment = true;
	RimTopRule.MaxAlignmentAngle = 5.0f;
	CompatibilityRules.Add(RimTopRule);

	// Rim board top face also accepts plywood edges (during sheathing)
	FSocketCompatibilityRule RimTopPlywoodRule;
	RimTopPlywoodRule.SourceSocketType = EConstructionSocketType::RimBoard_Top_Face;
	RimTopPlywoodRule.CompatibleSocketTypes.Add(EConstructionSocketType::Plywood_Edge);
	RimTopPlywoodRule.RequiredPhase = EConstructionPhase::FloorSheathing;
	RimTopPlywoodRule.SnapDistance = 30.0f;
	RimTopPlywoodRule.bCheckAlignment = false; // Plywood just needs to be on top
	RimTopPlywoodRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(RimTopPlywoodRule);

	// Rim board side face accepts perpendicular joist ends
	FSocketCompatibilityRule RimSideRule;
	RimSideRule.SourceSocketType = EConstructionSocketType::RimBoard_Side_Face;
	RimSideRule.CompatibleSocketTypes.Add(EConstructionSocketType::Joist_End);
	RimSideRule.RequiredPhase = EConstructionPhase::FloorFrame;
	RimSideRule.SnapDistance = 40.0f;
	RimSideRule.bCheckAlignment = true;
	RimSideRule.MaxAlignmentAngle = 5.0f;
	CompatibilityRules.Add(RimSideRule);

	// Rim board end corners connect to other rim board end corners
	FSocketCompatibilityRule RimCornerRule;
	RimCornerRule.SourceSocketType = EConstructionSocketType::RimBoard_End_Corner;
	RimCornerRule.CompatibleSocketTypes.Add(EConstructionSocketType::RimBoard_End_Corner);
	RimCornerRule.RequiredPhase = EConstructionPhase::FloorFrame;
	RimCornerRule.SnapDistance = 100.0f; // Increased from 50cm to help 4th board find corners when closing rectangle
	RimCornerRule.bCheckAlignment = true; // Enable alignment checking for precise 90-degree corners
	RimCornerRule.MaxAlignmentAngle = 95.0f; // Allow 90-degree corners with 5-degree tolerance
	CompatibilityRules.Add(RimCornerRule);
}

void ASocketManager::CreateJoistRules()
{
	// Joist ends connect to rim board faces
	FSocketCompatibilityRule JoistEndRule;
	JoistEndRule.SourceSocketType = EConstructionSocketType::Joist_End;
	JoistEndRule.CompatibleSocketTypes.Add(EConstructionSocketType::RimBoard_Top_Face);
	JoistEndRule.CompatibleSocketTypes.Add(EConstructionSocketType::RimBoard_Side_Face);
	JoistEndRule.RequiredPhase = EConstructionPhase::FloorFrame;
	JoistEndRule.SnapDistance = 40.0f;
	JoistEndRule.bCheckAlignment = true;
	JoistEndRule.MaxAlignmentAngle = 5.0f;
	CompatibilityRules.Add(JoistEndRule);

	// Joist top face accepts plywood edges
	FSocketCompatibilityRule JoistTopRule;
	JoistTopRule.SourceSocketType = EConstructionSocketType::Joist_Top_Face;
	JoistTopRule.CompatibleSocketTypes.Add(EConstructionSocketType::Plywood_Edge);
	JoistTopRule.RequiredPhase = EConstructionPhase::FloorSheathing;
	JoistTopRule.SnapDistance = 30.0f;
	JoistTopRule.bCheckAlignment = false; // Plywood just needs to be on top
	JoistTopRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(JoistTopRule);
}

void ASocketManager::CreatePlywoodRules()
{
	// Plywood edges snap to joist tops, rim board tops, and other plywood edges
	// NOTE: Plywood does NOT snap to EndCorner sockets — only to TopFace sockets
	FSocketCompatibilityRule PlywoodEdgeRule;
	PlywoodEdgeRule.SourceSocketType = EConstructionSocketType::Plywood_Edge;
	PlywoodEdgeRule.CompatibleSocketTypes.Add(EConstructionSocketType::Joist_Top_Face);
	PlywoodEdgeRule.CompatibleSocketTypes.Add(EConstructionSocketType::RimBoard_Top_Face); // Rim board perimeter
	PlywoodEdgeRule.CompatibleSocketTypes.Add(EConstructionSocketType::Plywood_Edge); // Sheet-to-sheet
	PlywoodEdgeRule.RequiredPhase = EConstructionPhase::FloorSheathing;
	PlywoodEdgeRule.SnapDistance = 50.0f;
	PlywoodEdgeRule.bCheckAlignment = false;
	PlywoodEdgeRule.MaxAlignmentAngle = 10.0f;
	CompatibilityRules.Add(PlywoodEdgeRule);
}

void ASocketManager::CreateBottomPlateRules()
{
	// Bottom plate bottom sockets snap to rim board top face (plate sits above plywood, above rim board)
	FSocketCompatibilityRule PlateBottomRule;
	PlateBottomRule.SourceSocketType = EConstructionSocketType::BottomPlate_Bottom;
	PlateBottomRule.CompatibleSocketTypes.Add(EConstructionSocketType::RimBoard_Top_Face);
	PlateBottomRule.RequiredPhase = EConstructionPhase::WallFrame;
	PlateBottomRule.SnapDistance = 50.0f;
	PlateBottomRule.bCheckAlignment = false; // Plate just needs to be on top
	PlateBottomRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(PlateBottomRule);

	// Rim board top face also accepts bottom plate bottom sockets
	FSocketCompatibilityRule RimTopPlateRule;
	RimTopPlateRule.SourceSocketType = EConstructionSocketType::RimBoard_Top_Face;
	RimTopPlateRule.CompatibleSocketTypes.Add(EConstructionSocketType::BottomPlate_Bottom);
	RimTopPlateRule.RequiredPhase = EConstructionPhase::WallFrame;
	RimTopPlateRule.SnapDistance = 50.0f;
	RimTopPlateRule.bCheckAlignment = false;
	RimTopPlateRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(RimTopPlateRule);

	// Bottom plate end sockets connect to other bottom plate end sockets (corners/inline)
	FSocketCompatibilityRule PlateEndRule;
	PlateEndRule.SourceSocketType = EConstructionSocketType::BottomPlate_End;
	PlateEndRule.CompatibleSocketTypes.Add(EConstructionSocketType::BottomPlate_End);
	PlateEndRule.RequiredPhase = EConstructionPhase::WallFrame;
	PlateEndRule.SnapDistance = 60.0f;
	PlateEndRule.bCheckAlignment = true;
	PlateEndRule.MaxAlignmentAngle = 95.0f; // Allow 90-degree corners with tolerance
	CompatibilityRules.Add(PlateEndRule);

	UE_LOG(LogTemp, Log, TEXT("SocketManager: Added bottom plate compatibility rules"));
}

void ASocketManager::CreateWallStudRules()
{
	// Wall stud bottom socket snaps to Wall_Bottom_Plate sockets (on bottom plate top face).
	// Snap distance is large (250cm) because the stud's bottom socket is ~117cm below
	// the actor origin (half the stud height). When the preview is near the plate,
	// the socket world position is far underground — needs a generous search radius.
	FSocketCompatibilityRule StudBottomRule;
	StudBottomRule.SourceSocketType = EConstructionSocketType::Wall_Stud_Bottom;
	StudBottomRule.CompatibleSocketTypes.Add(EConstructionSocketType::Wall_Bottom_Plate);
	StudBottomRule.RequiredPhase = EConstructionPhase::WallFrame;
	StudBottomRule.SnapDistance = 250.0f;
	StudBottomRule.bCheckAlignment = false;
	StudBottomRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(StudBottomRule);

	// Wall_Bottom_Plate sockets accept Wall_Stud_Bottom (reverse direction)
	FSocketCompatibilityRule PlateTopStudRule;
	PlateTopStudRule.SourceSocketType = EConstructionSocketType::Wall_Bottom_Plate;
	PlateTopStudRule.CompatibleSocketTypes.Add(EConstructionSocketType::Wall_Stud_Bottom);
	PlateTopStudRule.RequiredPhase = EConstructionPhase::WallFrame;
	PlateTopStudRule.SnapDistance = 250.0f;
	PlateTopStudRule.bCheckAlignment = false;
	PlateTopStudRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(PlateTopStudRule);

	UE_LOG(LogTemp, Log, TEXT("SocketManager: Added wall stud compatibility rules"));
}

void ASocketManager::CreateCornerPostRules()
{
	// Corner post bottom snaps to CornerPost_Seat sockets on plate ends.
	// Same generous snap distance as wall studs (socket is ~117cm below actor origin).
	FSocketCompatibilityRule PostBottomRule;
	PostBottomRule.SourceSocketType = EConstructionSocketType::CornerPost_Bottom;
	PostBottomRule.CompatibleSocketTypes.Add(EConstructionSocketType::CornerPost_Seat);
	PostBottomRule.RequiredPhase = EConstructionPhase::WallFrame;
	PostBottomRule.SnapDistance = 250.0f;
	PostBottomRule.bCheckAlignment = false;
	PostBottomRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(PostBottomRule);

	// Reverse: CornerPost_Seat accepts CornerPost_Bottom
	FSocketCompatibilityRule SeatRule;
	SeatRule.SourceSocketType = EConstructionSocketType::CornerPost_Seat;
	SeatRule.CompatibleSocketTypes.Add(EConstructionSocketType::CornerPost_Bottom);
	SeatRule.RequiredPhase = EConstructionPhase::WallFrame;
	SeatRule.SnapDistance = 250.0f;
	SeatRule.bCheckAlignment = false;
	SeatRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(SeatRule);

	UE_LOG(LogTemp, Log, TEXT("SocketManager: Added corner post compatibility rules"));
}

void ASocketManager::CreateDoorFrameRules()
{
	// Door frame bottom socket snaps to Wall_Bottom_Plate sockets (same as wall studs).
	// Same generous snap distance — door frame bottom socket is ~117cm below actor origin.
	FSocketCompatibilityRule DoorBottomRule;
	DoorBottomRule.SourceSocketType = EConstructionSocketType::DoorFrame_Bottom;
	DoorBottomRule.CompatibleSocketTypes.Add(EConstructionSocketType::Wall_Bottom_Plate);
	DoorBottomRule.RequiredPhase = EConstructionPhase::WallFrame;
	DoorBottomRule.SnapDistance = 250.0f;
	DoorBottomRule.bCheckAlignment = false;
	DoorBottomRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(DoorBottomRule);

	// Reverse: Wall_Bottom_Plate accepts DoorFrame_Bottom
	FSocketCompatibilityRule PlateTopDoorRule;
	PlateTopDoorRule.SourceSocketType = EConstructionSocketType::Wall_Bottom_Plate;
	PlateTopDoorRule.CompatibleSocketTypes.Add(EConstructionSocketType::DoorFrame_Bottom);
	PlateTopDoorRule.RequiredPhase = EConstructionPhase::WallFrame;
	PlateTopDoorRule.SnapDistance = 250.0f;
	PlateTopDoorRule.bCheckAlignment = false;
	PlateTopDoorRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(PlateTopDoorRule);

	UE_LOG(LogTemp, Log, TEXT("SocketManager: Added door frame compatibility rules"));
}

bool ASocketManager::AreSocketsCompatible(EConstructionSocketType SourceSocket, EConstructionSocketType TargetSocket, EConstructionPhase CurrentPhase) const
{
	for (const FSocketCompatibilityRule& Rule : CompatibilityRules)
	{
		if (Rule.SourceSocketType == SourceSocket)
		{
			// Free building mode - phase check disabled
			// Socket compatibility is now purely based on socket types

			// Check if target socket is in compatible list
			if (Rule.CompatibleSocketTypes.Contains(TargetSocket))
			{
				return true;
			}
		}
	}
	return false;
}

FSocketCompatibilityRule ASocketManager::GetCompatibilityRule(EConstructionSocketType SocketType) const
{
	for (const FSocketCompatibilityRule& Rule : CompatibilityRules)
	{
		if (Rule.SourceSocketType == SocketType)
		{
			return Rule;
		}
	}
	return FSocketCompatibilityRule(); // Return default if not found
}

bool ASocketManager::FindBestSnapPoint(
	const FConstructionSocket& SourceSocket,
	const FVector& WorldLocation,
	const FRotator& WorldRotation,
	TArray<ABuildablePiece*> NearbyPieces,
	EConstructionPhase CurrentPhase,
	FVector& OutSnapLocation,
	FRotator& OutSnapRotation,
	ABuildablePiece*& OutTargetPiece,
	FName& OutTargetSocketName)
{
	float BestScore = -1.0f;
	bool bFoundValidSnap = false;

	FSocketCompatibilityRule Rule = GetCompatibilityRule(SourceSocket.SocketType);

	// Debug: Check if we have any nearby pieces
	if (NearbyPieces.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindBestSnapPoint: No nearby pieces found for socket %s"), *SourceSocket.SocketName.ToString());
		return false;
	}

	for (ABuildablePiece* Piece : NearbyPieces)
	{
		if (!Piece) continue;

		// Get all sockets from this piece
		TArray<FConstructionSocket> TargetSockets = Piece->GetAllSockets();

		for (const FConstructionSocket& TargetSocket : TargetSockets)
		{
			// Skip occupied sockets UNLESS:
			// - Source is plywood/bottom plate targeting framing top faces
			// - Target is Wall_Bottom_Plate (continuous surface, multiple pieces sit on it)
			// - Source is TopPlate targeting stud/post/door frame tops (plate spans multiple studs)
			if (TargetSocket.bIsOccupied)
			{
				bool bPlywoodSource = (SourceSocket.SocketType == EConstructionSocketType::Plywood_Corner ||
				                       SourceSocket.SocketType == EConstructionSocketType::Plywood_Edge);
				bool bBottomPlateSource = (SourceSocket.SocketType == EConstructionSocketType::BottomPlate_Bottom);
				bool bFramingTarget = (TargetSocket.SocketType == EConstructionSocketType::RimBoard_Top_Face ||
				                       TargetSocket.SocketType == EConstructionSocketType::Joist_Top_Face);
				bool bWallPlateTarget = (TargetSocket.SocketType == EConstructionSocketType::Wall_Bottom_Plate);
				bool bTopPlateSource = (SourceSocket.SocketType == EConstructionSocketType::TopPlate_Bottom);
				bool bStudPostTopTarget = (TargetSocket.SocketType == EConstructionSocketType::Wall_Stud_Top ||
				                           TargetSocket.SocketType == EConstructionSocketType::CornerPost_Top ||
				                           TargetSocket.SocketType == EConstructionSocketType::DoorFrame_Top);
				if (!((bPlywoodSource || bBottomPlateSource) && bFramingTarget) &&
				    !bWallPlateTarget &&
				    !(bTopPlateSource && bStudPostTopTarget))
				{
					continue;
				}
			}

			// Check compatibility
			if (!AreSocketsCompatible(SourceSocket.SocketType, TargetSocket.SocketType, CurrentPhase)) continue;

			// Get world space position of target socket
			FVector TargetWorldLocation = Piece->GetActorTransform().TransformPosition(TargetSocket.LocalPosition);
			FRotator TargetWorldRotation = Piece->GetActorRotation() + TargetSocket.LocalRotation;

			// Check if within snap distance
			float Distance = FVector::Dist(WorldLocation, TargetWorldLocation);

			// Debug logging disabled (runs every frame - too spammy)
			// Only log successful snaps, not every check

			if (Distance > Rule.SnapDistance) continue;

			// Check alignment if required
			if (Rule.bCheckAlignment)
			{
				// For rim-to-rim corners, SKIP angle check - auto-rotation in FindSnapPoint handles it
				if (SourceSocket.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
					TargetSocket.SocketType == EConstructionSocketType::RimBoard_End_Corner)
				{
					// Skip angle validation for rim corners - auto-rotation handles it
				}
				else
				{
					// Standard alignment check for other socket types
					if (!CheckSocketAlignment(WorldLocation, WorldRotation, TargetWorldLocation, TargetWorldRotation, Rule.MaxAlignmentAngle))
					{
						continue;
					}
				}
			}

			// Calculate snap score (closer and better aligned = higher score)
			float Score = CalculateSnapScore(WorldLocation, TargetWorldLocation, WorldRotation, TargetWorldRotation);

			// RULE-BASED PRIORITY SCORING
			// Priority is the DOMINANT factor — a higher-priority snap ALWAYS beats
			// a lower-priority snap regardless of distance. Priority is multiplied by
			// a large weight so it can never be outweighed by distance/alignment scores.
			// Example: framing snap (Priority 600) at 50cm always beats
			//          sheet-to-sheet (Priority 500) at 0cm.
			if (ASnapRuleTable::Instance)
			{
				FSnapRule SnapRule;
				if (ASnapRuleTable::Instance->GetSnapRule(
					SourceSocket.SocketType, TargetSocket.SocketType,
					SourceSocket.SocketName, TargetSocket.SocketName, SnapRule))
				{
					Score += SnapRule.Priority * 100.0f;

					// Small alignment bonus for corner snaps
					if (SnapRule.ConnectionType == ESnapConnectionType::Corner_90 ||
						SnapRule.ConnectionType == ESnapConnectionType::Inline_0)
					{
						float AngleDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(WorldRotation.Yaw, TargetWorldRotation.Yaw));
						float AngleTo90 = FMath::Abs(AngleDiff - 90.0f);
						float AngleTo0 = FMath::Min(FMath::Abs(AngleDiff), FMath::Abs(AngleDiff - 180.0f));
						float AlignmentBonus = 20.0f / (FMath::Min(AngleTo90, AngleTo0) + 1.0f);
						Score += AlignmentBonus;
					}
				}
			}
			else if (SourceSocket.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
				TargetSocket.SocketType == EConstructionSocketType::RimBoard_End_Corner)
			{
				// FALLBACK: Hardcoded scoring when SnapRuleTable is not present
				bool bSourceIsLeft = SourceSocket.SocketName.ToString().Contains("Left");
				bool bTargetIsLeft = TargetSocket.SocketName.ToString().Contains("Left");
				bool bSameSide = (bSourceIsLeft == bTargetIsLeft);  // Left-Left or Right-Right = CORNER

				if (bSameSide)
				{
					// CORNER JOINT (Left-Left or Right-Right): HIGH priority
					Score += 150.0f;
				}
				else
				{
					// INLINE EXTENSION (Left-Right or Right-Left): LOWER priority
					Score += 80.0f;
				}

				// Small alignment bonus
				float AngleDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(WorldRotation.Yaw, TargetWorldRotation.Yaw));
				float AngleTo90 = FMath::Abs(AngleDiff - 90.0f);
				float AngleTo0 = FMath::Min(FMath::Abs(AngleDiff), FMath::Abs(AngleDiff - 180.0f));
				float AlignmentBonus = 20.0f / (FMath::Min(AngleTo90, AngleTo0) + 1.0f);
				Score += AlignmentBonus;
			}

			if (Score > BestScore)
			{
				BestScore = Score;

				// CRITICAL FIX: Return the socket locations, NOT actor locations
				// The calling code (BuildablePiece::FindSnapPoint) will calculate actor position
				OutSnapLocation = TargetWorldLocation;
				OutSnapRotation = TargetWorldRotation;
				OutTargetPiece = Piece;
				OutTargetSocketName = TargetSocket.SocketName;
				bFoundValidSnap = true;
			}
		}
	}

	return bFoundValidSnap;
}

bool ASocketManager::IsPlacementValid(const FVector& Location, const FRotator& Rotation, ABuildablePiece* Piece, EConstructionPhase CurrentPhase)
{
	if (!Piece) return false;

	// TODO: Add more validation checks:
	// - Gravity check (piece must be supported)
	// - Collision check (no overlap with other pieces)
	// - Phase check (can this piece be placed in current phase?)

	return true;
}

bool ASocketManager::CheckSocketAlignment(
	const FVector& SourceLocation,
	const FRotator& SourceRotation,
	const FVector& TargetLocation,
	const FRotator& TargetRotation,
	float MaxAngleDiff) const
{
	// Calculate angle difference between rotations
	FRotator DeltaRotation = (TargetRotation - SourceRotation).GetNormalized();

	float PitchDiff = FMath::Abs(DeltaRotation.Pitch);
	float YawDiff = FMath::Abs(DeltaRotation.Yaw);
	float RollDiff = FMath::Abs(DeltaRotation.Roll);

	// Check if all angles are within tolerance
	return (PitchDiff <= MaxAngleDiff && YawDiff <= MaxAngleDiff && RollDiff <= MaxAngleDiff);
}

float ASocketManager::CalculateSnapScore(
	const FVector& SourceLocation,
	const FVector& TargetLocation,
	const FRotator& SourceRotation,
	const FRotator& TargetRotation) const
{
	// Closer distance = higher score
	float Distance = FVector::Dist(SourceLocation, TargetLocation);
	float DistanceScore = 1000.0f / (Distance + 1.0f); // Avoid division by zero

	// Better alignment = higher score
	FRotator DeltaRotation = (TargetRotation - SourceRotation).GetNormalized();
	float AngleDiff = FMath::Abs(DeltaRotation.Yaw) + FMath::Abs(DeltaRotation.Pitch) + FMath::Abs(DeltaRotation.Roll);
	float AlignmentScore = 100.0f / (AngleDiff + 1.0f);

	return DistanceScore + AlignmentScore;
}

void ASocketManager::CreateTopPlateRules()
{
	// TopPlate bottom sockets snap to wall stud top, corner post top, and other top plate top faces
	FSocketCompatibilityRule TopPlateBottomRule;
	TopPlateBottomRule.SourceSocketType = EConstructionSocketType::TopPlate_Bottom;
	TopPlateBottomRule.CompatibleSocketTypes.Add(EConstructionSocketType::Wall_Stud_Top);
	TopPlateBottomRule.CompatibleSocketTypes.Add(EConstructionSocketType::CornerPost_Top);
	TopPlateBottomRule.CompatibleSocketTypes.Add(EConstructionSocketType::DoorFrame_Top);
	TopPlateBottomRule.CompatibleSocketTypes.Add(EConstructionSocketType::TopPlate_Top);
	TopPlateBottomRule.RequiredPhase = EConstructionPhase::WallFrame;
	TopPlateBottomRule.SnapDistance = 250.0f;
	TopPlateBottomRule.bCheckAlignment = false;
	TopPlateBottomRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(TopPlateBottomRule);

	// Reverse: stud/post tops accept TopPlate bottom
	FSocketCompatibilityRule StudTopRule;
	StudTopRule.SourceSocketType = EConstructionSocketType::Wall_Stud_Top;
	StudTopRule.CompatibleSocketTypes.Add(EConstructionSocketType::TopPlate_Bottom);
	StudTopRule.RequiredPhase = EConstructionPhase::WallFrame;
	StudTopRule.SnapDistance = 250.0f;
	StudTopRule.bCheckAlignment = false;
	StudTopRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(StudTopRule);

	FSocketCompatibilityRule PostTopRule;
	PostTopRule.SourceSocketType = EConstructionSocketType::CornerPost_Top;
	PostTopRule.CompatibleSocketTypes.Add(EConstructionSocketType::TopPlate_Bottom);
	PostTopRule.RequiredPhase = EConstructionPhase::WallFrame;
	PostTopRule.SnapDistance = 250.0f;
	PostTopRule.bCheckAlignment = false;
	PostTopRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(PostTopRule);

	FSocketCompatibilityRule DoorTopRule;
	DoorTopRule.SourceSocketType = EConstructionSocketType::DoorFrame_Top;
	DoorTopRule.CompatibleSocketTypes.Add(EConstructionSocketType::TopPlate_Bottom);
	DoorTopRule.RequiredPhase = EConstructionPhase::WallFrame;
	DoorTopRule.SnapDistance = 250.0f;
	DoorTopRule.bCheckAlignment = false;
	DoorTopRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(DoorTopRule);

	// Top plate end-to-end (corners)
	FSocketCompatibilityRule TopPlateEndRule;
	TopPlateEndRule.SourceSocketType = EConstructionSocketType::TopPlate_End;
	TopPlateEndRule.CompatibleSocketTypes.Add(EConstructionSocketType::TopPlate_End);
	TopPlateEndRule.RequiredPhase = EConstructionPhase::WallFrame;
	TopPlateEndRule.SnapDistance = 60.0f;
	TopPlateEndRule.bCheckAlignment = true;
	TopPlateEndRule.MaxAlignmentAngle = 95.0f;
	CompatibilityRules.Add(TopPlateEndRule);

	UE_LOG(LogTemp, Log, TEXT("SocketManager: Added top plate compatibility rules"));
}

void ASocketManager::CreateDoubleTopPlateRules()
{
	// DoubleTopPlate bottom sockets snap to TopPlate top face
	FSocketCompatibilityRule DblBottomRule;
	DblBottomRule.SourceSocketType = EConstructionSocketType::DoubleTopPlate_Bottom;
	DblBottomRule.CompatibleSocketTypes.Add(EConstructionSocketType::TopPlate_Top);
	DblBottomRule.RequiredPhase = EConstructionPhase::WallFrame;
	DblBottomRule.SnapDistance = 50.0f;
	DblBottomRule.bCheckAlignment = false;
	DblBottomRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(DblBottomRule);

	// Reverse: TopPlate top face accepts DoubleTopPlate bottom and TopPlate bottom (stacking)
	FSocketCompatibilityRule TopFaceRule;
	TopFaceRule.SourceSocketType = EConstructionSocketType::TopPlate_Top;
	TopFaceRule.CompatibleSocketTypes.Add(EConstructionSocketType::DoubleTopPlate_Bottom);
	TopFaceRule.CompatibleSocketTypes.Add(EConstructionSocketType::TopPlate_Bottom);
	TopFaceRule.RequiredPhase = EConstructionPhase::WallFrame;
	TopFaceRule.SnapDistance = 50.0f;
	TopFaceRule.bCheckAlignment = false;
	TopFaceRule.MaxAlignmentAngle = 15.0f;
	CompatibilityRules.Add(TopFaceRule);

	// DoubleTopPlate end-to-end
	FSocketCompatibilityRule DblEndRule;
	DblEndRule.SourceSocketType = EConstructionSocketType::DoubleTopPlate_End;
	DblEndRule.CompatibleSocketTypes.Add(EConstructionSocketType::DoubleTopPlate_End);
	DblEndRule.RequiredPhase = EConstructionPhase::WallFrame;
	DblEndRule.SnapDistance = 60.0f;
	DblEndRule.bCheckAlignment = true;
	DblEndRule.MaxAlignmentAngle = 95.0f;
	CompatibilityRules.Add(DblEndRule);

	UE_LOG(LogTemp, Log, TEXT("SocketManager: Added double top plate compatibility rules"));
}
