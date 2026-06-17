// Born To Shine - HUD with crosshair for building mode

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BornToShineHUD.generated.h"

/**
 * HUD class that displays a crosshair when in build mode
 */
UCLASS()
class BORNTOSHINE_API ABornToShineHUD : public AHUD
{
	GENERATED_BODY()

public:
	ABornToShineHUD();

	virtual void DrawHUD() override;

	// Toggle crosshair visibility
	void SetCrosshairVisible(bool bVisible) { bShowCrosshair = bVisible; }
	bool IsCrosshairVisible() const { return bShowCrosshair; }

	// Toggle delete-mode crosshair (green, shown during F7 delete mode)
	void SetDeleteCrosshairVisible(bool bVisible) { bShowDeleteCrosshair = bVisible; }
	bool IsDeleteCrosshairVisible() const { return bShowDeleteCrosshair; }

protected:
	// Whether to show the crosshair
	bool bShowCrosshair;

	// Whether to show the delete-mode crosshair (green)
	bool bShowDeleteCrosshair;

	// Crosshair settings
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float CrosshairSize;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float CrosshairThickness;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor CrosshairColor;

	// Delete crosshair settings
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float DeleteCrosshairSize;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float DeleteCrosshairThickness;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor DeleteCrosshairColor;

	// Optional icon drawn left of the money readout (text-only when unset).
	UPROPERTY(EditAnywhere, Category = "HUD")
	class UTexture2D* MoneyIcon = nullptr;

	// --- Suspicion 5-star heat meter (top-center) ---

	// Star icons; if unset, draws colored-shape fallbacks (filled = flashing, empty = dark outline).
	UPROPERTY(EditAnywhere, Category = "HUD")
	class UTexture2D* StarFilled = nullptr;

	UPROPERTY(EditAnywhere, Category = "HUD")
	class UTexture2D* StarEmpty = nullptr;

	UPROPERTY(EditAnywhere, Category = "HUD")
	float StarSize = 40.0f;

	UPROPERTY(EditAnywhere, Category = "HUD")
	float StarSpacing = 8.0f;

	// Distance from the top of the screen for the star row.
	UPROPERTY(EditAnywhere, Category = "HUD")
	float StarTopMargin = 20.0f;

	// Police-light flash: red/blue toggles per second at 1 star, plus per-extra-star.
	UPROPERTY(EditAnywhere, Category = "HUD")
	float HeatFlashBaseRate = 2.0f;

	UPROPERTY(EditAnywhere, Category = "HUD")
	float HeatFlashRatePerStar = 1.0f;

	// Always-on aiming dot at screen center (hidden while a build/delete crosshair is active).
	UPROPERTY(EditAnywhere, Category = "Crosshair")
	bool bShowCenterDot = true;

	UPROPERTY(EditAnywhere, Category = "Crosshair")
	float DotSize = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Crosshair")
	float DotOpacity = 0.85f;

	// Draw the crosshair
	void DrawCrosshair();

	// Draw the delete-mode crosshair
	void DrawDeleteCrosshair();

	// Draw the persistent "$<Money>" readout (top-right), read from the owning player character.
	void DrawMoney();

	// Draw the small always-on center dot.
	void DrawCenterDot();

	// Draw the 5-star suspicion meter (top-center), with police-light flash on filled stars.
	void DrawHeatMeter();
};
