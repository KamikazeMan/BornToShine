// Born To Shine - Player Controller

#include "MoonshinePlayerController.h"
#include "ConstructionPhaseManager.h"
#include "BuildablePiece.h"
#include "RimBoard.h"
#include "FloorJoist.h"
#include "BottomPlate.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

AMoonshinePlayerController::AMoonshinePlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableTouchEvents = false;
	BuildModeWidget = nullptr;
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

	// Dev quick save/load bindings
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &AMoonshinePlayerController::QuickSave);
		InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &AMoonshinePlayerController::QuickLoad);
	}
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
// Dev Quick Save (F5) — serialize all placed pieces to JSON
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

		Loaded++;
	}

	FString Msg = FString::Printf(TEXT("Quick-loaded %d pieces"), Loaded);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, Msg);
	UE_LOG(LogTemp, Warning, TEXT("QuickLoad: %s from %s"), *Msg, *SavePath);
}
