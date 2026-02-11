// Born To Shine - Player Controller

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MoonshinePlayerController.generated.h"

class ABuildablePiece;
class URadialPieceMenu;

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

	// Toggle delete mode on/off (F7)
	void ToggleDeleteMode();

	// Is delete mode active?
	bool IsDeleteModeActive() const { return bDeleteModeActive; }

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

	// Dev quick save/load (F6/F9)
	void QuickSave();
	void QuickLoad();

	// Per-tick highlight: line trace from camera to find piece under crosshair
	void UpdatePieceHighlight();

	// Clear any current piece highlight
	void ClearHighlight();

	// Delete mode toggle state (F7)
	bool bDeleteModeActive;

	// The piece currently highlighted (under crosshair)
	UPROPERTY()
	TWeakObjectPtr<ABuildablePiece> HighlightedPiece;

	// Line trace distance for piece detection
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delete")
	float DeleteTraceDistance;

	// --- Post-load helpers ---
	void RestoreSocketConnections(TArray<ABuildablePiece*>& LoadedPieces);
	void RestoreRectangleBuilderState(TArray<ABuildablePiece*>& LoadedPieces);

	// --- Radial piece selection menu (Tab) ---
	void OpenRadialMenu();
	void CloseRadialMenu();

	UPROPERTY()
	URadialPieceMenu* RadialMenu;

	bool bRadialMenuOpen;

	// UI Widget references
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UUserWidget> BuildModeWidgetClass;

	UPROPERTY()
	class UUserWidget* BuildModeWidget;
};
