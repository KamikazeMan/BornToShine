// Born To Shine - Rectangle Builder Component

#include "RectangleBuilder.h"
#include "RimBoard.h"
#include "BuildablePiece.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

URectangleBuilderComponent::URectangleBuilderComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.25f; // Only update 4x per second, not every frame

    CurrentState = ERectangleState::None;
    bShowGhostPreviews = true;
    GhostMaterial = nullptr;
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
        UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Rectangle COMPLETE with %d boards."), BoardCount);
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

    // GOAL: Board 3 is parallel to Board 1, starts at Board 2's far end.
    //
    // L-shape layout (example):
    //   Board1 runs along X-axis
    //   Board2 runs along Y-axis, connected at Board1's right end
    //   Board3 should run parallel to Board1 (same yaw), starting at Board2's far end
    //
    // We need:
    //   1. Board2's open (far) corner socket world position = Board3's connecting end
    //   2. Board1's rotation = Board3's rotation (parallel)
    //   3. Board3's actor position = corner pos + offset to board center along Board1's direction

    // Find the connected and open corner sockets on Board2
    TArray<FConstructionSocket> Sockets2 = Board2->GetAllSockets();
    FConstructionSocket* ConnectedSocket2 = nullptr;
    FConstructionSocket* OpenSocket2 = nullptr;

    for (FConstructionSocket& S : Sockets2)
    {
        if (S.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;
        if (S.bIsOccupied && S.ConnectedPiece.Get() == Board1)
        {
            ConnectedSocket2 = &S;
        }
        else if (!S.bIsOccupied)
        {
            OpenSocket2 = &S;
        }
    }

    if (!OpenSocket2) return Suggestion;

    // Board2's open corner = where Board3's connecting end goes
    FVector Board2OpenCornerWorld = Board2->GetActorTransform().TransformPosition(OpenSocket2->LocalPosition);

    // Board3 is parallel to Board1
    FRotator Board3Rotation = Board1->GetActorRotation();

    // Determine which direction Board3 should extend from the corner.
    // Board1's open corner tells us which way the rectangle is "growing".
    // Board3 should extend in the SAME direction as Board1 (from its connected end toward its open end).
    TArray<FConstructionSocket> Sockets1 = Board1->GetAllSockets();
    FConstructionSocket* ConnectedSocket1 = nullptr;
    FConstructionSocket* OpenSocket1 = nullptr;

    for (FConstructionSocket& S : Sockets1)
    {
        if (S.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;
        if (S.bIsOccupied && S.ConnectedPiece.Get() == Board2)
        {
            ConnectedSocket1 = &S;
        }
        else if (!S.bIsOccupied)
        {
            OpenSocket1 = &S;
        }
    }

    if (!ConnectedSocket1 || !OpenSocket1) return Suggestion;

    // Direction Board1 extends: from connected end toward open end (in world space)
    FVector Board1ConnectedWorld = Board1->GetActorTransform().TransformPosition(ConnectedSocket1->LocalPosition);
    FVector Board1OpenWorld = Board1->GetActorTransform().TransformPosition(OpenSocket1->LocalPosition);
    FVector Board1Direction = (Board1OpenWorld - Board1ConnectedWorld).GetSafeNormal();

    // Board3's length matches Board1
    int32 Board3LengthFeet = Board1->GetBoardLengthFeet();
    float Board3HalfLength = (Board3LengthFeet * 30.48f) / 2.0f;

    // Board3's connecting end is at Board2's open corner.
    // The connecting socket is at -HalfLen along the board's local X-axis.
    // So the actor center = corner position + Board1Direction * HalfLength
    // (Board3 extends away from the corner in the same direction as Board1)
    FVector Board3Center = Board2OpenCornerWorld + Board1Direction * Board3HalfLength;

    Suggestion.Position = Board3Center;
    Suggestion.Rotation = Board3Rotation;
    Suggestion.LeftTargetPiece = Board2;
    Suggestion.LeftTargetSocket = OpenSocket2->SocketName;
    Suggestion.LengthFeet = Board3LengthFeet;
    Suggestion.bIsValid = true;

    UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Board 3 suggestion - Pos=(%.1f, %.1f, %.1f), Rot=%.1f, Len=%dft, parallel to Board1"),
        Suggestion.Position.X, Suggestion.Position.Y, Suggestion.Position.Z,
        Suggestion.Rotation.Yaw, Suggestion.LengthFeet);

    return Suggestion;
}

FBoardSuggestion URectangleBuilderComponent::CalculateFourthBoardSuggestion(
    ARimBoard* Board1, ARimBoard* Board2, ARimBoard* Board3) const
{
    FBoardSuggestion Suggestion;

    if (!Board1 || !Board2 || !Board3) return Suggestion;

    // Find the two remaining open corner sockets across all three boards
    // These are where the 4th board connects
    struct FOpenCorner
    {
        ARimBoard* Board;
        FName SocketName;
        FVector WorldPos;
    };

    TArray<FOpenCorner> OpenCorners;

    ARimBoard* AllBoards[3] = { Board1, Board2, Board3 };
    for (ARimBoard* Board : AllBoards)
    {
        TArray<FConstructionSocket> Sockets = Board->GetAllSockets();
        for (const FConstructionSocket& S : Sockets)
        {
            if (S.SocketType == EConstructionSocketType::RimBoard_End_Corner && !S.bIsOccupied)
            {
                FOpenCorner Corner;
                Corner.Board = Board;
                Corner.SocketName = S.SocketName;
                Corner.WorldPos = Board->GetActorTransform().TransformPosition(S.LocalPosition);
                OpenCorners.Add(Corner);
            }
        }
    }

    // Need exactly 2 open corners for the closing board
    if (OpenCorners.Num() != 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Expected 2 open corners, found %d"), OpenCorners.Num());
        return Suggestion;
    }

    FVector Pos1 = OpenCorners[0].WorldPos;
    FVector Pos2 = OpenCorners[1].WorldPos;

    // Direction from corner 1 to corner 2
    FVector SpanDir = (Pos2 - Pos1).GetSafeNormal();
    float SpanDist = FVector::Dist(Pos1, Pos2);

    // Calculate length in feet (1 foot = 30.48 cm)
    int32 LengthFeet = FMath::RoundToInt(SpanDist / 30.48f);
    LengthFeet = FMath::Clamp(LengthFeet, 1, 16);

    // Board 4 rotation aligns with the span
    FRotator Board4Rot = SpanDir.Rotation();

    // Position is midpoint of the two open corners
    FVector MidPoint = (Pos1 + Pos2) / 2.0f;

    Suggestion.Position = MidPoint;
    Suggestion.Rotation = Board4Rot;
    Suggestion.LengthFeet = LengthFeet;
    Suggestion.LeftTargetPiece = OpenCorners[0].Board;
    Suggestion.LeftTargetSocket = OpenCorners[0].SocketName;
    Suggestion.RightTargetPiece = OpenCorners[1].Board;
    Suggestion.RightTargetSocket = OpenCorners[1].SocketName;
    Suggestion.bIsValid = true;

    UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Board 4 (closing) suggestion - Pos=(%.1f, %.1f, %.1f), Len=%dft"),
        Suggestion.Position.X, Suggestion.Position.Y, Suggestion.Position.Z, LengthFeet);

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

void URectangleBuilderComponent::UpdateGhostPreviews()
{
    // Ghost previews are rendered by spawning translucent board meshes
    // at suggested positions. This runs at 4Hz (TickInterval = 0.25).

    // For now, just log suggestions. Full ghost mesh spawning requires
    // a reference to the rim board mesh asset, which should be set in Blueprint.
    // The BuildingComponent can query GetGhostPreviews() to render them.
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
