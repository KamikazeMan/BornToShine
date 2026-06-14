// Born To Shine - Per-still loading UI: store ingredients in a still, then Start Distilling.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StillInventoryWidget.generated.h"

class UTextBlock;
class UButton;
class AMoonshineCharacter_Simple;
class AStillPartActor;

/**
 * Code-built loading UI (same style as the inventory widgets). Opened by E on a complete still's
 * pot; operates on THAT pot's OwningStand. Click to transfer ingredients between the player and
 * the still's own stash in both directions, then Start Distilling to consume the required amounts
 * and begin that stand's batch.
 */
UCLASS()
class BORNTOSHINE_API UStillInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Point the widget at a specific still and refresh the display.
	void SetupForStand(AMoonshineCharacter_Simple* InOwner, AStillPartActor* InStand);

	// Re-read counts and recolor (called after every transfer).
	void Refresh();

protected:
	UPROPERTY() UTextBlock* TitleText = nullptr;
	UPROPERTY() UTextBlock* WaterRowText = nullptr;
	UPROPERTY() UTextBlock* MashRowText = nullptr;
	UPROPERTY() UTextBlock* FirewoodRowText = nullptr;
	UPROPERTY() UTextBlock* StatusText = nullptr;

	UPROPERTY() UButton* StartButton = nullptr;

	TWeakObjectPtr<AMoonshineCharacter_Simple> Owner;
	TWeakObjectPtr<AStillPartActor> Stand;

	bool bShowShortWarning = false;

	// Transfer click handlers (player -> still, and still -> player) per ingredient.
	UFUNCTION() void OnAddWater();
	UFUNCTION() void OnTakeWater();
	UFUNCTION() void OnAddMash();
	UFUNCTION() void OnTakeMash();
	UFUNCTION() void OnAddFirewood();
	UFUNCTION() void OnTakeFirewood();

	UFUNCTION() void OnStartClicked();
	UFUNCTION() void OnCloseClicked();

	// Creates a small styled button with a centered text label.
	UButton* MakeButton(const FString& Label, const FString& WidgetTag);

	// Creates a styled count TextBlock for an ingredient row.
	UTextBlock* MakeRowText(const FString& WidgetTag);

	// Updates one row's "<name>: stored / req   (you: N)" text and red/green color.
	void RefreshRow(UTextBlock* RowText, FName Ingredient, const FString& DisplayName);
};
