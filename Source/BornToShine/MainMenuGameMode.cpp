// Born To Shine - Main menu game mode

#include "MainMenuGameMode.h"
#include "MainMenuWidget.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	DefaultPawnClass = ADefaultPawn::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	HUDClass = nullptr;
	MenuWidgetClass = UMainMenuWidget::StaticClass();
}

void AMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	TSubclassOf<UMainMenuWidget> WidgetClass = MenuWidgetClass ? MenuWidgetClass : TSubclassOf<UMainMenuWidget>(UMainMenuWidget::StaticClass());
	MenuWidget = CreateWidget<UMainMenuWidget>(PC, WidgetClass);
	if (MenuWidget)
	{
		MenuWidget->AddToViewport(100);
	}
}
