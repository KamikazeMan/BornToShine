// Born To Shine - Rectangle Builder Component

#include "RectangleBuilder.h"
#include "RimBoard.h"
#include "BuildablePiece.h"
#include "ConstructionPhaseManager.h"
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

    // PARALLELOGRAM METHOD:
    // Given an L-shape with 3 known corners (SharedCorner, Board1FarEnd, Board2FarEnd),
    // the 4th corner is: D = A + C - B (parallelogram property).
    // Board3 goes from C to D, with center at midpoint.
    //
    // Layout:
    //   A ----Board1---- B (shared corner)
    //                     |
    //                   Board2
    //                     |
    //   D ----Board3---- C

    // Step 1: Find the 3 corner positions from socket world positions.
    // SharedCorner (B): Board1's socket connected to Board2
    // Board1FarEnd (A): Board1's open socket
    // Board2FarEnd (C): Board2's open socket

    FVector SharedCorner = FVector::ZeroVector;   // B
    FVector Board1FarEnd = FVector::ZeroVector;    // A
    FVector Board2FarEnd = FVector::ZeroVector;    // C
    FName Board2OpenSocketName = NAME_None;
    bool bFoundShared = false;
    bool bFoundBoard1Far = false;
    bool bFoundBoard2Far = false;

    // Find Board1's connected (B) and open (A) corner sockets
    TArray<FConstructionSocket> Sockets1 = Board1->GetAllSockets();
    for (const FConstructionSocket& S : Sockets1)
    {
        if (S.SocketType != EConstructionSocketType::RimBoard_End_Corner) continue;
        if (S.bIsOccupied && S.ConnectedPiece.Get() == Board2)
        {
            SharedCorner = Board1->GetActorTransform().TransformPosition(S.LocalPosition);
            bFoundShared = true;
        }
        else if (!S.bIsOccupied)
        {
            Board1FarEnd = Board1->GetActorTransform().TransformPosition(S.LocalPosition);
            bFoundBoard1Far = true;
        }
    }

    // Find Board2's far (C) corner socket.
    // NOTE: Board2's sockets may NOT be marked as occupied (CommitPlacement only marks
    // the target piece's socket, not the source piece's). So we can't rely on bIsOccupied.
    // Instead, pick the corner socket FARTHEST from SharedCorner — that's the far end.
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
            Board2FarEnd = WorldPos;
            Board2OpenSocketName = S.SocketName;
            bFoundBoard2Far = true;
        }
    }

    if (!bFoundShared || !bFoundBoard1Far || !bFoundBoard2Far)
    {
        UE_LOG(LogTemp, Warning, TEXT("RectangleBuilder: Could not find all 3 corners (Shared=%d, B1Far=%d, B2Far=%d)"),
            bFoundShared, bFoundBoard1Far, bFoundBoard2Far);
        return Suggestion;
    }

    // Step 2: Compute 4th corner using parallelogram property: D = A + C - B
    FVector FourthCorner = Board1FarEnd + Board2FarEnd - SharedCorner;

    // Step 3: Board3 center is midpoint of C and D
    FVector Board3Center = (Board2FarEnd + FourthCorner) / 2.0f;

    // Step 4: Apply flush offset to tighten corners.
    // Sockets are at centerline (Y=0), so the parallelogram gives us centerline positions.
    // Board3 needs to shift perpendicular to its length, toward the inside of the
    // rectangle, by HalfWidth (1.905cm) to create flush corner joints.
    // The "inward" direction is from Board2's far end toward the shared corner (along Board2).
    FVector InwardDir = (SharedCorner - Board2FarEnd).GetSafeNormal();
    float FlushOffset = Board1->GetBoardHalfWidth(); // 1.905cm for 2x6
    Board3Center += InwardDir * FlushOffset;

    // Board3 is parallel to Board1 (same rotation)
    FRotator Board3Rotation = Board1->GetActorRotation();

    // Board3's length matches Board1
    int32 Board3LengthFeet = Board1->GetBoardLengthFeet();

    Suggestion.Position = Board3Center;
    Suggestion.Rotation = Board3Rotation;
    Suggestion.LeftTargetPiece = Board2;
    Suggestion.LeftTargetSocket = Board2OpenSocketName;
    Suggestion.LengthFeet = Board3LengthFeet;
    Suggestion.bIsValid = true;

    UE_LOG(LogTemp, Log, TEXT("RectangleBuilder: Board 3 suggestion - Pos=(%.1f, %.1f, %.1f), Rot=%.1f, Len=%dft | Corners: Shared=(%.1f,%.1f,%.1f) B1Far=(%.1f,%.1f,%.1f) B2Far=(%.1f,%.1f,%.1f) 4th=(%.1f,%.1f,%.1f)"),
        Board3Center.X, Board3Center.Y, Board3Center.Z,
        Board3Rotation.Yaw, Board3LengthFeet,
        SharedCorner.X, SharedCorner.Y, SharedCorner.Z,
        Board1FarEnd.X, Board1FarEnd.Y, Board1FarEnd.Z,
        Board2FarEnd.X, Board2FarEnd.Y, Board2FarEnd.Z,
        FourthCorner.X, FourthCorner.Y, FourthCorner.Z);

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

    // Apply flush offset to tighten corners.
    // Board4 needs to shift inward (toward the opposite side of the rectangle)
    // by HalfWidth (1.905cm). The inward direction is perpendicular to the span,
    // pointing toward Board2 (the opposite side).
    FVector Perp = FVector(-SpanDir.Y, SpanDir.X, 0.0f);
    FVector ToBoard2 = Board2->GetActorLocation() - MidPoint;
    if (FVector::DotProduct(Perp, ToBoard2) < 0.0f)
    {
        Perp = -Perp;
    }
    float FlushOffset = Board2->GetBoardHalfWidth(); // 1.905cm for 2x6
    MidPoint += Perp * FlushOffset;

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

    // 3. Mark as placed
    Board->SetPreviewMode(false);

    // 4. Occupy target sockets (bidirectional — mark both sides of each connection)
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

    // 5. Register with PhaseManager
    if (AConstructionPhaseManager::Instance)
    {
        AConstructionPhaseManager::Instance->RegisterPlacedPiece(Board);
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
