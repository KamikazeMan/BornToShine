// Born To Shine - Simplified Player Character using BuildingComponent

#include "MoonshineCharacter_Simple.h"
#include "MoonshinePlayerController.h"
#include "BuildingComponent.h"
#include "ConstructionPhaseManager.h"
#include "RimBoard.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"
#include "StillPartActor.h"
#include "BornToShineHUD.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"

AMoonshineCharacter_Simple::AMoonshineCharacter_Simple()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create third person camera boom
	ThirdPersonArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonArm"));
	ThirdPersonArm->SetupAttachment(RootComponent);
	ThirdPersonArm->TargetArmLength = 400.0f;
	ThirdPersonArm->bUsePawnControlRotation = true;
	ThirdPersonArm->bEnableCameraLag = true;
	ThirdPersonArm->CameraLagSpeed = 3.0f;

	// Create third person camera
	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(ThirdPersonArm, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;

	// Create first person camera - pushed forward to avoid seeing body
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(40.0f, 0.0f, 75.0f)); // Forward of head
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Create Building Component
	BuildingComponent = CreateDefaultSubobject<UBuildingComponent>(TEXT("BuildingComponent"));

	// Default settings
	bIsFirstPerson = false;
	WalkSpeed = 400.0f;
	SprintSpeed = 800.0f;
	MouseSensitivity = 1.0f;

	// Zoom defaults
	DefaultFOV = 90.0f;
	ZoomedFOV = 45.0f;
	ZoomInterpSpeed = 12.0f;
	bIsZooming = false;

	// Configure character movement
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = 600.0f;
	GetCharacterMovement()->AirControl = 0.3f;

	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	InventoryWidgetInstance = nullptr;

	// Don't rotate character with controller (camera is independent)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Character auto-rotates to face movement direction
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // Fast rotation
}

void AMoonshineCharacter_Simple::BeginPlay()
{
	Super::BeginPlay();

	// Set initial camera mode
	FirstPersonCamera->SetActive(false);
	ThirdPersonCamera->SetActive(true);

	// Setup enhanced input
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AMoonshineCharacter_Simple::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Smooth FOV zoom transition
	float TargetFOV = bIsZooming ? ZoomedFOV : DefaultFOV;
	UCameraComponent* ActiveCamera = bIsFirstPerson ? FirstPersonCamera : ThirdPersonCamera;
	if (ActiveCamera)
	{
		float CurrentFOV = ActiveCamera->FieldOfView;
		if (!FMath::IsNearlyEqual(CurrentFOV, TargetFOV, 0.1f))
		{
			float NewFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, ZoomInterpSpeed);
			FirstPersonCamera->SetFieldOfView(NewFOV);
			ThirdPersonCamera->SetFieldOfView(NewFOV);
		}
	}

	// Drive the still-part ghost preview while it's active.
	if (bIsPlacingStillGhost)
	{
		UpdateStillGhost();
	}

	// Show the operation prompt when aiming at the Pot of a completed still.
	UpdateStillPrompt();
}

void AMoonshineCharacter_Simple::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Movement
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMoonshineCharacter_Simple::Move);
		}

		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMoonshineCharacter_Simple::Look);
		}

		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::Jump);
		}

		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::Sprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AMoonshineCharacter_Simple::StopSprinting);
		}

		// Camera
		if (ToggleCameraAction)
		{
			EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::ToggleCameraMode);
		}

		// Building (delegates to BuildingComponent)
		if (ToggleBuildModeAction)
		{
			EnhancedInputComponent->BindAction(ToggleBuildModeAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::OnToggleBuildMode);
		}

		if (PlacePieceAction)
		{
			EnhancedInputComponent->BindAction(PlacePieceAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::OnPlacePiece);
		}

		if (NailPieceAction)
		{
			EnhancedInputComponent->BindAction(NailPieceAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::OnNailPiece);
		}

		// CyclePieceAction (Q/E) removed — player uses radial wheel (Tab) only

		if (ScaleAction)
		{
			EnhancedInputComponent->BindAction(ScaleAction, ETriggerEvent::Triggered, this, &AMoonshineCharacter_Simple::OnScalePiece);
		}

		// Rotation (2D axis - for gamepad or alternative input)
		if (RotateAction)
		{
			EnhancedInputComponent->BindAction(RotateAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::OnRotate);
		}

		// Phase advancement
		if (AdvancePhaseAction)
		{
			EnhancedInputComponent->BindAction(AdvancePhaseAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::OnAdvancePhase);
		}

		// Toggle board type (outside/inside for rim boards)
		if (ToggleBoardTypeAction)
		{
			EnhancedInputComponent->BindAction(ToggleBoardTypeAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::OnToggleBoardType);
		}

		// Zoom (right mouse button hold)
		if (ZoomAction)
		{
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::OnZoomStart);
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Completed, this, &AMoonshineCharacter_Simple::OnZoomStop);
		}

		if (ToggleInventoryAction)
		{
			EnhancedInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::HandleToggleInventoryAction);
		}
	}

	// Debug keys (raw bindings alongside Enhanced Input)
	PlayerInputComponent->BindKey(EKeys::Backslash, IE_Pressed, this, &AMoonshineCharacter_Simple::DebugGrantStillParts);
	PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &AMoonshineCharacter_Simple::DebugDumpInventory);

	// Still operation: E interacts with the Pot of a completed still.
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AMoonshineCharacter_Simple::InteractWithStill);
}

void AMoonshineCharacter_Simple::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMoonshineCharacter_Simple::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X * MouseSensitivity);
		AddControllerPitchInput(LookAxisVector.Y * MouseSensitivity * -1.0f); // Negated for correct look direction
	}
}

void AMoonshineCharacter_Simple::Jump()
{
	ACharacter::Jump();
}

void AMoonshineCharacter_Simple::Sprint()
{
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void AMoonshineCharacter_Simple::StopSprinting()
{
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void AMoonshineCharacter_Simple::ToggleCameraMode()
{
	bIsFirstPerson = !bIsFirstPerson;

	if (bIsFirstPerson)
	{
		// FIRST PERSON: Character body rotates with controller
		FirstPersonCamera->SetActive(true);
		ThirdPersonCamera->SetActive(false);

		// Make character rotate with controller (so camera looks out from eyes)
		bUseControllerRotationYaw = true;
		GetCharacterMovement()->bOrientRotationToMovement = false;

		// Hide character mesh so you don't see your own body
		GetMesh()->SetOwnerNoSee(true);

		UE_LOG(LogTemp, Log, TEXT("Switched to First Person"));
	}
	else
	{
		// THIRD PERSON: Character rotates toward movement direction
		FirstPersonCamera->SetActive(false);
		ThirdPersonCamera->SetActive(true);

		// Character doesn't rotate with controller, auto-rotates to movement
		bUseControllerRotationYaw = false;
		GetCharacterMovement()->bOrientRotationToMovement = true;

		// Show character mesh again
		GetMesh()->SetOwnerNoSee(false);

		UE_LOG(LogTemp, Log, TEXT("Switched to Third Person"));
	}
}

// Building delegates - all forward to BuildingComponent
void AMoonshineCharacter_Simple::OnToggleBuildMode()
{
	if (BuildingComponent)
	{
		BuildingComponent->ToggleBuildMode();
	}
}

void AMoonshineCharacter_Simple::OnPlacePiece()
{
	// If we're previewing a snap-able still part, the place button confirms the ghost.
	if (bIsPlacingStillGhost)
	{
		ConfirmStillGhostPlacement();
		return;
	}

	// If we're placing an inventory item, the place button confirms that instead of normal building.
	if (bIsPlacingItem)
	{
		ConfirmItemPlacement();
		return;
	}

	if (BuildingComponent)
	{
		BuildingComponent->PlaceCurrentPiece();
	}
}

void AMoonshineCharacter_Simple::OnRotate(const FInputActionValue& Value)
{
	if (!BuildingComponent) return;

	FVector2D RotationVector = Value.Get<FVector2D>();

	// X-axis: left (-1) / right (+1)
	if (RotationVector.X < -0.5f)
	{
		BuildingComponent->RotatePreviewLeft();
	}
	else if (RotationVector.X > 0.5f)
	{
		BuildingComponent->RotatePreviewRight();
	}

	// Y-axis could be used for pitch/roll rotations if needed
	// For now, just using horizontal rotation
}

void AMoonshineCharacter_Simple::OnScalePiece(const FInputActionValue& Value)
{
	if (BuildingComponent)
	{
		float ScaleDelta = Value.Get<float>();
		BuildingComponent->ScalePreview(ScaleDelta);
	}
}

void AMoonshineCharacter_Simple::OnCyclePieceType()
{
	if (BuildingComponent)
	{
		BuildingComponent->CyclePieceType();
	}
}

void AMoonshineCharacter_Simple::OnNailPiece()
{
	if (BuildingComponent)
	{
		BuildingComponent->NailLastPlacedPiece();
	}
}

void AMoonshineCharacter_Simple::OnAdvancePhase()
{
	if (AConstructionPhaseManager::Instance)
	{
		if (AConstructionPhaseManager::Instance->AdvanceToNextPhase())
		{
			// Phase advanced successfully
			EConstructionPhase CurrentPhase = AConstructionPhaseManager::Instance->GetCurrentPhase();
			FString PhaseName = AConstructionPhaseManager::Instance->GetCurrentPhaseName();
			UE_LOG(LogTemp, Log, TEXT("Advanced to phase: %s"), *PhaseName);

			// Show on-screen message
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("Phase: %s"), *PhaseName));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Cannot advance phase - requirements not met"));

			// Show warning message
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Cannot advance phase - place more foundation blocks"));
			}
		}
	}
}

void AMoonshineCharacter_Simple::OnZoomStart()
{
	bIsZooming = true;
}

void AMoonshineCharacter_Simple::OnZoomStop()
{
	bIsZooming = false;
}

void AMoonshineCharacter_Simple::OnToggleBoardType()
{
	AMoonshinePlayerController* PC = Cast<AMoonshinePlayerController>(GetController());

	// When delete mode is active (F7), X ALWAYS routes to delete — skip board type toggle
	if (PC && PC->IsDeleteModeActive())
	{
		PC->OnDeletePressed();
		return;
	}

	// Normal path: toggle board type if we have a rim board preview
	if (BuildingComponent)
	{
		ABuildablePiece* PreviewPiece = BuildingComponent->GetCurrentPreviewPiece();
		if (PreviewPiece)
		{
			ARimBoard* RimBoard = Cast<ARimBoard>(PreviewPiece);
			if (RimBoard)
			{
				RimBoard->ToggleBoardType();

				FString BoardType = RimBoard->bIsOutsideBoard ? TEXT("OUTSIDE") : TEXT("INSIDE");
				UE_LOG(LogTemp, Warning, TEXT("Toggled board type: %s"), *BoardType);

				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
						FString::Printf(TEXT("Board Type: %s"), *BoardType));
				}
				return;
			}
		}
	}

	// Fallthrough: X key pressed but not toggling board type -> try delete
	if (PC)
	{
		PC->OnDeletePressed();
	}
}

void AMoonshineCharacter_Simple::HandleToggleInventoryAction(const FInputActionValue& Value)
{
	ToggleInventoryUI();
}

void AMoonshineCharacter_Simple::ToggleInventoryUI()
{
	if (!InventoryWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryWidgetClass not set on player BP!"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	if (InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
	{
		InventoryWidgetInstance->RemoveFromParent();
		InventoryWidgetInstance = nullptr;
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}
	else
	{
		InventoryWidgetInstance = CreateWidget<UInventoryGridWidget>(PC, InventoryWidgetClass);
		if (InventoryWidgetInstance)
		{
			if (InventoryBackgroundTexture)
			{
				InventoryWidgetInstance->BackgroundTexture = InventoryBackgroundTexture;
			}
			InventoryWidgetInstance->SetInventoryComponent(Inventory);
			InventoryWidgetInstance->AddToViewport();
			PC->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(InventoryWidgetInstance->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
		}
	}
}

void AMoonshineCharacter_Simple::BeginItemPlacement(FName ItemID)
{
	// Ghost-preview flows:
	//   CinderBlockStand            -> floor-grid
	//   Pot/ThumperBody/WormBarrel  -> stand-snap (onto the stand)
	//   Cap/ThumperCap              -> vessel-snap (onto the placed vessel)
	if (ItemID == FName(TEXT("Pot")) || ItemID == FName(TEXT("ThumperBody")) ||
		ItemID == FName(TEXT("WormBarrel")) || ItemID == FName(TEXT("CinderBlockStand")) ||
		ItemID == FName(TEXT("Cap")) || ItemID == FName(TEXT("ThumperCap")) ||
		ItemID == FName(TEXT("CapArm")) ||
		ItemID == FName(TEXT("OutletPipe")) || ItemID == FName(TEXT("WormCoil")) ||
		ItemID == FName(TEXT("MasonJar")) || ItemID == FName(TEXT("MasonJarLid")))
	{
		BeginStillGhostPlacement(ItemID);
		return;
	}

	PendingPlacementItemID = ItemID;
	bIsPlacingItem = true;

	// Close the inventory UI if open, but keep the cursor so the player can click the floor.
	if (InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
	{
		InventoryWidgetInstance->RemoveFromParent();
		InventoryWidgetInstance = nullptr;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
	}

	UE_LOG(LogTemp, Warning, TEXT("Placement mode: %s — click the floor to place"), *ItemID.ToString());
}

void AMoonshineCharacter_Simple::ConfirmItemPlacement()
{
	if (!bIsPlacingItem) return;

	// Trace from the active camera forward to find the floor.
	UCameraComponent* ActiveCamera = bIsFirstPerson ? FirstPersonCamera : ThirdPersonCamera;
	const FVector TraceStart = ActiveCamera ? ActiveCamera->GetComponentLocation() : GetActorLocation();
	const FVector TraceDir = ActiveCamera ? ActiveCamera->GetForwardVector() : GetActorForwardVector();
	const FVector TraceEnd = TraceStart + TraceDir * 10000.0f;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

	if (bHit)
	{
		FItemDataRow RowData;
		const bool bHasData = Inventory ? Inventory->GetItemData(PendingPlacementItemID, RowData) : false;
		UStaticMesh* PartMesh = bHasData ? RowData.Mesh : nullptr;
		if (!PartMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("Placement: no mesh assigned for %s in the data table — spawning empty actor(s)"), *PendingPlacementItemID.ToString());
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Every still part (including CinderBlockStand, whose mesh already models all 3 stands)
		// spawns as a single AStillPartActor at the floor hit point. Raise it by FloorSpawnZOffset
		// so a center-pivot mesh sits on the floor instead of half-buried.
		FVector SpawnLocation = Hit.Location;
		SpawnLocation.Z += FloorSpawnZOffset;
		AStillPartActor* Part = GetWorld()->SpawnActor<AStillPartActor>(AStillPartActor::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);
		if (Part)
		{
			Part->InitFromItemData(PendingPlacementItemID, PartMesh);
			PlacedStillParts.Add(Part);

			if (Inventory)
			{
				Inventory->RemoveItem(PendingPlacementItemID, 1);
			}

			UE_LOG(LogTemp, Log, TEXT("Placed %s at Z=%.2f (FloorSpawnZOffset=%.2f) — %s"), *PendingPlacementItemID.ToString(), SpawnLocation.Z, FloorSpawnZOffset, *SpawnLocation.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Placement: trace hit nothing — aim at the floor and click again"));
	}

	bIsPlacingItem = false;
	PendingPlacementItemID = NAME_None;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}
}

// ---------------------------------------------------------------------------
// Still part ghost-preview snapping (Step 2: Pot onto the cinder block stand)
// ---------------------------------------------------------------------------

namespace
{
	// Local-space mount points on the single 3-stand cinder block mesh (cm).
	// The mesh pivot is at the MIDDLE stand, so offsets are measured from the middle.
	// Top surface Z=49 (blocks span Z=6..49). PotZAdjust is added on top of the Z.
	// Each vessel has exactly ONE valid stand so the still assembles correctly:
	//   Pot snaps ONLY to PotMountLocal, Thumper ONLY to ThumperMountLocal, Barrel ONLY to BarrelMountLocal.
	static const FVector PotMountLocal(-121.69f, 0.47f, 49.0f);   // LEFT stand   -> Pot
	static const FVector ThumperMountLocal(0.0f, -1.11f, 49.0f);  // MIDDLE stand -> Thumper Body
	static const FVector BarrelMountLocal(122.60f, 0.0f, 49.0f);  // RIGHT stand  -> Worm Barrel
}

AStillPartActor* AMoonshineCharacter_Simple::FindPlacedStand() const
{
	// The CinderBlockStand is a single mesh containing all 3 stands, so there's just one actor.
	return FindPlacedPart(FName(TEXT("CinderBlockStand")));
}

AStillPartActor* AMoonshineCharacter_Simple::FindPlacedPart(FName PartID) const
{
	for (AStillPartActor* Part : PlacedStillParts)
	{
		if (IsValid(Part) && Part->PartID == PartID)
		{
			return Part;
		}
	}
	return nullptr;
}

namespace
{
	// Fixed on-screen message keys so repeated calls overwrite in place instead of stacking.
	constexpr uint64 MsgKeyStillPrompt    = 0x5717; // aim-at-pot interact prompt (per tick)
	constexpr uint64 MsgKeyStillCountdown = 0x5718; // "Distilling… Ns" countdown (per tick)
	constexpr uint64 MsgKeyStillComplete  = 0x5719; // green "Still complete" transition
	constexpr uint64 MsgKeyBatchComplete  = 0x571A; // green "Batch complete" event
	constexpr uint64 MsgKeyNeedWater      = 0x571B; // red requirement failures (E presses)
	constexpr uint64 MsgKeyNeedMash       = 0x571C;
	constexpr uint64 MsgKeyNeedFirewood   = 0x571D;

	// Required parts for a complete Tier 2 Pot Still. MasonJarLid is intentionally EXCLUDED:
	// the empty MasonJar is the catch vessel and is required; the lid is a later output mechanic.
	static const FName RequiredStillParts[] = {
		FName(TEXT("CinderBlockStand")),
		FName(TEXT("Pot")),
		FName(TEXT("Cap")),
		FName(TEXT("CapArm")),
		FName(TEXT("ThumperBody")),
		FName(TEXT("ThumperCap")),
		FName(TEXT("OutletPipe")),
		FName(TEXT("WormBarrel")),
		FName(TEXT("WormCoil")),
		FName(TEXT("MasonJar")),
	};
}

bool AMoonshineCharacter_Simple::IsStillComplete() const
{
	// Tally which required types are present in a single pass over placed parts; ignore extras/dupes.
	bool bFound[UE_ARRAY_COUNT(RequiredStillParts)] = { false };
	for (const AStillPartActor* Part : PlacedStillParts)
	{
		if (!IsValid(Part)) continue;
		for (int32 i = 0; i < UE_ARRAY_COUNT(RequiredStillParts); ++i)
		{
			if (Part->PartID == RequiredStillParts[i]) { bFound[i] = true; break; }
		}
	}

	for (int32 i = 0; i < UE_ARRAY_COUNT(RequiredStillParts); ++i)
	{
		if (!bFound[i]) return false;
	}
	return true;
}

void AMoonshineCharacter_Simple::LogMissingStillParts() const
{
	FString Missing;
	for (const FName& Req : RequiredStillParts)
	{
		if (!FindPlacedPart(Req))
		{
			if (!Missing.IsEmpty()) Missing += TEXT(", ");
			Missing += Req.ToString();
		}
	}
	if (!Missing.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Still missing: %s"), *Missing);
	}
}

void AMoonshineCharacter_Simple::CheckStillCompletion()
{
	const bool bNowComplete = IsStillComplete();

	if (bNowComplete && !bStillComplete)
	{
		bStillComplete = true;
		UE_LOG(LogTemp, Warning, TEXT("=== STILL COMPLETE — ready to operate ==="));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(MsgKeyStillComplete, 5.0f, FColor::Green, TEXT("Still complete — ready to operate"));
		}
	}
	else if (!bNowComplete && bStillComplete)
	{
		bStillComplete = false;
		UE_LOG(LogTemp, Warning, TEXT("Still no longer complete"));
	}

	if (!bNowComplete)
	{
		LogMissingStillParts();
	}
}

AStillPartActor* AMoonshineCharacter_Simple::GetAimedPot() const
{
	// Same camera-forward trace pattern as UpdateStillGhost.
	FVector CamLoc = GetActorLocation();
	FRotator CamRot = GetActorRotation();
	if (AController* C = GetController())
	{
		C->GetPlayerViewPoint(CamLoc, CamRot);
	}
	const FVector TraceEnd = CamLoc + CamRot.Vector() * MaxAimDistanceCm;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, Params))
	{
		return nullptr;
	}

	AStillPartActor* Part = Cast<AStillPartActor>(Hit.GetActor());
	if (Part && Part->PartID == FName(TEXT("Pot")))
	{
		return Part;
	}
	return nullptr;
}

void AMoonshineCharacter_Simple::SetStillState(EStillState NewState)
{
	if (CurrentStillState == NewState) return;

	static const TCHAR* StateNames[] = { TEXT("Empty"), TEXT("Water"), TEXT("Mash"), TEXT("Lit"), TEXT("Running"), TEXT("Done") };
	UE_LOG(LogTemp, Warning, TEXT("Still state: %s -> %s"),
		StateNames[(uint8)CurrentStillState], StateNames[(uint8)NewState]);
	CurrentStillState = NewState;
}

void AMoonshineCharacter_Simple::InteractWithStill()
{
	// Interaction only works on a complete still while aiming at its Pot.
	if (!bStillComplete) return;
	if (!GetAimedPot()) return;

	switch (CurrentStillState)
	{
	case EStillState::Empty:
		if (!Inventory || !Inventory->HasItem(FName(TEXT("Water")), WaterCost))
		{
			const int32 Have = Inventory ? Inventory->GetItemCount(FName(TEXT("Water"))) : 0;
			UE_LOG(LogTemp, Warning, TEXT("Need %d Water (have %d)"), WaterCost, Have);
			if (GEngine) GEngine->AddOnScreenDebugMessage(MsgKeyNeedWater, 2.0f, FColor::Red, FString::Printf(TEXT("Need %d Water"), WaterCost));
			break;
		}
		Inventory->RemoveItem(FName(TEXT("Water")), WaterCost);
		SetStillState(EStillState::Water);
		UE_LOG(LogTemp, Warning, TEXT("Water added (consumed %d Water)"), WaterCost);
		break;

	case EStillState::Water:
		if (!Inventory || !Inventory->HasItem(FName(TEXT("Mash")), MashCost))
		{
			const int32 Have = Inventory ? Inventory->GetItemCount(FName(TEXT("Mash"))) : 0;
			UE_LOG(LogTemp, Warning, TEXT("Need %d Mash (have %d)"), MashCost, Have);
			if (GEngine) GEngine->AddOnScreenDebugMessage(MsgKeyNeedMash, 2.0f, FColor::Red, FString::Printf(TEXT("Need %d Mash"), MashCost));
			break;
		}
		Inventory->RemoveItem(FName(TEXT("Mash")), MashCost);
		SetStillState(EStillState::Mash);
		UE_LOG(LogTemp, Warning, TEXT("Mash added (consumed %d Mash)"), MashCost);
		break;

	case EStillState::Mash:
	{
		if (!Inventory || !Inventory->HasItem(FName(TEXT("Firewood")), FirewoodCost))
		{
			const int32 Have = Inventory ? Inventory->GetItemCount(FName(TEXT("Firewood"))) : 0;
			UE_LOG(LogTemp, Warning, TEXT("Need %d Firewood (have %d)"), FirewoodCost, Have);
			if (GEngine) GEngine->AddOnScreenDebugMessage(MsgKeyNeedFirewood, 2.0f, FColor::Red, FString::Printf(TEXT("Need %d Firewood (have %d)"), FirewoodCost, Have));
			break;
		}
		Inventory->RemoveItem(FName(TEXT("Firewood")), FirewoodCost);
		SetStillState(EStillState::Lit);
		SetStillState(EStillState::Running);
		UE_LOG(LogTemp, Warning, TEXT("Fire lit (consumed %d Firewood) — distilling"), FirewoodCost);
		GetWorldTimerManager().SetTimer(BatchTimerHandle, this,
			&AMoonshineCharacter_Simple::OnBatchComplete, FMath::Max(BatchTimeSeconds, 0.01f), false);
		break;
	}

	case EStillState::Running:
		break;

	case EStillState::Done:
		break;

	default:
		break;
	}
}

void AMoonshineCharacter_Simple::OnBatchComplete()
{
	SetStillState(EStillState::Done);
	UE_LOG(LogTemp, Warning, TEXT("=== BATCH COMPLETE ==="));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(MsgKeyBatchComplete, 5.0f, FColor::Green, TEXT("Batch complete — jar full"));
	}
	UE_LOG(LogTemp, Warning, TEXT("Would produce %d jars"), JarsPerRun);
}

void AMoonshineCharacter_Simple::UpdateStillPrompt()
{
	if (!bStillComplete) return;
	if (!GetAimedPot()) return;
	if (!GEngine) return;

	// Fixed keys so the per-tick prompt overwrites in place instead of stacking. The countdown
	// uses its own key separate from the interact prompt.
	FString Prompt;
	uint64 MsgKey = MsgKeyStillPrompt;
	switch (CurrentStillState)
	{
	case EStillState::Empty: Prompt = FString::Printf(TEXT("Press E: Add Water (%d)"), WaterCost); break;
	case EStillState::Water: Prompt = FString::Printf(TEXT("Press E: Add Mash (%d)"), MashCost); break;
	case EStillState::Mash:  Prompt = FString::Printf(TEXT("Press E: Light Fire (%d Firewood)"), FirewoodCost); break;
	case EStillState::Lit:   Prompt = TEXT("Distilling…"); MsgKey = MsgKeyStillCountdown; break;
	case EStillState::Running:
	{
		const float Remaining = GetWorldTimerManager().GetTimerRemaining(BatchTimerHandle);
		Prompt = FString::Printf(TEXT("Distilling… %ds"), FMath::Max(0, FMath::CeilToInt(Remaining)));
		MsgKey = MsgKeyStillCountdown;
		break;
	}
	case EStillState::Done:  Prompt = TEXT("Batch complete — jar full"); break;
	default: break;
	}

	GEngine->AddOnScreenDebugMessage(MsgKey, 0.2f, FColor::Yellow, Prompt);
}

void AMoonshineCharacter_Simple::SetGhostColor(const FLinearColor& Color)
{
	if (!GhostDynamicMaterial) return;
	// Match the framing ghost: try all known color parameter names + opacity.
	GhostDynamicMaterial->SetVectorParameterValue(FName("BaseColor"), Color);
	GhostDynamicMaterial->SetVectorParameterValue(FName("Base Color"), Color);
	GhostDynamicMaterial->SetVectorParameterValue(FName("Color"), Color);
	GhostDynamicMaterial->SetScalarParameterValue(FName("Opacity"), Color.A);
}

void AMoonshineCharacter_Simple::BeginStillGhostPlacement(FName PartID)
{
	// Tear down any prior ghost first.
	CancelStillGhost();

	GhostPartID = PartID;
	bIsPlacingStillGhost = true;
	bGhostFloorGridMode = (PartID == FName(TEXT("CinderBlockStand")));
	bGhostSnapValid = false;

	// Close the inventory UI if open. Use GameOnly + hidden cursor so mouse-look drives the camera
	// (the player aims the ghost with the crosshair). GameAndUI + visible cursor would capture the
	// mouse for UI and freeze camera look.
	if (InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
	{
		InventoryWidgetInstance->RemoveFromParent();
		InventoryWidgetInstance = nullptr;
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);

		if (ABornToShineHUD* HUD = Cast<ABornToShineHUD>(PC->GetHUD()))
		{
			HUD->SetCrosshairVisible(true);
		}
	}

	// Look up the part's mesh from the data table.
	UStaticMesh* PartMesh = nullptr;
	if (Inventory)
	{
		FItemDataRow RowData;
		if (Inventory->GetItemData(PartID, RowData))
		{
			PartMesh = RowData.Mesh;
		}
	}

	// Spawn the ghost actor (no collision blocking, translucent material).
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GhostStillPart = GetWorld()->SpawnActor<AStillPartActor>(AStillPartActor::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	if (GhostStillPart)
	{
		GhostStillPart->InitFromItemData(PartID, PartMesh);
		if (UStaticMeshComponent* GhostMesh = GhostStillPart->MeshComponent)
		{
			GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			UMaterialInterface* Base = GhostPreviewMaterial ? GhostPreviewMaterial : UMaterial::GetDefaultMaterial(MD_Surface);
			GhostDynamicMaterial = UMaterialInstanceDynamic::Create(Base, this);
			if (GhostDynamicMaterial)
			{
				const int32 NumMats = GhostMesh->GetNumMaterials();
				for (int32 m = 0; m < NumMats; ++m)
				{
					GhostMesh->SetMaterial(m, GhostDynamicMaterial);
				}
				SetGhostColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Still ghost: previewing %s (%s) — aim and click to place"),
		*PartID.ToString(), bGhostFloorGridMode ? TEXT("floor-grid") : TEXT("mount-snap"));
}

FVector AMoonshineCharacter_Simple::GhostVisualCenter(const FTransform& CandidateXform, const FVector& PivotFallback) const
{
	if (IsValid(GhostStillPart) && GhostStillPart->MeshComponent)
	{
		if (const UStaticMesh* GhostMesh = GhostStillPart->MeshComponent->GetStaticMesh())
		{
			// Mesh-local bounds center transformed by the transform the part WOULD have when snapped.
			// Independent of the ghost's current frame position, so there's no one-frame lag.
			const FVector LocalCenter = GhostMesh->GetBoundingBox().GetCenter();
			FTransform Xform = CandidateXform;
			Xform.SetScale3D(GhostStillPart->GetActorScale3D());
			return Xform.TransformPosition(LocalCenter);
		}
	}
	return PivotFallback;
}

void AMoonshineCharacter_Simple::UpdateStillGhost()
{
	if (!IsValid(GhostStillPart)) return;

	// Aim from the player's VIEWPOINT so the ghost follows mouse look every frame (not just body
	// movement). GetPlayerViewPoint reflects the current control rotation.
	FVector CamLoc = GetActorLocation();
	FRotator CamRot = GetActorRotation();
	if (AController* C = GetController())
	{
		C->GetPlayerViewPoint(CamLoc, CamRot);
	}
	const FVector CamFwd = CamRot.Vector();
	const FVector TraceStart = CamLoc;
	const FVector TraceEnd = CamLoc + CamFwd * MaxAimDistanceCm;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (GhostStillPart) Params.AddIgnoredActor(GhostStillPart);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
	const FVector AimPoint = bHit ? Hit.ImpactPoint : TraceEnd;

	bGhostSnapValid = false;

	if (bGhostFloorGridMode)
	{
		// Floor-grid placement (CinderBlockStand): snap aim X/Y to the nearest grid cell.
		const float GridSize = FMath::Max(StandGridSizeCm, 1.0f);
		FVector GridLoc;
		GridLoc.X = FMath::RoundToFloat(AimPoint.X / GridSize) * GridSize;
		GridLoc.Y = FMath::RoundToFloat(AimPoint.Y / GridSize) * GridSize;
		GridLoc.Z = AimPoint.Z + FloorSpawnZOffset;

		const FRotator GridRot(0.0f, StandPlacementYaw, 0.0f);

		GhostSnapTransform = FTransform(GridRot, GridLoc);
		GhostStillPart->SetActorLocationAndRotation(GridLoc, GridRot);
		bGhostSnapValid = true; // floor is always a valid target

		SetGhostColor(FLinearColor(0.0f, 1.0f, 0.0f, 0.5f)); // green = valid
		return;
	}

	// Cap-like snap: parts that snap onto a placed parent actor with a local offset.
	const bool bIsCapLike =
		GhostPartID == FName(TEXT("Cap")) || GhostPartID == FName(TEXT("ThumperCap")) ||
		GhostPartID == FName(TEXT("CapArm")) ||
		GhostPartID == FName(TEXT("OutletPipe")) || GhostPartID == FName(TEXT("WormCoil")) ||
		GhostPartID == FName(TEXT("MasonJar")) || GhostPartID == FName(TEXT("MasonJarLid"));

	if (bIsCapLike)
	{
		FName SnapTargetID;   // which placed part we snap onto
		FVector MountOffset;  // local offset on that target
		FRotator MountRotation = FRotator::ZeroRotator; // per-part fine rotation tweak

		if (GhostPartID == FName(TEXT("CapArm")))
		{
			SnapTargetID = FName(TEXT("Cap"));
			MountOffset = CapArmMountOffset;
			MountRotation = CapArmMountRotation;

			// Dual prerequisite: BOTH Cap AND ThumperCap must be placed.
			AStillPartActor* PC = FindPlacedPart(FName(TEXT("Cap")));
			AStillPartActor* PTC = FindPlacedPart(FName(TEXT("ThumperCap")));
			if (!PC || !PTC)
			{
				GhostStillPart->SetActorLocationAndRotation(AimPoint, FRotator::ZeroRotator);
				SetGhostColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
				return;
			}
		}
		else if (GhostPartID == FName(TEXT("OutletPipe")))
		{
			SnapTargetID = FName(TEXT("ThumperBody"));
			MountOffset = OutletPipeMountOffset;

			// Dual prerequisite: BOTH ThumperBody AND WormBarrel must be placed.
			AStillPartActor* PTB = FindPlacedPart(FName(TEXT("ThumperBody")));
			AStillPartActor* PWB = FindPlacedPart(FName(TEXT("WormBarrel")));
			if (!PTB || !PWB)
			{
				GhostStillPart->SetActorLocationAndRotation(AimPoint, FRotator::ZeroRotator);
				SetGhostColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
				return;
			}
		}
		else if (GhostPartID == FName(TEXT("WormCoil")))
		{
			SnapTargetID = FName(TEXT("WormBarrel"));
			MountOffset = WormCoilMountOffset;

			if (!FindPlacedPart(FName(TEXT("WormBarrel"))))
			{
				GhostStillPart->SetActorLocationAndRotation(AimPoint, FRotator::ZeroRotator);
				SetGhostColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
				return;
			}
		}
		else if (GhostPartID == FName(TEXT("MasonJar")))
		{
			SnapTargetID = FName(TEXT("WormBarrel"));
			MountOffset = MasonJarMountOffset;
			MountRotation = MasonJarMountRotation;

			AStillPartActor* PWB = FindPlacedPart(FName(TEXT("WormBarrel")));
			AStillPartActor* PWC = FindPlacedPart(FName(TEXT("WormCoil")));
			if (!PWB || !PWC)
			{
				GhostStillPart->SetActorLocationAndRotation(AimPoint, FRotator::ZeroRotator);
				SetGhostColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
				return;
			}
		}
		else if (GhostPartID == FName(TEXT("MasonJarLid")))
		{
			SnapTargetID = FName(TEXT("MasonJar"));
			MountOffset = MasonJarLidMountOffset;
			MountRotation = MasonJarLidMountRotation;

			if (!FindPlacedPart(FName(TEXT("MasonJar"))))
			{
				GhostStillPart->SetActorLocationAndRotation(AimPoint, FRotator::ZeroRotator);
				SetGhostColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f));
				return;
			}
		}
		else if (GhostPartID == FName(TEXT("ThumperCap")))
		{
			SnapTargetID = FName(TEXT("ThumperBody"));
			MountOffset = ThumperCapMountOffset;
		}
		else
		{
			SnapTargetID = FName(TEXT("Pot"));
			MountOffset = CapMountOffset;
		}

		AStillPartActor* SnapTarget = FindPlacedPart(SnapTargetID);
		if (SnapTarget)
		{
			const FVector MountWorld = SnapTarget->GetActorTransform().TransformPosition(MountOffset);
			const FRotator SnapRot = SnapTarget->GetActorRotation() + MountRotation;

			// Aim at the part's VISUAL center, not its pivot. Compute the mesh's local-space bounds
			// center and transform it by the candidate snapped transform — independent of the ghost's
			// current frame position (no one-frame lag). Snap POSITION stays MountWorld unchanged.
			const FVector AimTarget = GhostVisualCenter(FTransform(SnapRot, MountWorld), MountWorld);
			const float SnapRadius = IsValid(GhostStillPart) ? GhostStillPart->SnapRadiusCm : StillSnapRadiusCm;

			const FVector ToMount = AimTarget - CamLoc;
			const float Along = FVector::DotProduct(ToMount, CamFwd);
			const FVector ClosestOnRay = CamLoc + CamFwd * FMath::Clamp(Along, 0.0f, MaxAimDistanceCm);
			const float RayDist = FVector::Dist(ClosestOnRay, AimTarget);
			const bool bValid = (Along > 0.0f) && (RayDist <= SnapRadius);

			if (bValid)
			{
				GhostSnapTransform = FTransform(SnapRot, MountWorld);
				GhostStillPart->SetActorLocationAndRotation(MountWorld, SnapRot);
				bGhostSnapValid = true;
			}
		}

		if (bGhostSnapValid)
		{
			SetGhostColor(FLinearColor(0.0f, 1.0f, 0.0f, 0.5f)); // green = valid
		}
		else
		{
			GhostStillPart->SetActorLocationAndRotation(AimPoint, FRotator::ZeroRotator);
			SetGhostColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f)); // red = invalid
		}
		return;
	}

	// Mount-snap placement: each vessel has exactly ONE valid stand (Pot=left, Thumper=middle,
	// Barrel=right). Pick this vessel's mount + Z-adjust from the part ID.
	FVector MountLocal = PotMountLocal;
	float ZAdjust = PotZAdjust;
	if (GhostPartID == FName(TEXT("ThumperBody")))
	{
		MountLocal = ThumperMountLocal; ZAdjust = ThumperZAdjust;
	}
	else if (GhostPartID == FName(TEXT("WormBarrel")))
	{
		MountLocal = BarrelMountLocal; ZAdjust = BarrelZAdjust;
	}

	AStillPartActor* Stand = FindPlacedStand();
	if (Stand)
	{
		// TransformPosition respects the stand's rotation, so a rotated stand places vessels correctly.
		FVector MountWorld = Stand->GetActorTransform().TransformPosition(MountLocal);
		MountWorld.Z += ZAdjust;

		const FRotator SnapRot(0.0f, Stand->GetActorRotation().Yaw, 0.0f);

		// Aim at the vessel's VISUAL center, not its pivot. Snap POSITION stays MountWorld unchanged.
		const FVector AimTarget = GhostVisualCenter(FTransform(SnapRot, MountWorld), MountWorld);
		const float SnapRadius = IsValid(GhostStillPart) ? GhostStillPart->SnapRadiusCm : StillSnapRadiusCm;

		const FVector ToMount = AimTarget - CamLoc;
		const float Along = FVector::DotProduct(ToMount, CamFwd);
		const FVector ClosestOnRay = CamLoc + CamFwd * FMath::Clamp(Along, 0.0f, MaxAimDistanceCm);
		const float RayDist = FVector::Dist(ClosestOnRay, AimTarget);
		const bool bValid = (Along > 0.0f) && (RayDist <= SnapRadius);

		if (bValid)
		{
			GhostSnapTransform = FTransform(SnapRot, MountWorld);
			GhostStillPart->SetActorLocationAndRotation(MountWorld, SnapRot);
			bGhostSnapValid = true;
		}
	}

	if (bGhostSnapValid)
	{
		SetGhostColor(FLinearColor(0.0f, 1.0f, 0.0f, 0.5f)); // green = valid
	}
	else
	{
		GhostStillPart->SetActorLocationAndRotation(AimPoint, FRotator::ZeroRotator);
		SetGhostColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f)); // red = invalid
	}
}

void AMoonshineCharacter_Simple::ConfirmStillGhostPlacement()
{
	if (!bIsPlacingStillGhost) return;

	if (!bGhostSnapValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("Still ghost: %s has no valid placement here (bGhostSnapValid=false)."), *GhostPartID.ToString());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Confirming %s placement at %s (rot=%s)"),
		*GhostPartID.ToString(), *GhostSnapTransform.GetLocation().ToString(), *GhostSnapTransform.Rotator().ToString());

	// Look up the real mesh again for the placed (non-ghost) actor.
	UStaticMesh* PartMesh = nullptr;
	if (Inventory)
	{
		FItemDataRow RowData;
		if (Inventory->GetItemData(GhostPartID, RowData))
		{
			PartMesh = RowData.Mesh;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStillPartActor* Placed = GetWorld()->SpawnActor<AStillPartActor>(AStillPartActor::StaticClass(), GhostSnapTransform.GetLocation(), GhostSnapTransform.Rotator(), SpawnParams);
	if (Placed)
	{
		Placed->InitFromItemData(GhostPartID, PartMesh);
		PlacedStillParts.Add(Placed);

		if (Inventory)
		{
			Inventory->RemoveItem(GhostPartID, 1);
		}

		UE_LOG(LogTemp, Log, TEXT("Placed %s (snapped) at %s"), *GhostPartID.ToString(), *GhostSnapTransform.GetLocation().ToString());

		// Detection only: re-evaluate whether the full Tier 2 still is now assembled.
		CheckStillCompletion();
	}

	CancelStillGhost();
}

void AMoonshineCharacter_Simple::CancelStillGhost()
{
	if (IsValid(GhostStillPart))
	{
		GhostStillPart->Destroy();
	}
	GhostStillPart = nullptr;
	GhostDynamicMaterial = nullptr;
	bIsPlacingStillGhost = false;
	bGhostFloorGridMode = false;
	bGhostSnapValid = false;
	GhostPartID = NAME_None;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);

		if (ABornToShineHUD* HUD = Cast<ABornToShineHUD>(PC->GetHUD()))
		{
			HUD->SetCrosshairVisible(false);
		}
	}
}

void AMoonshineCharacter_Simple::DebugGrantStillParts()
{
	if (Inventory) Inventory->DebugGrantStillParts();
}

void AMoonshineCharacter_Simple::DebugDumpInventory()
{
	if (Inventory) Inventory->DebugLogInventory();
}
