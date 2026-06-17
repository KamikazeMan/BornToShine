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

	// --- Suspicion 5-star heat meter (GTA-style wanted level) ---

	// Star icons; if unset, draws star-shaped polygon fallbacks (earned = solid, empty = dim outline).
	UPROPERTY(EditAnywhere, Category = "HUD")
	class UTexture2D* StarFilled = nullptr;

	UPROPERTY(EditAnywhere, Category = "HUD")
	class UTexture2D* StarEmpty = nullptr;

	UPROPERTY(EditAnywhere, Category = "HUD")
	float StarSize = 40.0f;

	UPROPERTY(EditAnywhere, Category = "HUD")
	float StarSpacing = 8.0f;

	// Screen-space position for the heat meter (top-left corner of the star row).
	// X < 0 means "center horizontally"; Y is distance from top of screen.
	UPROPERTY(EditAnywhere, Category = "HUD")
	FVector2D HeatMeterScreenPos = FVector2D(-1.0f, 12.0f);

	// Earned-star color (single consistent color — warm white/gold like GTA, no red/blue strobe).
	UPROPERTY(EditAnywhere, Category = "HUD")
	FLinearColor StarColor = FLinearColor(1.0f, 0.9f, 0.6f, 1.0f);

	// Soft "you're wanted" brightness pulse on earned stars (sine). Speed in radians/sec-ish.
	UPROPERTY(EditAnywhere, Category = "HUD")
	float StarPulseSpeed = 2.0f;

	// How deep the pulse dims (0 = none, 0.3 = brightness oscillates ~0.7..1.0).
	UPROPERTY(EditAnywhere, Category = "HUD")
	float StarPulseDepth = 0.3f;

	// When heat is cooling down, flash the about-to-be-lost (top earned) star toward gray.
	UPROPERTY(EditAnywhere, Category = "HUD")
	bool bShowCoolingFlash = true;

	// Cooling-flash speed (faster than the idle pulse so it reads as "about to drop").
	UPROPERTY(EditAnywhere, Category = "HUD")
	float StarCoolFlashSpeed = 6.0f;

	// Brief brighten on a freshly-gained star before it settles (seconds).
	UPROPERTY(EditAnywhere, Category = "HUD")
	float NewStarFlashDuration = 0.4f;

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

	// Draw the GTA-style 5-star suspicion meter (solid earned stars, subtle pulse, cooling flash).
	void DrawHeatMeter();

	// Draw a 5-pointed star polygon at the given center via Canvas lines.
	void DrawStarPolygon(float CenterX, float CenterY, float OuterR, float InnerR, const FLinearColor& Color, bool bFilled);

	// --- Heat-trend tracking (HUD-local; does not touch accrual logic) ---
	// Compares heat frame-to-frame to tell "rising" from "cooling down" for the cooling flash.
	float PrevHeat = -1.0f;
	bool bHeatCoolingDown = false;
	int32 PrevStars = 0;
	float NewStarFlashUntil = 0.0f;
};
