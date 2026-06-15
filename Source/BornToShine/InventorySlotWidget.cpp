#include "InventorySlotWidget.h"
#include "InventoryComponent.h"
#include "MoonshineCharacter_Simple.h"
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
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UInventorySlotWidget::RebuildWidget()
{
	SelectionBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SelectionBorder"));
	SelectionBorder->SetPadding(FMargin(4.0f));
	ApplyCellBrush(false); // boxed cell: subtle rounded background + thin outline

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

	// Item NAME is intentionally NOT shown in-cell (it overflowed into neighbours). The name is
	// available on hover via the tooltip set in SetSlotData.

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

	// No click button: the slot handles mouse + drag directly (a button would swallow the mouse-down
	// and prevent drag detection). Clicks fall through to NativeOnMouseButtonUp.
	SelectionBorder->SetContent(Overlay);

	WidgetTree->RootWidget = SelectionBorder;

	return Super::RebuildWidget();
}

void UInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Hit-testable so the slot receives mouse-down (drag detection), clicks, and drops.
	SetVisibility(ESlateVisibility::Visible);
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
	CurrentQuantity = InQuantity;
	SlotInventory = InInventoryRef;
	SetRenderOpacity(1.0f); // a refresh always clears any leftover drag-dim

	if (InItemID == NAME_None || InQuantity <= 0 || !InInventoryRef)
	{
		ClearSlot();
		return;
	}

	FItemDataRow* Data = InInventoryRef->GetItemDataRaw(InItemID);
	if (!Data)
	{
		// Fallback: resolve via the owning player's inventory data table, so containers that don't
		// carry their own DT (e.g. the still storage / hotbar) still draw item icons.
		if (APawn* P = GetOwningPlayerPawn())
		{
			if (AMoonshineCharacter_Simple* C = Cast<AMoonshineCharacter_Simple>(P))
			{
				if (UInventoryComponent* PlayerInv = C->GetInventoryComponent())
				{
					Data = PlayerInv->GetItemDataRaw(InItemID);
				}
			}
		}
	}
	if (Data)
	{
		if (Data->Icon)
		{
			ColorRect->SetBrushFromTexture(Data->Icon);
			ColorRect->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			FSlateBrush WhiteBrush;
			WhiteBrush.TintColor = FSlateColor(FLinearColor::White);
			WhiteBrush.DrawAs = ESlateBrushDrawType::Image;
			ColorRect->SetBrush(WhiteBrush);
			ColorRect->SetColorAndOpacity(GetCategoryColor(Data->Category));
		}

		FString TooltipStr = FString::Printf(TEXT("%s\nCategory: %s\nID: %s"),
			*Data->DisplayName.ToString(), *Data->Category, *InItemID.ToString());
		SetToolTipText(FText::FromString(TooltipStr));
	}
	else
	{
		ColorRect->SetColorAndOpacity(GetCategoryColor(TEXT("General")));
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
	CurrentQuantity = 0;
	SetRenderOpacity(1.0f);
	FSlateBrush EmptyBrush;
	EmptyBrush.TintColor = FSlateColor(FLinearColor::White);
	EmptyBrush.DrawAs = ESlateBrushDrawType::Image;
	ColorRect->SetBrush(EmptyBrush);
	ColorRect->SetColorAndOpacity(FLinearColor(0.15f, 0.12f, 0.09f, 0.1f));
	QuantityText->SetText(FText::FromString(TEXT("")));
	SetToolTipText(FText::FromString(TEXT("")));
}

void UInventorySlotWidget::SetSelected(bool bSelected)
{
	bIsSelected = bSelected;
	ApplyCellBrush(bSelected);
}

void UInventorySlotWidget::ApplyCellBrush(bool bSelected)
{
	if (!SelectionBorder) return;

	// Boxed cell: subtle rounded fill + a thin outline (gold when this is the active hotbar slot).
	FSlateBrush Cell;
	Cell.DrawAs = ESlateBrushDrawType::RoundedBox;
	Cell.TintColor = FSlateColor(FLinearColor(0.12f, 0.09f, 0.06f, 0.55f));
	Cell.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Cell.OutlineSettings.CornerRadii = FVector4(6.0f, 6.0f, 6.0f, 6.0f);
	Cell.OutlineSettings.Width = bSelected ? 2.5f : 1.0f;
	Cell.OutlineSettings.Color = bSelected
		? FLinearColor(1.0f, 0.85f, 0.3f, 0.95f)
		: FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);
	SelectionBorder->SetBrush(Cell);
}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event)
{
	// Press on a filled slot arms drag detection; a release without movement is treated as a click.
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && CurrentItemID != NAME_None)
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}
	return Super::NativeOnMouseButtonDown(Geo, Event);
}

FReply UInventorySlotWidget::NativeOnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Event)
{
	// Reached only when no drag was detected => a plain click (e.g. select a still part to place).
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && CurrentItemID != NAME_None)
	{
		OnSlotClicked.Broadcast(SlotIndex);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(Geo, Event);
}

void UInventorySlotWidget::NativeOnDragDetected(const FGeometry& Geo, const FPointerEvent& Event, UDragDropOperation*& OutOperation)
{
	if (CurrentItemID == NAME_None || CurrentQuantity <= 0) return;

	UInventoryDragDropOperation* Op = NewObject<UInventoryDragDropOperation>(GetTransientPackage());
	Op->ItemID = CurrentItemID;
	Op->Count = CurrentQuantity;
	Op->SourceIndex = SlotIndex;
	Op->SourceInventory = SlotInventory;
	Op->Pivot = EDragPivot::MouseDown;

	// Drag visual: the item's icon following the cursor.
	if (SlotInventory)
	{
		if (const FItemDataRow* Data = SlotInventory->GetItemDataRaw(CurrentItemID))
		{
			if (Data->Icon)
			{
				UImage* DragImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				DragImage->SetBrushFromTexture(Data->Icon);
				DragImage->SetDesiredSizeOverride(FVector2D(64.0f, 64.0f));
				Op->DefaultDragVisual = DragImage;
			}
		}
	}

	SetRenderOpacity(0.4f); // dim the source while dragging
	OutOperation = Op;
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& Geo, const FDragDropEvent& Event, UDragDropOperation* InOperation)
{
	UInventoryDragDropOperation* Op = Cast<UInventoryDragDropOperation>(InOperation);
	if (Op && SlotInventory)
	{
		// Route to the owner: it picks whole-stack move vs the transfer-amount slider. Shift bypasses.
		OnSlotDrop.Broadcast(Op->SourceInventory.Get(), Op->SourceIndex, SlotInventory, SlotIndex, Op->Count, Event.IsShiftDown());
		return true;
	}
	return Super::NativeOnDrop(Geo, Event, InOperation);
}

void UInventorySlotWidget::NativeOnDragCancelled(const FDragDropEvent& Event, UDragDropOperation* InOperation)
{
	SetRenderOpacity(1.0f); // un-dim (covers inside-panel cancels that don't trigger a refresh)
	if (UInventoryDragDropOperation* Op = Cast<UInventoryDragDropOperation>(InOperation))
	{
		OnSlotDragCancelled.Broadcast(Op->SourceInventory.Get(), Op->SourceIndex, Event.GetScreenSpacePosition());
	}
	Super::NativeOnDragCancelled(Event, InOperation);
}
