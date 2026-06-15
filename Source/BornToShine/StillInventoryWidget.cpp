// Born To Shine - Per-still loading UI (single container view)

#include "StillInventoryWidget.h"
#include "InventorySlotWidget.h"
#include "InventoryComponent.h"
#include "StillPartActor.h"
#include "MoonshineCharacter_Simple.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

namespace
{
	const FLinearColor SiPanelDark(0.08f, 0.06f, 0.04f, 0.95f);
	const FLinearColor SiTextCream(1.0f, 0.95f, 0.8f, 1.0f);
	const FLinearColor SiTextGrey(0.7f, 0.7f, 0.7f, 1.0f);
	const FLinearColor SiMetGreen(0.45f, 0.9f, 0.35f, 1.0f);
	const FLinearColor SiShortRed(1.0f, 0.4f, 0.35f, 1.0f);

	FSlateBrush SiMakeRounded(const FLinearColor& Color, float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		return Brush;
	}

	UTextBlock* SiMakeLabel(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Text));
		FSlateFontInfo Font = T->GetFont();
		Font.Size = Size;
		T->SetFont(Font);
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetJustification(ETextJustify::Center);
		return T;
	}
}

TSharedRef<SWidget> UStillInventoryWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));

	Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrush(SiMakeRounded(SiPanelDark, 10.0f));
	Panel->SetPadding(FMargin(26.0f, 20.0f));
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(VBox);

	UTextBlock* Sub = SiMakeLabel(WidgetTree, TEXT("Drag ingredients from the hotbar  |  E to close"), 11, SiTextGrey);
	UVerticalBoxSlot* SubS = VBox->AddChildToVerticalBox(Sub);
	SubS->SetHorizontalAlignment(HAlign_Center);
	SubS->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	// The still's storage grid (no header label). Clear first: the widget is reused across opens
	// and RebuildWidget can re-run, which would otherwise leave stale slot entries that desync the
	// item->slot mapping in Refresh (visible slots end up blank).
	StorageSlotWidgets.Empty();

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(SlotPadding)); // visible gap between cells
	const int32 Cols = FMath::Max(1, StorageColumns);
	const int32 Count = FMath::Max(0, StorageSlots);
	for (int32 i = 0; i < Count; ++i)
	{
		UInventorySlotWidget* SlotWidget = WidgetTree->ConstructWidget<UInventorySlotWidget>(UInventorySlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		SlotWidget->OnSlotClicked.AddDynamic(this, &UStillInventoryWidget::HandleStorageSlotClicked);
		SlotWidget->OnSlotDragCancelled.AddDynamic(this, &UStillInventoryWidget::HandleSlotDragCancelled);
		SlotWidget->OnSlotDrop.AddDynamic(this, &UStillInventoryWidget::HandleSlotDrop);

		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SizeBox->SetWidthOverride(90.0f);
		SizeBox->SetHeightOverride(90.0f);
		SizeBox->AddChild(SlotWidget);

		UUniformGridSlot* Cell = Grid->AddChildToUniformGrid(SizeBox, i / Cols, i % Cols);
		Cell->SetHorizontalAlignment(HAlign_Fill);
		Cell->SetVerticalAlignment(VAlign_Fill);

		StorageSlotWidgets.Add(SlotWidget);
	}
	UVerticalBoxSlot* GridS = VBox->AddChildToVerticalBox(Grid);
	GridS->SetHorizontalAlignment(HAlign_Center);

	RequirementText = SiMakeLabel(WidgetTree, TEXT(""), 16, SiTextCream);
	UVerticalBoxSlot* ReqS = VBox->AddChildToVerticalBox(RequirementText);
	ReqS->SetHorizontalAlignment(HAlign_Center);
	ReqS->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 2.0f));

	StatusText = SiMakeLabel(WidgetTree, TEXT(""), 14, SiShortRed);
	UVerticalBoxSlot* StS = VBox->AddChildToVerticalBox(StatusText);
	StS->SetHorizontalAlignment(HAlign_Center);
	StS->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 6.0f));

	StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartButton"));
	{
		FButtonStyle S = StartButton->GetStyle();
		S.Normal = SiMakeRounded(FLinearColor(0.15f, 0.55f, 0.15f, 0.95f), 6.0f);
		S.Hovered = SiMakeRounded(FLinearColor(0.20f, 0.70f, 0.20f, 0.95f), 6.0f);
		S.Pressed = SiMakeRounded(FLinearColor(0.12f, 0.45f, 0.12f, 0.95f), 6.0f);
		StartButton->SetStyle(S);
	}
	StartButton->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnStartClicked);
	StartButton->AddChild(SiMakeLabel(WidgetTree, TEXT("Start Distilling"), 20, FLinearColor::White));
	UVerticalBoxSlot* StartS = VBox->AddChildToVerticalBox(StartButton);
	StartS->SetHorizontalAlignment(HAlign_Fill);
	StartS->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 4.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	{
		FButtonStyle S = CloseButton->GetStyle();
		S.Normal = SiMakeRounded(FLinearColor(0.20f, 0.15f, 0.09f, 0.95f), 4.0f);
		S.Hovered = SiMakeRounded(FLinearColor(0.32f, 0.24f, 0.12f, 0.95f), 4.0f);
		S.Pressed = SiMakeRounded(FLinearColor(0.40f, 0.30f, 0.15f, 0.95f), 4.0f);
		CloseButton->SetStyle(S);
	}
	CloseButton->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnCloseClicked);
	CloseButton->AddChild(SiMakeLabel(WidgetTree, TEXT("Close"), 14, SiTextCream));
	UVerticalBoxSlot* CloseS = VBox->AddChildToVerticalBox(CloseButton);
	CloseS->SetHorizontalAlignment(HAlign_Center);
	CloseS->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	WidgetTree->RootWidget = RootCanvas;
	return Super::RebuildWidget();
}

void UStillInventoryWidget::NativeDestruct()
{
	if (StorageInv) StorageInv->OnInventoryChanged.RemoveDynamic(this, &UStillInventoryWidget::HandleInventoryChanged);
	Super::NativeDestruct();
}

void UStillInventoryWidget::SetupForStand(AMoonshineCharacter_Simple* InOwner, AStillPartActor* InStand)
{
	if (StorageInv) StorageInv->OnInventoryChanged.RemoveDynamic(this, &UStillInventoryWidget::HandleInventoryChanged);

	Owner = InOwner;
	Stand = InStand;
	PlayerInv = InOwner ? InOwner->GetInventoryComponent() : nullptr;
	StorageInv = InStand ? InStand->StillStorage : nullptr;
	bShowShortWarning = false;

	// Make sure the still container can resolve item icons (share the player's item data table).
	if (StorageInv && PlayerInv && !StorageInv->ItemDataTable)
	{
		StorageInv->ItemDataTable = PlayerInv->ItemDataTable;
	}

	if (StorageInv) StorageInv->OnInventoryChanged.AddDynamic(this, &UStillInventoryWidget::HandleInventoryChanged);

	Refresh();
}

void UStillInventoryWidget::Refresh()
{
	if (StorageInv)
	{
		const TArray<FInventoryItem>& Items = StorageInv->GetItems();
		for (int32 i = 0; i < StorageSlotWidgets.Num(); ++i)
		{
			if (Items.IsValidIndex(i) && Items[i].Quantity > 0)
				StorageSlotWidgets[i]->SetSlotData(Items[i].ItemID, Items[i].Quantity, StorageInv);
			else
				StorageSlotWidgets[i]->SetSlotData(NAME_None, 0, StorageInv);
		}
	}

	if (RequirementText && Owner.IsValid() && StorageInv)
	{
		auto ReqStr = [this](const TCHAR* Name, FName Id) -> FString
		{
			return FString::Printf(TEXT("%s %d/%d"), Name, StorageInv->GetItemCount(Id), Owner->GetIngredientReq(Id));
		};
		RequirementText->SetText(FText::FromString(
			ReqStr(TEXT("Water"), FName(TEXT("Water"))) + TEXT(", ")
			+ ReqStr(TEXT("Mash"), FName(TEXT("Mash"))) + TEXT(", ")
			+ ReqStr(TEXT("Firewood"), FName(TEXT("Firewood")))));

		const bool bMet =
			StorageInv->GetItemCount(FName(TEXT("Water")))    >= Owner->GetIngredientReq(FName(TEXT("Water")))    &&
			StorageInv->GetItemCount(FName(TEXT("Mash")))     >= Owner->GetIngredientReq(FName(TEXT("Mash")))     &&
			StorageInv->GetItemCount(FName(TEXT("Firewood"))) >= Owner->GetIngredientReq(FName(TEXT("Firewood")));
		RequirementText->SetColorAndOpacity(FSlateColor(bMet ? SiMetGreen : SiShortRed));
	}

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(bShowShortWarning ? TEXT("Not Enough Ingredients") : TEXT("")));
	}
}

bool UStillInventoryWidget::IsScreenInsidePanel(const FVector2D& ScreenPos) const
{
	return Panel && Panel->GetCachedGeometry().IsUnderLocation(ScreenPos);
}

void UStillInventoryWidget::HandleStorageSlotClicked(int32 Index)
{
	// Click-transfer fallback: withdraw 1 of this storage slot's item back to the player.
	if (!PlayerInv || !StorageInv) return;
	const TArray<FInventoryItem>& Items = StorageInv->GetItems();
	if (!Items.IsValidIndex(Index) || Items[Index].Quantity <= 0) return;

	const FName ItemID = Items[Index].ItemID;
	const int32 Added = PlayerInv->AddItem(ItemID, 1);
	if (Added > 0)
	{
		StorageInv->RemoveItem(ItemID, Added);
	}
}

void UStillInventoryWidget::HandleSlotDragCancelled(UInventoryComponent* SourceInventory, int32 SourceIndex, FVector2D ScreenPos)
{
	if (Owner.IsValid())
	{
		Owner->HandleInventoryDragRelease(SourceInventory, SourceIndex, ScreenPos);
	}
}

void UStillInventoryWidget::HandleSlotDrop(UInventoryComponent* SourceInventory, int32 SourceIndex, UInventoryComponent* TargetInventory, int32 TargetIndex, int32 Count, bool bShiftDown)
{
	if (Owner.IsValid())
	{
		Owner->HandleSlotDrop(SourceInventory, SourceIndex, TargetInventory, TargetIndex, Count, bShiftDown);
	}
}

void UStillInventoryWidget::HandleInventoryChanged()
{
	Refresh();
}

void UStillInventoryWidget::OnStartClicked()
{
	if (!Owner.IsValid() || !Stand.IsValid()) return;

	if (Owner->TryStartDistilling(Stand.Get()))
	{
		Owner->CloseStillInventory();
	}
	else
	{
		bShowShortWarning = true;
		Refresh();
	}
}

void UStillInventoryWidget::OnCloseClicked()
{
	if (Owner.IsValid())
	{
		Owner->CloseStillInventory();
	}
}
