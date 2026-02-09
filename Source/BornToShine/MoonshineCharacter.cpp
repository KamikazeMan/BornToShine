// Born To Shine - Player Character with Third/First Person Toggle

#include "MoonshineCharacter.h"
#include "BuildablePiece.h"
#include "RimBoard.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"

AMoonshineCharacter::AMoonshineCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create third person camera boom - positioned to avoid seeing character body
	ThirdPersonArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonArm"));
	ThirdPersonArm->SetupAttachment(RootComponent);
	ThirdPersonArm->TargetArmLength = 450.0f;  // Further behind character
	ThirdPersonArm->SetRelativeLocation(FVector(-40.0f, 0.0f, 140.0f));  // High above head, slightly back
	ThirdPersonArm->SocketOffset = FVector(0.0f, 80.0f, 40.0f);  // Offset right and up for over-shoulder
	ThirdPersonArm->bUsePawnControlRotation = true;
	ThirdPersonArm->bEnableCameraLag = true;
	ThirdPersonArm->CameraLagSpeed = 8.0f;  // Faster lag for responsive feel
	ThirdPersonArm->bDoCollisionTest = true;  // Camera avoids walls
	ThirdPersonArm->ProbeSize = 12.0f;  // Collision probe size
	ThirdPersonArm->ProbeChannel = ECC_Camera;  // Use camera collision channel

	// Create third person camera
	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(ThirdPersonArm, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;

	// Create first person camera - pushed forward to avoid seeing body
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(40.0f, 0.0f, 75.0f)); // Forward of head, eye height
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Default settings
	bIsFirstPerson = false;
	bIsInBuildMode = false;
	WalkSpeed = 400.0f;
	SprintSpeed = 800.0f;
	MouseSensitivity = 1.0f;

	ThirdPersonArmLength = 450.0f;
	ThirdPersonArmOffset = FVector(0.0f, 80.0f, 40.0f);

	// Build mode settings
	CurrentPreviewPiece = nullptr;
	CurrentPieceTypeIndex = 0;
	BuildRaycastDistance = 2000.0f; // 20 meters
	PiecePreviewDistance = 300.0f;  // 3 meters from camera

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

void AMoonshineCharacter::BeginPlay()
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

void AMoonshineCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update preview piece position if in build mode
	if (bIsInBuildMode && CurrentPreviewPiece)
	{
		UpdatePreviewPiecePosition();
	}
}

void AMoonshineCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Movement
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMoonshineCharacter::Move);
		}

		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMoonshineCharacter::Look);
		}

		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMoonshineCharacter::Jump);
		}

		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AMoonshineCharacter::Sprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AMoonshineCharacter::StopSprinting);
		}

		// Camera
		if (ToggleCameraAction)
		{
			EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &AMoonshineCharacter::ToggleCameraMode);
		}

		// Build mode
		if (ToggleBuildModeAction)
		{
			EnhancedInputComponent->BindAction(ToggleBuildModeAction, ETriggerEvent::Started, this, &AMoonshineCharacter::ToggleBuildMode);
		}

		if (PlacePieceAction)
		{
			EnhancedInputComponent->BindAction(PlacePieceAction, ETriggerEvent::Started, this, &AMoonshineCharacter::OnPlacePiece);
		}

		if (NailPieceAction)
		{
			EnhancedInputComponent->BindAction(NailPieceAction, ETriggerEvent::Started, this, &AMoonshineCharacter::OnNailPiece);
		}

		if (CyclePieceAction)
		{
			EnhancedInputComponent->BindAction(CyclePieceAction, ETriggerEvent::Started, this, &AMoonshineCharacter::OnCyclePieceType);
		}

		if (ToggleBoardTypeAction)
		{
			EnhancedInputComponent->BindAction(ToggleBoardTypeAction, ETriggerEvent::Started, this, &AMoonshineCharacter::OnToggleBoardType);
		}
	}
}

void AMoonshineCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// Find forward and right directions
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// Add movement
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMoonshineCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X * MouseSensitivity);
		AddControllerPitchInput(LookAxisVector.Y * MouseSensitivity * -1.0f); // Negated for correct look direction
	}
}

void AMoonshineCharacter::Jump()
{
	ACharacter::Jump();
}

void AMoonshineCharacter::Sprint()
{
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void AMoonshineCharacter::StopSprinting()
{
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void AMoonshineCharacter::ToggleCameraMode()
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

void AMoonshineCharacter::ToggleBuildMode()
{
	bIsInBuildMode = !bIsInBuildMode;

	if (bIsInBuildMode)
	{
		UE_LOG(LogTemp, Log, TEXT("Entered Build Mode"));
		SpawnPreviewPiece();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Exited Build Mode"));
		DestroyPreviewPiece();
	}
}

void AMoonshineCharacter::SpawnPreviewPiece()
{
	if (AvailablePieceTypes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No piece types available for building"));
		return;
	}

	// Ensure index is valid
	if (CurrentPieceTypeIndex >= AvailablePieceTypes.Num())
	{
		CurrentPieceTypeIndex = 0;
	}

	// Destroy existing preview piece
	DestroyPreviewPiece();

	// Spawn new preview piece
	FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * PiecePreviewDistance;
	FRotator SpawnRotation = FRotator::ZeroRotator;

	CurrentPreviewPiece = GetWorld()->SpawnActor<ABuildablePiece>(
		AvailablePieceTypes[CurrentPieceTypeIndex],
		SpawnLocation,
		SpawnRotation
	);

	if (CurrentPreviewPiece)
	{
		CurrentPreviewPiece->SetPreviewMode(true);
		UE_LOG(LogTemp, Log, TEXT("Spawned preview piece"));
	}
}

void AMoonshineCharacter::DestroyPreviewPiece()
{
	if (CurrentPreviewPiece)
	{
		CurrentPreviewPiece->Destroy();
		CurrentPreviewPiece = nullptr;
	}
}

void AMoonshineCharacter::UpdatePreviewPiecePosition()
{
	if (!CurrentPreviewPiece) return;

	FVector PlacementLocation;
	FVector PlacementNormal;

	if (GetPlacementLocation(PlacementLocation, PlacementNormal))
	{
		// Update preview piece position
		CurrentPreviewPiece->UpdatePreviewPosition(PlacementLocation, FRotator::ZeroRotator);
	}
	else
	{
		// No valid placement location, place in front of camera
		UCameraComponent* ActiveCamera = bIsFirstPerson ? FirstPersonCamera : ThirdPersonCamera;
		FVector CameraLocation = ActiveCamera->GetComponentLocation();
		FVector CameraForward = ActiveCamera->GetForwardVector();

		FVector DefaultLocation = CameraLocation + (CameraForward * PiecePreviewDistance);
		CurrentPreviewPiece->UpdatePreviewPosition(DefaultLocation, FRotator::ZeroRotator);
	}
}

bool AMoonshineCharacter::GetPlacementLocation(FVector& OutLocation, FVector& OutNormal)
{
	UCameraComponent* ActiveCamera = bIsFirstPerson ? FirstPersonCamera : ThirdPersonCamera;
	FVector CameraLocation = ActiveCamera->GetComponentLocation();
	FVector CameraForward = ActiveCamera->GetForwardVector();

	FVector Start = CameraLocation;
	FVector End = Start + (CameraForward * BuildRaycastDistance);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (CurrentPreviewPiece)
	{
		QueryParams.AddIgnoredActor(CurrentPreviewPiece);
	}

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	if (bHit)
	{
		OutLocation = HitResult.Location;
		OutNormal = HitResult.Normal;
		return true;
	}

	return false;
}

void AMoonshineCharacter::OnPlacePiece()
{
	if (!bIsInBuildMode || !CurrentPreviewPiece) return;

	// Try to place the piece
	if (CurrentPreviewPiece->TryPlace())
	{
		// Success! Spawn a new preview piece
		CurrentPreviewPiece = nullptr; // The placed piece is no longer the preview
		SpawnPreviewPiece();
		UE_LOG(LogTemp, Log, TEXT("Piece placed successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot place piece at this location"));
	}
}

void AMoonshineCharacter::OnNailPiece()
{
	if (!bIsInBuildMode) return;

	// Find the last placed piece (not nailed yet)
	// TODO: Implement piece selection system
	UE_LOG(LogTemp, Log, TEXT("Nail piece - TODO: Implement piece selection"));
}

void AMoonshineCharacter::OnCyclePieceType()
{
	if (!bIsInBuildMode) return;

	CurrentPieceTypeIndex = (CurrentPieceTypeIndex + 1) % AvailablePieceTypes.Num();
	SpawnPreviewPiece();

	UE_LOG(LogTemp, Log, TEXT("Cycled to piece type %d"), CurrentPieceTypeIndex);
}

void AMoonshineCharacter::OnRotateLeft()
{
	if (CurrentPreviewPiece)
	{
		CurrentPreviewPiece->RotateLeft();
	}
}

void AMoonshineCharacter::OnRotateRight()
{
	if (CurrentPreviewPiece)
	{
		CurrentPreviewPiece->RotateRight();
	}
}

void AMoonshineCharacter::OnRotatePitch(float Value)
{
	if (CurrentPreviewPiece && FMath::Abs(Value) > 0.1f)
	{
		if (Value > 0)
		{
			CurrentPreviewPiece->RotateFront();
		}
		else
		{
			CurrentPreviewPiece->RotateBack();
		}
	}
}

void AMoonshineCharacter::OnRotateRoll(float Value)
{
	if (CurrentPreviewPiece && FMath::Abs(Value) > 0.1f)
	{
		CurrentPreviewPiece->RotateRoll(Value * 15.0f); // 15 degrees per input
	}
}

void AMoonshineCharacter::OnScalePiece(float Value)
{
	if (CurrentPreviewPiece && FMath::Abs(Value) > 0.1f)
	{
		CurrentPreviewPiece->ScalePiece(Value * 0.1f); // 0.1 scale per scroll
	}
}

void AMoonshineCharacter::OnToggleBoardType()
{
	if (!bIsInBuildMode || !CurrentPreviewPiece) return;

	// Only works on rim boards
	ARimBoard* RimBoard = Cast<ARimBoard>(CurrentPreviewPiece);
	if (RimBoard)
	{
		RimBoard->ToggleBoardType();
		UE_LOG(LogTemp, Warning, TEXT("Toggled board type: %s"),
			RimBoard->bIsOutsideBoard ? TEXT("OUTSIDE") : TEXT("INSIDE"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Toggle board type only works on rim boards"));
	}
}
