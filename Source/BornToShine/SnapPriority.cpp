// Born To Shine - Context-Aware Snap Priority System

#include "SnapPriority.h"

float USnapPriority::CalculateContextualScore(
    EConstructionSocketType SourceType,
    EConstructionSocketType TargetType,
    float Distance,
    const FSnapPriorityContext& Context,
    const FVector& TargetPieceLocation)
{
    // Start with base priority
    float Score = (float)GetBasePriority(SourceType, TargetType);

    // Apply distance weighting
    // If a foundation socket is 5cm away and a corner is 400cm away,
    // the distance weight should help the foundation win
    float DistWeight = DistanceWeight(Distance);

    // Distance matters more for low-priority connections
    // For corners (high priority), distance is a minor tiebreaker
    // For foundation (low priority), distance can make it competitive
    if (Score < 100.0f)
    {
        // Low base priority: distance matters a lot
        Score = Score + DistWeight * 50.0f;
    }
    else
    {
        // High base priority: distance is a tiebreaker
        Score = Score + DistWeight * 10.0f;
    }

    // Apply player intent bonus
    float IntentBonus = PlayerIntentBonus(Context, TargetPieceLocation);
    Score += IntentBonus;

    return Score;
}

int32 USnapPriority::GetBasePriority(EConstructionSocketType SourceType, EConstructionSocketType TargetType)
{
    // Rim-to-Rim corner (highest)
    if (SourceType == EConstructionSocketType::RimBoard_End_Corner &&
        TargetType == EConstructionSocketType::RimBoard_End_Corner)
    {
        return 1000;
    }

    // Rim-to-Rim side
    if (SourceType == EConstructionSocketType::RimBoard_Side_Face &&
        TargetType == EConstructionSocketType::RimBoard_Side_Face)
    {
        return 900;
    }

    // Joist-to-Rim
    if ((SourceType == EConstructionSocketType::Joist_End &&
         (TargetType == EConstructionSocketType::RimBoard_Top_Face ||
          TargetType == EConstructionSocketType::RimBoard_Side_Face)) ||
        ((SourceType == EConstructionSocketType::RimBoard_Top_Face ||
          SourceType == EConstructionSocketType::RimBoard_Side_Face) &&
         TargetType == EConstructionSocketType::Joist_End))
    {
        return 800;
    }

    // Rim bottom to Foundation (lowest)
    if ((SourceType == EConstructionSocketType::RimBoard_Bottom_End &&
         (TargetType == EConstructionSocketType::Foundation_Corner ||
          TargetType == EConstructionSocketType::Foundation_Side)) ||
        ((SourceType == EConstructionSocketType::Foundation_Corner ||
          SourceType == EConstructionSocketType::Foundation_Side) &&
         TargetType == EConstructionSocketType::RimBoard_Bottom_End))
    {
        return 10;
    }

    return 0;
}

float USnapPriority::DistanceWeight(float Distance, float MaxDistance)
{
    // Returns 0.0 to 1.0, where closer = higher
    // Uses smooth falloff so very close snaps get strong boost
    if (Distance >= MaxDistance) return 0.0f;

    float Normalized = 1.0f - (Distance / MaxDistance);
    // Square it for stronger preference for nearby targets
    return Normalized * Normalized;
}

float USnapPriority::PlayerIntentBonus(
    const FSnapPriorityContext& Context,
    const FVector& TargetPieceLocation)
{
    // How well-aligned is the player's look direction with the target piece?
    FVector ToTarget = (TargetPieceLocation - Context.CameraLocation).GetSafeNormal();
    float DotProduct = FVector::DotProduct(Context.CameraForward, ToTarget);

    // DotProduct: 1.0 = looking directly at target, 0.0 = perpendicular, -1.0 = looking away
    // Only give bonus for targets the player is somewhat looking at
    if (DotProduct > 0.7f) // Within approximately 45 degree cone
    {
        // Scale from 0 to 50 bonus points based on alignment
        float AlignmentFactor = (DotProduct - 0.7f) / 0.3f; // 0 to 1
        return AlignmentFactor * 50.0f;
    }

    return 0.0f;
}
