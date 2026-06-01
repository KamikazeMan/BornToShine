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
#include "StillPartActor.h"
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

		// Rotation - separate left/right for arrow keys
		if (RotateLeftAction)
		{
			EnhancedInputComponent->BindAction(RotateLeftAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::OnRotateLeft);
		}

		if (RotateRightAction)
		{
			EnhancedInputComponent->BindAction(RotateRightAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::OnRotateRight);
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

void AMoonshineCharacter_Simple::OnRotateLeft()
{
	UE_LOG(LogTemp, Log, TEXT("OnRotateLeft called"));
	if (BuildingComponent)
	{
		BuildingComponent->RotatePreviewLeft();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRotateLeft: BuildingComponent is null!"));
	}
}

void AMoonshineCharacter_Simple::OnRotateRight()
{
	UE_LOG(LogTemp, Log, TEXT("OnRotateRight called"));
	if (BuildingComponent)
	{
		BuildingComponent->RotatePreviewRight();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRotateRight: BuildingComponent is null!"));
	}
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
	// The Pot uses the ghost-preview snapping flow (snaps onto the cinder block stand).
	if (ItemID == FName(TEXT("Pot")))
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
	// Top surface Z=49 (blocks span Z=6..49). All at Y=0.18. PotZAdjust is added on top of the Z.
	static const FVector PotMountLocal(-0.18f, 0.18f, 49.0f);    // left stand
	static const FVector ThumperMountLocal(47.73f, 0.18f, 49.0f); // middle stand
	static const FVector BarrelMountLocal(96.51f, 0.18f, 49.0f);  // right stand

	// How close the player's aim must be to the mount point (world cm) to snap.
	static constexpr float StillSnapRadiusCm = 100.0f;
}

AStillPartActor* AMoonshineCharacter_Simple::FindPlacedStand() const
{
	// The CinderBlockStand is a single mesh containing all 3 stands, so there's just one actor.
	for (AStillPartActor* Part : PlacedStillParts)
	{
		if (IsValid(Part) && Part->PartID == FName(TEXT("CinderBlockStand")))
		{
			return Part;
		}
	}
	return nullptr;
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
	bGhostSnapValid = false;

	// Close the inventory UI if open, keep the cursor so the player can aim and click.
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

	UE_LOG(LogTemp, Warning, TEXT("Still ghost: previewing %s — aim at its stand and click to place"), *PartID.ToString());
}

void AMoonshineCharacter_Simple::UpdateStillGhost()
{
	if (!IsValid(GhostStillPart)) return;

	// Camera-forward trace for where the player is aiming.
	UCameraComponent* ActiveCamera = bIsFirstPerson ? FirstPersonCamera : ThirdPersonCamera;
	const FVector TraceStart = ActiveCamera ? ActiveCamera->GetComponentLocation() : GetActorLocation();
	const FVector TraceDir = ActiveCamera ? ActiveCamera->GetForwardVector() : GetActorForwardVector();
	const FVector TraceEnd = TraceStart + TraceDir * 2000.0f;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	// CRITICAL: the ghost must not block the trace, or the impact point is always on the ghost
	// right in front of the camera, collapsing "nearest stand" to whichever stand is closest to
	// the camera every frame.
	if (GhostStillPart) Params.AddIgnoredActor(GhostStillPart);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
	const FVector AimPoint = bHit ? Hit.ImpactPoint : TraceEnd;

	UE_LOG(LogTemp, Warning, TEXT("Ghost aim point: %s (bHit=%d, hitActor=%s)"), *AimPoint.ToString(), bHit ? 1 : 0, bHit && Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("none"));

	bGhostSnapValid = false;

	// The stand is a single mesh with 3 stands; pick whichever of its 3 mount points the player
	// is aiming nearest to (pot can go on any of the three for now — sequencing comes later).
	AStillPartActor* Stand = FindPlacedStand();
	if (Stand)
	{
		const FTransform StandXform = Stand->GetActorTransform();
		const FVector PotMountWorld     = StandXform.TransformPosition(PotMountLocal);
		const FVector ThumperMountWorld = StandXform.TransformPosition(ThumperMountLocal);
		const FVector BarrelMountWorld  = StandXform.TransformPosition(BarrelMountLocal);

		const float PotDist     = FVector::Dist(AimPoint, PotMountWorld);
		const float ThumperDist = FVector::Dist(AimPoint, ThumperMountWorld);
		const float BarrelDist  = FVector::Dist(AimPoint, BarrelMountWorld);

		FVector ChosenMount = PotMountWorld;
		float ChosenDist = PotDist;
		const TCHAR* ChosenName = TEXT("Pot");
		if (ThumperDist < ChosenDist) { ChosenMount = ThumperMountWorld; ChosenDist = ThumperDist; ChosenName = TEXT("Thumper"); }
		if (BarrelDist < ChosenDist)  { ChosenMount = BarrelMountWorld;  ChosenDist = BarrelDist;  ChosenName = TEXT("Barrel"); }

		UE_LOG(LogTemp, Warning, TEXT("Mounts: Pot=%s(%.1f) Thumper=%s(%.1f) Barrel=%s(%.1f) -> chose %s(%.1f)"),
			*PotMountWorld.ToString(), PotDist,
			*ThumperMountWorld.ToString(), ThumperDist,
			*BarrelMountWorld.ToString(), BarrelDist,
			ChosenName, ChosenDist);

		if (ChosenDist <= StillSnapRadiusCm)
		{
			FVector MountWorld = ChosenMount;
			MountWorld.Z += PotZAdjust;

			// Keep the ghost upright, matching the stand's yaw only.
			const FRotator SnapRot(0.0f, Stand->GetActorRotation().Yaw, 0.0f);
			GhostSnapTransform = FTransform(SnapRot, MountWorld);
			GhostStillPart->SetActorLocationAndRotation(MountWorld, SnapRot);
			bGhostSnapValid = true;
		}
	}

	if (bGhostSnapValid)
	{
		SetGhostColor(FLinearColor(0.0f, 1.0f, 0.0f, 0.5f)); // green = valid
		UE_LOG(LogTemp, VeryVerbose, TEXT("Pot ghost snapped Z=%.2f (mount 49.0 + PotZAdjust=%.2f)"), GhostSnapTransform.GetLocation().Z, PotZAdjust);
	}
	else
	{
		// Free-follow the aim point, invalid tint.
		GhostStillPart->SetActorLocationAndRotation(AimPoint, FRotator::ZeroRotator);
		SetGhostColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f)); // red = invalid
	}
}

void AMoonshineCharacter_Simple::ConfirmStillGhostPlacement()
{
	if (!bIsPlacingStillGhost) return;

	if (!bGhostSnapValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("Still ghost: %s must be placed on its stand."), *GhostPartID.ToString());
		return;
	}

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

		UE_LOG(LogTemp, Log, TEXT("Placed %s (snapped) at %s — PotZAdjust=%.2f"), *GhostPartID.ToString(), *GhostSnapTransform.GetLocation().ToString(), PotZAdjust);
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
	bGhostSnapValid = false;
	GhostPartID = NAME_None;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
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
