// Born To Shine - Game instance: persists across level transitions (menu -> gameplay)

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BornToShineGameInstance.generated.h"

UCLASS()
class BORNTOSHINE_API UBornToShineGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// When true the gameplay level should load the existing save on BeginPlay.
	// Set by Continue / Load Game; cleared by New Game.
	UPROPERTY()
	bool bShouldLoadSave = false;

	// If non-empty the gameplay level loads THIS slot instead of the default.
	UPROPERTY()
	FString PendingLoadSlot;

	// Convenience: does a save file exist for the default slot?
	bool HasExistingSave() const;
};
