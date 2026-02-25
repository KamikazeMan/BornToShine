// Born To Shine - Snap Rule Lookup Table
// Pre-computed connection rules: given a pair of socket types,
// look up the exact relative transform instead of computing it at runtime.

#pragma once

#include "CoreMinimal.h"
#include "ConstructionTypes.h"
#include "SnapRuleTable.generated.h"

/**
 * Defines the connection type between two sockets
 */
UENUM(BlueprintType)
enum class ESnapConnectionType : uint8
{
	Corner_90,      // 90-degree L-joint (Left-Left or Right-Right)
	Inline_0,       // Straight extension (Left-Right or Right-Left)
	Foundation,     // Board sitting on foundation
	DualEnd_Span,   // Board spanning between two corners
	TopFace,        // Joist sitting on top of rim board
	None
};

/**
 * Pre-computed snap rule: for a given socket pair, the exact relative transform
 */
USTRUCT(BlueprintType)
struct FSnapRule
{
	GENERATED_BODY()

	// What type of connection this represents
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESnapConnectionType ConnectionType;

	// Rotation offset relative to target piece's rotation
	// For Corner_90: +90 or -90 (sign determined by player intent at runtime)
	// For Inline_0: 0
	// For Foundation: preserved from player rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float YawOffset;

	// Position offset applied AFTER snapping to make joints flush
	// This replaces the old Y=HalfWidth socket hack
	// Applied in the target piece's local space
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector FlushOffset;

	// Whether the yaw sign needs to be determined at runtime (by player look direction)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bYawSignFromPlayerIntent;

	// Snap priority (higher = preferred when multiple candidates exist)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Priority;

	FSnapRule()
		: ConnectionType(ESnapConnectionType::None)
		, YawOffset(0.f)
		, FlushOffset(FVector::ZeroVector)
		, bYawSignFromPlayerIntent(false)
		, Priority(0)
	{}
};

/**
 * Key for the rule lookup: source socket name pattern + target socket name pattern
 * We use socket NAME patterns (Left/Right) rather than just types, because
 * Left-Left vs Left-Right determines corner vs inline
 */
USTRUCT()
struct FSnapRuleKey
{
	GENERATED_BODY()

	// Whether source socket is a "Left" or "Right" end
	bool bSourceIsLeft;

	// Whether target socket is a "Left" or "Right" end
	bool bTargetIsLeft;

	// Source socket type
	EConstructionSocketType SourceType;

	// Target socket type
	EConstructionSocketType TargetType;

	FSnapRuleKey()
		: bSourceIsLeft(false), bTargetIsLeft(false)
		, SourceType(EConstructionSocketType::None)
		, TargetType(EConstructionSocketType::None)
	{}

	FSnapRuleKey(bool SrcLeft, bool TgtLeft, EConstructionSocketType SrcType, EConstructionSocketType TgtType)
		: bSourceIsLeft(SrcLeft), bTargetIsLeft(TgtLeft), SourceType(SrcType), TargetType(TgtType)
	{}

	bool operator==(const FSnapRuleKey& Other) const
	{
		return bSourceIsLeft == Other.bSourceIsLeft
			&& bTargetIsLeft == Other.bTargetIsLeft
			&& SourceType == Other.SourceType
			&& TargetType == Other.TargetType;
	}

	friend uint32 GetTypeHash(const FSnapRuleKey& Key)
	{
		return HashCombine(
			HashCombine(GetTypeHash(Key.bSourceIsLeft), GetTypeHash(Key.bTargetIsLeft)),
			HashCombine(GetTypeHash((uint8)Key.SourceType), GetTypeHash((uint8)Key.TargetType))
		);
	}
};

/**
 * Singleton lookup table for snap rules
 * Place one of these in the level alongside SocketManager
 */
UCLASS()
class BORNTOSHINE_API ASnapRuleTable : public AActor
{
	GENERATED_BODY()

public:
	ASnapRuleTable();

	static ASnapRuleTable* Instance;

	// Look up the snap rule for a given socket pair
	// Returns true if a rule exists, fills OutRule
	bool GetSnapRule(
		EConstructionSocketType SourceType,
		EConstructionSocketType TargetType,
		const FName& SourceSocketName,
		const FName& TargetSocketName,
		FSnapRule& OutRule
	) const;

	// Get the flush offset for a corner joint, given board width
	// This is the post-snap correction that makes boards flush at corners
	static FVector CalculateFlushOffset(float BoardHalfWidth, const FRotator& TargetRotation, bool bExtendRight);

protected:
	virtual void BeginPlay() override;

private:
	// The lookup table: socket pair -> snap rule
	TMap<FSnapRuleKey, FSnapRule> RuleTable;

	// Initialize all rules
	void InitializeRules();

	// Helper to add rules
	void AddCornerRules(float BoardHalfWidth);
	void AddInlineRules();
	void AddFoundationRules();
	void AddJoistRules();
	void AddPlywoodRules();
	void AddBottomPlateRules();
	void AddTopPlateRules();
	void AddDoorFrameRules();
	void AddWindowFrameRules();
};
