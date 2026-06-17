// Born To Shine - Main menu: Continue / New Game / Load Game / Quit

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;

UCLASS()
class BORNTOSHINE_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Menu")
	UTexture2D* BackgroundTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Menu")
	FName GameplayMapName = TEXT("MainLevel");

protected:
	UPROPERTY() UImage* BackgroundImage = nullptr;
	UPROPERTY() UButton* ContinueButton = nullptr;
	UPROPERTY() UButton* NewGameButton = nullptr;
	UPROPERTY() UButton* LoadGameButton = nullptr;
	UPROPERTY() UButton* QuitButton = nullptr;
	UPROPERTY() UTextBlock* TitleText = nullptr;
	UPROPERTY() UTextBlock* VersionText = nullptr;

	UFUNCTION() void OnContinueClicked();
	UFUNCTION() void OnNewGameClicked();
	UFUNCTION() void OnLoadGameClicked();
	UFUNCTION() void OnQuitClicked();

	void OpenGameplayLevel(bool bLoadExistingSave);
};
