// Born To Shine - Game Mode

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BornToShineGameMode.generated.h"

/**
 * Main game mode for Born To Shine
 * Initializes construction managers and sets up the game
 */
UCLASS()
class BORNTOSHINE_API ABornToShineGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABornToShineGameMode();

	virtual void BeginPlay() override;

	// Get the socket manager
	UFUNCTION(BlueprintCallable, Category = "Construction")
	class ASocketManager* GetSocketManager() const { return SocketManager; }

	// Get the construction phase manager
	UFUNCTION(BlueprintCallable, Category = "Construction")
	class AConstructionPhaseManager* GetPhaseManager() const { return PhaseManager; }

protected:
	// Socket manager instance
	UPROPERTY()
	class ASocketManager* SocketManager;

	// Construction phase manager instance
	UPROPERTY()
	class AConstructionPhaseManager* PhaseManager;

	// Initialize construction system
	void InitializeConstructionSystem();
};
