#include "InventorySlotWidget.h"
#include "InventoryComponent.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

TSharedRef<SWidget> UInventorySlotWidget::RebuildWidget()
{
	SelectionBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SelectionBorder"));
	SelectionBorder->SetBrushColor(FLinearColor(0.1f, 0.07f, 0.04f, 0.15f));
	SelectionBorder->SetPadding(FMargin(3.0f));

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SlotOverlay"));

	ColorRect = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ColorRect"));
	FSlateBrush WhiteBrush;
	WhiteBrush.TintColor = FSlateColor(FLinearColor::White);
	WhiteBrush.DrawAs = ESlateBrushDrawType::Image;
	ColorRect->SetBrush(WhiteBrush);
	ColorRect->SetColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f, 0.1f));
	UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(ColorRect);
	IconSlot->SetHorizontalAlignment(HAlign_Fill);
	IconSlot->SetVerticalAlignment(VAlign_Fill);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetText(FText::FromString(TEXT("")));
	NameText->SetJustification(ETextJustify::Center);
	FSlateFontInfo NameFont = NameText->GetFont();
	NameFont.Size = 14;
	NameText->SetFont(NameFont);
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.8f, 1.0f)));
	UOverlaySlot* NameSlot = Overlay->AddChildToOverlay(NameText);
	NameSlot->SetHorizontalAlignment(HAlign_Center);
	NameSlot->SetVerticalAlignment(VAlign_Bottom);
	NameSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 4.0f));

	QuantityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuantityText"));
	QuantityText->SetText(FText::FromString(TEXT("")));
	FSlateFontInfo QtyFont = QuantityText->GetFont();
	QtyFont.Size = 18;
	QtyFont.OutlineSettings.OutlineSize = 1;
	QuantityText->SetFont(QtyFont);
	QuantityText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UOverlaySlot* QtySlot = Overlay->AddChildToOverlay(QuantityText);
	QtySlot->SetHorizontalAlignment(HAlign_Right);
	QtySlot->SetVerticalAlignment(VAlign_Top);
	QtySlot->SetPadding(FMargin(0.0f, 4.0f, 6.0f, 0.0f));

	ClickButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ClickButton"));
	FButtonStyle TransparentStyle = ClickButton->GetStyle();
	TransparentStyle.Normal.TintColor = FSlateColor(FLinearColor(1, 1, 1, 0));
	TransparentStyle.Hovered.TintColor = FSlateColor(FLinearColor(1, 0.9f, 0.5f, 0.15f));
	TransparentStyle.Pressed.TintColor = FSlateColor(FLinearColor(1, 0.8f, 0.3f, 0.3f));
	ClickButton->SetStyle(TransparentStyle);
	ClickButton->OnClicked.AddDynamic(this, &UInventorySlotWidget::HandleClicked);
	UOverlaySlot* BtnSlot = Overlay->AddChildToOverlay(ClickButton);
	BtnSlot->SetHorizontalAlignment(HAlign_Fill);
	BtnSlot->SetVerticalAlignment(VAlign_Fill);

	SelectionBorder->SetContent(Overlay);

	WidgetTree->RootWidget = SelectionBorder;

	return Super::RebuildWidget();
}

void UInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetSelected(false);
	ClearSlot();
}

FLinearColor UInventorySlotWidget::GetCategoryColor(const FString& Category) const
{
	if (Category == TEXT("StillPart")) return FLinearColor(0.7f, 0.4f, 0.1f, 0.4f);
	if (Category == TEXT("Ingredient")) return FLinearColor(0.4f, 0.7f, 0.2f, 0.4f);
	if (Category == TEXT("Tool")) return FLinearColor(0.3f, 0.3f, 0.4f, 0.4f);
	if (Category == TEXT("Jar")) return FLinearColor(0.9f, 0.85f, 0.6f, 0.4f);
	return FLinearColor(0.4f, 0.4f, 0.4f, 0.4f);
}

void UInventorySlotWidget::SetSlotData(FName InItemID, int32 InQuantity, UInventoryComponent* InInventoryRef)
{
	CurrentItemID = InItemID;

	if (InItemID == NAME_None || InQuantity <= 0 || !InInventoryRef)
	{
		ClearSlot();
		return;
	}

	FItemDataRow* Data = InInventoryRef->GetItemDataRaw(InItemID);
	if (Data)
	{
		ColorRect->SetColorAndOpacity(GetCategoryColor(Data->Category));
		NameText->SetText(Data->DisplayName);

		FString TooltipStr = FString::Printf(TEXT("%s\nCategory: %s\nID: %s"),
			*Data->DisplayName.ToString(), *Data->Category, *InItemID.ToString());
		SetToolTipText(FText::FromString(TooltipStr));
	}
	else
	{
		ColorRect->SetColorAndOpacity(GetCategoryColor(TEXT("General")));
		NameText->SetText(FText::FromName(InItemID));
		SetToolTipText(FText::FromName(InItemID));
	}

	if (InQuantity > 1)
	{
		QuantityText->SetText(FText::FromString(FString::Printf(TEXT("x%d"), InQuantity)));
	}
	else
	{
		QuantityText->SetText(FText::FromString(TEXT("")));
	}
}

void UInventorySlotWidget::ClearSlot()
{
	CurrentItemID = NAME_None;
	ColorRect->SetColorAndOpacity(FLinearColor(0.15f, 0.12f, 0.09f, 0.1f));
	NameText->SetText(FText::FromString(TEXT("")));
	QuantityText->SetText(FText::FromString(TEXT("")));
	SetToolTipText(FText::FromString(TEXT("")));
}

void UInventorySlotWidget::SetSelected(bool bSelected)
{
	bIsSelected = bSelected;
	if (bSelected)
	{
		SelectionBorder->SetBrushColor(FLinearColor(0.95f, 0.75f, 0.1f, 0.9f));
	}
	else
	{
		SelectionBorder->SetBrushColor(FLinearColor(0.1f, 0.07f, 0.04f, 0.15f));
	}
}

void UInventorySlotWidget::HandleClicked()
{
	OnSlotClicked.Broadcast(SlotIndex);
}
