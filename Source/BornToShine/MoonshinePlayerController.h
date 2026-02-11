// Born To Shine - Player Controller

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MoonshinePlayerController.generated.h"

class ABuildablePiece;

/**
 * Custom Player Controller for Born To Shine
 * Handles input mapping, UI management, delete system, and dev save/load
 */
UCLASS()
class BORNTOSHINE_API AMoonshinePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMoonshinePlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	// Delete the currently highlighted piece (called from character's X key handler)
	void OnDeletePressed();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// Show/hide build UI
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowBuildModeUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideBuildModeUI();

	// Get current construction phase
	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetCurrentPhaseDescription() const;

	// Dev quick save/load (F5/F9)
	void QuickSave();
	void QuickLoad();

	// Per-tick highlight: line trace from camera to find piece under crosshair
	void UpdatePieceHighlight();

	// The piece currently highlighted (under crosshair)
	UPROPERTY()
	TWeakObjectPtr<ABuildablePiece> HighlightedPiece;

	// Line trace distance for piece detection
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delete")
	float DeleteTraceDistance;

	// --- Post-load helpers ---
	void RestoreSocketConnections(TArray<ABuildablePiece*>& LoadedPieces);
	void RestoreRectangleBuilderState(TArray<ABuildablePiece*>& LoadedPieces);

	// UI Widget references
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UUserWidget> BuildModeWidgetClass;

	UPROPERTY()
	class UUserWidget* BuildModeWidget;
};
