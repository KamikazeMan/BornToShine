// Born To Shine - Player hotbar

#include "HotbarWidget.h"
#include "InventorySlotWidget.h"
#include "InventoryComponent.h"
#include "MoonshineCharacter_Simple.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

TSharedRef<SWidget> UHotbarWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));

	// Bottom-center dark panel holding the slot row.
	PanelBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HotbarPanel"));
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.08f, 0.06f, 0.04f, 0.85f));
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(8.0f, 8.0f, 8.0f, 8.0f);
		PanelBackground->SetBrush(Brush);
	}
	PanelBackground->SetPadding(FMargin(6.0f));
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelBackground);
	PanelSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	PanelSlot->SetAutoSize(true);
	PanelSlot->SetPosition(FVector2D(0.0f, -24.0f)); // sit a little above the screen bottom

	Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HotbarRow"));
	PanelBackground->SetContent(Row);

	const int32 Count = FMath::Max(0, NumSlots);
	for (int32 i = 0; i < Count; ++i)
	{
		UInventorySlotWidget* SlotWidget = WidgetTree->ConstructWidget<UInventorySlotWidget>(
			UInventorySlotWidget::StaticClass(), *FString::Printf(TEXT("HotbarSlot_%d"), i));
		SlotWidget->SlotIndex = i; // hotbar slot i == inventory slot i
		SlotWidget->OnSlotClicked.AddDynamic(this, &UHotbarWidget::HandleSlotClicked);
		SlotWidget->OnSlotDragCancelled.AddDynamic(this, &UHotbarWidget::HandleSlotDragCancelled);

		USizeBox* SlotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("HotbarSlotSize_%d"), i));
		SlotSize->SetWidthOverride(90.0f);
		SlotSize->SetHeightOverride(90.0f);
		SlotSize->AddChild(SlotWidget);

		UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(SlotSize);
		HSlot->SetPadding(FMargin(SlotPadding * 0.5f, 0.0f)); // half each side => SlotPadding gap between cells
		HSlot->SetVerticalAlignment(VAlign_Center);

		SlotWidgets.Add(SlotWidget);
	}

	WidgetTree->RootWidget = RootCanvas;
	return Super::RebuildWidget();
}

void UHotbarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Visible but not blocking gameplay clicks; individual slots are hit-testable for drag/drop.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshSlots();
	SetActiveSlot(ActiveSlot);
}

void UHotbarWidget::NativeDestruct()
{
	if (InventoryRef)
	{
		InventoryRef->OnInventoryChanged.RemoveDynamic(this, &UHotbarWidget::HandleInventoryChanged);
	}
	Super::NativeDestruct();
}

void UHotbarWidget::SetInventoryComponent(UInventoryComponent* InInventory)
{
	if (InventoryRef)
	{
		InventoryRef->OnInventoryChanged.RemoveDynamic(this, &UHotbarWidget::HandleInventoryChanged);
	}
	InventoryRef = InInventory;
	if (InventoryRef)
	{
		InventoryRef->OnInventoryChanged.AddDynamic(this, &UHotbarWidget::HandleInventoryChanged);
	}
	RefreshSlots();
}

void UHotbarWidget::RefreshSlots()
{
	if (!InventoryRef) return;

	const TArray<FInventoryItem>& Items = InventoryRef->GetItems();
	for (int32 i = 0; i < SlotWidgets.Num(); ++i)
	{
		// Always pass the inventory ref (even when empty) so every slot is a valid drop target.
		if (Items.IsValidIndex(i) && Items[i].Quantity > 0)
		{
			SlotWidgets[i]->SetSlotData(Items[i].ItemID, Items[i].Quantity, InventoryRef);
		}
		else
		{
			SlotWidgets[i]->SetSlotData(NAME_None, 0, InventoryRef);
		}
	}
	SetActiveSlot(ActiveSlot); // re-apply highlight after a refresh
}

void UHotbarWidget::SetActiveSlot(int32 Index)
{
	ActiveSlot = Index;
	for (int32 i = 0; i < SlotWidgets.Num(); ++i)
	{
		SlotWidgets[i]->SetSelected(i == Index);
	}
}

bool UHotbarWidget::IsScreenInsidePanel(const FVector2D& ScreenPos) const
{
	return PanelBackground && PanelBackground->GetCachedGeometry().IsUnderLocation(ScreenPos);
}

void UHotbarWidget::HandleSlotClicked(int32 SlotIndex)
{
	// Clicking a hotbar slot selects it (does not enter placement like the main grid).
	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		if (AMoonshineCharacter_Simple* Character = Cast<AMoonshineCharacter_Simple>(Pawn))
		{
			Character->SelectHotbarSlot(SlotIndex);
		}
	}
}

void UHotbarWidget::HandleSlotDragCancelled(UInventoryComponent* SourceInventory, int32 SourceIndex, FVector2D ScreenPos)
{
	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		if (AMoonshineCharacter_Simple* Character = Cast<AMoonshineCharacter_Simple>(Pawn))
		{
			Character->HandleInventoryDragRelease(SourceInventory, SourceIndex, ScreenPos);
		}
	}
}

void UHotbarWidget::HandleInventoryChanged()
{
	RefreshSlots();
}
