// Born To Shine - Per-still loading UI (split player/storage view)

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
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

namespace
{
	const FLinearColor PanelDark(0.08f, 0.06f, 0.04f, 0.95f);
	const FLinearColor TitleGold(0.95f, 0.8f, 0.2f, 1.0f);
	const FLinearColor TextCream(1.0f, 0.95f, 0.8f, 1.0f);
	const FLinearColor TextGrey(0.7f, 0.7f, 0.7f, 1.0f);
	const FLinearColor MetGreen(0.45f, 0.9f, 0.35f, 1.0f);
	const FLinearColor ShortRed(1.0f, 0.4f, 0.35f, 1.0f);

	FSlateBrush MakeRounded(const FLinearColor& Color, float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		return Brush;
	}

	UTextBlock* MakeLabel(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color)
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

UUniformGridPanel* UStillInventoryWidget::BuildGrid(int32 NumSlots, int32 Columns, bool bStorageSide, TArray<UInventorySlotWidget*>& OutSlots)
{
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	const int32 Cols = FMath::Max(1, Columns);
	for (int32 i = 0; i < NumSlots; ++i)
	{
		UInventorySlotWidget* SlotWidget = WidgetTree->ConstructWidget<UInventorySlotWidget>(UInventorySlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		if (bStorageSide)
		{
			SlotWidget->OnSlotClicked.AddDynamic(this, &UStillInventoryWidget::HandleStorageSlotClicked);
		}
		else
		{
			SlotWidget->OnSlotClicked.AddDynamic(this, &UStillInventoryWidget::HandlePlayerSlotClicked);
		}
		SlotWidget->OnSlotDragCancelled.AddDynamic(this, &UStillInventoryWidget::HandleSlotDragCancelled);

		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SizeBox->SetWidthOverride(80.0f);
		SizeBox->SetHeightOverride(80.0f);
		SizeBox->AddChild(SlotWidget);

		UUniformGridSlot* Cell = Grid->AddChildToUniformGrid(SizeBox, i / Cols, i % Cols);
		Cell->SetHorizontalAlignment(HAlign_Center);
		Cell->SetVerticalAlignment(VAlign_Center);

		OutSlots.Add(SlotWidget);
	}
	return Grid;
}

TSharedRef<SWidget> UStillInventoryWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));

	Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrush(MakeRounded(PanelDark, 10.0f));
	Panel->SetPadding(FMargin(26.0f, 20.0f));
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(VBox);

	TitleText = MakeLabel(WidgetTree, TEXT("STILL"), 40, TitleGold);
	UVerticalBoxSlot* TS = VBox->AddChildToVerticalBox(TitleText);
	TS->SetHorizontalAlignment(HAlign_Center);
	TS->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

	UTextBlock* Sub = MakeLabel(WidgetTree, TEXT("Drag ingredients into the still  |  E to close"), 11, TextGrey);
	UVerticalBoxSlot* SubS = VBox->AddChildToVerticalBox(Sub);
	SubS->SetHorizontalAlignment(HAlign_Center);
	SubS->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	// Split: player items (left) | still storage (right).
	UHorizontalBox* Split = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UVerticalBox* LeftCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LeftCol->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("YOUR ITEMS"), 16, TextCream));
	LeftCol->AddChildToVerticalBox(BuildGrid(FMath::Max(0, PlayerSlots), PlayerColumns, false, PlayerSlotWidgets));
	UHorizontalBoxSlot* LS = Split->AddChildToHorizontalBox(LeftCol);
	LS->SetPadding(FMargin(0.0f, 0.0f, 24.0f, 0.0f));
	LS->SetVerticalAlignment(VAlign_Top);

	UVerticalBox* RightCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	RightCol->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("STILL STORAGE"), 16, TextCream));
	RightCol->AddChildToVerticalBox(BuildGrid(FMath::Max(0, StorageSlots), StorageColumns, true, StorageSlotWidgets));
	UHorizontalBoxSlot* RS = Split->AddChildToHorizontalBox(RightCol);
	RS->SetVerticalAlignment(VAlign_Top);

	UVerticalBoxSlot* SplitSlot = VBox->AddChildToVerticalBox(Split);
	SplitSlot->SetHorizontalAlignment(HAlign_Center);

	RequirementText = MakeLabel(WidgetTree, TEXT(""), 16, TextCream);
	UVerticalBoxSlot* ReqS = VBox->AddChildToVerticalBox(RequirementText);
	ReqS->SetHorizontalAlignment(HAlign_Center);
	ReqS->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 2.0f));

	StatusText = MakeLabel(WidgetTree, TEXT(""), 14, ShortRed);
	UVerticalBoxSlot* StS = VBox->AddChildToVerticalBox(StatusText);
	StS->SetHorizontalAlignment(HAlign_Center);
	StS->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 6.0f));

	StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartButton"));
	{
		FButtonStyle S = StartButton->GetStyle();
		S.Normal = MakeRounded(FLinearColor(0.15f, 0.55f, 0.15f, 0.95f), 6.0f);
		S.Hovered = MakeRounded(FLinearColor(0.20f, 0.70f, 0.20f, 0.95f), 6.0f);
		S.Pressed = MakeRounded(FLinearColor(0.12f, 0.45f, 0.12f, 0.95f), 6.0f);
		StartButton->SetStyle(S);
	}
	StartButton->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnStartClicked);
	UTextBlock* StartLabel = MakeLabel(WidgetTree, TEXT("Start Distilling"), 20, FLinearColor::White);
	StartButton->AddChild(StartLabel);
	UVerticalBoxSlot* StartS = VBox->AddChildToVerticalBox(StartButton);
	StartS->SetHorizontalAlignment(HAlign_Fill);
	StartS->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 4.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	{
		FButtonStyle S = CloseButton->GetStyle();
		S.Normal = MakeRounded(FLinearColor(0.20f, 0.15f, 0.09f, 0.95f), 4.0f);
		S.Hovered = MakeRounded(FLinearColor(0.32f, 0.24f, 0.12f, 0.95f), 4.0f);
		S.Pressed = MakeRounded(FLinearColor(0.40f, 0.30f, 0.15f, 0.95f), 4.0f);
		CloseButton->SetStyle(S);
	}
	CloseButton->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnCloseClicked);
	CloseButton->AddChild(MakeLabel(WidgetTree, TEXT("Close"), 14, TextCream));
	UVerticalBoxSlot* CloseS = VBox->AddChildToVerticalBox(CloseButton);
	CloseS->SetHorizontalAlignment(HAlign_Center);
	CloseS->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	WidgetTree->RootWidget = RootCanvas;
	return Super::RebuildWidget();
}

void UStillInventoryWidget::NativeDestruct()
{
	if (PlayerInv) PlayerInv->OnInventoryChanged.RemoveDynamic(this, &UStillInventoryWidget::HandleInventoryChanged);
	if (StorageInv) StorageInv->OnInventoryChanged.RemoveDynamic(this, &UStillInventoryWidget::HandleInventoryChanged);
	Super::NativeDestruct();
}

void UStillInventoryWidget::SetupForStand(AMoonshineCharacter_Simple* InOwner, AStillPartActor* InStand)
{
	// Unbind any previous bindings (the widget is reused across stands).
	if (PlayerInv) PlayerInv->OnInventoryChanged.RemoveDynamic(this, &UStillInventoryWidget::HandleInventoryChanged);
	if (StorageInv) StorageInv->OnInventoryChanged.RemoveDynamic(this, &UStillInventoryWidget::HandleInventoryChanged);

	Owner = InOwner;
	Stand = InStand;
	PlayerInv = InOwner ? InOwner->GetInventoryComponent() : nullptr;
	StorageInv = InStand ? InStand->StillStorage : nullptr;
	bShowShortWarning = false;

	if (PlayerInv) PlayerInv->OnInventoryChanged.AddDynamic(this, &UStillInventoryWidget::HandleInventoryChanged);
	if (StorageInv) StorageInv->OnInventoryChanged.AddDynamic(this, &UStillInventoryWidget::HandleInventoryChanged);

	Refresh();
}

void UStillInventoryWidget::Refresh()
{
	if (Owner.IsValid() && Stand.IsValid() && TitleText)
	{
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("STILL %d"), Owner->GetStandNumber(Stand.Get()))));
	}

	if (PlayerInv)
	{
		const TArray<FInventoryItem>& Items = PlayerInv->GetItems();
		for (int32 i = 0; i < PlayerSlotWidgets.Num(); ++i)
		{
			if (Items.IsValidIndex(i) && Items[i].Quantity > 0)
				PlayerSlotWidgets[i]->SetSlotData(Items[i].ItemID, Items[i].Quantity, PlayerInv);
			else
				PlayerSlotWidgets[i]->SetSlotData(NAME_None, 0, PlayerInv);
		}
	}

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

	// Requirements scan the still storage.
	if (RequirementText && Owner.IsValid() && StorageInv)
	{
		auto ReqStr = [this](const TCHAR* Name, FName Id) -> FString
		{
			const int32 Have = StorageInv->GetItemCount(Id);
			const int32 Req = Owner->GetIngredientReq(Id);
			return FString::Printf(TEXT("%s %d/%d"), Name, Have, Req);
		};
		const FString Line = ReqStr(TEXT("Water"), FName(TEXT("Water"))) + TEXT("    ")
			+ ReqStr(TEXT("Mash"), FName(TEXT("Mash"))) + TEXT("    ")
			+ ReqStr(TEXT("Firewood"), FName(TEXT("Firewood")));

		const bool bMet =
			StorageInv->GetItemCount(FName(TEXT("Water")))    >= Owner->GetIngredientReq(FName(TEXT("Water")))    &&
			StorageInv->GetItemCount(FName(TEXT("Mash")))     >= Owner->GetIngredientReq(FName(TEXT("Mash")))     &&
			StorageInv->GetItemCount(FName(TEXT("Firewood"))) >= Owner->GetIngredientReq(FName(TEXT("Firewood")));

		RequirementText->SetText(FText::FromString(Line));
		RequirementText->SetColorAndOpacity(FSlateColor(bMet ? MetGreen : ShortRed));
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

void UStillInventoryWidget::HandlePlayerSlotClicked(int32 Index)
{
	// Click-transfer fallback: deposit 1 of this player slot's item into the still storage.
	if (!PlayerInv || !StorageInv) return;
	const TArray<FInventoryItem>& Items = PlayerInv->GetItems();
	if (!Items.IsValidIndex(Index) || Items[Index].Quantity <= 0) return;

	const FName ItemID = Items[Index].ItemID;
	const int32 Added = StorageInv->AddItem(ItemID, 1);
	if (Added > 0)
	{
		PlayerInv->RemoveItem(ItemID, Added);
	}
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
