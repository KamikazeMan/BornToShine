// Born To Shine - Player Controller

#include "MoonshinePlayerController.h"
#include "ConstructionPhaseManager.h"
#include "Blueprint/UserWidget.h"

AMoonshinePlayerController::AMoonshinePlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableTouchEvents = false;
	BuildModeWidget = nullptr;
}

void AMoonshinePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Set input mode to game only
	SetInputMode(FInputModeGameOnly());
}

void AMoonshinePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Additional input bindings can be added here
	// Most input is handled by the Character class
}

void AMoonshinePlayerController::ShowBuildModeUI()
{
	if (BuildModeWidgetClass && !BuildModeWidget)
	{
		BuildModeWidget = CreateWidget<UUserWidget>(this, BuildModeWidgetClass);
		if (BuildModeWidget)
		{
			BuildModeWidget->AddToViewport();
		}
	}
	else if (BuildModeWidget)
	{
		BuildModeWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void AMoonshinePlayerController::HideBuildModeUI()
{
	if (BuildModeWidget)
	{
		BuildModeWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

FString AMoonshinePlayerController::GetCurrentPhaseDescription() const
{
	if (AConstructionPhaseManager::Instance)
	{
		return AConstructionPhaseManager::Instance->GetPhaseRequirements();
	}
	return TEXT("Phase Manager not found");
}
