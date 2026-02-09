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

protected:
	// Whether to show the crosshair
	bool bShowCrosshair;

	// Crosshair settings
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float CrosshairSize;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float CrosshairThickness;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor CrosshairColor;

	// Draw the crosshair
	void DrawCrosshair();
};
