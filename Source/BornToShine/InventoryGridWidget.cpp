#include "InventoryGridWidget.h"
#include "InventorySlotWidget.h"
#include "InventoryComponent.h"
#include "MoonshineCharacter_Simple.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
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
	SlotGrid->SetSlotPadding(FMargin(SlotPadding)); // visible gap between cells
	UVerticalBoxSlot* GridSlot = VBox->AddChildToVerticalBox(SlotGrid);
	GridSlot->SetPadding(FMargin(16.0f, 4.0f, 16.0f, 4.0f));
	GridSlot->SetHorizontalAlignment(HAlign_Center);
	GridSlot->SetVerticalAlignment(VAlign_Center);

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
		SlotWidget->OnSlotDragCancelled.AddDynamic(this, &UInventoryGridWidget::HandleSlotDragCancelled);

		USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("SlotSizeBox_%d"), i));
		SlotSizeBox->SetWidthOverride(200.0f);
		SlotSizeBox->SetHeightOverride(200.0f);
		SlotSizeBox->AddChild(SlotWidget);

		UUniformGridSlot* GridCellSlot = SlotGrid->AddChildToUniformGrid(SlotSizeBox, i / NumColumns, i % NumColumns);
		GridCellSlot->SetHorizontalAlignment(HAlign_Center);
		GridCellSlot->SetVerticalAlignment(VAlign_Center);
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
		// Always pass the inventory ref (even when empty) so every slot is a valid drop target.
		if (i < Items.Num() && Items[i].Quantity > 0)
		{
			SlotWidgets[i]->SetSlotData(Items[i].ItemID, Items[i].Quantity, InventoryRef);
		}
		else
		{
			SlotWidgets[i]->SetSlotData(NAME_None, 0, InventoryRef);
		}
	}

	StatusText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d items  |  I to close"), Items.Num(), TotalSlots)));
}

void UInventoryGridWidget::HandleSlotClicked(int32 SlotIndex)
{
	if (!InventoryRef) return;

	const TArray<FInventoryItem>& Items = InventoryRef->GetItems();

	// Ignore clicks on empty slots
	if (SlotIndex < 0 || SlotIndex >= Items.Num()) return;

	const FName ItemID = Items[SlotIndex].ItemID;
	if (ItemID == NAME_None) return;

	// Hand off to the player character to enter placement mode (this also closes the inventory).
	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		if (AMoonshineCharacter_Simple* Character = Cast<AMoonshineCharacter_Simple>(Pawn))
		{
			Character->BeginItemPlacement(ItemID);
		}
	}
}

void UInventoryGridWidget::HandleInventoryChanged()
{
	RefreshGrid();
}

bool UInventoryGridWidget::IsScreenInsidePanel(const FVector2D& ScreenPos) const
{
	return BackgroundImage && BackgroundImage->GetCachedGeometry().IsUnderLocation(ScreenPos);
}

void UInventoryGridWidget::HandleSlotDragCancelled(UInventoryComponent* SourceInventory, int32 SourceIndex, FVector2D ScreenPos)
{
	// The character decides cancel-vs-world-drop against ALL open inventory panels (grid + hotbar +
	// still UI), so releasing over another panel cancels rather than dropping to world.
	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		if (AMoonshineCharacter_Simple* Character = Cast<AMoonshineCharacter_Simple>(Pawn))
		{
			Character->HandleInventoryDragRelease(SourceInventory, SourceIndex, ScreenPos);
		}
	}
}
