// Born To Shine - Simplified Player Character using BuildingComponent

#include "MoonshineCharacter_Simple.h"
#include "BuildingComponent.h"
#include "ConstructionPhaseManager.h"
#include "RimBoard.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

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

		if (CyclePieceAction)
		{
			EnhancedInputComponent->BindAction(CyclePieceAction, ETriggerEvent::Started, this, &AMoonshineCharacter_Simple::OnCyclePieceType);
		}

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
	}
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
	if (!BuildingComponent) return;

	// Get the current preview piece from the building component
	ABuildablePiece* PreviewPiece = BuildingComponent->GetCurrentPreviewPiece();
	if (!PreviewPiece) return;

	// Only works on rim boards
	ARimBoard* RimBoard = Cast<ARimBoard>(PreviewPiece);
	if (RimBoard)
	{
		RimBoard->ToggleBoardType();

		FString BoardType = RimBoard->bIsOutsideBoard ? TEXT("OUTSIDE") : TEXT("INSIDE");
		UE_LOG(LogTemp, Warning, TEXT("Toggled board type: %s"), *BoardType);

		// Show on-screen message
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
				FString::Printf(TEXT("Board Type: %s"), *BoardType));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Toggle board type only works on rim boards"));
	}
}
