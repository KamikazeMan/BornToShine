// Born To Shine - Context-Aware Snap Priority System
// Replaces fixed priority numbers with distance-weighted, context-aware scoring.

#pragma once

#include "CoreMinimal.h"
#include "ConstructionTypes.h"
#include "SnapPriority.generated.h"

/**
 * Context for evaluating snap priority
 */
USTRUCT()
struct FSnapPriorityContext
{
    GENERATED_BODY()

    // Player camera position and direction
    FVector CameraLocation;
    FVector CameraForward;

    // Position of the piece being placed
    FVector PieceLocation;

    // What type of piece the player is placing
    EPieceType PlacingPieceType;

    FSnapPriorityContext()
        : CameraLocation(FVector::ZeroVector)
        , CameraForward(FVector::ForwardVector)
        , PieceLocation(FVector::ZeroVector)
        , PlacingPieceType(EPieceType::None)
    {}
};

/**
 * Static utility class for context-aware snap priority calculation
 */
UCLASS()
class BORNTOSHINE_API USnapPriority : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Calculate context-aware snap score.
     * Combines base priority with distance weighting and player intent.
     *
     * @param SourceType Socket type on the piece being placed
     * @param TargetType Socket type on the target piece
     * @param Distance Distance between source and target sockets (cm)
     * @param Context Player context (camera, piece position)
     * @param TargetPieceLocation World position of the target piece
     * @return Final priority score (higher = preferred)
     */
    static float CalculateContextualScore(
        EConstructionSocketType SourceType,
        EConstructionSocketType TargetType,
        float Distance,
        const FSnapPriorityContext& Context,
        const FVector& TargetPieceLocation
    );

    /**
     * Get base priority for a socket pair (replaces GetSocketConnectionPriority).
     * Same values as before but as a static function.
     */
    static int32 GetBasePriority(EConstructionSocketType SourceType, EConstructionSocketType TargetType);

private:
    /**
     * Calculate distance weight -- closer targets score higher.
     * Uses inverse distance with a soft falloff to avoid harsh cutoffs.
     */
    static float DistanceWeight(float Distance, float MaxDistance = 500.0f);

    /**
     * Calculate player intent bonus -- if the crosshair is pointing near the target,
     * boost its score. This helps when a foundation socket is nearby but the player
     * is clearly looking at a corner socket farther away.
     */
    static float PlayerIntentBonus(
        const FSnapPriorityContext& Context,
        const FVector& TargetPieceLocation
    );
};
