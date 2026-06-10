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

/** Operating state of a completed still. Linear progression Empty -> ... -> Done. */
UENUM(BlueprintType)
enum class EStillState : uint8
{
	Empty   UMETA(DisplayName="Empty"),
	Water   UMETA(DisplayName="Water Added"),
	Mash    UMETA(DisplayName="Mash Added"),
	Lit     UMETA(DisplayName="Fire Lit"),
	Running UMETA(DisplayName="Distilling"),
	Done    UMETA(DisplayName="Batch Complete")
};

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

	// Current player money (read by the HUD readout).
	UFUNCTION(BlueprintCallable, Category = "Selling")
	int32 GetMoney() const { return Money; }

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

	// --- Still assembly completion (detection only; no operation/state machine yet) ---

	// True only when every required Tier 2 Pot Still part is placed. The empty MasonJar (catch
	// vessel) IS required; the MasonJarLid is an output mechanic and is EXCLUDED.
	bool IsStillComplete() const;

	// Re-evaluates IsStillComplete() and logs/notifies only on a false<->true transition.
	void CheckStillCompletion();

	// Logs the required part types not yet placed (excludes MasonJarLid).
	void LogMissingStillParts() const;

	// Latched completion state; only transitions trigger logging/on-screen messages.
	bool bStillComplete = false;

	// --- Still operation (skeleton: placeholder timer, no real ingredients/jars yet) ---

	// Current operating state of the completed still.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Moonshine")
	EStillState CurrentStillState = EStillState::Empty;

	// How long a distilling run takes once the fire is lit (seconds).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	float BatchTimeSeconds = 300.0f;

	// Jars produced per completed run. Stored now; consumed by the output increment later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	int32 JarsPerRun = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	int32 WaterCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	int32 MashCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	int32 FirewoodCost = 3;

	// Running-state countdown timer.
	FTimerHandle BatchTimerHandle;

	// Jars still waiting in the sealed jar when the inventory couldn't hold the whole batch.
	// While > 0 the jar stays sealed and each E press collects as much as fits.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Moonshine")
	int32 RemainingJars = 0;

	// E-key interaction: advance the still's state when aiming at the Pot and the still is complete.
	void InteractWithStill();

	// Returns whatever actor the camera-forward interaction trace hits within MaxAimDistanceCm.
	AActor* GetAimedActor() const;

	// Returns the placed still part the player is currently aiming at, or nullptr.
	class AStillPartActor* GetAimedStillPart() const;

	// Returns the placed Pot the player is currently aiming at, or nullptr.
	class AStillPartActor* GetAimedPot() const;

	// Returns the sealed MasonJar the player is currently aiming at, or nullptr.
	class AStillPartActor* GetAimedSealedJar() const;

	// Collects the batch from a sealed jar: grants as many MoonshineJar as fit. Only once the whole
	// batch is collected does the lid return to inventory, the jar reset, and the state go Empty.
	void CollectMoonshine(class AStillPartActor* Jar);

	// Selection-time prerequisite check for still parts (same prerequisites the ghost enforces).
	// Returns false with a player-facing message when the part can't be placed yet.
	bool CheckStillPartPrereqs(FName PartID, FString& OutMsg) const;

	// --- Save/Load (v1: inventory, money, placed still parts; brew state not saved) ---

	UFUNCTION(BlueprintCallable, Category="SaveLoad")
	void SaveGame();

	UFUNCTION(BlueprintCallable, Category="SaveLoad")
	void LoadGame();

	// --- Selling (placeholder buyer; sell-all, no partial-sale UI) ---

	// Player money. Read by the HUD for the on-screen readout.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Selling")
	int32 Money = 0;

	// Adds (or subtracts) money, clamped at >= 0, with an event log line.
	UFUNCTION(BlueprintCallable, Category="Selling")
	void AddMoney(int32 Amount);

	// Returns the buyer the player is currently aiming at (camera-forward trace), or nullptr.
	class ABuyerActor* GetAimedBuyer() const;

	// Sells ALL MoonshineJar in inventory to the buyer.
	void SellMoonshine(class ABuyerActor* Buyer);

	// Applies a state transition with a single concise log line.
	void SetStillState(EStillState NewState);

	// Running timer callback: Running -> Done.
	void OnBatchComplete();

	// Per-tick on-screen prompt shown while aiming at the Pot of a complete still.
	void UpdateStillPrompt();

	// Tint helper for the ghost (green = valid snap, red = invalid).
	void SetGhostColor(const FLinearColor& Color);

	// World-space VISUAL center the ghost mesh would have at CandidateXform. Used as the snap
	// AIM target so the player aims at the part's body, not its (possibly offset) pivot. Falls
	// back to PivotFallback when the ghost has no mesh.
	FVector GhostVisualCenter(const FTransform& CandidateXform, const FVector& PivotFallback) const;

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
