// Born To Shine - Simplified Player Character using BuildingComponent

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "InventoryComponent.h"
#include "InventoryGridWidget.h"
#include "StillPartActor.h" // EStillState lives with the per-stand state container
#include "MoonshineCharacter_Simple.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UInteractionHUDWidget;
class AWorldPickupActor;
class UHotbarWidget;

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
	float StandPlacementYaw = 270.0f;

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

	// Raw suspicion heat (0..100); read by the HUD meter.
	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	float GetSuspicionHeat() const { return SuspicionHeat; }

	// Current "star" count (0..5) = floor(SuspicionHeat / HeatPerStar), clamped.
	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	int32 GetSuspicionStars() const;

	// Adds (or subtracts) suspicion heat, clamped 0..100.
	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	void AddSuspicionHeat(float Amount);

	// --- Per-still loading UI API (called by UStillInventoryWidget) ---

	// The player's main inventory component (the loading UI shows it alongside the still storage).
	UInventoryComponent* GetInventoryComponent() const { return Inventory; }

	// Required amount of an ingredient for one batch (Water/Mash/Firewood).
	int32 GetIngredientReq(FName Ingredient) const;

	// 1-based display number of a stand by placement order.
	int32 GetStandNumber(class AStillPartActor* Stand) const;

	// Consume the required amounts from the stand's STORAGE and begin its batch. Returns false
	// (consuming nothing) when storage is short.
	bool TryStartDistilling(class AStillPartActor* Stand);

	// Close the still loading UI if open (mouse/input restored to gameplay).
	void CloseStillInventory();

	// --- World item drop / pickup (called by the inventory grid and pickup actors) ---

	// Remove Count of ItemId from the main inventory and spawn a physical pickup in front of the player.
	void DropItemToWorld(FName ItemId, int32 Count);

	// Add a pickup's contents back to the inventory; destroys it on a full pickup, leaves a partial.
	bool TryPickup(class AWorldPickupActor* Pickup);

	// Walk-over hook from a pickup's proximity sphere (only acts if bAutoPickupOnOverlap).
	void NotifyPickupOverlap(class AWorldPickupActor* Pickup);

	// --- Hotbar (a view onto inventory slots 0..HotbarSlots-1) ---

	// Select the active hotbar slot (clamped; updates the highlight). Called by keys and the widget.
	void SelectHotbarSlot(int32 Index);

	// Decide cancel-vs-world-drop for an inventory drag released off all slots, tested against
	// every open inventory panel (main grid + hotbar + still UI). Drops from the SOURCE container.
	void HandleInventoryDragRelease(class UInventoryComponent* SourceInventory, int32 SourceIndex, FVector2D ScreenPos);

	// A stack dropped onto a slot: whole-stack move, or (count > 1, cross-container, no Shift) the
	// transfer-amount slider. Called by every container widget's slot-drop handler.
	void HandleSlotDrop(class UInventoryComponent* SourceInventory, int32 SourceIndex, class UInventoryComponent* TargetInventory, int32 TargetIndex, int32 Count, bool bShiftDown);

	// Transfer-amount popup result hooks.
	void ConfirmTransferAmount(int32 Amount);
	void CancelTransferAmount();

	// The independent hotbar container (its own storage, NOT a view of the main inventory).
	UInventoryComponent* GetHotbarInventory() const { return HotbarInventory; }

protected:
	virtual void BeginPlay() override;

	// --- Still ghost preview state ---
	bool bIsPlacingStillGhost = false;       // true while previewing a snap-able still part (e.g. Pot)
	bool bGhostFloorGridMode = false;        // true: floor-grid placement (stand); false: mount-snap (vessel)
	FName GhostPartID;                        // which part the ghost represents
	bool bGhostSnapValid = false;             // is the ghost currently within snap range of its mount?
	FTransform GhostSnapTransform;            // the snapped world transform when valid

	// Stand the pending ghost would belong to (chosen snap candidate's stand); applied on confirm.
	TWeakObjectPtr<class AStillPartActor> GhostSnapOwningStand;

	UPROPERTY()
	class AStillPartActor* GhostStillPart = nullptr;     // the live ghost actor

	UPROPERTY()
	UMaterialInstanceDynamic* GhostDynamicMaterial = nullptr;

	// Begin/Update/Confirm/Cancel for the still ghost preview flow.
	void BeginStillGhostPlacement(FName PartID);
	void UpdateStillGhost();                  // called each Tick while previewing
	void ConfirmStillGhostPlacement();        // called on click while previewing
	void CancelStillGhost();                  // tears down the ghost actor

	// Finds the FIRST placed CinderBlockStand actor (nullptr if none).
	class AStillPartActor* FindPlacedStand() const;

	// Finds the first placed still part with the given PartID (nullptr if none).
	class AStillPartActor* FindPlacedPart(FName PartID) const;

	// Stand a part belongs to: the part itself if it IS a stand, else its OwningStand.
	class AStillPartActor* StandOfPart(class AStillPartActor* Part) const;

	// Finds the part of the given type belonging to the given stand (the stand answers for
	// "CinderBlockStand"). Nullptr if that mount is unoccupied.
	class AStillPartActor* FindPartOnStand(FName PartID, class AStillPartActor* Stand) const;

	// --- Still assembly completion (detection only; no operation/state machine yet) ---

	// True only when every required Tier 2 Pot Still part is present ON THIS STAND. The empty
	// MasonJar (catch vessel) IS required; the MasonJarLid is an output mechanic and is EXCLUDED.
	bool IsStillComplete(class AStillPartActor* Stand) const;

	// First stand (in PlacedStillParts order) whose still is complete; nullptr if none.
	class AStillPartActor* FindFirstCompleteStand() const;

	// Re-evaluates completion ("at least one complete still") and logs only on transitions.
	void CheckStillCompletion();

	// Logs the required part types not yet placed (excludes MasonJarLid).
	void LogMissingStillParts() const;

	// "At least one complete still exists" (legacy aggregate; interactions are per-stand now).
	bool bStillComplete = false;

	// Stands currently known complete — per-stand transition tracking so the SECOND still also
	// gets its "complete" toast when it finishes assembly.
	TSet<TWeakObjectPtr<class AStillPartActor>> CompletedStands;

	// --- Still operation (skeleton: placeholder timer, no real ingredients/jars yet) ---

	// How long a distilling run takes once the fire is lit (seconds). Shared by every still.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	float BatchTimeSeconds = 300.0f;

	// Safety ceiling on how many running-still countdown lines the HUD lists. Beyond this, the
	// last line collapses to "+N more…".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD")
	int32 MaxTimerLines = 8;

	// Jars produced per completed run. Stored now; consumed by the output increment later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	int32 JarsPerRun = 15;

	// Ingredients required to start one batch (consumed from the still's stash by Start Distilling).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	int32 ReqWater = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	int32 ReqMash = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	int32 ReqFirewood = 3;

	// Slot count of each still's storage container.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	int32 StillStorageSlots = 8;

	// E-key interaction: routes to the aimed part's OWN stand (opens the loading UI on the pot).
	void InteractWithStill();

	// The per-still loading UI (created on demand, like InteractionHUD).
	UPROPERTY()
	class UStillInventoryWidget* StillInventoryWidgetInstance = nullptr;

	// Stand whose storage the open loading UI is editing.
	TWeakObjectPtr<class AStillPartActor> ActiveStillUIStand;

	// Open the loading UI for a stand (closes the main inventory if it is open).
	void OpenStillInventory(class AStillPartActor* Stand);

	// Point a stand's storage container at the item data table and the configured slot count.
	void ConfigureStillStorage(class AStillPartActor* Stand);

	// Per-frame batch timers: every running stand accumulates BatchElapsed independently.
	void TickStillBatches(float DeltaTime);

	// 1-based number of a stand by placement order (for "STILL 2" style display/logs).
	int32 StandNumber(class AStillPartActor* Stand) const;

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

	// Manual save (F5): loud — toast + ping.
	UFUNCTION(BlueprintCallable, Category="SaveLoad")
	void SaveGame();

	UFUNCTION(BlueprintCallable, Category="SaveLoad")
	void LoadGame();

	// Return to the main menu level (use from pause menu / ESC).
	UFUNCTION(BlueprintCallable, Category="Menu")
	void ReturnToMainMenu();

	// Kill switch for all autosaving (testing sessions).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save")
	bool bAutosaveEnabled = true;

	// True once the player has taken a meaningful action (placed a part, sold, started distilling).
	// Prevents a fresh-start empty world from autosaving over an existing save slot.
	bool bHasPlayerProgressed = false;

	// Part placements debounce into one save this many seconds after the last placement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save")
	float AutosaveDebounceSeconds = 10.0f;

	// Silent autosave: no toast/sound, just the corner "Saving…" indicator.
	void AutoSave();

	// (Re)starts the debounce timer; the save fires once placements stop.
	void RequestAutosaveDebounced();

	// Shared snapshot-and-write core used by both save paths. Returns SaveGameToSlot's success.
	bool DoSaveGame();

	FTimerHandle AutosaveDebounceHandle;

	// --- Suspicion / heat (foundation + HUD; no police AI yet) ---

	// Accumulated heat (0..100). Raised by selling and by running stills; decays over time.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Suspicion")
	float SuspicionHeat = 0.0f;

	// Heat per "star"; star count = floor(SuspicionHeat / HeatPerStar), clamped 0..5.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Suspicion")
	float HeatPerStar = 20.0f;

	// Heat added per jar sold (multiplied by the number of jars in a sale).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Suspicion")
	float HeatPerJarSold = 0.5f;

	// Heat added per RUNNING still per second (summed across all running stills).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Suspicion")
	float HeatPerStillPerSecond = 0.2f;

	// Heat shed per second (always applied; sources add on top).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Suspicion")
	float HeatDecayPerSecond = 0.5f;

	// Per-tick heat reconciliation: always decay, add per running still, clamp, log star changes.
	void TickSuspicion(float DeltaTime);

	// Last star count we logged, so transitions log once (not per tick). -1 = never logged.
	int32 LastLoggedStars = -1;

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

	// Applies a state transition on one stand with a single concise log line.
	void SetStandState(class AStillPartActor* Stand, EStillState NewState);

	// Batch finished on this stand: Running -> Done, fill ITS jar, sound at ITS pot.
	void OnBatchComplete(class AStillPartActor* Stand);

	// Per-tick interaction HUD update: contextual prompt + distill countdown.
	void UpdateStillPrompt();

	// Code-built interaction HUD overlay ([E] prompt, countdown bar, toasts).
	UPROPERTY()
	UInteractionHUDWidget* InteractionHUD = nullptr;

	// Toast helper (no-ops safely before the HUD exists).
	void ShowToast(const FString& Text, bool bSuccess);

	// --- Audio (all sounds assigned in BP defaults; unset sounds simply don't play) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* FireLoopSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* BoilSteamLoopSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* DripLoopSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* WaterAddSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* MashAddSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* FireIgniteSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* BatchCompleteSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* LidPlaceSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* JarCollectSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* SellSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* PartPlaceSound = nullptr;

	// Played when a world pickup is collected (and reused as the drop clink). Null = silent.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* PickupSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* ToastSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* InventoryOpenSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundBase* InventoryCloseSound = nullptr;

	// Global SFX balance knob, multiplied into every play call.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	float MasterSfxVolume = 1.0f;

	// The drip loop starts at this fraction of the Running timer (0.8 = last 20%).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	float DripStartFraction = 0.8f;

	// When true, the toast ping only plays for failure (red) toasts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	bool bToastSoundOnFailureOnly = false;

	// Sphere radius for the E-interaction sweep — forgiveness when aiming at the pot/jar/buyer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Moonshine")
	float InteractTraceRadiusCm = 12.0f;

	// --- World drop / pickup tuning ---

	// Fallback mesh for dropped items whose DT_Items row has no Mesh (so a pickup is always visible).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Pickup")
	UStaticMesh* DefaultPickupMesh = nullptr;

	// Walk over a pickup to collect it (default: aim + interact instead).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Pickup")
	bool bAutoPickupOnOverlap = false;

	// Optional "Drop <N>?" confirm before dropping a stack (off by default — drops are recoverable).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Pickup")
	bool bConfirmStackDrop = false;

	// Spawn placement + toss for a dropped pickup.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Pickup")
	float DropForwardDistance = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Pickup")
	float DropUpOffset = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Pickup")
	float DropTossStrength = 250.0f;

	// --- Hotbar config/state ---

	// How many inventory slots (0..N-1) the hotbar mirrors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	int32 HotbarSlots = 6;

	// Currently selected hotbar slot (0-based).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI")
	int32 ActiveHotbarSlot = 0;

	// The always-on hotbar overlay.
	UPROPERTY()
	class UHotbarWidget* HotbarWidget = nullptr;

	// The hotbar's own independent storage (separate from the main Inventory; starts empty).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	UInventoryComponent* HotbarInventory;

	// Show the transfer-amount slider when dragging a stack > 1 between containers (Shift bypasses).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	bool bSplitStackOnTransfer = true;

	// Transfer-amount popup + the pending transfer it confirms.
	UPROPERTY() class UTransferAmountWidget* TransferAmountWidget = nullptr;
	TWeakObjectPtr<UInventoryComponent> PendingTransferSource;
	TWeakObjectPtr<UInventoryComponent> PendingTransferTarget;
	int32 PendingTransferSourceIndex = -1;

	void BeginTransferAmount(UInventoryComponent* Source, int32 SourceIndex, UInventoryComponent* Target, int32 MaxAmount);
	void CloseTransferAmount();

	// Scroll-wheel cycle of the active slot (wraps). Skipped during still-ghost placement.
	void CycleHotbarSlot(int32 Direction);

	// Stub for "using" the active hotbar item (eat/equip/etc.) — wired to a key, no-op for now.
	void UseActiveHotbarItem();

	// Raw-key handlers for hotbar selection (number keys 1..6) and scroll cycling.
	void OnHotbar1(); void OnHotbar2(); void OnHotbar3();
	void OnHotbar4(); void OnHotbar5(); void OnHotbar6();
	void OnHotbarScrollUp();
	void OnHotbarScrollDown();

	// The pickup the player is currently aiming at (camera-forward sweep), or nullptr.
	class AWorldPickupActor* GetAimedPickup() const;

	// Resolve the world mesh for an item: its DT_Items Mesh, else DefaultPickupMesh.
	class UStaticMesh* ResolveItemMesh(FName ItemId) const;

	// Spawn a physical pickup of ItemId x Count in front of the player (does NOT touch inventory).
	void SpawnWorldPickup(FName ItemId, int32 Count);

	// Optional attenuation override for the still loops; a default (~15 m audible) is built lazily.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	class USoundAttenuation* LoopAttenuation = nullptr;

	// Live loop handles per stand (spawned on demand, stopped manually). Weak pointers stay
	// GC-safe without UPROPERTY; the components are owned by the audio system while playing.
	struct FStillLoops
	{
		TWeakObjectPtr<class UAudioComponent> Fire;
		TWeakObjectPtr<class UAudioComponent> Boil;
		TWeakObjectPtr<class UAudioComponent> Drip;
	};
	TMap<TWeakObjectPtr<class AStillPartActor>, FStillLoops> StillLoopMap;

	UPROPERTY() class USoundAttenuation* DefaultLoopAttenuation = nullptr;

	// Per-tick reconciliation of the three still loops against state/timer/jar.
	void UpdateStillAudio();

	// One-shot helpers; warn once per property name if the sound is unassigned.
	void PlaySfxAt(class USoundBase* Sound, const TCHAR* PropertyName, const FVector& Location);
	void PlaySfx2D(class USoundBase* Sound, const TCHAR* PropertyName);
	bool CheckSoundAssigned(class USoundBase* Sound, const TCHAR* PropertyName);
	class USoundAttenuation* GetLoopAttenuation();

	// Property names already warned about (one-shot warnings only).
	TSet<FName> WarnedMissingSounds;

	// --- VFX (Niagara; assigned in BP defaults; unset systems simply don't spawn). A parallel
	// system that MIRRORS the audio loops exactly — per-stand handles reconciled each tick in
	// lockstep with UpdateStillAudio, same conditions, same attach points. ---

	// Loop systems (one set per running still).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	class UNiagaraSystem* FireVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	class UNiagaraSystem* SteamVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	class UNiagaraSystem* DripVFX = nullptr;

	// One-shot systems (fire-and-forget at location).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	class UNiagaraSystem* IgniteBurstVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	class UNiagaraSystem* CollectPoofVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	class UNiagaraSystem* PlacePuffVFX = nullptr;

	// Local attach offsets so each loop seats correctly on its part (tune in PIE).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	FVector FireVfxOffset = FVector(0.0f, 0.0f, -45.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	FVector SteamVfxOffset = FVector(0.0f, 0.0f, 30.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	FVector DripVfxOffset = FVector(0.0f, 0.0f, 20.0f);

	// Fire light (pure code, no asset). Lives with the fire, seated at the fire position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	float FireLightIntensity = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	float FireLightRadius = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	FLinearColor FireLightColor = FLinearColor(1.0f, 0.45f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	bool bFireLightFlicker = true;

	// Live VFX/light handles per stand — parallel to StillLoopMap, reconciled in lockstep with it.
	struct FStillVfx
	{
		TWeakObjectPtr<class UNiagaraComponent> Fire;
		TWeakObjectPtr<class UNiagaraComponent> Steam;
		TWeakObjectPtr<class UNiagaraComponent> Drip;
		TWeakObjectPtr<class UPointLightComponent> FireLight;
	};
	TMap<TWeakObjectPtr<class AStillPartActor>, FStillVfx> StillVfxMap;

	// Per-tick reconciliation of the still VFX + fire light, mirroring UpdateStillAudio exactly.
	void UpdateStillVFX();

	// One-shot VFX helper; warns once per property name if the system is unassigned.
	void SpawnVfxAt(class UNiagaraSystem* System, const TCHAR* PropertyName, const FVector& Location);
	bool CheckVfxAssigned(class UNiagaraSystem* System, const TCHAR* PropertyName);

	// VFX property names already warned about (one-shot warnings only).
	TSet<FName> WarnedMissingVfx;

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
