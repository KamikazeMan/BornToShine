// Born To Shine - Player Controller

#include "MoonshinePlayerController.h"
#include "ConstructionTypes.h"
#include "ConstructionPhaseManager.h"
#include "BuildablePiece.h"
#include "BuildingComponent.h"
#include "RimBoard.h"
#include "FloorJoist.h"
#include "BottomPlate.h"
#include "SocketManager.h"
#include "RectangleBuilder.h"
#include "MoonshineCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"

AMoonshinePlayerController::AMoonshinePlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableTouchEvents = false;
	BuildModeWidget = nullptr;
	DeleteTraceDistance = 2000.0f; // 20 meters
	bDeleteModeActive = false;
}

void AMoonshinePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Set input mode to game only
	SetInputMode(FInputModeGameOnly());
}

void AMoonshinePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Dev quick save/load bindings (F6/F9 — F5 conflicts with UE5 shader complexity view)
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &AMoonshinePlayerController::QuickSave);
		InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &AMoonshinePlayerController::QuickLoad);
		InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &AMoonshinePlayerController::ToggleDeleteMode);
		// X key delete is routed through MoonshineCharacter_Simple::OnToggleBoardType
		// (Enhanced Input consumes X before legacy BindKey fires)
	}
}

// ---------------------------------------------------------------------------
// Per-tick: update highlight + check piece under crosshair
// ---------------------------------------------------------------------------
void AMoonshinePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bDeleteModeActive)
	{
		UpdatePieceHighlight();

		// Show delete mode indicator
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(43, 0.0f, FColor::Red, TEXT("** DELETE MODE (F7 to exit) **"));
		}
	}
}

void AMoonshinePlayerController::UpdatePieceHighlight()
{
	// Get camera view
	FVector CamLoc;
	FRotator CamRot;
	GetPlayerViewPoint(CamLoc, CamRot);

	FVector TraceEnd = CamLoc + CamRot.Vector() * DeleteTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetPawn());

	// Also ignore the preview piece — otherwise it blocks the trace and
	// prevents placed pieces behind it from being highlighted/deleted.
	if (APawn* MyPawn = GetPawn())
	{
		if (UBuildingComponent* BC = MyPawn->FindComponentByClass<UBuildingComponent>())
		{
			if (ABuildablePiece* Preview = BC->GetCurrentPreviewPiece())
			{
				Params.AddIgnoredActor(Preview);
			}
		}
	}

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, Params);

	ABuildablePiece* HitPiece = nullptr;
	if (bHit)
	{
		HitPiece = Cast<ABuildablePiece>(Hit.GetActor());

		// Only highlight placed or nailed pieces, not preview pieces
		if (HitPiece && HitPiece->GetPieceState() == EPieceState::Preview)
		{
			HitPiece = nullptr;
		}
	}

	// Update highlight state
	ABuildablePiece* CurrentHighlight = HighlightedPiece.Get();

	if (CurrentHighlight != HitPiece)
	{
		// Unhighlight old piece
		if (CurrentHighlight && CurrentHighlight->IsHighlighted())
		{
			CurrentHighlight->SetHighlighted(false);
		}

		// Highlight new piece
		if (HitPiece)
		{
			HitPiece->SetHighlighted(true);
		}

		HighlightedPiece = HitPiece;
	}

	// Show delete hint when hovering over a piece
	if (HitPiece && GEngine)
	{
		FString PieceName = UEnum::GetDisplayValueAsText(HitPiece->GetPieceType()).ToString();
		FString PieceStateName = UEnum::GetDisplayValueAsText(HitPiece->GetPieceState()).ToString();
		// In delete mode, X always works (even on nailed pieces)
		FString Hint = FString::Printf(TEXT("[X] Delete: %s (%s)"), *PieceName, *PieceStateName);
		GEngine->AddOnScreenDebugMessage(42, 0.0f, FColor::Yellow, Hint);
	}
}

// ---------------------------------------------------------------------------
// Delete System (X key)
// ---------------------------------------------------------------------------
void AMoonshinePlayerController::OnDeletePressed()
{
	// Called from MoonshineCharacter::OnToggleBoardType when X key
	// is NOT being used for board type toggle (already filtered there).

	ABuildablePiece* Target = HighlightedPiece.Get();
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnDeletePressed: No highlighted piece to delete"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, TEXT("No piece targeted - look at a piece first"));
		return;
	}

	bool bShiftHeld = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);

	if (Target->GetPieceState() == EPieceState::Nailed)
	{
		// In delete mode (F7), allow deleting nailed pieces with just X
		// Outside delete mode, require Shift+X as a safety measure
		if (!bDeleteModeActive && !bShiftHeld)
		{
			FString Msg = TEXT("Piece is nailed! Use F7 delete mode, or hold Shift+X.");
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, Msg);
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("Deleting nailed piece: %s"), *Target->GetName());
	}

	// Clear highlight before removing
	Target->SetHighlighted(false);
	HighlightedPiece = nullptr;

	// Remove the piece (handles socket cleanup, PhaseManager unregister, and Destroy)
	FString PieceName = Target->GetName();
	Target->Remove();

	FString Msg = FString::Printf(TEXT("Deleted: %s"), *PieceName);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, Msg);
	UE_LOG(LogTemp, Log, TEXT("Deleted piece: %s"), *PieceName);
}

// ---------------------------------------------------------------------------
// Toggle Delete Mode (F7)
// ---------------------------------------------------------------------------
void AMoonshinePlayerController::ToggleDeleteMode()
{
	bDeleteModeActive = !bDeleteModeActive;

	if (bDeleteModeActive)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Delete Mode ON - Look at a piece and press X to delete"));
		UE_LOG(LogTemp, Log, TEXT("Delete Mode: ENABLED"));
	}
	else
	{
		ClearHighlight();
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Delete Mode OFF"));
		UE_LOG(LogTemp, Log, TEXT("Delete Mode: DISABLED"));
	}
}

void AMoonshinePlayerController::ClearHighlight()
{
	ABuildablePiece* CurrentHL = HighlightedPiece.Get();
	if (CurrentHL && CurrentHL->IsHighlighted())
	{
		CurrentHL->SetHighlighted(false);
	}
	HighlightedPiece = nullptr;
}

void AMoonshinePlayerController::ShowBuildModeUI()
{
	if (BuildModeWidgetClass && !BuildModeWidget)
	{
		BuildModeWidget = CreateWidget<UUserWidget>(this, BuildModeWidgetClass);
		if (BuildModeWidget)
		{
			BuildModeWidget->AddToViewport();
		}
	}
	else if (BuildModeWidget)
	{
		BuildModeWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void AMoonshinePlayerController::HideBuildModeUI()
{
	if (BuildModeWidget)
	{
		BuildModeWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

FString AMoonshinePlayerController::GetCurrentPhaseDescription() const
{
	if (AConstructionPhaseManager::Instance)
	{
		return AConstructionPhaseManager::Instance->GetPhaseRequirements();
	}
	return TEXT("Phase Manager not found");
}

// ---------------------------------------------------------------------------
// Dev Quick Save (F6) — serialize all placed pieces to JSON
// ---------------------------------------------------------------------------
void AMoonshinePlayerController::QuickSave()
{
	if (!AConstructionPhaseManager::Instance)
	{
		UE_LOG(LogTemp, Error, TEXT("QuickSave: No PhaseManager"));
		return;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> PiecesArray;

	// Walk every piece type and serialize each placed piece
	for (uint8 t = 0; t <= (uint8)EPieceType::Rafter; t++)
	{
		TArray<ABuildablePiece*> Pieces =
			AConstructionPhaseManager::Instance->GetPiecesOfType((EPieceType)t);

		for (ABuildablePiece* Piece : Pieces)
		{
			if (!Piece) continue;

			TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();

			// Blueprint class path (so we spawn the right BP on load)
			Obj->SetStringField(TEXT("class"), Piece->GetClass()->GetPathName());

			// Transform
			FVector Pos = Piece->GetActorLocation();
			FRotator Rot = Piece->GetActorRotation();
			Obj->SetNumberField(TEXT("px"), Pos.X);
			Obj->SetNumberField(TEXT("py"), Pos.Y);
			Obj->SetNumberField(TEXT("pz"), Pos.Z);
			Obj->SetNumberField(TEXT("rPitch"), Rot.Pitch);
			Obj->SetNumberField(TEXT("rYaw"),   Rot.Yaw);
			Obj->SetNumberField(TEXT("rRoll"),  Rot.Roll);

			// Piece type + state
			Obj->SetNumberField(TEXT("type"),  (int32)Piece->GetPieceType());
			Obj->SetNumberField(TEXT("state"), (int32)Piece->GetPieceState());

			// Per-type properties
			if (ARimBoard* Rim = Cast<ARimBoard>(Piece))
			{
				Obj->SetNumberField(TEXT("lengthFeet"), Rim->GetBoardLengthFeet());
				Obj->SetBoolField(TEXT("isOutside"),    Rim->bIsOutsideBoard);
			}
			else if (ABottomPlate* Plate = Cast<ABottomPlate>(Piece))
			{
				Obj->SetNumberField(TEXT("lengthFeet"), Plate->GetBoardLengthFeet());
			}

			PiecesArray.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}

	Root->SetArrayField(TEXT("pieces"), PiecesArray);

	// Write to Saved/DevSave.json
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Root, Writer);

	FString SavePath = FPaths::ProjectSavedDir() / TEXT("DevSave.json");
	FFileHelper::SaveStringToFile(OutputString, *SavePath);

	FString Msg = FString::Printf(TEXT("Quick-saved %d pieces"), PiecesArray.Num());
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, Msg);
	UE_LOG(LogTemp, Warning, TEXT("QuickSave: %s -> %s"), *Msg, *SavePath);
}

// ---------------------------------------------------------------------------
// Dev Quick Load (F9) — destroy all pieces, then respawn from JSON
// ---------------------------------------------------------------------------
void AMoonshinePlayerController::QuickLoad()
{
	FString SavePath = FPaths::ProjectSavedDir() / TEXT("DevSave.json");
	FString JsonString;

	if (!FFileHelper::LoadFileToString(JsonString, *SavePath))
	{
		FString Msg = TEXT("No save file found (Saved/DevSave.json)");
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, Msg);
		UE_LOG(LogTemp, Warning, TEXT("QuickLoad: %s"), *Msg);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Failed to parse save file"));
		return;
	}

	// Clear highlight if active
	if (ABuildablePiece* HL = HighlightedPiece.Get())
	{
		HL->SetHighlighted(false);
		HighlightedPiece = nullptr;
	}

	// ---- Destroy all existing placed pieces ----
	if (AConstructionPhaseManager::Instance)
	{
		for (uint8 t = 0; t <= (uint8)EPieceType::Rafter; t++)
		{
			// GetPiecesOfType returns a copy, safe to iterate while destroying
			TArray<ABuildablePiece*> Pieces =
				AConstructionPhaseManager::Instance->GetPiecesOfType((EPieceType)t);
			for (ABuildablePiece* Piece : Pieces)
			{
				if (Piece)
				{
					AConstructionPhaseManager::Instance->UnregisterPiece(Piece);
					Piece->Destroy();
				}
			}
		}
	}

	// ---- Respawn from save data ----
	const TArray<TSharedPtr<FJsonValue>>& PiecesArray = Root->GetArrayField(TEXT("pieces"));
	TArray<ABuildablePiece*> LoadedPieces;
	int32 Loaded = 0;

	for (const TSharedPtr<FJsonValue>& Val : PiecesArray)
	{
		TSharedPtr<FJsonObject> Obj = Val->AsObject();
		if (!Obj.IsValid()) continue;

		// Resolve the Blueprint class
		FString ClassPath = Obj->GetStringField(TEXT("class"));
		UClass* PieceClass = StaticLoadClass(ABuildablePiece::StaticClass(), nullptr, *ClassPath);
		if (!PieceClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("QuickLoad: Could not load class %s"), *ClassPath);
			continue;
		}

		FVector Pos(
			Obj->GetNumberField(TEXT("px")),
			Obj->GetNumberField(TEXT("py")),
			Obj->GetNumberField(TEXT("pz"))
		);
		FRotator Rot(
			Obj->GetNumberField(TEXT("rPitch")),
			Obj->GetNumberField(TEXT("rYaw")),
			Obj->GetNumberField(TEXT("rRoll"))
		);

		ABuildablePiece* Piece = GetWorld()->SpawnActor<ABuildablePiece>(PieceClass, Pos, Rot);
		if (!Piece) continue;

		// Restore per-type properties (length, board type) BEFORE extending mesh
		EPieceType PType = (EPieceType)(int32)Obj->GetNumberField(TEXT("type"));

		if (ARimBoard* Rim = Cast<ARimBoard>(Piece))
		{
			bool bWantOutside = Obj->GetBoolField(TEXT("isOutside"));
			if (bWantOutside != Rim->bIsOutsideBoard)
			{
				Rim->ToggleBoardType();
			}
			int32 Len = (int32)Obj->GetNumberField(TEXT("lengthFeet"));
			if (Len > 0 && Len != Rim->GetBoardLengthFeet())
			{
				Rim->SetBoardLengthFeet(Len);
			}
			// Extend mesh for flush corners (rim boards only, not joists)
			if (PType == EPieceType::RimBoard)
			{
				Rim->ExtendMeshForFlushCorners();
			}
		}
		else if (ABottomPlate* Plate = Cast<ABottomPlate>(Piece))
		{
			int32 Len = (int32)Obj->GetNumberField(TEXT("lengthFeet"));
			if (Len > 0 && Len != Plate->GetBoardLengthFeet())
			{
				Plate->SetBoardLengthFeet(Len);
			}
			Plate->ExtendMeshForFlushCorners();
		}

		// Take out of preview mode (makes it solid + visible)
		Piece->SetPreviewMode(false);

		// Restore nailed state
		EPieceState SavedState = (EPieceState)(int32)Obj->GetNumberField(TEXT("state"));
		if (SavedState == EPieceState::Nailed)
		{
			Piece->NailInPlace();
		}

		// Register with phase manager
		if (AConstructionPhaseManager::Instance)
		{
			AConstructionPhaseManager::Instance->RegisterPlacedPiece(Piece);
		}

		LoadedPieces.Add(Piece);
		Loaded++;
	}

	// ---- POST-LOAD: Restore socket connections and RectangleBuilder state ----
	RestoreSocketConnections(LoadedPieces);
	RestoreRectangleBuilderState(LoadedPieces);

	FString Msg = FString::Printf(TEXT("Quick-loaded %d pieces (sockets restored)"), Loaded);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, Msg);
	UE_LOG(LogTemp, Warning, TEXT("QuickLoad: %s from %s"), *Msg, *SavePath);
}

// ---------------------------------------------------------------------------
// Post-load: Restore bidirectional socket connections via proximity matching
// ---------------------------------------------------------------------------
void AMoonshinePlayerController::RestoreSocketConnections(TArray<ABuildablePiece*>& LoadedPieces)
{
	int32 ConnectionsRestored = 0;

	for (int32 i = 0; i < LoadedPieces.Num(); i++)
	{
		ABuildablePiece* PieceA = LoadedPieces[i];
		if (!PieceA) continue;

		TArray<FConstructionSocket> SocketsA = PieceA->GetAllSockets();

		for (int32 j = i + 1; j < LoadedPieces.Num(); j++)
		{
			ABuildablePiece* PieceB = LoadedPieces[j];
			if (!PieceB) continue;

			TArray<FConstructionSocket> SocketsB = PieceB->GetAllSockets();

			for (const FConstructionSocket& SA : SocketsA)
			{
				if (SA.bIsOccupied) continue;

				FVector WorldPosA = PieceA->GetActorTransform().TransformPosition(SA.LocalPosition);

				for (const FConstructionSocket& SB : SocketsB)
				{
					if (SB.bIsOccupied) continue;

					FVector WorldPosB = PieceB->GetActorTransform().TransformPosition(SB.LocalPosition);

					float Dist = FVector::Dist(WorldPosA, WorldPosB);

					// Check if within snap tolerance (5cm)
					if (Dist < 5.0f)
					{
						// Verify these socket types can actually connect
						if (ASocketManager::Instance &&
							ASocketManager::Instance->AreSocketsCompatible(
								SA.SocketType, SB.SocketType,
								AConstructionPhaseManager::Instance ? AConstructionPhaseManager::Instance->GetCurrentPhase() : EConstructionPhase::Foundation))
						{
							PieceA->OccupySocket(SA.SocketName, PieceB);
							PieceB->OccupySocket(SB.SocketName, PieceA);
							ConnectionsRestored++;
						}
					}
				}
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("QuickLoad: Restored %d socket connections"), ConnectionsRestored);
}

// ---------------------------------------------------------------------------
// Post-load: Restore RectangleBuilder state from loaded rim boards
// ---------------------------------------------------------------------------
void AMoonshinePlayerController::RestoreRectangleBuilderState(TArray<ABuildablePiece*>& LoadedPieces)
{
	// Find the RectangleBuilder component on the player's pawn
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return;

	URectangleBuilderComponent* RectBuilder = MyPawn->FindComponentByClass<URectangleBuilderComponent>();
	if (!RectBuilder) return;

	// Collect loaded rim boards (not joists — joists inherit from ARimBoard
	// but have PieceType == FloorJoist)
	TArray<ARimBoard*> LoadedRimBoards;
	for (ABuildablePiece* Piece : LoadedPieces)
	{
		if (Piece && Piece->GetPieceType() == EPieceType::RimBoard)
		{
			ARimBoard* Rim = Cast<ARimBoard>(Piece);
			if (Rim)
			{
				LoadedRimBoards.Add(Rim);
			}
		}
	}

	// If we have exactly 4 rim boards, simulate the rectangle completion
	// by calling OnRimBoardPlaced for each one in order. The RectangleBuilder
	// will detect L-shape, U-shape, and Complete states, then auto-calculate
	// joist and plate suggestions.
	if (LoadedRimBoards.Num() >= 4)
	{
		// Feed the first 4 boards to the RectangleBuilder
		for (int32 i = 0; i < 4 && i < LoadedRimBoards.Num(); i++)
		{
			RectBuilder->OnRimBoardPlaced(LoadedRimBoards[i]);
		}

		UE_LOG(LogTemp, Warning, TEXT("QuickLoad: Fed %d rim boards to RectangleBuilder (state=%d)"),
			FMath::Min(LoadedRimBoards.Num(), 4), (int32)RectBuilder->GetRectangleState());
	}
}
