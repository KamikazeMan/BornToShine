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

	// Draw the crosshair
	void DrawCrosshair();

	// Draw the delete-mode crosshair
	void DrawDeleteCrosshair();

	// Draw the persistent "$<Money>" readout (top-right), read from the owning player character.
	void DrawMoney();
};
