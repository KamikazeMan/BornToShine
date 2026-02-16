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
class AFloorJoist;
class ABottomPlate;
class AWallStud;
class ATopPlate;

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
 * Data for a suggested joist placement
 */
USTRUCT(BlueprintType)
struct FJoistSuggestion
{
    GENERATED_BODY()

    // World position for the joist center
    UPROPERTY(BlueprintReadOnly)
    FVector Position;

    // World rotation (perpendicular to through boards)
    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation;

    // Required length in feet
    UPROPERTY(BlueprintReadOnly)
    int32 LengthFeet;

    // Which rim board top-face sockets this joist connects to
    UPROPERTY(BlueprintReadOnly)
    FName Board1TargetSocket;

    UPROPERTY(BlueprintReadOnly)
    FName Board3TargetSocket;

    // The rim boards this joist spans between
    UPROPERTY(BlueprintReadOnly)
    ABuildablePiece* Board1;

    UPROPERTY(BlueprintReadOnly)
    ABuildablePiece* Board3;

    // Joist index (0-based, for tracking placement order)
    int32 JoistIndex;

    bool bIsValid;

    FJoistSuggestion()
        : Position(FVector::ZeroVector)
        , Rotation(FRotator::ZeroRotator)
        , LengthFeet(0)
        , Board1(nullptr)
        , Board3(nullptr)
        , JoistIndex(0)
        , bIsValid(false)
    {}
};

/**
 * Data for a suggested bottom plate placement (auto-placed above rim boards)
 */
USTRUCT(BlueprintType)
struct FPlateSuggestion
{
    GENERATED_BODY()

    // World position for the plate center
    UPROPERTY(BlueprintReadOnly)
    FVector Position;

    // World rotation (matches the rim board below)
    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation;

    // Required length in feet (matches the rim board below)
    UPROPERTY(BlueprintReadOnly)
    int32 LengthFeet;

    // The rim board this plate sits directly above
    UPROPERTY(BlueprintReadOnly)
    ARimBoard* SourceRimBoard;

    // Plate index (0-3, for tracking placement order)
    int32 PlateIndex;

    bool bIsValid;

    FPlateSuggestion()
        : Position(FVector::ZeroVector)
        , Rotation(FRotator::ZeroRotator)
        , LengthFeet(0)
        , SourceRimBoard(nullptr)
        , PlateIndex(0)
        , bIsValid(false)
    {}
};

/**
 * Data for a suggested wall stud placement (auto-placed on bottom plates)
 * Interior studs only, at 16" OC. Corner posts handle the plate ends.
 */
USTRUCT(BlueprintType)
struct FStudSuggestion
{
    GENERATED_BODY()

    // World position for the stud center (plate top + StudHeight/2)
    UPROPERTY(BlueprintReadOnly)
    FVector Position;

    // World rotation (same yaw as the bottom plate)
    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation;

    // Height of this stud in cm
    UPROPERTY(BlueprintReadOnly)
    float StudHeightCm;

    // The bottom plate this stud sits on
    UPROPERTY(BlueprintReadOnly)
    ABottomPlate* SourcePlate;

    // Which Wall_Bottom_Plate socket on the plate
    UPROPERTY(BlueprintReadOnly)
    FName PlateSocketName;

    // Stud index (0-based, for tracking placement order)
    int32 StudIndex;

    bool bIsValid;

    FStudSuggestion()
        : Position(FVector::ZeroVector)
        , Rotation(FRotator::ZeroRotator)
        , StudHeightCm(235.27f)
        , SourcePlate(nullptr)
        , StudIndex(0)
        , bIsValid(false)
    {}
};

/**
 * Data for a suggested top plate placement (auto-placed above wall studs).
 * Covers both the first top plate (clicks 1-4) and double top plate (clicks 5-8)
 * in a single unified suggestion array.
 */
USTRUCT(BlueprintType)
struct FTopPlateSuggestion
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector Position;

    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation;

    // Length in cm (may include 3.5" overlap for double plates)
    UPROPERTY(BlueprintReadOnly)
    float LengthCm;

    // The bottom plate this top plate mirrors (null for double plates)
    UPROPERTY(BlueprintReadOnly)
    ABottomPlate* SourceBottomPlate;

    // True for the second layer (double top plate, suggestions 4-7)
    bool bIsDoubleTopPlate;

    int32 PlateIndex;
    bool bIsValid;

    FTopPlateSuggestion()
        : Position(FVector::ZeroVector)
        , Rotation(FRotator::ZeroRotator)
        , LengthCm(0.0f)
        , SourceBottomPlate(nullptr)
        , bIsDoubleTopPlate(false)
        , PlateIndex(0)
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

    // Does the RectangleBuilder have an active suggestion for the next board?
    // When true, BuildingComponent should bypass normal snap detection and use the suggestion directly.
    bool HasActiveSuggestion() const;

    // Get the primary (first) active suggestion
    FBoardSuggestion GetActiveSuggestion() const;

    // Apply a suggestion to a rim board: sets its length, position, rotation, and commits placement.
    // Returns true if successful. Called by BuildingComponent instead of TryPlace().
    bool ApplySuggestionToBoard(ARimBoard* Board);

    // --- Joist Layout System ---

    // Does the builder have joist suggestions ready?
    bool HasJoistSuggestions() const { return JoistSuggestions.Num() > 0 && PlacedJoistCount < JoistSuggestions.Num(); }

    // Get the next joist suggestion (first unplaced)
    FJoistSuggestion GetNextJoistSuggestion() const;

    // Apply a joist suggestion: sets position, rotation, length
    bool ApplyJoistSuggestion(AFloorJoist* Joist);

    // Get all joist suggestions (for ghost previews)
    TArray<FJoistSuggestion> GetJoistSuggestions() const { return JoistSuggestions; }

    // How many joists have been placed in the current layout
    int32 GetPlacedJoistCount() const { return PlacedJoistCount; }

    // --- Bottom Plate Layout System ---

    // Does the builder have plate suggestions ready?
    bool HasPlateSuggestions() const { return PlateSuggestions.Num() > 0 && PlacedPlateCount < PlateSuggestions.Num(); }

    // Get the next plate suggestion (first unplaced)
    FPlateSuggestion GetNextPlateSuggestion() const;

    // Apply a plate suggestion: sets position, rotation, length, extends mesh
    bool ApplyPlateSuggestion(ABottomPlate* Plate);

    // Get all plate suggestions
    TArray<FPlateSuggestion> GetPlateSuggestions() const { return PlateSuggestions; }

    // How many plates have been placed in the current layout
    int32 GetPlacedPlateCount() const { return PlacedPlateCount; }

    // --- Wall Stud Layout System ---

    // Does the builder have stud suggestions ready?
    bool HasStudSuggestions() const { return StudSuggestions.Num() > 0 && PlacedStudCount < StudSuggestions.Num(); }

    // Get the next stud suggestion (first unplaced)
    FStudSuggestion GetNextStudSuggestion() const;

    // Apply a stud suggestion: sets position, rotation, height
    bool ApplyStudSuggestion(AWallStud* Stud);

    // Get all stud suggestions
    TArray<FStudSuggestion> GetStudSuggestions() const { return StudSuggestions; }

    // How many studs have been placed in the current layout
    int32 GetPlacedStudCount() const { return PlacedStudCount; }

    // --- Top Plate Layout System ---

    // Does the builder have top plate suggestions ready?
    bool HasTopPlateSuggestions() const { return TopPlateSuggestions.Num() > 0 && PlacedTopPlateCount < TopPlateSuggestions.Num(); }

    // Get the next top plate suggestion (first unplaced)
    FTopPlateSuggestion GetNextTopPlateSuggestion() const;

    // Apply a top plate suggestion: sets position, rotation, length
    bool ApplyTopPlateSuggestion(ATopPlate* Plate);

    // Get all top plate suggestions
    TArray<FTopPlateSuggestion> GetTopPlateSuggestions() const { return TopPlateSuggestions; }

    // How many top plates have been placed
    int32 GetPlacedTopPlateCount() const { return PlacedTopPlateCount; }

    // Get the ghost preview locations for rendering
    UFUNCTION(BlueprintCallable, Category = "Construction|Rectangle")
    TArray<FBoardSuggestion> GetGhostPreviews() const { return CurrentSuggestions; }

    // Check if an existing placed piece of the given type is near the position
    bool OverlapsExistingPiece(EPieceType Type, const FVector& Position, float Tolerance = 50.0f) const;

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
    void SpawnGhostForSuggestion(const FBoardSuggestion& Suggestion);

    // --- Joist Layout ---

    // Calculate joist positions after rectangle completes
    void CalculateJoistLayout(ARimBoard* Board1, ARimBoard* Board2, ARimBoard* Board3, ARimBoard* Board4);

    // Joist suggestions for the completed rectangle
    TArray<FJoistSuggestion> JoistSuggestions;

    // Number of joists placed so far
    int32 PlacedJoistCount;

    // Stored rectangle geometry for joist calculations
    // Board1 and Board3 are the "through" boards (parallel, joists span between them)
    // Board2 and Board4 are the "end" boards (perpendicular)
    UPROPERTY()
    ARimBoard* ThroughBoard1;

    UPROPERTY()
    ARimBoard* ThroughBoard3;

    // --- Bottom Plate Layout ---

    // Calculate bottom plate positions after rectangle completes
    void CalculatePlateLayout(ARimBoard* Board1, ARimBoard* Board2, ARimBoard* Board3, ARimBoard* Board4);

    // Plate suggestions for the completed rectangle (one per rim board)
    TArray<FPlateSuggestion> PlateSuggestions;

    // Number of plates placed so far
    int32 PlacedPlateCount;

    // All 4 rim boards from the last completed rectangle (for plate reference)
    UPROPERTY()
    TArray<ARimBoard*> CompletedRimBoards;

    // --- Wall Stud Layout ---

    // Calculate stud positions from all placed bottom plates
    void CalculateStudLayout();

    // Stud suggestions for all placed bottom plates
    TArray<FStudSuggestion> StudSuggestions;

    // Number of studs placed so far
    int32 PlacedStudCount;

    // Placed bottom plates (tracked for stud layout calculation)
    UPROPERTY()
    TArray<ABottomPlate*> PlacedBottomPlates;

    // --- Top Plate Layout ---

    // Calculate top plate positions after all studs are placed
    void CalculateTopPlateLayout();

    // Top plate suggestions (one per bottom plate, mirrors its position at stud-top height)
    TArray<FTopPlateSuggestion> TopPlateSuggestions;

    // Number of top plates placed so far
    int32 PlacedTopPlateCount;

    // Placed top plates (tracked for double top plate layout)
    UPROPERTY()
    TArray<ATopPlate*> PlacedTopPlates;

};
