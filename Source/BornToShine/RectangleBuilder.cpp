// Born To Shine - Rectangle Builder Component

#include "RectangleBuilder.h"
#include "RimBoard.h"
#include "FloorJoist.h"
#include "BottomPlate.h"
#include "WallStud.h"
#include "TopPlate.h"
#include "DoubleTopPlate.h"
#include "PlywoodSheet.h"
#include "BuildablePiece.h"
#include "ConstructionPhaseManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

URectangleBuilderComponent::URectangleBuilderComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.25f; // Only update 4x per second, not every frame

    CurrentState = ERectangleState::None;
    bShowGhostPreviews = true;
    GhostMaterial = nullptr;
    PlacedJoistCount = 0;
    ThroughBoard1 = nullptr;
    ThroughBoard3 = nullptr;
    PlacedPlateCount = 0;
    PlacedStudCount = 0;
    PlacedTopPlateCount = 0;
    PlacedDoubleTopPlateCount = 0;
}

void URectangleBuilderComponent::BeginPlay()
{
    Super::BeginPlay();
}

void URectangleBuilderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update ghost previews if enabled
    if (bShowGhostPreviews && CurrentSuggestions.Num() > 0)
    {
        UpdateGhostPreviews();
    }
}

void URectangleBuilderComponent::OnRimBoardPlaced(ARimBoard* Board)
{
    if (!Board) return;

    TrackedBoards.AddUnique(Board);
    RecalculateState();

    // When L-shape is detected (2 boards at 90°), extend BOTH boards' meshes
    // by HalfWidth on each end so board faces sit flush at the corner.
    if (CurrentState == ERectangleState::LShape && TrackedBoards.Num() == 2)
    {
        TrackedBoards[0]->ExtendMeshForFlushCorners(); // Board 1
        TrackedBoards[1]->ExtendMeshForFlushCorners(); // Board 2
    }

    UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Board placed. Tracking %d boards. State: %d"),
        TrackedBoards.Num(), (int32)CurrentState);
}

void URectangleBuilderComponent::OnRimBoardRemoved(ARimBoard* Board)
{
    if (!Board) return;

    TrackedBoards.Remove(Board);
    RecalculateState();

    UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Board removed. Tracking %d boards."), TrackedBoards.Num());
}

void URectangleBuilderComponent::RecalculateState()
{
    CurrentSuggestions.Empty();

    // Clean up any destroyed boards
    TrackedBoards.RemoveAll([](ARimBoard* B) { return !IsValid(B); });

    int32 BoardCount = TrackedBoards.Num();

    if (BoardCount == 0)
    {
        CurrentState = ERectangleState::None;
        ClearGhostPreviews();
        return;
    }

    if (BoardCount == 1)
    {
        CurrentState = ERectangleState::OneBoard;
        // No suggestions yet — player places board 2 manually with corner snap
        ClearGhostPreviews();
        return;
    }

    if (BoardCount == 2)
    {
        // Check if they form an L-shape (connected at 90 degrees)
        if (AreConnectedAtCorner(TrackedBoards[0], TrackedBoards[1]))
        {
            CurrentState = ERectangleState::LShape;

            // Calculate where board 3 should go
            FBoardSuggestion Suggestion3 = CalculateThirdBoardSuggestion(TrackedBoards[0], TrackedBoards[1]);
            if (Suggestion3.bIsValid)
            {
                CurrentSuggestions.Add(Suggestion3);
            }

            UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: L-shape detected! Suggesting board 3."));
        }
        else
        {
            // Two boards but not forming an L — could be inline extension
            CurrentState = ERectangleState::OneBoard; // Treat as independent
            ClearGhostPreviews();
        }
        return;
    }

    if (BoardCount == 3)
    {
        // Check if they form a U-shape (board1-board2 at corner, board2-board3 at corner or board1-board3)
        bool bIsUShape = false;

        // Check all connection patterns for U-shape
        if (AreConnectedAtCorner(TrackedBoards[0], TrackedBoards[1]) &&
            AreConnectedAtCorner(TrackedBoards[1], TrackedBoards[2]))
        {
            bIsUShape = true;
        }
        else if (AreConnectedAtCorner(TrackedBoards[0], TrackedBoards[1]) &&
                 AreConnectedAtCorner(TrackedBoards[0], TrackedBoards[2]))
        {
            bIsUShape = true;
        }

        if (bIsUShape)
        {
            CurrentState = ERectangleState::UShape;

            // Calculate the closing board
            FBoardSuggestion Suggestion4 = CalculateFourthBoardSuggestion(
                TrackedBoards[0], TrackedBoards[1], TrackedBoards[2]);
            if (Suggestion4.bIsValid)
            {
                CurrentSuggestions.Add(Suggestion4);
            }

            UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: U-shape detected! Suggesting closing board."));
        }
        else
        {
            CurrentState = ERectangleState::LShape; // Partial
            ClearGhostPreviews();
        }
        return;
    }

    if (BoardCount >= 4)
    {
        CurrentState = ERectangleState::Complete;
        ClearGhostPreviews();
        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Rectangle COMPLETE with %d boards."), BoardCount);

        // Print final world positions of all 4 board corners for debugging flush alignment
        for (int32 i = 0; i < TrackedBoards.Num() && i < 4; ++i)
        {
            ARimBoard* Board = TrackedBoards[i];
            if (!Board) continue;

            TArray<FConstructionSocket> BoardSockets = Board->GetAllSockets();
            for (const FConstructionSocket& S : BoardSockets)
            {
                if (S.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;
                FVector WorldPos = Board->GetActorTransform().TransformPosition(S.LocalPosition);
                UE_LOG(LogTemp, Warning, TEXT("  Board %d [%s] socket %s: World=(%.3f, %.3f, %.3f) | ActorPos=(%.3f, %.3f, %.3f) Yaw=%.1f"),
                    i + 1, *Board->GetName(), *S.SocketName.ToString(),
                    WorldPos.X, WorldPos.Y, WorldPos.Z,
                    Board->GetActorLocation().X, Board->GetActorLocation().Y, Board->GetActorLocation().Z,
                    Board->GetActorRotation().Yaw);
            }
        }

        // Calculate joist and plate layouts before resetting tracked boards
        if (TrackedBoards.Num() >= 4)
        {
            CalculateJoistLayout(TrackedBoards[0], TrackedBoards[1], TrackedBoards[2], TrackedBoards[3]);
            CalculatePlateLayout(TrackedBoards[0], TrackedBoards[1], TrackedBoards[2], TrackedBoards[3]);
        }

        // Reset for the next rectangle
        TrackedBoards.Empty();
        CurrentSuggestions.Empty();
        CurrentState = ERectangleState::None;
        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Reset. Ready for new rectangle."));
        return;
    }
}

bool URectangleBuilderComponent::AreConnectedAtCorner(ARimBoard* Board1, ARimBoard* Board2) const
{
    if (!Board1 || !Board2) return false;

    // Check if any corner socket on Board1 is occupied by Board2 (or vice versa)
    TArray<FConstructionSocket> Sockets1 = Board1->GetAllSockets();
    for (const FConstructionSocket& Socket : Sockets1)
    {
        if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
            Socket.bIsOccupied && Socket.ConnectedPiece.Get() == Board2)
        {
            // Verify it's a 90 degree connection by checking yaw difference
            float YawDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(
                Board1->GetActorRotation().Yaw, Board2->GetActorRotation().Yaw));
            // Should be near 90 or 270 degrees for a corner
            if (FMath::Abs(YawDiff - 90.0f) < 10.0f || FMath::Abs(YawDiff - 270.0f) < 10.0f)
            {
                return true;
            }
        }
    }

    // Check reverse direction
    TArray<FConstructionSocket> Sockets2 = Board2->GetAllSockets();
    for (const FConstructionSocket& Socket : Sockets2)
    {
        if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
            Socket.bIsOccupied && Socket.ConnectedPiece.Get() == Board1)
        {
            float YawDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(
                Board1->GetActorRotation().Yaw, Board2->GetActorRotation().Yaw));
            if (FMath::Abs(YawDiff - 90.0f) < 10.0f || FMath::Abs(YawDiff - 270.0f) < 10.0f)
            {
                return true;
            }
        }
    }

    return false;
}

bool URectangleBuilderComponent::DetectLShape(ARimBoard* Board1, ARimBoard* Board2, FVector& OutCornerPos) const
{
    if (!AreConnectedAtCorner(Board1, Board2)) return false;

    // Find the shared corner position
    TArray<FConstructionSocket> Sockets1 = Board1->GetAllSockets();
    for (const FConstructionSocket& Socket : Sockets1)
    {
        if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
            Socket.bIsOccupied && Socket.ConnectedPiece.Get() == Board2)
        {
            OutCornerPos = Board1->GetActorTransform().TransformPosition(Socket.LocalPosition);
            return true;
        }
    }

    return false;
}

FBoardSuggestion URectangleBuilderComponent::CalculateThirdBoardSuggestion(ARimBoard* Board1, ARimBoard* Board2) const
{
    FBoardSuggestion Suggestion;

    if (!Board1 || !Board2) return Suggestion;

    // ACTOR TRANSFORM METHOD:
    // Compute Board 3 from the clean actor positions and rotations of Board 1 and Board 2,
    // NOT from socket world positions (which accumulate snap rounding errors).
    //
    // Board 3 is parallel to Board 1:
    //   - Same rotation as Board 1 (exact Yaw)
    //   - Same position along Board 1's forward axis (same actor X for Yaw=0)
    //   - Offset along Board 2's forward axis by Board 2's effective length
    //
    // Layout:
    //   A ----Board1---- B (shared corner)
    //                     |
    //                   Board2
    //                     |
    //   D ----Board3---- C

    // Board 3 rotation = Board 1 rotation (exactly parallel)
    FRotator Board3Rotation = Board1->GetActorRotation();

    // Board 3 length = Board 1 length
    int32 Board3LengthFeet = Board1->GetBoardLengthFeet();

    // Compute Board 3 center position:
    // Start from Board 1's center, then offset along Board 2's forward direction
    // by Board 2's effective length.
    FVector Board1Center = Board1->GetActorLocation();
    FVector Board2Forward = Board2->GetActorRotation().RotateVector(FVector::ForwardVector);
    float Board2Length = Board2->GetEffectiveLength();

    // Determine which direction Board 2 extends from the shared corner.
    // Board 2's forward might point toward or away from Board 1's shared end.
    // Use the sign: if Board 2's forward points away from Board 1 center, offset in +forward.
    // Otherwise offset in -forward.
    FVector Board2Center = Board2->GetActorLocation();
    FVector Board1ToBoard2 = (Board2Center - Board1Center).GetSafeNormal();
    float ForwardDot = FVector::DotProduct(Board1ToBoard2, Board2Forward);

    // Board 3 center = Board 1 center + Board2Length along the appropriate direction
    FVector OffsetDir = (ForwardDot > 0) ? Board2Forward : -Board2Forward;
    FVector Board3Center = Board1Center + OffsetDir * Board2Length;

    // Find Board 2's open socket name for the connection reference
    FName Board2OpenSocketName = NAME_None;
    FVector SharedCorner = FVector::ZeroVector;
    TArray<FConstructionSocket> Sockets1 = Board1->GetAllSockets();
    for (const FConstructionSocket& S : Sockets1)
    {
        if (S.SocketType == EConstructionSocketType::RimBoard_End_Corner &&
            S.bIsOccupied && S.ConnectedPiece.Get() == Board2)
        {
            SharedCorner = Board1->GetActorTransform().TransformPosition(S.LocalPosition);
            break;
        }
    }
    TArray<FConstructionSocket> Sockets2 = Board2->GetAllSockets();
    float BestDist = -1.0f;
    for (const FConstructionSocket& S : Sockets2)
    {
        if (S.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;
        FVector WorldPos = Board2->GetActorTransform().TransformPosition(S.LocalPosition);
        float Dist = FVector::Dist(WorldPos, SharedCorner);
        if (Dist > BestDist)
        {
            BestDist = Dist;
            Board2OpenSocketName = S.SocketName;
        }
    }

    Suggestion.Position = Board3Center;
    Suggestion.Rotation = Board3Rotation;
    Suggestion.LeftTargetPiece = Board2;
    Suggestion.LeftTargetSocket = Board2OpenSocketName;
    Suggestion.LengthFeet = Board3LengthFeet;
    Suggestion.bIsValid = true;

    UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Board 3 suggestion - Pos=(%.1f, %.1f, %.1f), Rot=%.1f, Len=%dft | B1Center=(%.1f,%.1f,%.1f) B2Center=(%.1f,%.1f,%.1f) B2Fwd=(%.3f,%.3f,%.3f) B2Len=%.1f"),
        Board3Center.X, Board3Center.Y, Board3Center.Z,
        Board3Rotation.Yaw, Board3LengthFeet,
        Board1Center.X, Board1Center.Y, Board1Center.Z,
        Board2Center.X, Board2Center.Y, Board2Center.Z,
        Board2Forward.X, Board2Forward.Y, Board2Forward.Z,
        Board2Length);

    return Suggestion;
}

FBoardSuggestion URectangleBuilderComponent::CalculateFourthBoardSuggestion(
    ARimBoard* Board1, ARimBoard* Board2, ARimBoard* Board3) const
{
    FBoardSuggestion Suggestion;

    if (!Board1 || !Board2 || !Board3) return Suggestion;

    // ACTOR TRANSFORM METHOD:
    // Board 4 is parallel to Board 2:
    //   - Same rotation as Board 2 (exact Yaw)
    //   - Same position along Board 2's forward axis (same actor Y for Yaw=-90)
    //   - Offset along Board 1's forward axis by Board 1's effective length
    //     in the direction from Board 2 toward Board 1's far end
    //
    // Layout:
    //   A ----Board1---- B (shared corner)
    //   |                 |
    // Board4            Board2
    //   |                 |
    //   D ----Board3---- C

    // Board 4 rotation = Board 2 rotation (exactly parallel)
    FRotator Board4Rotation = Board2->GetActorRotation();

    // Board 4 length = Board 2 length
    int32 Board4LengthFeet = Board2->GetBoardLengthFeet();

    // Compute Board 4 center position:
    // Start from Board 2's center, offset along Board 1's forward direction
    // by Board 1's effective length, toward Board 1's far end (away from shared corner).
    FVector Board2Center = Board2->GetActorLocation();
    FVector Board1Forward = Board1->GetActorRotation().RotateVector(FVector::ForwardVector);
    float Board1Length = Board1->GetEffectiveLength();

    // Determine which direction along Board 1 leads to the far end (away from Board 2).
    // The shared corner is near Board 2, so we offset in the OPPOSITE direction.
    FVector Board1Center = Board1->GetActorLocation();
    FVector Board2ToBoard1 = (Board1Center - Board2Center).GetSafeNormal();
    float ForwardDot = FVector::DotProduct(Board2ToBoard1, Board1Forward);

    // Board 4 center = Board 2 center + Board1Length along the direction away from Board 2
    // (toward the far end of Board 1 where Board 4 should be)
    FVector OffsetDir = (ForwardDot > 0) ? Board1Forward : -Board1Forward;
    FVector Board4Center = Board2Center + OffsetDir * Board1Length;

    // Find open sockets on Board 1 and Board 3 for connection references
    // Board 4 connects to Board 1's open end and Board 3's open end
    ARimBoard* LeftBoard = nullptr;
    FName LeftSocket = NAME_None;
    ARimBoard* RightBoard = nullptr;
    FName RightSocket = NAME_None;

    ARimBoard* AllBoards[3] = { Board1, Board2, Board3 };
    for (ARimBoard* Board : AllBoards)
    {
        TArray<FConstructionSocket> Sockets = Board->GetAllSockets();
        for (const FConstructionSocket& S : Sockets)
        {
            if (S.SocketType == EConstructionSocketType::RimBoard_End_Corner && !S.bIsOccupied)
            {
                if (!LeftBoard)
                {
                    LeftBoard = Board;
                    LeftSocket = S.SocketName;
                }
                else if (!RightBoard)
                {
                    RightBoard = Board;
                    RightSocket = S.SocketName;
                }
            }
        }
    }

    if (!LeftBoard || !RightBoard)
    {
        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Could not find 2 open corner sockets for Board 4"));
        return Suggestion;
    }

    Suggestion.Position = Board4Center;
    Suggestion.Rotation = Board4Rotation;
    Suggestion.LengthFeet = Board4LengthFeet;
    Suggestion.LeftTargetPiece = LeftBoard;
    Suggestion.LeftTargetSocket = LeftSocket;
    Suggestion.RightTargetPiece = RightBoard;
    Suggestion.RightTargetSocket = RightSocket;
    Suggestion.bIsValid = true;

    UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Board 4 (closing) suggestion - Pos=(%.1f, %.1f, %.1f), Rot=%.1f, Len=%dft | B2Center=(%.1f,%.1f,%.1f) B1Fwd=(%.3f,%.3f,%.3f) B1Len=%.1f"),
        Board4Center.X, Board4Center.Y, Board4Center.Z,
        Board4Rotation.Yaw, Board4LengthFeet,
        Board2Center.X, Board2Center.Y, Board2Center.Z,
        Board1Forward.X, Board1Forward.Y, Board1Forward.Z,
        Board1Length);

    return Suggestion;
}

bool URectangleBuilderComponent::MatchesSuggestion(const FVector& Position, float Tolerance, FBoardSuggestion& OutSuggestion) const
{
    for (const FBoardSuggestion& Suggestion : CurrentSuggestions)
    {
        if (!Suggestion.bIsValid) continue;

        if (FVector::Dist(Position, Suggestion.Position) < Tolerance)
        {
            OutSuggestion = Suggestion;
            return true;
        }
    }
    return false;
}

TArray<FConstructionSocket> URectangleBuilderComponent::GetOpenCornerSockets(ARimBoard* Board) const
{
    TArray<FConstructionSocket> OpenSockets;
    if (!Board) return OpenSockets;

    TArray<FConstructionSocket> AllSockets = Board->GetAllSockets();
    for (const FConstructionSocket& Socket : AllSockets)
    {
        if (Socket.SocketType == EConstructionSocketType::RimBoard_End_Corner && !Socket.bIsOccupied)
        {
            OpenSockets.Add(Socket);
        }
    }
    return OpenSockets;
}

bool URectangleBuilderComponent::HasActiveSuggestion() const
{
    return (CurrentState == ERectangleState::LShape || CurrentState == ERectangleState::UShape)
        && CurrentSuggestions.Num() > 0
        && CurrentSuggestions[0].bIsValid;
}

FBoardSuggestion URectangleBuilderComponent::GetActiveSuggestion() const
{
    if (HasActiveSuggestion())
    {
        return CurrentSuggestions[0];
    }
    return FBoardSuggestion();
}

bool URectangleBuilderComponent::ApplySuggestionToBoard(ARimBoard* Board)
{
    if (!Board || !HasActiveSuggestion()) return false;

    FBoardSuggestion Suggestion = GetActiveSuggestion();

    // 1. Resize to the suggested length
    if (Suggestion.LengthFeet > 0 && Suggestion.LengthFeet != Board->GetBoardLengthFeet())
    {
        Board->SetBoardLengthFeet(Suggestion.LengthFeet);
        UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Resized board to %d ft"), Suggestion.LengthFeet);
    }

    // 2. Set exact position and rotation — no snap detection involved
    Board->SetActorLocation(Suggestion.Position);
    Board->SetActorRotation(Suggestion.Rotation);

    // 4. Mark as placed (and auto-nail if BP says so)
    Board->SetPreviewMode(false);
    if (Board->ShouldAutoNail())
    {
        Board->NailInPlace();
    }

    // 4b. Extend mesh for flush corners on boards placed via suggestion (3 and 4)
    Board->ExtendMeshForFlushCorners();

    // 5. Occupy target sockets (bidirectional — mark both sides of each connection)
    if (Suggestion.LeftTargetPiece)
    {
        Suggestion.LeftTargetPiece->OccupySocket(Suggestion.LeftTargetSocket, Board);

        // Also mark this board's nearest corner socket as connected to the left target
        TArray<FConstructionSocket> BoardSockets = Board->GetAllSockets();
        FName NearestSocketName = NAME_None;
        float NearestDist = FLT_MAX;
        FVector TargetSocketWorld = Suggestion.LeftTargetPiece->GetActorTransform().TransformPosition(
            Suggestion.LeftTargetPiece->GetSocketByName(Suggestion.LeftTargetSocket)->LocalPosition);
        for (const FConstructionSocket& S : BoardSockets)
        {
            if (S.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;
            FVector WorldPos = Board->GetActorTransform().TransformPosition(S.LocalPosition);
            float Dist = FVector::Dist(WorldPos, TargetSocketWorld);
            if (Dist < NearestDist)
            {
                NearestDist = Dist;
                NearestSocketName = S.SocketName;
            }
        }
        if (NearestSocketName != NAME_None)
        {
            Board->OccupySocket(NearestSocketName, Suggestion.LeftTargetPiece);
        }
    }
    if (Suggestion.RightTargetPiece)
    {
        Suggestion.RightTargetPiece->OccupySocket(Suggestion.RightTargetSocket, Board);

        // Also mark this board's nearest corner socket as connected to the right target
        TArray<FConstructionSocket> BoardSockets = Board->GetAllSockets();
        FName NearestSocketName = NAME_None;
        float NearestDist = FLT_MAX;
        FVector TargetSocketWorld = Suggestion.RightTargetPiece->GetActorTransform().TransformPosition(
            Suggestion.RightTargetPiece->GetSocketByName(Suggestion.RightTargetSocket)->LocalPosition);
        for (const FConstructionSocket& S : BoardSockets)
        {
            if (S.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;
            FVector WorldPos = Board->GetActorTransform().TransformPosition(S.LocalPosition);
            float Dist = FVector::Dist(WorldPos, TargetSocketWorld);
            if (Dist < NearestDist)
            {
                NearestDist = Dist;
                NearestSocketName = S.SocketName;
            }
        }
        if (NearestSocketName != NAME_None)
        {
            Board->OccupySocket(NearestSocketName, Suggestion.RightTargetPiece);
        }
    }

    // 6. Register with PhaseManager
    if (AConstructionPhaseManager::Instance)
    {
        AConstructionPhaseManager::Instance->RegisterPlacedPiece(Board);
    }

    // Debug: Print the final world positions of this board's corner sockets
    TArray<FConstructionSocket> FinalSockets = Board->GetAllSockets();
    for (const FConstructionSocket& S : FinalSockets)
    {
        if (S.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;
        FVector WorldPos = Board->GetActorTransform().TransformPosition(S.LocalPosition);
        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Placed board [%s] socket %s -> World=(%.3f, %.3f, %.3f)"),
            *Board->GetName(), *S.SocketName.ToString(),
            WorldPos.X, WorldPos.Y, WorldPos.Z);
    }

    UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Board placed via suggestion at (%.1f, %.1f, %.1f) Yaw=%.1f"),
        Suggestion.Position.X, Suggestion.Position.Y, Suggestion.Position.Z, Suggestion.Rotation.Yaw);

    return true;
}

void URectangleBuilderComponent::SpawnGhostForSuggestion(const FBoardSuggestion& Suggestion)
{
    if (!Suggestion.bIsValid) return;

    UWorld* World = GetWorld();
    if (!World) return;

    // Spawn a simple actor with a scaled cube mesh as ghost preview
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* Ghost = World->SpawnActor<AActor>(AActor::StaticClass(), Suggestion.Position, Suggestion.Rotation, SpawnParams);
    if (!Ghost) return;

    UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(Ghost);
    MeshComp->SetupAttachment(Ghost->GetRootComponent());
    MeshComp->RegisterComponent();

    // Use engine cube mesh scaled to board dimensions
    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh)
    {
        MeshComp->SetStaticMesh(CubeMesh);

        // Scale to board dimensions (length x width x height)
        float BoardLengthCm = Suggestion.LengthFeet * 30.48f;
        MeshComp->SetRelativeScale3D(FVector(
            BoardLengthCm / 100.0f,
            3.81f / 100.0f,    // Board width (1.5")
            13.97f / 100.0f    // Board height (5.5")
        ));
    }

    // Make it translucent
    if (GhostMaterial)
    {
        MeshComp->SetMaterial(0, GhostMaterial);
    }
    else
    {
        // Create a simple translucent material
        UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(
            MeshComp->GetMaterial(0), Ghost);
        if (DynMat)
        {
            DynMat->SetVectorParameterValue(FName("BaseColor"), FLinearColor(0.2f, 0.5f, 1.0f, 0.3f));
            MeshComp->SetMaterial(0, DynMat);
        }
    }

    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    GhostActors.Add(Ghost);
}

void URectangleBuilderComponent::UpdateGhostPreviews()
{
    // Clear old ghosts and respawn — runs at 4Hz
    ClearGhostPreviews();

    for (const FBoardSuggestion& Suggestion : CurrentSuggestions)
    {
        SpawnGhostForSuggestion(Suggestion);
    }
}

void URectangleBuilderComponent::ClearGhostPreviews()
{
    for (AActor* Ghost : GhostActors)
    {
        if (Ghost)
        {
            Ghost->Destroy();
        }
    }
    GhostActors.Empty();
}

void URectangleBuilderComponent::CalculateJoistLayout(ARimBoard* Board1, ARimBoard* Board2, ARimBoard* Board3, ARimBoard* Board4)
{
    if (!Board1 || !Board2 || !Board3 || !Board4) return;

    JoistSuggestions.Empty();
    PlacedJoistCount = 0;

    // Board1 and Board3 are the "through" boards (parallel to each other)
    // Board2 and Board4 are the "end" boards (perpendicular)
    // Joists run perpendicular to the through boards, from Board1 to Board3
    ThroughBoard1 = Board1;
    ThroughBoard3 = Board3;

    // Through boards share the same rotation — joists are perpendicular
    FRotator ThroughRotation = Board1->GetActorRotation();
    FVector ThroughForward = ThroughRotation.RotateVector(FVector::ForwardVector);

    // Joist rotation: perpendicular to through boards
    FRotator JoistRotation = Board2->GetActorRotation();

    // Calculate the span (distance between Board1 and Board3 centers along the perpendicular axis)
    FVector Board1Center = Board1->GetActorLocation();
    FVector Board3Center = Board3->GetActorLocation();
    FVector Board2Forward = Board2->GetActorRotation().RotateVector(FVector::ForwardVector);

    float SpanDistance = FMath::Abs(FVector::DotProduct(Board3Center - Board1Center, Board2Forward));

    // Joist length = span between inside faces of Board1 and Board3
    // Subtract one BoardWidth (the joists butt up against the inside faces)
    float JoistSpanCm = SpanDistance - Board1->BoardWidth;
    int32 JoistLengthFeet = FMath::RoundToInt(JoistSpanCm / 30.48f);
    JoistLengthFeet = FMath::Clamp(JoistLengthFeet, 1, 16);

    // Joist Z position: same center as rim boards so tops are flush
    float JoistZ = Board1Center.Z;

    // 16" OC spacing along the through boards
    float Spacing = 40.64f; // 16" = 40.64cm
    float ThroughLength = Board1->GetEffectiveLength();
    float HalfThroughLen = ThroughLength / 2.0f;

    // Start from one end, offset by the spacing from the end board
    // Standard framing: first joist at 16" from the end, then every 16"
    float StartOffset = Spacing;
    float CurrentOffset = -HalfThroughLen + StartOffset;

    // Center of joist span (midpoint between Board1 and Board3)
    FVector SpanCenter = (Board1Center + Board3Center) / 2.0f;
    // Project SpanCenter along the through-board direction at each offset
    FVector SpanPerp = Board2Forward; // Direction from Board1 toward Board3

    int32 JoistIndex = 0;
    while (CurrentOffset < HalfThroughLen - StartOffset / 2.0f)
    {
        FVector JoistCenter = SpanCenter + ThroughForward * CurrentOffset;
        JoistCenter.Z = JoistZ;

        // Find the matching top-face socket names on Board1 and Board3
        FName Board1Socket = FName(*FString::Printf(TEXT("TopFace_%d"), JoistIndex));
        FName Board3Socket = FName(*FString::Printf(TEXT("TopFace_%d"), JoistIndex));

        FJoistSuggestion Suggestion;
        Suggestion.Position = JoistCenter;
        Suggestion.Rotation = JoistRotation;
        Suggestion.LengthFeet = JoistLengthFeet;
        Suggestion.Board1 = Board1;
        Suggestion.Board3 = Board3;
        Suggestion.Board1TargetSocket = Board1Socket;
        Suggestion.Board3TargetSocket = Board3Socket;
        Suggestion.JoistIndex = JoistIndex;
        Suggestion.bIsValid = true;

        JoistSuggestions.Add(Suggestion);

        CurrentOffset += Spacing;
        JoistIndex++;
    }

    UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Calculated %d joist positions at 16\" OC. Span=%.1fcm, JoistLen=%dft, Z=%.1f"),
        JoistSuggestions.Num(), JoistSpanCm, JoistLengthFeet, JoistZ);
}

FJoistSuggestion URectangleBuilderComponent::GetNextJoistSuggestion() const
{
    if (PlacedJoistCount < JoistSuggestions.Num())
    {
        return JoistSuggestions[PlacedJoistCount];
    }
    return FJoistSuggestion();
}

bool URectangleBuilderComponent::ApplyJoistSuggestion(AFloorJoist* Joist)
{
    if (!Joist || !HasJoistSuggestions()) return false;

    FJoistSuggestion Suggestion = GetNextJoistSuggestion();
    if (!Suggestion.bIsValid) return false;

    // Resize joist to match the span
    if (Suggestion.LengthFeet > 0 && Suggestion.LengthFeet != Joist->GetBoardLengthFeet())
    {
        Joist->SetBoardLengthFeet(Suggestion.LengthFeet);
    }

    // Set position and rotation
    Joist->SetActorLocation(Suggestion.Position);
    Joist->SetActorRotation(Suggestion.Rotation);

    // Mark as placed (and auto-nail if BP says so)
    Joist->SetPreviewMode(false);
    if (Joist->ShouldAutoNail())
    {
        Joist->NailInPlace();
    }

    // Occupy top-face sockets on the through boards
    if (Suggestion.Board1)
    {
        ABuildablePiece* Board1Piece = Cast<ABuildablePiece>(Suggestion.Board1);
        if (Board1Piece)
        {
            Board1Piece->OccupySocket(Suggestion.Board1TargetSocket, Joist);
        }
    }
    if (Suggestion.Board3)
    {
        ABuildablePiece* Board3Piece = Cast<ABuildablePiece>(Suggestion.Board3);
        if (Board3Piece)
        {
            Board3Piece->OccupySocket(Suggestion.Board3TargetSocket, Joist);
        }
    }

    // Register with PhaseManager
    if (AConstructionPhaseManager::Instance)
    {
        AConstructionPhaseManager::Instance->RegisterPlacedPiece(Joist);
    }

    PlacedJoistCount++;

    UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Placed joist %d/%d at (%.1f, %.1f, %.1f) Yaw=%.1f"),
        PlacedJoistCount, JoistSuggestions.Num(),
        Suggestion.Position.X, Suggestion.Position.Y, Suggestion.Position.Z,
        Suggestion.Rotation.Yaw);

    return true;
}

void URectangleBuilderComponent::CalculatePlateLayout(ARimBoard* Board1, ARimBoard* Board2, ARimBoard* Board3, ARimBoard* Board4)
{
    if (!Board1 || !Board2 || !Board3 || !Board4) return;

    PlateSuggestions.Empty();
    PlacedPlateCount = 0;

    // Store all 4 rim boards for reference
    CompletedRimBoards.Empty();
    CompletedRimBoards.Add(Board1);
    CompletedRimBoards.Add(Board2);
    CompletedRimBoards.Add(Board3);
    CompletedRimBoards.Add(Board4);

    const float PlateHalfHeight = 8.89f / 2.0f;      // 3.5" / 2 = 4.445cm

    // --- Find the actual plywood top Z from placed plywood actors ---
    // The plate must sit ON TOP of the plywood, not under it.
    // Formula: plate center Z = plywood top Z + PlateHalfHeight
    //          plywood top Z  = plywood actor Z + plywood half thickness
    float PlywoodTopZ = 0.0f;
    bool bFoundPlywood = false;

    if (AConstructionPhaseManager::Instance)
    {
        TArray<ABuildablePiece*> PlywoodPieces =
            AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::Plywood);

        // Find the highest plywood top surface (in case multiple sheets)
        for (ABuildablePiece* Piece : PlywoodPieces)
        {
            APlywoodSheet* Ply = Cast<APlywoodSheet>(Piece);
            if (!Ply) continue;

            float PlyActorZ = Ply->GetActorLocation().Z;
            float PlyHalfThickness = Ply->SheetThickness / 2.0f;
            float ThisTopZ = PlyActorZ + PlyHalfThickness;

            UE_LOG(LogTemp, Warning, TEXT("PLATE_Z_DEBUG: Found plywood '%s' ActorZ=%.3f  HalfThickness=%.3f  TopZ=%.3f"),
                *Ply->GetName(), PlyActorZ, PlyHalfThickness, ThisTopZ);

            if (!bFoundPlywood || ThisTopZ > PlywoodTopZ)
            {
                PlywoodTopZ = ThisTopZ;
            }
            bFoundPlywood = true;
        }
    }

    // Fallback: if no plywood placed yet, estimate from rim board Z
    float FallbackZOffset = 0.0f;
    if (!bFoundPlywood)
    {
        const float RimBoardHalfHeight = 13.97f / 2.0f;  // 6.985cm
        const float PlywoodThickness = 1.905f;            // 3/4" = 1.905cm
        FallbackZOffset = RimBoardHalfHeight + PlywoodThickness + PlateHalfHeight;
        UE_LOG(LogTemp, Warning, TEXT("PLATE_Z_DEBUG: No plywood found! Using fallback ZOffset=%.3f from rim board center"), FallbackZOffset);
    }

    // Inward Y offset: shift plate toward the building center so its outer face
    // aligns with the plywood outer edge (not overhanging).
    const float PlateHalfWidth = 3.81f / 2.0f;       // 1.905cm
    const float FlushTweak = 0.47625f;               // 3/16" extra inward
    const float InwardOffset = PlateHalfWidth + FlushTweak;

    // Rectangle center — used to determine "inward" direction for each board
    FVector RectCenter = (Board1->GetActorLocation() + Board2->GetActorLocation() +
                          Board3->GetActorLocation() + Board4->GetActorLocation()) / 4.0f;

    ARimBoard* Boards[4] = { Board1, Board2, Board3, Board4 };

    for (int32 i = 0; i < 4; i++)
    {
        ARimBoard* Board = Boards[i];

        // Compute inward direction: perpendicular to the board, pointing toward rectangle center
        FVector BoardRight = Board->GetActorRotation().RotateVector(FVector::RightVector);
        FVector ToCenter = RectCenter - Board->GetActorLocation();
        float Dot = FVector::DotProduct(ToCenter, BoardRight);
        FVector InwardDir = BoardRight * FMath::Sign(Dot);

        // Calculate plate center Z from actual plywood top surface
        float PlateCenterZ;
        if (bFoundPlywood)
        {
            PlateCenterZ = PlywoodTopZ + PlateHalfHeight;
        }
        else
        {
            PlateCenterZ = Board->GetActorLocation().Z + FallbackZOffset;
        }

        FPlateSuggestion Suggestion;
        FVector BoardXY = Board->GetActorLocation();
        Suggestion.Position = FVector(BoardXY.X, BoardXY.Y, PlateCenterZ) + InwardDir * InwardOffset;
        Suggestion.Rotation = Board->GetActorRotation();
        Suggestion.LengthFeet = Board->GetBoardLengthFeet();
        Suggestion.SourceRimBoard = Board;
        Suggestion.PlateIndex = i;
        Suggestion.bIsValid = true;

        PlateSuggestions.Add(Suggestion);

        UE_LOG(LogTemp, Warning, TEXT("PLATE_Z_DEBUG: Plate %d  RimBoardZ=%.3f  PlywoodTopZ=%.3f  PlateCenterZ=%.3f  PlatePos=(%.1f, %.1f, %.1f)"),
            i, Board->GetActorLocation().Z, PlywoodTopZ, PlateCenterZ,
            Suggestion.Position.X, Suggestion.Position.Y, Suggestion.Position.Z);
    }

    UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Calculated %d bottom plate positions (PlywoodFound=%d, PlywoodTopZ=%.3f, PlateHalfH=%.3f)"),
        PlateSuggestions.Num(), bFoundPlywood, PlywoodTopZ, PlateHalfHeight);
}

FPlateSuggestion URectangleBuilderComponent::GetNextPlateSuggestion() const
{
    if (PlacedPlateCount < PlateSuggestions.Num())
    {
        return PlateSuggestions[PlacedPlateCount];
    }
    return FPlateSuggestion();
}

bool URectangleBuilderComponent::ApplyPlateSuggestion(ABottomPlate* Plate)
{
    if (!Plate || !HasPlateSuggestions()) return false;

    FPlateSuggestion Suggestion = GetNextPlateSuggestion();
    if (!Suggestion.bIsValid) return false;

    // Resize plate to match the rim board below
    if (Suggestion.LengthFeet > 0 && Suggestion.LengthFeet != Plate->GetBoardLengthFeet())
    {
        Plate->SetBoardLengthFeet(Suggestion.LengthFeet);
    }

    // --- Recalculate plate Z from actual plywood actors ---
    // CalculatePlateLayout runs when the rectangle completes (before plywood is placed),
    // so the Z in the suggestion is a fallback estimate. Now that plywood is placed,
    // recalculate Z from the real plywood top surface.
    const float PlateHalfHeight = 8.89f / 2.0f; // 4.445cm
    FVector PlatePos = Suggestion.Position;

    if (AConstructionPhaseManager::Instance)
    {
        TArray<ABuildablePiece*> PlywoodPieces =
            AConstructionPhaseManager::Instance->GetPiecesOfType(EPieceType::Plywood);

        float BestPlywoodTopZ = 0.0f;
        bool bFoundPlywood = false;

        for (ABuildablePiece* Piece : PlywoodPieces)
        {
            APlywoodSheet* Ply = Cast<APlywoodSheet>(Piece);
            if (!Ply) continue;

            float PlyTopZ = Ply->GetActorLocation().Z - Ply->SheetThickness / 2.0f;

            if (!bFoundPlywood || PlyTopZ > BestPlywoodTopZ)
            {
                BestPlywoodTopZ = PlyTopZ;
            }
            bFoundPlywood = true;
        }

        if (bFoundPlywood)
        {
            float NewZ = BestPlywoodTopZ + PlateHalfHeight;
            UE_LOG(LogTemp, Warning, TEXT("PLATE_Z_FIX: PlywoodTopZ=%.3f + PlateHalfH=%.3f = NewZ=%.3f (was %.3f, delta=%.3f)"),
                BestPlywoodTopZ, PlateHalfHeight, NewZ, PlatePos.Z, NewZ - PlatePos.Z);
            PlatePos.Z = NewZ;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PLATE_Z_FIX: No plywood found at placement time! Using suggestion Z=%.3f"), PlatePos.Z);
        }
    }

    // Set position and rotation (on top of the plywood, above the rim board)
    Plate->SetActorLocation(PlatePos);
    Plate->SetActorRotation(Suggestion.Rotation);

    // Mark as placed (and auto-nail if BP says so)
    Plate->SetPreviewMode(false);
    if (Plate->ShouldAutoNail())
    {
        Plate->NailInPlace();
    }

    // Extend mesh for flush corners (same visual fix as rim boards)
    Plate->ExtendMeshForFlushCorners();

    // Register with PhaseManager
    if (AConstructionPhaseManager::Instance)
    {
        AConstructionPhaseManager::Instance->RegisterPlacedPiece(Plate);
    }

    // Track placed plates for stud layout
    PlacedBottomPlates.Add(Plate);
    PlacedPlateCount++;

    UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Placed bottom plate %d/%d at (%.1f, %.1f, %.1f) Yaw=%.1f, %dft"),
        PlacedPlateCount, PlateSuggestions.Num(),
        PlatePos.X, PlatePos.Y, PlatePos.Z,
        Suggestion.Rotation.Yaw, Suggestion.LengthFeet);

    // After all plates are placed, calculate stud layout
    if (PlacedPlateCount >= PlateSuggestions.Num())
    {
        CalculateStudLayout();
    }

    return true;
}

void URectangleBuilderComponent::CalculateStudLayout()
{
    StudSuggestions.Empty();
    PlacedStudCount = 0;

    if (PlacedBottomPlates.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: CalculateStudLayout - No placed bottom plates!"));
        return;
    }

    // Default stud height: 92-5/8" = 235.27cm
    const float DefaultStudHeightCm = 235.27f;
    const float StudWidthCm = 3.81f; // 1.5" stud width (along wall)

    int32 TotalStudIndex = 0;

    for (ABottomPlate* Plate : PlacedBottomPlates)
    {
        if (!Plate) continue;

        FRotator PlateRotation = Plate->GetActorRotation();

        // Interior studs at 16" OC — placed at each Wall_Bottom_Plate socket on the plate.
        // Corner posts (separate piece type) handle the plate ends.
        TArray<FConstructionSocket> PlateSockets = Plate->GetAllSockets();
        for (const FConstructionSocket& Socket : PlateSockets)
        {
            if (Socket.SocketType != EConstructionSocketType::Wall_Bottom_Plate) continue;

            FVector SocketWorldPos = Plate->GetActorTransform().TransformPosition(Socket.LocalPosition);
            FVector StudCenter = SocketWorldPos;
            StudCenter.Z += DefaultStudHeightCm / 2.0f;

            FStudSuggestion Sug;
            Sug.Position = StudCenter;
            Sug.Rotation = PlateRotation;
            Sug.StudHeightCm = DefaultStudHeightCm;
            Sug.SourcePlate = Plate;
            Sug.PlateSocketName = Socket.SocketName;
            Sug.StudIndex = TotalStudIndex;
            Sug.bIsValid = true;
            StudSuggestions.Add(Sug);
            TotalStudIndex++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Calculated %d wall stud positions (16\" OC interior only) across %d plates"),
        StudSuggestions.Num(), PlacedBottomPlates.Num());
}

FStudSuggestion URectangleBuilderComponent::GetNextStudSuggestion() const
{
    if (PlacedStudCount < StudSuggestions.Num())
    {
        return StudSuggestions[PlacedStudCount];
    }
    return FStudSuggestion();
}

bool URectangleBuilderComponent::ApplyStudSuggestion(AWallStud* Stud)
{
    if (!Stud) return false;
    if (!HasStudSuggestions()) return false;

    FStudSuggestion Suggestion = StudSuggestions[PlacedStudCount];
    if (!Suggestion.bIsValid) return false;

    // Set stud height if different from default
    if (!FMath::IsNearlyEqual(Suggestion.StudHeightCm, Stud->GetStudHeightCm(), 0.1f))
    {
        Stud->SetStudHeightInches(Suggestion.StudHeightCm / 2.54f);
    }

    // Set position and rotation
    Stud->SetActorLocation(Suggestion.Position);
    Stud->SetActorRotation(Suggestion.Rotation);

    // Mark as placed
    Stud->SetPreviewMode(false);
    if (Stud->ShouldAutoNail())
    {
        Stud->NailInPlace();
    }

    // Occupy the Wall_Bottom_Plate socket on the source plate
    if (Suggestion.SourcePlate && Suggestion.PlateSocketName != NAME_None)
    {
        Suggestion.SourcePlate->OccupySocket(Suggestion.PlateSocketName, Stud);
        Stud->OccupySocket(FName(TEXT("StudBottom")), Suggestion.SourcePlate);
    }

    // Register with PhaseManager
    if (AConstructionPhaseManager::Instance)
    {
        AConstructionPhaseManager::Instance->RegisterPlacedPiece(Stud);
    }

    PlacedStudCount++;

    UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Placed wall stud %d/%d at (%.1f, %.1f, %.1f) Yaw=%.1f"),
        PlacedStudCount, StudSuggestions.Num(),
        Suggestion.Position.X, Suggestion.Position.Y, Suggestion.Position.Z,
        Suggestion.Rotation.Yaw);

    // After all studs are placed, calculate top plate layout
    if (PlacedStudCount >= StudSuggestions.Num())
    {
        CalculateTopPlateLayout();
    }

    return true;
}

// =============================================================================
// Top Plate Layout
// =============================================================================

void URectangleBuilderComponent::CalculateTopPlateLayout()
{
    TopPlateSuggestions.Empty();
    PlacedTopPlateCount = 0;

    if (PlacedBottomPlates.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: CalculateTopPlateLayout - No placed bottom plates!"));
        return;
    }

    // Top plate dimensions (same 2x4 as bottom plate)
    const float PlateHalfHeight = 8.89f / 2.0f;  // 4.445cm
    const float BottomPlateHeight = 8.89f;         // 3.5"
    const float DefaultStudHeight = 235.27f;       // 92-5/8"

    for (int32 i = 0; i < PlacedBottomPlates.Num(); i++)
    {
        ABottomPlate* BotPlate = PlacedBottomPlates[i];
        if (!BotPlate) continue;

        // Top plate center Z = bottom plate center Z + bottom plate half height + stud height + top plate half height
        // This puts the top plate sitting directly on top of the studs
        float TopPlateZ = BotPlate->GetActorLocation().Z + (BottomPlateHeight / 2.0f) + DefaultStudHeight + PlateHalfHeight;

        FTopPlateSuggestion Sug;
        Sug.Position = FVector(BotPlate->GetActorLocation().X, BotPlate->GetActorLocation().Y, TopPlateZ);
        Sug.Rotation = BotPlate->GetActorRotation();
        Sug.LengthFeet = BotPlate->GetBoardLengthFeet();
        Sug.SourceBottomPlate = BotPlate;
        Sug.PlateIndex = i;
        Sug.bIsValid = true;

        TopPlateSuggestions.Add(Sug);

        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: TopPlate %d suggestion - Pos=(%.1f, %.1f, %.1f) Rot=%.1f Len=%dft (BotPlateZ=%.1f)"),
            i, Sug.Position.X, Sug.Position.Y, Sug.Position.Z,
            Sug.Rotation.Yaw, Sug.LengthFeet, BotPlate->GetActorLocation().Z);
    }

    UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Calculated %d top plate positions (above wall studs)"),
        TopPlateSuggestions.Num());
}

FTopPlateSuggestion URectangleBuilderComponent::GetNextTopPlateSuggestion() const
{
    if (PlacedTopPlateCount < TopPlateSuggestions.Num())
    {
        return TopPlateSuggestions[PlacedTopPlateCount];
    }
    return FTopPlateSuggestion();
}

bool URectangleBuilderComponent::ApplyTopPlateSuggestion(ATopPlate* Plate)
{
    if (!Plate || !HasTopPlateSuggestions()) return false;

    FTopPlateSuggestion Suggestion = GetNextTopPlateSuggestion();
    if (!Suggestion.bIsValid) return false;

    // Resize to match the bottom plate below
    if (Suggestion.LengthFeet > 0 && Suggestion.LengthFeet != Plate->GetBoardLengthFeet())
    {
        Plate->SetBoardLengthFeet(Suggestion.LengthFeet);
    }

    // Set position and rotation
    Plate->SetActorLocation(Suggestion.Position);
    Plate->SetActorRotation(Suggestion.Rotation);

    // Mark as placed
    Plate->SetPreviewMode(false);
    if (Plate->ShouldAutoNail())
    {
        Plate->NailInPlace();
    }

    // Extend mesh for flush corners (same visual fix as bottom plates)
    Plate->ExtendMeshForFlushCorners();

    // Register with PhaseManager
    if (AConstructionPhaseManager::Instance)
    {
        AConstructionPhaseManager::Instance->RegisterPlacedPiece(Plate);
    }

    // Track placed top plates
    PlacedTopPlates.Add(Plate);
    PlacedTopPlateCount++;

    UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Placed top plate %d/%d at (%.1f, %.1f, %.1f) Yaw=%.1f, %dft"),
        PlacedTopPlateCount, TopPlateSuggestions.Num(),
        Suggestion.Position.X, Suggestion.Position.Y, Suggestion.Position.Z,
        Suggestion.Rotation.Yaw, Suggestion.LengthFeet);

    // After all top plates are placed, calculate double top plate layout
    if (PlacedTopPlateCount >= TopPlateSuggestions.Num())
    {
        CalculateDoubleTopPlateLayout();
    }

    return true;
}

// =============================================================================
// Double Top Plate Layout
// =============================================================================

void URectangleBuilderComponent::CalculateDoubleTopPlateLayout()
{
    DoubleTopPlateSuggestions.Empty();
    PlacedDoubleTopPlateCount = 0;

    if (PlacedTopPlates.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: CalculateDoubleTopPlateLayout - No placed top plates!"));
        return;
    }

    // Double top plate sits directly on top of the first top plate
    const float TopPlateHeight = 8.89f;           // 3.5"
    const float DblPlateHalfHeight = 8.89f / 2.0f; // 4.445cm
    const float OverlapCm = 8.89f;                 // 3.5" overlap at corners

    for (int32 i = 0; i < PlacedTopPlates.Num(); i++)
    {
        ATopPlate* FirstPlate = PlacedTopPlates[i];
        if (!FirstPlate) continue;

        // Double top plate center Z = first top plate center Z + topPlateHalfHeight + dblPlateHalfHeight
        float DblPlateZ = FirstPlate->GetActorLocation().Z + (TopPlateHeight / 2.0f) + DblPlateHalfHeight;

        // Overlap pattern: walls at index 0 and 2 overlap at BOTH ends (extend 3.5" past each end)
        // Walls at index 1 and 3 are the same length as their first top plate (no overlap)
        bool bHasOverlap = (i == 0 || i == 2);
        float BaseLengthCm = FirstPlate->GetBoardLengthFeet() * 30.48f;
        float DblPlateLengthCm = bHasOverlap ? (BaseLengthCm + 2.0f * OverlapCm) : BaseLengthCm;

        FDoubleTopPlateSuggestion Sug;
        Sug.Position = FVector(FirstPlate->GetActorLocation().X, FirstPlate->GetActorLocation().Y, DblPlateZ);
        Sug.Rotation = FirstPlate->GetActorRotation();
        Sug.LengthCm = DblPlateLengthCm;
        Sug.PlateIndex = i;
        Sug.bIsValid = true;

        DoubleTopPlateSuggestions.Add(Sug);

        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: DoubleTopPlate %d suggestion - Pos=(%.1f, %.1f, %.1f) Rot=%.1f Len=%.1fcm (overlap=%d, base=%.1fcm)"),
            i, Sug.Position.X, Sug.Position.Y, Sug.Position.Z,
            Sug.Rotation.Yaw, Sug.LengthCm, bHasOverlap, BaseLengthCm);
    }

    UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Calculated %d double top plate positions (stacked on first top plates)"),
        DoubleTopPlateSuggestions.Num());
}

FDoubleTopPlateSuggestion URectangleBuilderComponent::GetNextDoubleTopPlateSuggestion() const
{
    if (PlacedDoubleTopPlateCount < DoubleTopPlateSuggestions.Num())
    {
        return DoubleTopPlateSuggestions[PlacedDoubleTopPlateCount];
    }
    return FDoubleTopPlateSuggestion();
}

bool URectangleBuilderComponent::ApplyDoubleTopPlateSuggestion(ADoubleTopPlate* Plate)
{
    if (!Plate || !HasDoubleTopPlateSuggestions()) return false;

    FDoubleTopPlateSuggestion Suggestion = GetNextDoubleTopPlateSuggestion();
    if (!Suggestion.bIsValid) return false;

    // Set length in cm (may not be a round number of feet due to overlap)
    Plate->SetBoardLengthCm(Suggestion.LengthCm);

    // Set position and rotation
    Plate->SetActorLocation(Suggestion.Position);
    Plate->SetActorRotation(Suggestion.Rotation);

    // Mark as placed
    Plate->SetPreviewMode(false);
    if (Plate->ShouldAutoNail())
    {
        Plate->NailInPlace();
    }

    // Register with PhaseManager
    if (AConstructionPhaseManager::Instance)
    {
        AConstructionPhaseManager::Instance->RegisterPlacedPiece(Plate);
    }

    PlacedDoubleTopPlateCount++;

    UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Placed double top plate %d/%d at (%.1f, %.1f, %.1f) Yaw=%.1f, %.1fcm"),
        PlacedDoubleTopPlateCount, DoubleTopPlateSuggestions.Num(),
        Suggestion.Position.X, Suggestion.Position.Y, Suggestion.Position.Z,
        Suggestion.Rotation.Yaw, Suggestion.LengthCm);

    return true;
}

