// Born To Shine - Transfer-amount popup

#include "TransferAmountWidget.h"
#include "MoonshineCharacter_Simple.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"

namespace
{
	const FLinearColor TaPanelDark(0.08f, 0.06f, 0.04f, 0.97f);
	const FLinearColor TaTextCream(1.0f, 0.95f, 0.8f, 1.0f);

	FSlateBrush TaMakeRounded(const FLinearColor& Color, float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		return Brush;
	}

	UButton* TaMakeButton(UWidgetTree* Tree, const FString& Label, const FLinearColor& Color)
	{
		UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle S = B->GetStyle();
		S.Normal = TaMakeRounded(Color, 5.0f);
		S.Hovered = TaMakeRounded(Color * 1.25f, 5.0f);
		S.Pressed = TaMakeRounded(Color * 0.8f, 5.0f);
		B->SetStyle(S);
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Label));
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		T->SetJustification(ETextJustify::Center);
		B->AddChild(T);
		return B;
	}
}

TSharedRef<SWidget> UTransferAmountWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));

	// Full-screen backdrop absorbs clicks behind the popup while it's up.
	UButton* Backdrop = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Backdrop"));
	{
		FButtonStyle BS = Backdrop->GetStyle();
		FSlateBrush Dim;
		Dim.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
		Dim.DrawAs = ESlateBrushDrawType::Image;
		BS.Normal = Dim; BS.Hovered = Dim; BS.Pressed = Dim;
		Backdrop->SetStyle(BS);
	}
	UCanvasPanelSlot* BackSlot = RootCanvas->AddChildToCanvas(Backdrop);
	BackSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackSlot->SetOffsets(FMargin(0.0f));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrush(TaMakeRounded(TaPanelDark, 8.0f));
	Panel->SetPadding(FMargin(22.0f, 18.0f));
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(VBox);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("Transfer how many?")));
	Title->SetColorAndOpacity(FSlateColor(TaTextCream));
	Title->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TS = VBox->AddChildToVerticalBox(Title);
	TS->SetHorizontalAlignment(HAlign_Center);
	TS->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	AmountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = AmountText->GetFont();
	Font.Size = 22;
	AmountText->SetFont(Font);
	AmountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.3f, 1.0f)));
	AmountText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* AS = VBox->AddChildToVerticalBox(AmountText);
	AS->SetHorizontalAlignment(HAlign_Center);
	AS->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	AmountSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	AmountSlider->SetMinValue(1.0f);
	AmountSlider->SetMaxValue(1.0f);
	AmountSlider->SetStepSize(1.0f);
	AmountSlider->SetValue(1.0f);
	AmountSlider->OnValueChanged.AddDynamic(this, &UTransferAmountWidget::OnSliderChanged);
	UVerticalBoxSlot* SS = VBox->AddChildToVerticalBox(AmountSlider);
	SS->SetHorizontalAlignment(HAlign_Fill);
	SS->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UButton* ConfirmBtn = TaMakeButton(WidgetTree, TEXT("Confirm"), FLinearColor(0.15f, 0.55f, 0.15f, 0.95f));
	ConfirmBtn->OnClicked.AddDynamic(this, &UTransferAmountWidget::OnConfirm);
	UHorizontalBoxSlot* CB = Buttons->AddChildToHorizontalBox(ConfirmBtn);
	CB->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

	UButton* CancelBtn = TaMakeButton(WidgetTree, TEXT("Cancel"), FLinearColor(0.45f, 0.18f, 0.15f, 0.95f));
	CancelBtn->OnClicked.AddDynamic(this, &UTransferAmountWidget::OnCancel);
	Buttons->AddChildToHorizontalBox(CancelBtn);

	UVerticalBoxSlot* BtnSlot = VBox->AddChildToVerticalBox(Buttons);
	BtnSlot->SetHorizontalAlignment(HAlign_Center);

	WidgetTree->RootWidget = RootCanvas;
	return Super::RebuildWidget();
}

void UTransferAmountWidget::Setup(AMoonshineCharacter_Simple* InOwner, int32 InMax)
{
	Owner = InOwner;
	MaxAmount = FMath::Max(1, InMax);
	if (AmountSlider)
	{
		AmountSlider->SetMinValue(1.0f);
		AmountSlider->SetMaxValue((float)MaxAmount);
		AmountSlider->SetValue((float)MaxAmount); // default to the whole stack
	}
	UpdateAmountText();
}

int32 UTransferAmountWidget::CurrentAmount() const
{
	if (!AmountSlider) return MaxAmount;
	return FMath::Clamp(FMath::RoundToInt(AmountSlider->GetValue()), 1, MaxAmount);
}

void UTransferAmountWidget::UpdateAmountText()
{
	if (AmountText)
	{
		AmountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentAmount(), MaxAmount)));
	}
}

void UTransferAmountWidget::OnSliderChanged(float Value)
{
	UpdateAmountText();
}

void UTransferAmountWidget::OnConfirm()
{
	if (Owner.IsValid())
	{
		Owner->ConfirmTransferAmount(CurrentAmount());
	}
}

void UTransferAmountWidget::OnCancel()
{
	if (Owner.IsValid())
	{
		Owner->CancelTransferAmount();
	}
}
