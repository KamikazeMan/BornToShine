// Born To Shine - Player Character with Third/First Person Toggle

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "MoonshineCharacter.generated.h"

/**
 * Player character with camera modes and build system interaction
 */
UCLASS()
class BORNTOSHINE_API AMoonshineCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMoonshineCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Toggle between first and third person view
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ToggleCameraMode();

	// Get current camera mode
	UFUNCTION(BlueprintCallable, Category = "Camera")
	bool IsFirstPerson() const { return bIsFirstPerson; }

	// Enter/exit build mode
	UFUNCTION(BlueprintCallable, Category = "Building")
	void ToggleBuildMode();

	UFUNCTION(BlueprintCallable, Category = "Building")
	bool IsInBuildMode() const { return bIsInBuildMode; }

protected:
	virtual void BeginPlay() override;

	// Input callbacks
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Jump();
	void Sprint();
	void StopSprinting();

	// Build mode input callbacks
	void OnPlacePiece();
	void OnRotateLeft();
	void OnRotateRight();
	void OnRotatePitch(float Value);
	void OnRotateRoll(float Value);
	void OnScalePiece(float Value);
	void OnCyclePieceType();
	void OnNailPiece();
	void OnToggleBoardType(); // Toggle between outside and inside board for rim boards
	void OnZoomStart();  // Hold RMB to zoom in
	void OnZoomStop();   // Release RMB to zoom out

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
	class UInputAction* ToggleBoardTypeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* ZoomAction;

	// Camera components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* ThirdPersonArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* ThirdPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* FirstPersonCamera;

	// Camera settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ThirdPersonArmLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector ThirdPersonArmOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bIsFirstPerson;

	// Movement settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SprintSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MouseSensitivity;

	// Build mode
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	bool bIsInBuildMode;

	// Current piece being placed
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	class ABuildablePiece* CurrentPreviewPiece;

	// Available piece types
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	TArray<TSubclassOf<class ABuildablePiece>> AvailablePieceTypes;

	// Current piece type index
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	int32 CurrentPieceTypeIndex;

	// Build mode settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	float BuildRaycastDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	float PiecePreviewDistance;

	// Zoom settings (right mouse button hold)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float DefaultFOV;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomedFOV;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomInterpSpeed;

	bool bIsZooming;

	// Update preview piece position
	void UpdatePreviewPiecePosition();

	// Spawn preview piece
	void SpawnPreviewPiece();

	// Destroy preview piece
	void DestroyPreviewPiece();

	// Get piece placement location from camera raycast
	bool GetPlacementLocation(FVector& OutLocation, FVector& OutNormal);
};
