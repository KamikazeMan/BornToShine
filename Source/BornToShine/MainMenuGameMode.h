// Born To Shine - Main menu game mode: shows the menu widget, no gameplay pawn

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

UCLASS()
class BORNTOSHINE_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainMenuGameMode();
	virtual void BeginPlay() override;

protected:
	UPROPERTY()
	class UMainMenuWidget* MenuWidget = nullptr;
};
