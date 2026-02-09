// Born To Shine - Player Controller

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MoonshinePlayerController.generated.h"

/**
 * Custom Player Controller for Born To Shine
 * Handles input mapping and UI management
 */
UCLASS()
class BORNTOSHINE_API AMoonshinePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMoonshinePlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// Show/hide build UI
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowBuildModeUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideBuildModeUI();

	// Get current construction phase
	UFUNCTION(BlueprintCallable, Category = "Construction")
	FString GetCurrentPhaseDescription() const;

	// UI Widget references
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UUserWidget> BuildModeWidgetClass;

	UPROPERTY()
	class UUserWidget* BuildModeWidget;
};
