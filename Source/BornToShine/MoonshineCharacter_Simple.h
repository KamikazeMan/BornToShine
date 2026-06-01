// Born To Shine - Simplified Player Character using BuildingComponent

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "InventoryComponent.h"
#include "InventoryGridWidget.h"
#include "MoonshineCharacter_Simple.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class AStillPartActor;

/**
 * Simplified player character that uses BuildingComponent for all construction logic
 * Much cleaner architecture - character handles movement, component handles building
 */
UCLASS()
class BORNTOSHINE_API AMoonshineCharacter_Simple : public ACharacter
{
	GENERATED_BODY()

public:
	AMoonshineCharacter_Simple();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	UInventoryComponent* Inventory;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	TSubclassOf<UInventoryGridWidget> InventoryWidgetClass;

	UPROPERTY()
	UInventoryGridWidget* InventoryWidgetInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	UTexture2D* InventoryBackgroundTexture;

	UPROPERTY()
	bool bIsPlacingItem = false;

	UPROPERTY()
	FName PendingPlacementItemID;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void BeginItemPlacement(FName ItemID);

	UFUNCTION()
	void ConfirmItemPlacement();   // called on world click while placing

	// --- Still part assembly (ghost-preview snapping) ---

	// All still parts placed in the world (used to find the stand for snapping).
	UPROPERTY()
	TArray<class AStillPartActor*> PlacedStillParts;

	// Optional translucent material for the ghost preview; falls back to engine default if unset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Still")
	UMaterialInterface* GhostPreviewMaterial;

	// Z fine-tune added to the Pot snap so its pivot rests on the stand top (dial without recompiling logic).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Still")
	float PotZAdjust = 0.0f;

	// Raises floor-placed still parts so a center-pivot mesh sits ON the floor instead of half-buried.
	// 7.62cm = half the 6-inch stand height (starting guess).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	float FloorSpawnZOffset = 7.62f;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void ToggleInventoryUI();

	UFUNCTION()
	void DebugGrantStillParts();

	UFUNCTION()
	void DebugDumpInventory();

	// Toggle between first and third person view
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ToggleCameraMode();

	// Get current camera mode
	UFUNCTION(BlueprintCallable, Category = "Camera")
	bool IsFirstPerson() const { return bIsFirstPerson; }

protected:
	virtual void BeginPlay() override;

	// --- Still ghost preview state ---
	bool bIsPlacingStillGhost = false;       // true while previewing a snap-able still part (e.g. Pot)
	FName GhostPartID;                        // which part the ghost represents
	bool bGhostSnapValid = false;             // is the ghost currently within snap range of its mount?
	FTransform GhostSnapTransform;            // the snapped world transform when valid

	UPROPERTY()
	class AStillPartActor* GhostStillPart = nullptr;     // the live ghost actor

	UPROPERTY()
	UMaterialInstanceDynamic* GhostDynamicMaterial = nullptr;

	// Begin/Update/Confirm/Cancel for the still ghost preview flow.
	void BeginStillGhostPlacement(FName PartID);
	void UpdateStillGhost();                  // called each Tick while previewing
	void ConfirmStillGhostPlacement();        // called on click while previewing
	void CancelStillGhost();                  // tears down the ghost actor

	// Finds the most recently placed CinderBlockStand in PlacedStillParts (nullptr if none).
	class AStillPartActor* FindPlacedStand() const;

	// Tint helper for the ghost (green = valid snap, red = invalid).
	void SetGhostColor(const FLinearColor& Color);

	// Input callbacks - Movement
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Jump();
	void Sprint();
	void StopSprinting();

	// Input callbacks - Building (delegates to BuildingComponent)
	void OnToggleBuildMode();
	void OnPlacePiece();
	void OnRotate(const FInputActionValue& Value); // Handles 2D rotation input
	void OnRotateLeft();
	void OnRotateRight();
	void OnScalePiece(const FInputActionValue& Value);
	void OnCyclePieceType();
	void OnNailPiece();
	void OnAdvancePhase(); // Advance to next construction phase
	void OnToggleBoardType(); // Toggle between outside and inside board for rim boards
	void OnZoomStart();  // Hold RMB to zoom in
	void OnZoomStop();   // Release RMB to zoom out
	void HandleToggleInventoryAction(const FInputActionValue& Value);

	// Enhanced Input Actions
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* ToggleCameraAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* ToggleBuildModeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* PlacePieceAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* RotateAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* RotateLeftAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* RotateRightAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* ScaleAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* CyclePieceAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* NailPieceAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* AdvancePhaseAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* ToggleBoardTypeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* ZoomAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	class UInputAction* ToggleInventoryAction;

	// Camera components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* ThirdPersonArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* ThirdPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* FirstPersonCamera;

	// Building Component (handles all construction logic)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	class UBuildingComponent* BuildingComponent;

	// Camera settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bIsFirstPerson;

	// Movement settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SprintSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MouseSensitivity;

	// Zoom settings (right mouse button hold)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float DefaultFOV;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomedFOV;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomInterpSpeed;

	bool bIsZooming;
};
