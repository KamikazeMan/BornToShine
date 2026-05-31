#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryItemTypes.h"
#include "InventorySlotWidget.generated.h"

class UBorder;
class UTextBlock;
class UImage;
class UButton;
class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotClicked, int32, SlotIndex);

UCLASS()
class BORNTOSHINE_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void SetSlotData(FName InItemID, int32 InQuantity, UInventoryComponent* InInventoryRef);
	void ClearSlot();
	void SetSelected(bool bSelected);

	int32 SlotIndex = -1;
	FOnSlotClicked OnSlotClicked;

protected:
	UPROPERTY() UBorder* SelectionBorder;
	UPROPERTY() UImage* ColorRect;
	UPROPERTY() UTextBlock* NameText;
	UPROPERTY() UTextBlock* QuantityText;
	UPROPERTY() UButton* ClickButton;

	FName CurrentItemID;
	bool bIsSelected = false;

	UFUNCTION() void HandleClicked();
	FLinearColor GetCategoryColor(const FString& Category) const;
};
