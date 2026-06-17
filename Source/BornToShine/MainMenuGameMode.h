// Born To Shine - Main menu game mode: shows the menu widget, no gameplay pawn

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

class UMainMenuWidget;

UCLASS()
class BORNTOSHINE_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainMenuGameMode();
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Menu")
	TSubclassOf<UMainMenuWidget> MenuWidgetClass;

protected:
	UPROPERTY()
	UMainMenuWidget* MenuWidget = nullptr;
};
