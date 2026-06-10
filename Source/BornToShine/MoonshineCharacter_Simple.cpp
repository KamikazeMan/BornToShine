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
#include "BuyerActor.h"
#include "BornToShineHUD.h"
#include "BornToShineSaveGame.h"
#include "InteractionHUDWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"

namespace
{
	// Save slot identity.
	const TCHAR* SaveSlotName = TEXT("BornToShineSlot");
	constexpr int32 SaveUserIndex = 0;
}

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

	// Interaction HUD overlay (created before auto-load so load-time toasts can show).
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		InteractionHUD = CreateWidget<UInteractionHUDWidget>(PC, UInteractionHUDWidget::StaticClass());
		if (InteractionHUD)
		{
			InteractionHUD->SetVisibility(ESlateVisibility::HitTestInvisible);
			InteractionHUD->AddToViewport(5);
		}
	}

	// Resume from the save slot if one exists. LoadGame clears current inventory first, so the
	// default starting grants are never duplicated; with no save, nothing happens here.
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		LoadGame();
	}
}

void AMoonshineCharacter_Simple::ShowToast(const FString& Text, bool bSuccess)
{
	if (InteractionHUD && InteractionHUD->AddToast(Text, bSuccess))
	{
		// Ping only when a NEW toast appeared (refreshes stay silent).
		if (!bToastSoundOnFailureOnly || !bSuccess)
		{
			PlaySfx2D(ToastSound, TEXT("ToastSound"));
		}
	}
}

bool AMoonshineCharacter_Simple::CheckSoundAssigned(USoundBase* Sound, const TCHAR* PropertyName)
{
	if (Sound) return true;

	const FName Key(PropertyName);
	if (!WarnedMissingSounds.Contains(Key))
	{
		WarnedMissingSounds.Add(Key);
		UE_LOG(LogTemp, Warning, TEXT("Audio: %s not assigned"), PropertyName);
	}
	return false;
}

void AMoonshineCharacter_Simple::PlaySfxAt(USoundBase* Sound, const TCHAR* PropertyName, const FVector& Location)
{
	if (!CheckSoundAssigned(Sound, PropertyName)) return;
	UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, MasterSfxVolume);
}

void AMoonshineCharacter_Simple::PlaySfx2D(USoundBase* Sound, const TCHAR* PropertyName)
{
	if (!CheckSoundAssigned(Sound, PropertyName)) return;
	UGameplayStatics::PlaySound2D(this, Sound, MasterSfxVolume);
}

USoundAttenuation* AMoonshineCharacter_Simple::GetLoopAttenuation()
{
	if (LoopAttenuation) return LoopAttenuation; // editor-assigned override

	if (!DefaultLoopAttenuation)
	{
		// Audible out to ~15 m, full volume up close.
		DefaultLoopAttenuation = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& S = DefaultLoopAttenuation->Attenuation;
		S.bAttenuate = true;
		S.AttenuationShape = EAttenuationShape::Sphere;
		S.AttenuationShapeExtents = FVector(150.0f, 0.0f, 0.0f);
		S.FalloffDistance = 1350.0f;
	}
	return DefaultLoopAttenuation;
}

void AMoonshineCharacter_Simple::UpdateStillAudio()
{
	// Drop loop entries whose stand no longer exists (stop anything still playing).
	for (auto It = StillLoopMap.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			if (UAudioComponent* AC = It->Value.Fire.Get()) AC->Stop();
			if (UAudioComponent* AC = It->Value.Boil.Get()) AC->Stop();
			if (UAudioComponent* AC = It->Value.Drip.Get()) AC->Stop();
			It.RemoveCurrent();
		}
	}

	// Reconcile each stand's loops against ITS OWN state/timer/jar.
	for (AStillPartActor* Stand : PlacedStillParts)
	{
		if (!IsValid(Stand) || Stand->PartID != FName(TEXT("CinderBlockStand"))) continue;

		FStillLoops& Loops = StillLoopMap.FindOrAdd(Stand);

		const bool bBurning =
			Stand->StillState == EStillState::Lit || Stand->StillState == EStillState::Running;

		AStillPartActor* Pot = FindPartOnStand(FName(TEXT("Pot")), Stand);
		AStillPartActor* Cap = FindPartOnStand(FName(TEXT("Cap")), Stand);
		AStillPartActor* Jar = FindPartOnStand(FName(TEXT("MasonJar")), Stand);

		// Fire crackle at this stand's pot.
		if (bBurning && !Loops.Fire.IsValid() && Pot && CheckSoundAssigned(FireLoopSound, TEXT("FireLoopSound")))
		{
			Loops.Fire = UGameplayStatics::SpawnSoundAttached(FireLoopSound, Pot->MeshComponent, NAME_None,
				FVector::ZeroVector, EAttachLocation::SnapToTarget, true, MasterSfxVolume, 1.0f, 0.0f, GetLoopAttenuation());
		}
		else if (!bBurning && Loops.Fire.IsValid())
		{
			Loops.Fire->Stop();
			Loops.Fire.Reset();
		}

		// Boil/steam hiss at this stand's cap (falls back to its pot).
		USceneComponent* SteamAttach = Cap ? Cap->MeshComponent : (Pot ? Pot->MeshComponent : nullptr);
		if (bBurning && !Loops.Boil.IsValid() && SteamAttach && CheckSoundAssigned(BoilSteamLoopSound, TEXT("BoilSteamLoopSound")))
		{
			Loops.Boil = UGameplayStatics::SpawnSoundAttached(BoilSteamLoopSound, SteamAttach, NAME_None,
				FVector::ZeroVector, EAttachLocation::SnapToTarget, true, MasterSfxVolume, 1.0f, 0.0f, GetLoopAttenuation());
		}
		else if (!bBurning && Loops.Boil.IsValid())
		{
			Loops.Boil->Stop();
			Loops.Boil.Reset();
		}

		// Drip at this stand's jar: while it holds moonshine, or in the tail end of ITS run.
		bool bDrip = Jar && Jar->bIsFull;
		if (!bDrip && Jar && Stand->StillState == EStillState::Running)
		{
			const float Elapsed = Stand->BatchElapsed / FMath::Max(BatchTimeSeconds, 0.01f);
			bDrip = Elapsed >= DripStartFraction;
		}

		if (bDrip && !Loops.Drip.IsValid() && Jar && CheckSoundAssigned(DripLoopSound, TEXT("DripLoopSound")))
		{
			Loops.Drip = UGameplayStatics::SpawnSoundAttached(DripLoopSound, Jar->MeshComponent, NAME_None,
				FVector::ZeroVector, EAttachLocation::SnapToTarget, true, MasterSfxVolume, 1.0f, 0.0f, GetLoopAttenuation());
		}
		else if (!bDrip && Loops.Drip.IsValid())
		{
			Loops.Drip->Stop();
			Loops.Drip.Reset();
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

	// Advance every running still's batch timer independently.
	TickStillBatches(DeltaTime);

	// Show the operation prompt when aiming at the Pot of a completed still.
	UpdateStillPrompt();

	// Reconcile the still audio loops (fire/steam/drip) against state, timer and jar.
	UpdateStillAudio();
}

void AMoonshineCharacter_Simple::TickStillBatches(float DeltaTime)
{
	for (AStillPartActor* Stand : PlacedStillParts)
	{
		if (!IsValid(Stand) || Stand->PartID != FName(TEXT("CinderBlockStand"))) continue;
		if (!Stand->bBatchRunning) continue;

		Stand->BatchElapsed += DeltaTime;
		if (Stand->BatchElapsed >= FMath::Max(BatchTimeSeconds, 0.01f))
		{
			Stand->bBatchRunning = false;
			OnBatchComplete(Stand);
		}
	}
}

int32 AMoonshineCharacter_Simple::StandNumber(AStillPartActor* Stand) const
{
	int32 Number = 0;
	for (AStillPartActor* Part : PlacedStillParts)
	{
		if (IsValid(Part) && Part->PartID == FName(TEXT("CinderBlockStand")))
		{
			++Number;
			if (Part == Stand) return Number;
		}
	}
	return 0;
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

	// Save/Load debug keys.
	PlayerInputComponent->BindKey(EKeys::F5, IE_Pressed, this, &AMoonshineCharacter_Simple::SaveGame);
	PlayerInputComponent->BindKey(EKeys::F9, IE_Pressed, this, &AMoonshineCharacter_Simple::LoadGame);
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
		PlaySfx2D(InventoryCloseSound, TEXT("InventoryCloseSound"));
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
			PlaySfx2D(InventoryOpenSound, TEXT("InventoryOpenSound"));
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
		// Check prerequisites at SELECTION time so we never enter ghost mode that can only stay
		// red (e.g. lid on an empty jar). The item stays in inventory, nothing is consumed.
		FString PrereqMsg;
		if (!CheckStillPartPrereqs(ItemID, PrereqMsg))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s"), *PrereqMsg);
			ShowToast(PrereqMsg, false);
			return;
		}
		BeginStillGhostPlacement(ItemID);
		return;
	}

	// Items with no placement role (consumables, sale goods, any meshless row) can't be placed.
	// Never spawn an empty actor for them — block placement-mode entry outright.
	{
		FItemDataRow RowData;
		const bool bHasMesh = Inventory && Inventory->GetItemData(ItemID, RowData) && RowData.Mesh != nullptr;
		const bool bNonPlaceable =
			ItemID == FName(TEXT("Water")) || ItemID == FName(TEXT("Mash")) ||
			ItemID == FName(TEXT("Firewood")) || ItemID == FName(TEXT("MoonshineJar"));
		if (bNonPlaceable || !bHasMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("Can't place %s — no placement role/mesh"), *ItemID.ToString());
			ShowToast(TEXT("Can't place this item"), false);
			return;
		}
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
			// Never spawn an empty actor for a meshless item; consume nothing, exit placement mode.
			UE_LOG(LogTemp, Warning, TEXT("Placement: no mesh assigned for %s in the data table — placement cancelled"), *PendingPlacementItemID.ToString());
			ShowToast(TEXT("Can't place this item"), false);
		}
		else
		{
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
				PlaySfxAt(PartPlaceSound, TEXT("PartPlaceSound"), SpawnLocation);

				RequestAutosaveDebounced(); // autosave: part placed (debounced)
			}
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

AStillPartActor* AMoonshineCharacter_Simple::StandOfPart(AStillPartActor* Part) const
{
	if (!IsValid(Part)) return nullptr;
	if (Part->PartID == FName(TEXT("CinderBlockStand"))) return Part;
	return Part->OwningStand.Get();
}

AStillPartActor* AMoonshineCharacter_Simple::FindPartOnStand(FName PartID, AStillPartActor* Stand) const
{
	if (!IsValid(Stand)) return nullptr;
	if (PartID == FName(TEXT("CinderBlockStand"))) return Stand;

	for (AStillPartActor* Part : PlacedStillParts)
	{
		if (IsValid(Part) && Part->PartID == PartID && Part->OwningStand.Get() == Stand)
		{
			return Part;
		}
	}
	return nullptr;
}

namespace
{
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

bool AMoonshineCharacter_Simple::IsStillComplete(AStillPartActor* Stand) const
{
	if (!IsValid(Stand) || Stand->PartID != FName(TEXT("CinderBlockStand"))) return false;

	for (const FName& Req : RequiredStillParts)
	{
		if (!FindPartOnStand(Req, Stand)) return false;
	}
	return true;
}

AStillPartActor* AMoonshineCharacter_Simple::FindFirstCompleteStand() const
{
	for (AStillPartActor* Part : PlacedStillParts)
	{
		if (IsValid(Part) && Part->PartID == FName(TEXT("CinderBlockStand")) && IsStillComplete(Part))
		{
			return Part;
		}
	}
	return nullptr;
}

void AMoonshineCharacter_Simple::LogMissingStillParts() const
{
	// Report against the first stand (or globally when no stand is placed yet).
	AStillPartActor* Stand = FindPlacedStand();

	FString Missing;
	for (const FName& Req : RequiredStillParts)
	{
		const bool bPresent = Stand ? (FindPartOnStand(Req, Stand) != nullptr) : false;
		if (!bPresent)
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
	// Per-stand transition tracking: each still (including the second, third, ...) gets its own
	// "complete" toast when it finishes assembly, and its own "no longer complete" log.
	bool bAnyComplete = false;

	// Purge stale entries for destroyed stands.
	for (auto It = CompletedStands.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) It.RemoveCurrent();
	}

	for (AStillPartActor* Stand : PlacedStillParts)
	{
		if (!IsValid(Stand) || Stand->PartID != FName(TEXT("CinderBlockStand"))) continue;

		const bool bComplete = IsStillComplete(Stand);
		bAnyComplete |= bComplete;

		const TWeakObjectPtr<AStillPartActor> Key(Stand);
		if (bComplete && !CompletedStands.Contains(Key))
		{
			CompletedStands.Add(Key);
			UE_LOG(LogTemp, Warning, TEXT("=== STILL %d COMPLETE — ready to operate ==="), StandNumber(Stand));
			ShowToast(FString::Printf(TEXT("Still %d complete — ready to operate"), StandNumber(Stand)), true);
		}
		else if (!bComplete && CompletedStands.Contains(Key))
		{
			CompletedStands.Remove(Key);
			UE_LOG(LogTemp, Warning, TEXT("Still %d no longer complete"), StandNumber(Stand));
		}
	}

	bStillComplete = bAnyComplete;

	if (!bAnyComplete)
	{
		LogMissingStillParts();
	}
}

AActor* AMoonshineCharacter_Simple::GetAimedActor() const
{
	// Camera-forward sweep (sphere for forgiveness; closest hit wins). The still-part GHOST
	// placement trace and ray-proximity snap test are separate and stay line-based.
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
	if (!GetWorld()->SweepSingleByChannel(Hit, CamLoc, TraceEnd, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(InteractTraceRadiusCm), Params))
	{
		return nullptr;
	}

	return Hit.GetActor();
}

AStillPartActor* AMoonshineCharacter_Simple::GetAimedStillPart() const
{
	return Cast<AStillPartActor>(GetAimedActor());
}

ABuyerActor* AMoonshineCharacter_Simple::GetAimedBuyer() const
{
	return Cast<ABuyerActor>(GetAimedActor());
}

AStillPartActor* AMoonshineCharacter_Simple::GetAimedPot() const
{
	// Each pot routes to ITS OWN stand's state machine — valid when that stand's still is complete.
	AStillPartActor* Part = GetAimedStillPart();
	if (!Part || Part->PartID != FName(TEXT("Pot"))) return nullptr;

	AStillPartActor* Stand = StandOfPart(Part);
	return (Stand && IsStillComplete(Stand)) ? Part : nullptr;
}

AStillPartActor* AMoonshineCharacter_Simple::GetAimedSealedJar() const
{
	// A jar can only be sealed if its own still ran a batch, so sealed == collectible.
	AStillPartActor* Part = GetAimedStillPart();
	return (Part && Part->PartID == FName(TEXT("MasonJar")) && Part->bIsSealed) ? Part : nullptr;
}

void AMoonshineCharacter_Simple::SetStandState(AStillPartActor* Stand, EStillState NewState)
{
	if (!IsValid(Stand) || Stand->StillState == NewState) return;

	static const TCHAR* StateNames[] = { TEXT("Empty"), TEXT("Water"), TEXT("Mash"), TEXT("Lit"), TEXT("Running"), TEXT("Done") };
	UE_LOG(LogTemp, Warning, TEXT("Still %d state: %s -> %s"), StandNumber(Stand),
		StateNames[(uint8)Stand->StillState], StateNames[(uint8)NewState]);
	Stand->StillState = NewState;
}

void AMoonshineCharacter_Simple::InteractWithStill()
{
	// Selling to a buyer is its own interaction, independent of the still.
	if (ABuyerActor* Buyer = GetAimedBuyer())
	{
		SellMoonshine(Buyer);
		return;
	}

	// Collecting from a sealed jar is its own interaction, independent of the pot flow.
	if (AStillPartActor* SealedJar = GetAimedSealedJar())
	{
		CollectMoonshine(SealedJar);
		return;
	}

	// Otherwise interaction routes to the aimed Pot's OWN stand (GetAimedPot already verified
	// that stand's still is complete). Two stills can be in different states simultaneously.
	AStillPartActor* Pot = GetAimedPot();
	if (!Pot) return;
	AStillPartActor* Stand = StandOfPart(Pot);
	if (!Stand) return;

	switch (Stand->StillState)
	{
	case EStillState::Empty:
		if (!Inventory || !Inventory->HasItem(FName(TEXT("Water")), WaterCost))
		{
			const int32 Have = Inventory ? Inventory->GetItemCount(FName(TEXT("Water"))) : 0;
			UE_LOG(LogTemp, Warning, TEXT("Need %d Water (have %d)"), WaterCost, Have);
			ShowToast(FString::Printf(TEXT("Need %d Water (have %d)"), WaterCost, Have), false);
			break;
		}
		Inventory->RemoveItem(FName(TEXT("Water")), WaterCost);
		SetStandState(Stand, EStillState::Water);
		UE_LOG(LogTemp, Warning, TEXT("Water added (consumed %d Water)"), WaterCost);
		PlaySfxAt(WaterAddSound, TEXT("WaterAddSound"), Pot->GetActorLocation());
		break;

	case EStillState::Water:
		if (!Inventory || !Inventory->HasItem(FName(TEXT("Mash")), MashCost))
		{
			const int32 Have = Inventory ? Inventory->GetItemCount(FName(TEXT("Mash"))) : 0;
			UE_LOG(LogTemp, Warning, TEXT("Need %d Mash (have %d)"), MashCost, Have);
			ShowToast(FString::Printf(TEXT("Need %d Mash (have %d)"), MashCost, Have), false);
			break;
		}
		Inventory->RemoveItem(FName(TEXT("Mash")), MashCost);
		SetStandState(Stand, EStillState::Mash);
		UE_LOG(LogTemp, Warning, TEXT("Mash added (consumed %d Mash)"), MashCost);
		PlaySfxAt(MashAddSound, TEXT("MashAddSound"), Pot->GetActorLocation());
		break;

	case EStillState::Mash:
	{
		if (!Inventory || !Inventory->HasItem(FName(TEXT("Firewood")), FirewoodCost))
		{
			const int32 Have = Inventory ? Inventory->GetItemCount(FName(TEXT("Firewood"))) : 0;
			UE_LOG(LogTemp, Warning, TEXT("Need %d Firewood (have %d)"), FirewoodCost, Have);
			ShowToast(FString::Printf(TEXT("Need %d Firewood (have %d)"), FirewoodCost, Have), false);
			break;
		}
		Inventory->RemoveItem(FName(TEXT("Firewood")), FirewoodCost);
		SetStandState(Stand, EStillState::Lit);
		SetStandState(Stand, EStillState::Running);
		UE_LOG(LogTemp, Warning, TEXT("Fire lit (consumed %d Firewood) — distilling"), FirewoodCost);
		PlaySfxAt(FireIgniteSound, TEXT("FireIgniteSound"), Pot->GetActorLocation());
		Stand->BatchElapsed = 0.0f;
		Stand->bBatchRunning = true;
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

void AMoonshineCharacter_Simple::OnBatchComplete(AStillPartActor* Stand)
{
	if (!IsValid(Stand)) return;

	SetStandState(Stand, EStillState::Done);
	UE_LOG(LogTemp, Warning, TEXT("=== BATCH COMPLETE (still %d) ==="), StandNumber(Stand));
	ShowToast(TEXT("Batch complete — jar is full"), true);

	// Fill THIS stand's catch vessel so the lid can be snapped on to seal it.
	if (AStillPartActor* Jar = FindPartOnStand(FName(TEXT("MasonJar")), Stand))
	{
		Jar->bIsFull = true;
	}
	UE_LOG(LogTemp, Warning, TEXT("Jar is full — snap the lid to seal it"));

	if (AStillPartActor* Pot = FindPartOnStand(FName(TEXT("Pot")), Stand))
	{
		PlaySfxAt(BatchCompleteSound, TEXT("BatchCompleteSound"), Pot->GetActorLocation());
	}
}

void AMoonshineCharacter_Simple::CollectMoonshine(AStillPartActor* Jar)
{
	if (!IsValid(Jar) || !Jar->bIsSealed) return;
	if (!Inventory) return;

	// Fresh collection starts the full batch; otherwise keep draining THIS jar's remainder.
	if (Jar->RemainingJars <= 0)
	{
		Jar->RemainingJars = JarsPerRun;
	}

	// Only credit what ACTUALLY fits — AddItem reports the real added count.
	const int32 Added = Inventory->AddItem(FName(TEXT("MoonshineJar")), Jar->RemainingJars);
	Jar->RemainingJars -= Added;

	if (Added > 0)
	{
		PlaySfxAt(JarCollectSound, TEXT("JarCollectSound"), Jar->GetActorLocation());
	}

	if (Jar->RemainingJars > 0)
	{
		// Inventory full: jar stays sealed, lid stays on, state stays Done. E collects the rest later.
		const int32 CollectedSoFar = JarsPerRun - Jar->RemainingJars;
		UE_LOG(LogTemp, Warning, TEXT("Inventory full — collected %d of %d jars, %d still in the jar"),
			CollectedSoFar, JarsPerRun, Jar->RemainingJars);
		ShowToast(FString::Printf(TEXT("Inventory full — collected %d of %d jars, press E to collect the rest"),
			CollectedSoFar, JarsPerRun), false);
		return;
	}

	// Whole batch collected. The lid is reusable — return it to inventory.
	Inventory->AddItem(FName(TEXT("MasonJarLid")), 1);

	// Remove THIS jar's lid actor from the world and the tracking list (other stills keep theirs).
	// The lid is not in the required-parts set, so this cannot flip bStillComplete.
	AStillPartActor* JarStand = StandOfPart(Jar);
	for (int32 i = PlacedStillParts.Num() - 1; i >= 0; --i)
	{
		AStillPartActor* Part = PlacedStillParts[i];
		if (IsValid(Part) && Part->PartID == FName(TEXT("MasonJarLid")) &&
			(JarStand == nullptr || StandOfPart(Part) == JarStand))
		{
			Part->Destroy();
			PlacedStillParts.RemoveAt(i);
		}
	}

	// The jar stays placed, empty and ready for the next batch. ITS still resets to Empty.
	Jar->bIsFull = false;
	Jar->bIsSealed = false;

	SetStandState(JarStand, EStillState::Empty);
	UE_LOG(LogTemp, Warning, TEXT("Collected %d MoonshineJar; still reset to Empty"), JarsPerRun);
	ShowToast(FString::Printf(TEXT("Collected %d jars of moonshine!"), JarsPerRun), true);

	AutoSave(); // immediate autosave: collection is a high-value moment
}

bool AMoonshineCharacter_Simple::CheckStillPartPrereqs(FName PartID, FString& OutMsg) const
{
	// Mirrors the PER-STAND candidate logic the ghost-snap uses: the part is selectable when at
	// least one stand offers a valid, unoccupied mount with that stand's own prerequisites met.

	// Vessels: need any stand with a free mount for this vessel type.
	if (PartID == FName(TEXT("Pot")) || PartID == FName(TEXT("ThumperBody")) || PartID == FName(TEXT("WormBarrel")))
	{
		bool bAnyStand = false;
		for (AStillPartActor* Part : PlacedStillParts)
		{
			if (!IsValid(Part) || Part->PartID != FName(TEXT("CinderBlockStand"))) continue;
			bAnyStand = true;
			if (!FindPartOnStand(PartID, Part)) return true; // free mount found
		}
		OutMsg = bAnyStand
			? FString::Printf(TEXT("No free stand mount for %s"), *PartID.ToString())
			: FString::Printf(TEXT("%s requires the CinderBlockStand to be placed first"), *PartID.ToString());
		return false;
	}

	// Cap-like: same target/prereq table as the ghost snap.
	FName TargetType = NAME_None;
	FName SecondPrereq = NAME_None;
	if (PartID == FName(TEXT("Cap")))             { TargetType = FName(TEXT("Pot")); }
	else if (PartID == FName(TEXT("ThumperCap"))) { TargetType = FName(TEXT("ThumperBody")); }
	else if (PartID == FName(TEXT("CapArm")))     { TargetType = FName(TEXT("Cap")); SecondPrereq = FName(TEXT("ThumperCap")); }
	else if (PartID == FName(TEXT("OutletPipe"))) { TargetType = FName(TEXT("ThumperBody")); SecondPrereq = FName(TEXT("WormBarrel")); }
	else if (PartID == FName(TEXT("WormCoil")))   { TargetType = FName(TEXT("WormBarrel")); }
	else if (PartID == FName(TEXT("MasonJar")))   { TargetType = FName(TEXT("WormBarrel")); SecondPrereq = FName(TEXT("WormCoil")); }
	else if (PartID == FName(TEXT("MasonJarLid"))){ TargetType = FName(TEXT("MasonJar")); }

	if (TargetType == NAME_None)
	{
		// CinderBlockStand (and anything unlisted) has no selection-time prerequisite.
		return true;
	}

	const bool bIsLid = (PartID == FName(TEXT("MasonJarLid")));
	bool bAnyTarget = false;
	bool bAnyPrereqMissing = false;
	bool bAnyEmptyJar = false;

	for (AStillPartActor* Target : PlacedStillParts)
	{
		if (!IsValid(Target) || Target->PartID != TargetType) continue;
		bAnyTarget = true;

		AStillPartActor* TargetStand = StandOfPart(Target);
		if (!TargetStand) continue;
		if (FindPartOnStand(PartID, TargetStand)) continue; // occupied
		if (SecondPrereq != NAME_None && !FindPartOnStand(SecondPrereq, TargetStand))
		{
			bAnyPrereqMissing = true;
			continue;
		}
		if (bIsLid && !Target->bIsFull)
		{
			bAnyEmptyJar = true;
			continue;
		}
		return true; // valid candidate exists
	}

	if (!bAnyTarget)
	{
		OutMsg = FString::Printf(TEXT("%s requires %s to be placed first"), *PartID.ToString(), *TargetType.ToString());
	}
	else if (bAnyEmptyJar)
	{
		OutMsg = TEXT("Jar is empty — nothing to seal");
	}
	else if (bAnyPrereqMissing)
	{
		OutMsg = FString::Printf(TEXT("%s requires %s to be placed first"), *PartID.ToString(), *SecondPrereq.ToString());
	}
	else
	{
		OutMsg = FString::Printf(TEXT("No free %s for %s"), *TargetType.ToString(), *PartID.ToString());
	}
	return false;
}

void AMoonshineCharacter_Simple::SaveGame()
{
	// Manual save: the player asked for it — confirm loudly.
	DoSaveGame();
	ShowToast(TEXT("Game saved"), true);
}

void AMoonshineCharacter_Simple::AutoSave()
{
	if (!bAutosaveEnabled) return;

	// Silent: no toast, no sound — just the fading corner indicator.
	DoSaveGame();
	if (InteractionHUD)
	{
		InteractionHUD->ShowSaveIndicator();
	}
}

void AMoonshineCharacter_Simple::RequestAutosaveDebounced()
{
	if (!bAutosaveEnabled) return;

	// Restarting the timer coalesces a burst of placements into a single save.
	GetWorldTimerManager().SetTimer(AutosaveDebounceHandle, this,
		&AMoonshineCharacter_Simple::AutoSave, FMath::Max(AutosaveDebounceSeconds, 0.1f), false);
}

void AMoonshineCharacter_Simple::DoSaveGame()
{
	UBornToShineSaveGame* Save = Cast<UBornToShineSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UBornToShineSaveGame::StaticClass()));
	if (!Save) return;

	if (Inventory)
	{
		for (const FInventoryItem& Item : Inventory->GetItems())
		{
			FSavedInventoryItem Saved;
			Saved.ItemID = Item.ItemID;
			Saved.Count = Item.Quantity;
			Save->InventoryItems.Add(Saved);
		}
	}
	Save->Money = Money;

	// v2: persist ownership as an index into the stand list (stands in PlacedStillParts order).
	TArray<AStillPartActor*> Stands;
	for (AStillPartActor* Part : PlacedStillParts)
	{
		if (IsValid(Part) && Part->PartID == FName(TEXT("CinderBlockStand")))
		{
			Stands.Add(Part);
		}
	}

	for (AStillPartActor* Part : PlacedStillParts)
	{
		if (!IsValid(Part)) continue;
		FSavedStillPart SavedPart;
		SavedPart.PartID = Part->PartID;
		SavedPart.Transform = Part->GetActorTransform();
		SavedPart.bIsFull = Part->bIsFull;
		SavedPart.bIsSealed = Part->bIsSealed;
		SavedPart.StandIndex = (Part->PartID != FName(TEXT("CinderBlockStand")) && Part->OwningStand.IsValid())
			? Stands.IndexOfByKey(Part->OwningStand.Get())
			: INDEX_NONE;

		// v3: per-stand operating state. Mid-batch (Lit/Running) saves as Empty — ingredients
		// consumed by an interrupted run are not refunded (documented limitation).
		EStillState SavedState = Part->StillState;
		if (SavedState == EStillState::Lit || SavedState == EStillState::Running)
		{
			SavedState = EStillState::Empty;
		}
		SavedPart.StillState = (uint8)SavedState;

		Save->StillParts.Add(SavedPart);
	}

	UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, SaveUserIndex);
	UE_LOG(LogTemp, Warning, TEXT("Game saved: %d items, $%d, %d still parts"),
		Save->InventoryItems.Num(), Save->Money, Save->StillParts.Num());
}

void AMoonshineCharacter_Simple::LoadGame()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("No save found"));
		return;
	}

	UBornToShineSaveGame* Save = Cast<UBornToShineSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (!Save)
	{
		UE_LOG(LogTemp, Warning, TEXT("No save found"));
		return;
	}

	// Clear current state before restoring.
	if (Inventory)
	{
		Inventory->ClearInventory();
	}
	for (AStillPartActor* Part : PlacedStillParts)
	{
		if (IsValid(Part))
		{
			Part->Destroy();
		}
	}
	PlacedStillParts.Empty();

	// Restore inventory stacks and money.
	if (Inventory)
	{
		for (const FSavedInventoryItem& Item : Save->InventoryItems)
		{
			Inventory->AddItem(Item.ItemID, Item.Count);
		}
	}
	Money = Save->Money;

	// Respawn placed parts: same spawn path + data-table mesh assignment the placement flow uses.
	// Track spawned stands in order so v2 StandIndex ownership can be resolved afterwards.
	TArray<AStillPartActor*> SpawnedStands;
	TArray<AStillPartActor*> SpawnedParts;   // parallel to Save->StillParts (nullptr on spawn failure)

	for (const FSavedStillPart& SavedPart : Save->StillParts)
	{
		UStaticMesh* PartMesh = nullptr;
		if (Inventory)
		{
			FItemDataRow RowData;
			if (Inventory->GetItemData(SavedPart.PartID, RowData))
			{
				PartMesh = RowData.Mesh;
			}
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStillPartActor* Part = GetWorld()->SpawnActor<AStillPartActor>(AStillPartActor::StaticClass(),
			SavedPart.Transform.GetLocation(), SavedPart.Transform.Rotator(), SpawnParams);
		if (Part)
		{
			Part->InitFromItemData(SavedPart.PartID, PartMesh);
			Part->bIsFull = SavedPart.bIsFull;
			Part->bIsSealed = SavedPart.bIsSealed;
			if (SavedPart.PartID == FName(TEXT("CinderBlockStand")))
			{
				// v3 restores per-stand state (v1/v2 default to Empty). Batches never resume.
				Part->StillState = (Save->SaveVersion >= 3) ? (EStillState)SavedPart.StillState : EStillState::Empty;
				Part->bBatchRunning = false;
				Part->BatchElapsed = 0.0f;
				SpawnedStands.Add(Part);
			}
			PlacedStillParts.Add(Part);
		}
		SpawnedParts.Add(Part);
	}

	// Resolve ownership.
	if (Save->SaveVersion >= 2)
	{
		for (int32 i = 0; i < Save->StillParts.Num(); ++i)
		{
			AStillPartActor* Part = SpawnedParts.IsValidIndex(i) ? SpawnedParts[i] : nullptr;
			const int32 StandIndex = Save->StillParts[i].StandIndex;
			if (Part && Part->PartID != FName(TEXT("CinderBlockStand")) && SpawnedStands.IsValidIndex(StandIndex))
			{
				Part->OwningStand = SpawnedStands[StandIndex];
			}
		}
	}
	else
	{
		// Legacy v1 migration: no ownership data. With exactly one stand, everything belongs to it.
		if (SpawnedStands.Num() == 1)
		{
			for (AStillPartActor* Part : SpawnedParts)
			{
				if (Part && Part->PartID != FName(TEXT("CinderBlockStand")))
				{
					Part->OwningStand = SpawnedStands[0];
				}
			}
		}
		else if (SpawnedStands.Num() > 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("Load: v1 save with %d stands — part ownership cannot be migrated"), SpawnedStands.Num());
		}
	}

	// Recompute readiness per stand. Completion tracking restarts from scratch; audio loops
	// reconcile on the next tick. Mid-batch state was not saved (per-stand limitation).
	CompletedStands.Empty();
	CheckStillCompletion();

	UE_LOG(LogTemp, Warning, TEXT("Game loaded: %d items, $%d, %d still parts"),
		Save->InventoryItems.Num(), Save->Money, Save->StillParts.Num());
	ShowToast(TEXT("Game loaded"), true);
}

void AMoonshineCharacter_Simple::AddMoney(int32 Amount)
{
	Money = FMath::Max(0, Money + Amount);
	UE_LOG(LogTemp, Warning, TEXT("Money: +$%d (total $%d)"), Amount, Money);
}

void AMoonshineCharacter_Simple::SellMoonshine(ABuyerActor* Buyer)
{
	if (!IsValid(Buyer) || !Inventory) return;

	const int32 JarCount = Inventory->GetItemCount(FName(TEXT("MoonshineJar")));
	if (JarCount <= 0) return; // nothing to sell — prompt already says so

	Inventory->RemoveItem(FName(TEXT("MoonshineJar")), JarCount);
	const int32 Total = JarCount * Buyer->PricePerJar;
	AddMoney(Total);

	UE_LOG(LogTemp, Warning, TEXT("Sold %d jars for $%d"), JarCount, Total);
	ShowToast(FString::Printf(TEXT("Sold %d jars — $%d! (Total: $%d)"), JarCount, Total, Money), true);
	PlaySfx2D(SellSound, TEXT("SellSound"));

	AutoSave(); // immediate autosave: sale is a high-value moment
}

void AMoonshineCharacter_Simple::UpdateStillPrompt()
{
	if (!InteractionHUD) return;

	// Countdowns are visible while any still is Running, regardless of where the player looks.
	// Nearest running still first; up to 3 lines stacked; the bar shows the nearest one's fill.
	{
		struct FRunningStill { AStillPartActor* Stand; float DistSq; };
		TArray<FRunningStill> RunningStills;
		const FVector PlayerLoc = GetActorLocation();

		for (AStillPartActor* Stand : PlacedStillParts)
		{
			if (IsValid(Stand) && Stand->PartID == FName(TEXT("CinderBlockStand")) &&
				Stand->StillState == EStillState::Running)
			{
				RunningStills.Add({ Stand, float(FVector::DistSquared(PlayerLoc, Stand->GetActorLocation())) });
			}
		}

		if (RunningStills.Num() > 0)
		{
			RunningStills.Sort([](const FRunningStill& A, const FRunningStill& B) { return A.DistSq < B.DistSq; });

			const float Total = FMath::Max(BatchTimeSeconds, 0.01f);
			FString Lines;
			const int32 NumLines = FMath::Min(RunningStills.Num(), 3);
			for (int32 i = 0; i < NumLines; ++i)
			{
				AStillPartActor* Stand = RunningStills[i].Stand;
				const int32 Remaining = FMath::Max(0, FMath::CeilToInt(Total - Stand->BatchElapsed));
				if (!Lines.IsEmpty()) Lines += TEXT("\n");
				Lines += FString::Printf(TEXT("STILL %d  %d:%02d"), StandNumber(Stand), Remaining / 60, Remaining % 60);
			}

			InteractionHUD->ShowTimer(Lines, RunningStills[0].Stand->BatchElapsed / Total);
		}
		else
		{
			InteractionHUD->HideTimer();
		}
	}

	// Contextual [E] prompt from whatever interactable is aimed at; empty string = hidden.
	FString Prompt;

	if (ABuyerActor* Buyer = GetAimedBuyer())
	{
		const int32 JarCount = Inventory ? Inventory->GetItemCount(FName(TEXT("MoonshineJar"))) : 0;
		Prompt = (JarCount > 0)
			? FString::Printf(TEXT("Sell moonshine (%d jars @ $%d)"), JarCount, Buyer->PricePerJar)
			: FString(TEXT("No moonshine to sell"));
	}
	else if (AStillPartActor* SealedJar = GetAimedSealedJar())
	{
		const int32 ToCollect = (SealedJar->RemainingJars > 0) ? SealedJar->RemainingJars : JarsPerRun;
		Prompt = FString::Printf(TEXT("Collect moonshine (%d jars)"), ToCollect);
	}
	else if (AStillPartActor* Pot = GetAimedPot())
	{
		// Prompt reads the aimed pot's OWN stand state (GetAimedPot verified completeness).
		AStillPartActor* Stand = StandOfPart(Pot);
		switch (Stand ? Stand->StillState : EStillState::Empty)
		{
		case EStillState::Empty: Prompt = FString::Printf(TEXT("Add Water (%d)"), WaterCost); break;
		case EStillState::Water: Prompt = FString::Printf(TEXT("Add Mash (%d)"), MashCost); break;
		case EStillState::Mash:  Prompt = FString::Printf(TEXT("Light Fire (%d Firewood)"), FirewoodCost); break;
		case EStillState::Done:  Prompt = TEXT("Batch complete — jar is full"); break;
		default: break; // Lit/Running: the countdown bar covers it
		}
	}

	if (Prompt.IsEmpty())
	{
		InteractionHUD->ClearPrompt();
	}
	else
	{
		InteractionHUD->SetPrompt(Prompt);
	}
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
		// Per-part snap config: target type, optional second prerequisite (must exist on the SAME
		// stand as the candidate target), and the mount offset/rotation. Prereqs are evaluated
		// PER STAND so a complete still #1 never satisfies requirements for still #2's parts.
		FName SnapTargetID;
		FName SecondPrereq = NAME_None;
		FVector MountOffset;
		FRotator MountRotation = FRotator::ZeroRotator;

		if (GhostPartID == FName(TEXT("CapArm")))
		{
			SnapTargetID = FName(TEXT("Cap"));
			SecondPrereq = FName(TEXT("ThumperCap"));
			MountOffset = CapArmMountOffset;
			MountRotation = CapArmMountRotation;
		}
		else if (GhostPartID == FName(TEXT("OutletPipe")))
		{
			SnapTargetID = FName(TEXT("ThumperBody"));
			SecondPrereq = FName(TEXT("WormBarrel"));
			MountOffset = OutletPipeMountOffset;
		}
		else if (GhostPartID == FName(TEXT("WormCoil")))
		{
			SnapTargetID = FName(TEXT("WormBarrel"));
			MountOffset = WormCoilMountOffset;
		}
		else if (GhostPartID == FName(TEXT("MasonJar")))
		{
			SnapTargetID = FName(TEXT("WormBarrel"));
			SecondPrereq = FName(TEXT("WormCoil"));
			MountOffset = MasonJarMountOffset;
			MountRotation = MasonJarMountRotation;
		}
		else if (GhostPartID == FName(TEXT("MasonJarLid")))
		{
			SnapTargetID = FName(TEXT("MasonJar"));
			MountOffset = MasonJarLidMountOffset;
			MountRotation = MasonJarLidMountRotation;
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

		const bool bIsLid = (GhostPartID == FName(TEXT("MasonJarLid")));
		const float SnapRadius = IsValid(GhostStillPart) ? GhostStillPart->SnapRadiusCm : StillSnapRadiusCm;

		// Scan every valid target candidate and keep the one nearest the aim ray.
		AStillPartActor* BestTarget = nullptr;
		float BestRayDist = TNumericLimits<float>::Max();
		FTransform BestXform;
		bool bSawEmptyJar = false;

		for (AStillPartActor* Target : PlacedStillParts)
		{
			if (!IsValid(Target) || Target->PartID != SnapTargetID) continue;

			AStillPartActor* TargetStand = StandOfPart(Target);
			if (!TargetStand) continue;

			// Mount occupied: a part of the ghost's type already belongs to this stand.
			if (FindPartOnStand(GhostPartID, TargetStand)) continue;

			// Per-stand prerequisite (e.g. CapArm needs this stand's ThumperCap too).
			if (SecondPrereq != NAME_None && !FindPartOnStand(SecondPrereq, TargetStand)) continue;

			// The lid only seals a FULL jar.
			if (bIsLid && !Target->bIsFull) { bSawEmptyJar = true; continue; }

			const FVector MountWorld = Target->GetActorTransform().TransformPosition(MountOffset);
			const FRotator SnapRot = Target->GetActorRotation() + MountRotation;

			// Aim at the part's VISUAL center, not its pivot (no one-frame lag; see GhostVisualCenter).
			const FVector AimTarget = GhostVisualCenter(FTransform(SnapRot, MountWorld), MountWorld);
			const FVector ToMount = AimTarget - CamLoc;
			const float Along = FVector::DotProduct(ToMount, CamFwd);
			const FVector ClosestOnRay = CamLoc + CamFwd * FMath::Clamp(Along, 0.0f, MaxAimDistanceCm);
			const float RayDist = FVector::Dist(ClosestOnRay, AimTarget);

			if (Along > 0.0f && RayDist < BestRayDist)
			{
				BestRayDist = RayDist;
				BestTarget = Target;
				BestXform = FTransform(SnapRot, MountWorld);
			}
		}

		if (bIsLid && !BestTarget && bSawEmptyJar)
		{
			// Per-tick caller: AddToast dedupes identical fresh messages (refresh, not stack).
			ShowToast(TEXT("Jar is empty — nothing to seal"), false);
		}

		if (BestTarget && BestRayDist <= SnapRadius)
		{
			GhostSnapTransform = BestXform;
			GhostSnapOwningStand = StandOfPart(BestTarget);
			GhostStillPart->SetActorLocationAndRotation(BestXform.GetLocation(), BestXform.Rotator());
			bGhostSnapValid = true;
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

	// Candidate stands: every placed stand whose mount for THIS vessel type is unoccupied.
	// Among candidates, snap to the one whose mount point is nearest the camera aim ray.
	{
		const float SnapRadius = IsValid(GhostStillPart) ? GhostStillPart->SnapRadiusCm : StillSnapRadiusCm;

		AStillPartActor* BestStand = nullptr;
		float BestRayDist = TNumericLimits<float>::Max();
		FTransform BestXform;

		for (AStillPartActor* Stand : PlacedStillParts)
		{
			if (!IsValid(Stand) || Stand->PartID != FName(TEXT("CinderBlockStand"))) continue;

			// Mount occupied: this stand already has a vessel of this type.
			if (FindPartOnStand(GhostPartID, Stand)) continue;

			// TransformPosition respects the stand's rotation, so a rotated stand places vessels correctly.
			FVector MountWorld = Stand->GetActorTransform().TransformPosition(MountLocal);
			MountWorld.Z += ZAdjust;

			const FRotator SnapRot(0.0f, Stand->GetActorRotation().Yaw, 0.0f);

			// Aim at the vessel's VISUAL center, not its pivot. Snap POSITION stays MountWorld unchanged.
			const FVector AimTarget = GhostVisualCenter(FTransform(SnapRot, MountWorld), MountWorld);
			const FVector ToMount = AimTarget - CamLoc;
			const float Along = FVector::DotProduct(ToMount, CamFwd);
			const FVector ClosestOnRay = CamLoc + CamFwd * FMath::Clamp(Along, 0.0f, MaxAimDistanceCm);
			const float RayDist = FVector::Dist(ClosestOnRay, AimTarget);

			if (Along > 0.0f && RayDist < BestRayDist)
			{
				BestRayDist = RayDist;
				BestStand = Stand;
				BestXform = FTransform(SnapRot, MountWorld);
			}
		}

		if (BestStand && BestRayDist <= SnapRadius)
		{
			GhostSnapTransform = BestXform;
			GhostSnapOwningStand = BestStand;
			GhostStillPart->SetActorLocationAndRotation(BestXform.GetLocation(), BestXform.Rotator());
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
		// Stands own themselves (OwningStand stays null); every other part records its stand,
		// chosen by the snap candidate selection in UpdateStillGhost.
		if (GhostPartID != FName(TEXT("CinderBlockStand")))
		{
			Placed->OwningStand = GhostSnapOwningStand;
		}
		PlacedStillParts.Add(Placed);

		if (Inventory)
		{
			Inventory->RemoveItem(GhostPartID, 1);
		}

		UE_LOG(LogTemp, Log, TEXT("Placed %s (snapped) at %s"), *GhostPartID.ToString(), *GhostSnapTransform.GetLocation().ToString());
		PlaySfxAt(PartPlaceSound, TEXT("PartPlaceSound"), GhostSnapTransform.GetLocation());

		// Placing the lid on a full jar seals THAT stand's jar (ghost validity guaranteed it's full).
		if (GhostPartID == FName(TEXT("MasonJarLid")))
		{
			if (AStillPartActor* Jar = FindPartOnStand(FName(TEXT("MasonJar")), GhostSnapOwningStand.Get()))
			{
				Jar->bIsSealed = true;
				UE_LOG(LogTemp, Warning, TEXT("Jar sealed — press E on the jar to collect"));
				PlaySfxAt(LidPlaceSound, TEXT("LidPlaceSound"), Jar->GetActorLocation());
			}
		}

		// Detection only: re-evaluate whether the full Tier 2 still is now assembled.
		CheckStillCompletion();

		RequestAutosaveDebounced(); // autosave: part placed (debounced)
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
	GhostSnapOwningStand.Reset();

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
