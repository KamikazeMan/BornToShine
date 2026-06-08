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

	// Per-vessel Z fine-tune (Thumper/Barrel are separate meshes with possibly different pivots).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	float ThumperZAdjust = 13.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	float BarrelZAdjust = 13.5f;

	// Local offset from a vessel's origin to where its cap mounts (caps snap onto the placed vessel).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	FVector CapMountOffset = FVector(0.0f, 0.0f, 109.2f);        // Pot origin -> Cap mount

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	FVector ThumperCapMountOffset = FVector(0.0f, 0.0f, 78.13f); // ThumperBody origin -> ThumperCap mount

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	FVector CapArmMountOffset = FVector(70.69f, 0.0f, 12.0f);    // Cap origin -> CapArm mount

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	FRotator CapArmMountRotation = FRotator(0.0f, 0.0f, 0.0f);   // fine rotation tweak so cap arm aligns with thumper cap

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	FVector OutletPipeMountOffset = FVector(59.73f, 0.0f, 78.0f); // ThumperBody origin -> OutletPipe mount

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	FVector WormCoilMountOffset = FVector(-34.72f, 0.0f, 0.0f);   // WormBarrel origin -> WormCoil mount

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	FVector MasonJarMountOffset = FVector(48.67f, 0.285f, -49.57f); // WormBarrel origin -> MasonJar mount

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	FRotator MasonJarMountRotation = FRotator(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	FVector MasonJarLidMountOffset = FVector(-0.025f, 0.01f, 6.93f); // MasonJar origin -> MasonJarLid mount

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	FRotator MasonJarLidMountRotation = FRotator(0.0f, 0.0f, 0.0f);

	// Raises floor-placed still parts so a center-pivot mesh sits ON the floor instead of half-buried.
	// 7.62cm = half the 6-inch stand height (starting guess).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	float FloorSpawnZOffset = 7.62f;

	// Grid cell size for snapping floor-placed stands (CinderBlockStand ghost).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	float StandGridSizeCm = 100.0f;

	// Fixed yaw applied to the CinderBlockStand when placed (tune in PIE; try 90 or -90).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	float StandPlacementYaw = 90.0f;

	// Max distance (cm) the player can aim to reach a snap point; also the trace length.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	float MaxAimDistanceCm = 500.0f;

	// Perpendicular ray-to-mount distance (cm) for a snap to be valid.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Placement")
	float StillSnapRadiusCm = 35.0f;

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
	bool bGhostFloorGridMode = false;        // true: floor-grid placement (stand); false: mount-snap (vessel)
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

	// Finds the single placed CinderBlockStand actor (nullptr if none).
	class AStillPartActor* FindPlacedStand() const;

	// Finds the first placed still part with the given PartID (nullptr if none).
	class AStillPartActor* FindPlacedPart(FName PartID) const;

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
