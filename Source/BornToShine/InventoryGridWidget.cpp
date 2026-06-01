#include "InventoryGridWidget.h"
#include "InventorySlotWidget.h"
#include "InventoryComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UInventoryGridWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));

	UOverlay* MainOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MainOverlay"));
	UCanvasPanelSlot* OverlayCanvasSlot = RootCanvas->AddChildToCanvas(MainOverlay);
	OverlayCanvasSlot->SetAnchors(FAnchors(0.05f, 0.05f, 0.95f, 0.95f));
	OverlayCanvasSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));

	BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundImage"));
	FSlateBrush WhiteBrush;
	WhiteBrush.TintColor = FSlateColor(FLinearColor::White);
	WhiteBrush.DrawAs = ESlateBrushDrawType::Image;
	BackgroundImage->SetBrush(WhiteBrush);
	BackgroundImage->SetColorAndOpacity(FLinearColor(0.08f, 0.06f, 0.04f, 0.95f));
	UOverlaySlot* BgSlot = MainOverlay->AddChildToOverlay(BackgroundImage);
	BgSlot->SetHorizontalAlignment(HAlign_Fill);
	BgSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("SUPPLIES")));
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 48;
	TitleText->SetFont(TitleFont);
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.8f, 0.2f, 1.0f)));
	TitleText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText);
	TitleSlot->SetPadding(FMargin(0.0f, 30.0f, 0.0f, 20.0f));
	TitleSlot->SetHorizontalAlignment(HAlign_Center);

	SlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("SlotGrid"));
	SlotGrid->SetSlotPadding(FMargin(10.0f));
	SlotGrid->SetMinDesiredSlotWidth(140.0f);
	SlotGrid->SetMinDesiredSlotHeight(140.0f);
	UVerticalBoxSlot* GridSlot = VBox->AddChildToVerticalBox(SlotGrid);
	GridSlot->SetPadding(FMargin(16.0f, 4.0f, 16.0f, 4.0f));
	GridSlot->SetHorizontalAlignment(HAlign_Center);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetText(FText::FromString(TEXT("Press I to close")));
	FSlateFontInfo StatusFont = StatusText->GetFont();
	StatusFont.Size = 11;
	StatusText->SetFont(StatusFont);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
	StatusText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* StatusSlot = VBox->AddChildToVerticalBox(StatusText);
	StatusSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 12.0f));
	StatusSlot->SetHorizontalAlignment(HAlign_Center);

	UOverlaySlot* VBoxSlot = MainOverlay->AddChildToOverlay(VBox);
	VBoxSlot->SetHorizontalAlignment(HAlign_Fill);
	VBoxSlot->SetVerticalAlignment(VAlign_Fill);

	int32 TotalSlots = NumColumns * NumRows;
	for (int32 i = 0; i < TotalSlots; i++)
	{
		UInventorySlotWidget* SlotWidget = WidgetTree->ConstructWidget<UInventorySlotWidget>(UInventorySlotWidget::StaticClass(), *FString::Printf(TEXT("Slot_%d"), i));
		SlotWidget->SlotIndex = i;
		SlotWidget->OnSlotClicked.AddDynamic(this, &UInventoryGridWidget::HandleSlotClicked);

		SlotGrid->AddChildToUniformGrid(SlotWidget, i / NumColumns, i % NumColumns);
		SlotWidgets.Add(SlotWidget);
	}

	WidgetTree->RootWidget = RootCanvas;

	return Super::RebuildWidget();
}

void UInventoryGridWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BackgroundTexture && BackgroundImage)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(BackgroundTexture);
		Brush.ImageSize = FVector2D(700.0f, 500.0f);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		BackgroundImage->SetBrush(Brush);
	}

	RefreshGrid();
}

void UInventoryGridWidget::NativeDestruct()
{
	if (InventoryRef)
	{
		InventoryRef->OnInventoryChanged.RemoveDynamic(this, &UInventoryGridWidget::HandleInventoryChanged);
	}
	Super::NativeDestruct();
}

void UInventoryGridWidget::SetInventoryComponent(UInventoryComponent* InInventory)
{
	if (InventoryRef)
	{
		InventoryRef->OnInventoryChanged.RemoveDynamic(this, &UInventoryGridWidget::HandleInventoryChanged);
	}

	InventoryRef = InInventory;

	if (InventoryRef)
	{
		InventoryRef->OnInventoryChanged.AddDynamic(this, &UInventoryGridWidget::HandleInventoryChanged);
	}
}

void UInventoryGridWidget::RefreshGrid()
{
	if (!InventoryRef) return;

	const TArray<FInventoryItem>& Items = InventoryRef->GetItems();
	int32 TotalSlots = SlotWidgets.Num();

	for (int32 i = 0; i < TotalSlots; i++)
	{
		if (i < Items.Num() && Items[i].Quantity > 0)
		{
			SlotWidgets[i]->SetSlotData(Items[i].ItemID, Items[i].Quantity, InventoryRef);
		}
		else
		{
			SlotWidgets[i]->ClearSlot();
		}
	}

	StatusText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d items  |  I to close"), Items.Num(), TotalSlots)));
}

void UInventoryGridWidget::HandleSlotClicked(int32 SlotIndex)
{
	if (SelectedSlotIndex >= 0 && SelectedSlotIndex < SlotWidgets.Num())
	{
		SlotWidgets[SelectedSlotIndex]->SetSelected(false);
	}

	if (SlotIndex >= 0 && SlotIndex < SlotWidgets.Num())
	{
		SlotWidgets[SlotIndex]->SetSelected(true);
		SelectedSlotIndex = SlotIndex;

		if (InventoryRef)
		{
			const TArray<FInventoryItem>& Items = InventoryRef->GetItems();
			if (SlotIndex < Items.Num())
			{
				UE_LOG(LogTemp, Log, TEXT("Inventory: Selected slot %d — %s x%d"), SlotIndex, *Items[SlotIndex].ItemID.ToString(), Items[SlotIndex].Quantity);
			}
		}
	}
}

void UInventoryGridWidget::HandleInventoryChanged()
{
	RefreshGrid();
}
