// Born To Shine - Simplified Player Character using BuildingComponent

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "MoonshineCharacter_Simple.generated.h"

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

	// Toggle between first and third person view
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ToggleCameraMode();

	// Get current camera mode
	UFUNCTION(BlueprintCallable, Category = "Camera")
	bool IsFirstPerson() const { return bIsFirstPerson; }

protected:
	virtual void BeginPlay() override;

	// Input callbacks - Movement
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Jump();
	void Sprint();
	void StopSprinting();

	// Input callbacks - Building (delegates to BuildingComponent)
	void OnToggleBuildMode();
	void OnPlacePiece();
	void OnRotateLeft();
	void OnRotateRight();
	void OnScalePiece(const FInputActionValue& Value);
	void OnCyclePieceType();
	void OnNailPiece();

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
};
