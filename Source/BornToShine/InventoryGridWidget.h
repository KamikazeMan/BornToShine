#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryGridWidget.generated.h"

class UInventoryComponent;
class UInventorySlotWidget;
class UUniformGridPanel;
class UTextBlock;
class UImage;

UCLASS()
class BORNTOSHINE_API UInventoryGridWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void SetInventoryComponent(UInventoryComponent* InInventory);
	void RefreshGrid();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	UTexture2D* BackgroundTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 NumColumns = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 NumRows = 4;

protected:
	UPROPERTY() UUniformGridPanel* SlotGrid;
	UPROPERTY() UTextBlock* TitleText;
	UPROPERTY() UTextBlock* StatusText;
	UPROPERTY() UImage* BackgroundImage;

	UPROPERTY() TArray<UInventorySlotWidget*> SlotWidgets;
	UPROPERTY() UInventoryComponent* InventoryRef;

	int32 SelectedSlotIndex = -1;

	UFUNCTION() void HandleSlotClicked(int32 SlotIndex);
	UFUNCTION() void HandleInventoryChanged();
};
