// Born To Shine - Simplified Player Character using BuildingComponent

#include "MoonshineCharacter_Simple.h"
#include "BuildingComponent.h"
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

	// Create first person camera
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f)); // Eye height
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Create Building Component
	BuildingComponent = CreateDefaultSubobject<UBuildingComponent>(TEXT("BuildingComponent"));

	// Default settings
	bIsFirstPerson = false;
	WalkSpeed = 400.0f;
	SprintSpeed = 800.0f;
	MouseSensitivity = 1.0f;

	// Configure character movement
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = 600.0f;
	GetCharacterMovement()->AirControl = 0.3f;

	// Use controller rotation
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
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
		AddControllerPitchInput(LookAxisVector.Y * MouseSensitivity);
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
		FirstPersonCamera->SetActive(true);
		ThirdPersonCamera->SetActive(false);
		UE_LOG(LogTemp, Log, TEXT("Switched to First Person"));
	}
	else
	{
		FirstPersonCamera->SetActive(false);
		ThirdPersonCamera->SetActive(true);
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

void AMoonshineCharacter_Simple::OnRotateLeft()
{
	if (BuildingComponent)
	{
		BuildingComponent->RotatePreviewLeft();
	}
}

void AMoonshineCharacter_Simple::OnRotateRight()
{
	if (BuildingComponent)
	{
		BuildingComponent->RotatePreviewRight();
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
