// Born To Shine - Small popup asking how many of a stack to transfer (slider + Confirm/Cancel).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TransferAmountWidget.generated.h"

class USlider;
class UTextBlock;
class UButton;
class AMoonshineCharacter_Simple;

UCLASS()
class BORNTOSHINE_API UTransferAmountWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Configure for a pending transfer of up to InMax units; defaults the slider to the full amount.
	void Setup(AMoonshineCharacter_Simple* InOwner, int32 InMax);

protected:
	UPROPERTY() USlider* AmountSlider = nullptr;
	UPROPERTY() UTextBlock* AmountText = nullptr;

	TWeakObjectPtr<AMoonshineCharacter_Simple> Owner;
	int32 MaxAmount = 1;

	int32 CurrentAmount() const;
	void UpdateAmountText();

	UFUNCTION() void OnSliderChanged(float Value);
	UFUNCTION() void OnConfirm();
	UFUNCTION() void OnCancel();
};
