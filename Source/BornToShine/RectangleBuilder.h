// Born To Shine - Rectangle Builder Component
// Makes rectangular floor frames a first-class building concept.
// Tracks placed rim boards and auto-calculates positions for boards 3 and 4.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ConstructionTypes.h"
#include "RectangleBuilder.generated.h"

class ABuildablePiece;
class ARimBoard;

/**
 * Tracks the state of rectangle construction
 */
UENUM(BlueprintType)
enum class ERectangleState : uint8
{
    None,           // No boards placed or not tracking
    OneBoard,       // First board placed
    LShape,         // Two boards at 90 degrees forming an L
    UShape,         // Three boards forming a U
    Complete,       // Four boards forming closed rectangle
};

/**
 * Data for a suggested board placement (ghost preview)
 */
USTRUCT(BlueprintType)
struct FBoardSuggestion
{
    GENERATED_BODY()

    // World position for the suggested board
    UPROPERTY(BlueprintReadOnly)
    FVector Position;

    // World rotation for the suggested board
    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation;

    // Required length in feet
    UPROPERTY(BlueprintReadOnly)
    int32 LengthFeet;

    // Which corner sockets this board would connect to
    UPROPERTY(BlueprintReadOnly)
    FName LeftTargetSocket;

    UPROPERTY(BlueprintReadOnly)
    FName RightTargetSocket;

    // Target pieces for each end
    UPROPERTY(BlueprintReadOnly)
    ABuildablePiece* LeftTargetPiece;

    UPROPERTY(BlueprintReadOnly)
    ABuildablePiece* RightTargetPiece;

    bool bIsValid;

    FBoardSuggestion()
        : Position(FVector::ZeroVector)
        , Rotation(FRotator::ZeroRotator)
        , LengthFeet(0)
        , LeftTargetPiece(nullptr)
        , RightTargetPiece(nullptr)
        , bIsValid(false)
    {}
};

/**
 * Component that tracks rectangle construction progress.
 * Attach to the same actor as BuildingComponent.
 * Recognizes L-shapes from placed boards and suggests/auto-places remaining boards.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BORNTOSHINE_API URectangleBuilderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URectangleBuilderComponent();

    // Called when a rim board is placed — updates rectangle tracking
    void OnRimBoardPlaced(ARimBoard* Board);

    // Called when a rim board is removed
    void OnRimBoardRemoved(ARimBoard* Board);

    // Get current rectangle state
    UFUNCTION(BlueprintCallable, Category = "Construction|Rectangle")
    ERectangleState GetRectangleState() const { return CurrentState; }

    // Get suggestions for next board(s)
    UFUNCTION(BlueprintCallable, Category = "Construction|Rectangle")
    TArray<FBoardSuggestion> GetBoardSuggestions() const { return CurrentSuggestions; }

    // Check if a specific position/rotation matches a suggestion (for auto-snap)
    bool MatchesSuggestion(const FVector& Position, float Tolerance, FBoardSuggestion& OutSuggestion) const;

    // Get the ghost preview locations for rendering
    UFUNCTION(BlueprintCallable, Category = "Construction|Rectangle")
    TArray<FBoardSuggestion> GetGhostPreviews() const { return CurrentSuggestions; }

    // Enable/disable ghost preview rendering
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Rectangle")
    bool bShowGhostPreviews;

    // Ghost preview material (translucent blue)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Rectangle")
    class UMaterialInterface* GhostMaterial;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    // Current rectangle construction state
    ERectangleState CurrentState;

    // Tracked rim boards in placement order
    UPROPERTY()
    TArray<ARimBoard*> TrackedBoards;

    // Current suggestions for next boards
    TArray<FBoardSuggestion> CurrentSuggestions;

    // Ghost preview actors (spawned meshes showing where boards should go)
    UPROPERTY()
    TArray<AActor*> GhostActors;

    // Recalculate state and suggestions when boards change
    void RecalculateState();

    // Detect L-shape from two boards
    bool DetectLShape(ARimBoard* Board1, ARimBoard* Board2, FVector& OutCornerPos) const;

    // Calculate board 3 suggestion from L-shape
    FBoardSuggestion CalculateThirdBoardSuggestion(ARimBoard* Board1, ARimBoard* Board2) const;

    // Calculate board 4 suggestion (closing board) from U-shape
    FBoardSuggestion CalculateFourthBoardSuggestion(ARimBoard* Board1, ARimBoard* Board2, ARimBoard* Board3) const;

    // Find open (unoccupied) corner sockets on a board
    TArray<FConstructionSocket> GetOpenCornerSockets(ARimBoard* Board) const;

    // Check if two boards are connected at a 90 degree corner
    bool AreConnectedAtCorner(ARimBoard* Board1, ARimBoard* Board2) const;

    // Spawn/update ghost preview actors
    void UpdateGhostPreviews();
    void ClearGhostPreviews();
};
