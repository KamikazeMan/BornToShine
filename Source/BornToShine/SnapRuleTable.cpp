// Born To Shine - Snap Rule Lookup Table Implementation

#include "SnapRuleTable.h"

ASnapRuleTable* ASnapRuleTable::Instance = nullptr;

ASnapRuleTable::ASnapRuleTable()
{
	PrimaryActorTick.bCanEverTick = false;
	Instance = this;
}

void ASnapRuleTable::BeginPlay()
{
	Super::BeginPlay();
	InitializeRules();
	UE_LOG(LogTemp, Log, TEXT("SnapRuleTable: Initialized with %d rules"), RuleTable.Num());
}

void ASnapRuleTable::InitializeRules()
{
	RuleTable.Empty();

	// RimBoard half-width: 1.5" actual = 3.81 cm, so half = 1.905 cm
	const float RimBoardHalfWidth = 3.81f / 2.0f; // 1.905 cm

	AddCornerRules(RimBoardHalfWidth);
	AddInlineRules();
	AddFoundationRules();
	AddJoistRules();
	AddPlywoodRules();
	AddBottomPlateRules();
}

void ASnapRuleTable::AddCornerRules(float BoardHalfWidth)
{
	// CORNER JOINTS: Left-Left (both boards have their left end meeting)
	// This creates a 90-degree L-joint. The incoming board rotates +/-90 degrees
	// and the flush offset pushes it by one half-width so the faces are flush.
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ true,
			/*TgtLeft=*/ true,
			EConstructionSocketType::RimBoard_End_Corner,
			EConstructionSocketType::RimBoard_End_Corner
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Corner_90;
		Rule.YawOffset = 90.0f;
		Rule.bYawSignFromPlayerIntent = true; // Player decides CW or CCW
		Rule.FlushOffset = FVector(0.0f, BoardHalfWidth, 0.0f); // Applied in target local space
		Rule.Priority = 1000; // Highest priority for corner joints
		RuleTable.Add(Key, Rule);

		UE_LOG(LogTemp, Log, TEXT("SnapRuleTable: Added Left-Left corner rule (Priority=%d, FlushOffset.Y=%.3f)"),
			Rule.Priority, Rule.FlushOffset.Y);
	}

	// CORNER JOINTS: Right-Right (both boards have their right end meeting)
	// Same as Left-Left but mirrored
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ false,
			/*TgtLeft=*/ false,
			EConstructionSocketType::RimBoard_End_Corner,
			EConstructionSocketType::RimBoard_End_Corner
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Corner_90;
		Rule.YawOffset = 90.0f;
		Rule.bYawSignFromPlayerIntent = true; // Player decides CW or CCW
		Rule.FlushOffset = FVector(0.0f, BoardHalfWidth, 0.0f); // Applied in target local space
		Rule.Priority = 1000; // Highest priority for corner joints
		RuleTable.Add(Key, Rule);

		UE_LOG(LogTemp, Log, TEXT("SnapRuleTable: Added Right-Right corner rule (Priority=%d, FlushOffset.Y=%.3f)"),
			Rule.Priority, Rule.FlushOffset.Y);
	}
}

void ASnapRuleTable::AddInlineRules()
{
	// INLINE EXTENSION: Left-Right (source left end meets target right end)
	// Boards continue in a straight line, no rotation needed, no flush offset
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ true,
			/*TgtLeft=*/ false,
			EConstructionSocketType::RimBoard_End_Corner,
			EConstructionSocketType::RimBoard_End_Corner
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Inline_0;
		Rule.YawOffset = 0.0f;
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector::ZeroVector; // No offset for inline
		Rule.Priority = 800; // Lower than corners
		RuleTable.Add(Key, Rule);

		UE_LOG(LogTemp, Log, TEXT("SnapRuleTable: Added Left-Right inline rule (Priority=%d)"), Rule.Priority);
	}

	// INLINE EXTENSION: Right-Left (source right end meets target left end)
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ false,
			/*TgtLeft=*/ true,
			EConstructionSocketType::RimBoard_End_Corner,
			EConstructionSocketType::RimBoard_End_Corner
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Inline_0;
		Rule.YawOffset = 0.0f;
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector::ZeroVector; // No offset for inline
		Rule.Priority = 800; // Lower than corners
		RuleTable.Add(Key, Rule);

		UE_LOG(LogTemp, Log, TEXT("SnapRuleTable: Added Right-Left inline rule (Priority=%d)"), Rule.Priority);
	}
}

void ASnapRuleTable::AddFoundationRules()
{
	// FOUNDATION: RimBoard bottom end connecting to foundation side
	// Rotation is player-controlled (board can be in any orientation on foundation)
	// We add rules for both left and right bottom sockets
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ true, // BottomEnd_Left
			/*TgtLeft=*/ false, // Foundation side (doesn't have left/right concept, using false as default)
			EConstructionSocketType::RimBoard_Bottom_End,
			EConstructionSocketType::Foundation_Side
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Foundation;
		Rule.YawOffset = 0.0f; // Player-controlled rotation
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector::ZeroVector;
		Rule.Priority = 10; // Low priority, foundation snapping is simple
		RuleTable.Add(Key, Rule);
	}

	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ false, // BottomEnd_Right
			/*TgtLeft=*/ false, // Foundation side
			EConstructionSocketType::RimBoard_Bottom_End,
			EConstructionSocketType::Foundation_Side
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Foundation;
		Rule.YawOffset = 0.0f; // Player-controlled rotation
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector::ZeroVector;
		Rule.Priority = 10;
		RuleTable.Add(Key, Rule);
	}

	UE_LOG(LogTemp, Log, TEXT("SnapRuleTable: Added foundation rules (Priority=10)"));
}

bool ASnapRuleTable::GetSnapRule(
	EConstructionSocketType SourceType,
	EConstructionSocketType TargetType,
	const FName& SourceSocketName,
	const FName& TargetSocketName,
	FSnapRule& OutRule) const
{
	// Determine left/right from socket names
	FString SourceStr = SourceSocketName.ToString();
	FString TargetStr = TargetSocketName.ToString();

	bool bSourceIsLeft = SourceStr.Contains(TEXT("Left"));
	bool bTargetIsLeft = TargetStr.Contains(TEXT("Left"));

	FSnapRuleKey Key(bSourceIsLeft, bTargetIsLeft, SourceType, TargetType);

	const FSnapRule* Found = RuleTable.Find(Key);
	if (Found)
	{
		OutRule = *Found;
		return true;
	}

	return false;
}

void ASnapRuleTable::AddJoistRules()
{
	// JOIST TOP-FACE: Joist end snaps to rim board top face socket.
	// Joist drops down so its top is flush with the rim board top.
	// Top-face socket is at rim center + halfHeight, joist center should be
	// at rim center, so offset = -halfHeight.
	const float BoardHeight = 13.97f; // 5.5" in cm
	const float HalfBoardHeight = BoardHeight / 2.0f;

	// JoistEnd_Left -> RimBoard_Top_Face
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ true,
			/*TgtLeft=*/ false, // Top face sockets don't have Left/Right
			EConstructionSocketType::Joist_End,
			EConstructionSocketType::RimBoard_Top_Face
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::TopFace;
		Rule.YawOffset = 90.0f; // Joist runs perpendicular to rim board
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector(0.0f, 0.0f, -HalfBoardHeight); // Drop joist so top is flush with rim top
		Rule.Priority = 900;
		RuleTable.Add(Key, Rule);
	}

	// JoistEnd_Right -> RimBoard_Top_Face
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ false,
			/*TgtLeft=*/ false,
			EConstructionSocketType::Joist_End,
			EConstructionSocketType::RimBoard_Top_Face
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::TopFace;
		Rule.YawOffset = 90.0f;
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector(0.0f, 0.0f, -HalfBoardHeight);
		Rule.Priority = 900;
		RuleTable.Add(Key, Rule);
	}

	UE_LOG(LogTemp, Log, TEXT("SnapRuleTable: Added joist top-face rules (Priority=900, Z-offset=%.2f)"), -HalfBoardHeight);
}

void ASnapRuleTable::AddPlywoodRules()
{
	// Plywood sheet thickness: 3/4" = 1.905cm
	const float SheetHalfThickness = 1.905f / 2.0f;

	// NOTE: Plywood does NOT snap to EndCorner sockets — only to TopFace sockets.
	// Corner sockets on plywood still exist for positioning but don't connect to EndCorner.

	// PLYWOOD EDGE -> JOIST TOP FACE
	// Sheet edge rests on top of a joist.
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ false,
			/*TgtLeft=*/ false,
			EConstructionSocketType::Plywood_Edge,
			EConstructionSocketType::Joist_Top_Face
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::TopFace;
		Rule.YawOffset = 0.0f;
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector(0.0f, 0.0f, SheetHalfThickness);
		Rule.Priority = 600;
		RuleTable.Add(Key, Rule);
	}

	// PLYWOOD EDGE -> RIM BOARD TOP FACE
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ false,
			/*TgtLeft=*/ false,
			EConstructionSocketType::Plywood_Edge,
			EConstructionSocketType::RimBoard_Top_Face
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::TopFace;
		Rule.YawOffset = 0.0f;
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector(0.0f, 0.0f, SheetHalfThickness);
		Rule.Priority = 600;
		RuleTable.Add(Key, Rule);
	}

	// PLYWOOD EDGE -> PLYWOOD EDGE (sheet-to-sheet)
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ false,
			/*TgtLeft=*/ false,
			EConstructionSocketType::Plywood_Edge,
			EConstructionSocketType::Plywood_Edge
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Inline_0;
		Rule.YawOffset = 0.0f;
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector::ZeroVector;
		Rule.Priority = 500;
		RuleTable.Add(Key, Rule);
	}

	UE_LOG(LogTemp, Log, TEXT("SnapRuleTable: Added plywood rules (Edge=%d, Sheet-to-sheet=%d)"),
		600, 500);
}

void ASnapRuleTable::AddBottomPlateRules()
{
	// 2x4 bottom plate half-width: 1.5" = 3.81cm / 2 = 1.905cm
	const float PlateHalfWidth = 3.81f / 2.0f;

	// BOTTOM PLATE -> RIM BOARD TOP FACE
	// Plate sits on top of plywood, above the rim board.
	// Z offset is handled in BuildablePiece::DetectSnapCandidates.
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ false,
			/*TgtLeft=*/ false,
			EConstructionSocketType::BottomPlate_Bottom,
			EConstructionSocketType::RimBoard_Top_Face
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::TopFace;
		Rule.YawOffset = 0.0f; // Plate aligns with rim board (same direction)
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector::ZeroVector;
		Rule.Priority = 700;
		RuleTable.Add(Key, Rule);
	}

	// BOTTOM PLATE END -> BOTTOM PLATE END (corner joint: Left-Left)
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ true,
			/*TgtLeft=*/ true,
			EConstructionSocketType::BottomPlate_End,
			EConstructionSocketType::BottomPlate_End
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Corner_90;
		Rule.YawOffset = 90.0f;
		Rule.bYawSignFromPlayerIntent = true;
		Rule.FlushOffset = FVector(0.0f, PlateHalfWidth, 0.0f);
		Rule.Priority = 900;
		RuleTable.Add(Key, Rule);
	}

	// BOTTOM PLATE END -> BOTTOM PLATE END (corner joint: Right-Right)
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ false,
			/*TgtLeft=*/ false,
			EConstructionSocketType::BottomPlate_End,
			EConstructionSocketType::BottomPlate_End
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Corner_90;
		Rule.YawOffset = 90.0f;
		Rule.bYawSignFromPlayerIntent = true;
		Rule.FlushOffset = FVector(0.0f, PlateHalfWidth, 0.0f);
		Rule.Priority = 900;
		RuleTable.Add(Key, Rule);
	}

	// BOTTOM PLATE END -> BOTTOM PLATE END (inline: Left-Right)
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ true,
			/*TgtLeft=*/ false,
			EConstructionSocketType::BottomPlate_End,
			EConstructionSocketType::BottomPlate_End
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Inline_0;
		Rule.YawOffset = 0.0f;
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector::ZeroVector;
		Rule.Priority = 700;
		RuleTable.Add(Key, Rule);
	}

	// BOTTOM PLATE END -> BOTTOM PLATE END (inline: Right-Left)
	{
		FSnapRuleKey Key(
			/*SrcLeft=*/ false,
			/*TgtLeft=*/ true,
			EConstructionSocketType::BottomPlate_End,
			EConstructionSocketType::BottomPlate_End
		);

		FSnapRule Rule;
		Rule.ConnectionType = ESnapConnectionType::Inline_0;
		Rule.YawOffset = 0.0f;
		Rule.bYawSignFromPlayerIntent = false;
		Rule.FlushOffset = FVector::ZeroVector;
		Rule.Priority = 700;
		RuleTable.Add(Key, Rule);
	}

	UE_LOG(LogTemp, Log, TEXT("SnapRuleTable: Added bottom plate rules (TopFace=%d, Corner=%d, Inline=%d)"),
		700, 900, 700);
}

FVector ASnapRuleTable::CalculateFlushOffset(float BoardHalfWidth, const FRotator& TargetRotation, bool bExtendRight)
{
	// The flush offset moves the incoming board so its face is flush with the target board's end
	// This is applied in world space, perpendicular to the target board
	FVector TargetRight = TargetRotation.RotateVector(FVector::RightVector);
	// Offset by one board half-width along target's right vector (or negative for left)
	float Sign = bExtendRight ? 1.0f : -1.0f;
	return TargetRight * BoardHalfWidth * Sign;
}
