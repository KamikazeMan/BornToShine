// Born To Shine - Per-still loading UI

#include "StillInventoryWidget.h"
#include "MoonshineCharacter_Simple.h"
#include "StillPartActor.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"

namespace
{
	// Palette shared with the inventory widgets.
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
}

UButton* UStillInventoryWidget::MakeButton(const FString& Label, const FString& WidgetTag)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *WidgetTag);
	FButtonStyle Style = Button->GetStyle();
	Style.Normal = MakeRounded(FLinearColor(0.20f, 0.15f, 0.09f, 0.95f), 4.0f);
	Style.Hovered = MakeRounded(FLinearColor(0.32f, 0.24f, 0.12f, 0.95f), 4.0f);
	Style.Pressed = MakeRounded(FLinearColor(0.40f, 0.30f, 0.15f, 0.95f), 4.0f);
	Button->SetStyle(Style);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(WidgetTag + TEXT("_Label")));
	Text->SetText(FText::FromString(Label));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 14;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(TextCream));
	Text->SetJustification(ETextJustify::Center);
	Button->AddChild(Text);
	return Button;
}

UTextBlock* UStillInventoryWidget::MakeRowText(const FString& WidgetTag)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *WidgetTag);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 18;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(TextCream));
	Text->SetJustification(ETextJustify::Center);
	return Text;
}

TSharedRef<SWidget> UStillInventoryWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));

	// Full-screen backdrop: dims the scene AND absorbs stray mouse clicks so they don't reach the
	// build system while the loading UI is open. A UButton reliably consumes the click; keyboard
	// stays with the viewport (input mode has no widget focus) so E still closes the UI.
	UButton* Backdrop = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Backdrop"));
	{
		FButtonStyle BackStyle = Backdrop->GetStyle();
		FSlateBrush Dim;
		Dim.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
		Dim.DrawAs = ESlateBrushDrawType::Image;
		BackStyle.Normal = Dim;
		BackStyle.Hovered = Dim;
		BackStyle.Pressed = Dim;
		Backdrop->SetStyle(BackStyle);
	}
	UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackdropSlot->SetOffsets(FMargin(0.0f));

	// Centered dark panel.
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrush(MakeRounded(PanelDark, 10.0f));
	Panel->SetPadding(FMargin(28.0f, 22.0f));
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox"));
	Panel->SetContent(VBox);

	// Title.
	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("STILL")));
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 40;
	TitleText->SetFont(TitleFont);
	TitleText->SetColorAndOpacity(FSlateColor(TitleGold));
	TitleText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

	UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Subtitle"));
	Subtitle->SetText(FText::FromString(TEXT("Load ingredients, then Start Distilling  |  E to close")));
	FSlateFontInfo SubFont = Subtitle->GetFont();
	SubFont.Size = 11;
	Subtitle->SetFont(SubFont);
	Subtitle->SetColorAndOpacity(FSlateColor(TextGrey));
	Subtitle->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* SubSlot = VBox->AddChildToVerticalBox(Subtitle);
	SubSlot->SetHorizontalAlignment(HAlign_Center);
	SubSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	// One row per ingredient: [Take -]  <count text>  [Add +].
	auto AddRow = [this, VBox](UTextBlock*& OutRowText, const FString& Tag,
		UButton*& OutTake, UButton*& OutAdd)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *(Tag + TEXT("_Row")));

		OutTake = MakeButton(TEXT("Take -"), Tag + TEXT("_Take"));
		UHorizontalBoxSlot* TakeSlot = Row->AddChildToHorizontalBox(OutTake);
		TakeSlot->SetVerticalAlignment(VAlign_Center);
		TakeSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));

		OutRowText = MakeRowText(Tag + TEXT("_Count"));
		UHorizontalBoxSlot* CountSlot = Row->AddChildToHorizontalBox(OutRowText);
		CountSlot->SetVerticalAlignment(VAlign_Center);
		CountSlot->SetHorizontalAlignment(HAlign_Center);
		CountSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		OutAdd = MakeButton(TEXT("Add +"), Tag + TEXT("_Add"));
		UHorizontalBoxSlot* AddSlot = Row->AddChildToHorizontalBox(OutAdd);
		AddSlot->SetVerticalAlignment(VAlign_Center);
		AddSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));

		UVerticalBoxSlot* RowVSlot = VBox->AddChildToVerticalBox(Row);
		RowVSlot->SetPadding(FMargin(0.0f, 5.0f));
		RowVSlot->SetHorizontalAlignment(HAlign_Fill);
	};

	UButton *WTake = nullptr, *WAdd = nullptr, *MTake = nullptr, *MAdd = nullptr, *FTake = nullptr, *FAdd = nullptr;
	AddRow(WaterRowText, TEXT("Water"), WTake, WAdd);
	AddRow(MashRowText, TEXT("Mash"), MTake, MAdd);
	AddRow(FirewoodRowText, TEXT("Firewood"), FTake, FAdd);

	WTake->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnTakeWater);
	WAdd->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnAddWater);
	MTake->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnTakeMash);
	MAdd->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnAddMash);
	FTake->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnTakeFirewood);
	FAdd->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnAddFirewood);

	// Status line (short-on-ingredients warning).
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetText(FText::FromString(TEXT("")));
	FSlateFontInfo StatusFont = StatusText->GetFont();
	StatusFont.Size = 14;
	StatusText->SetFont(StatusFont);
	StatusText->SetColorAndOpacity(FSlateColor(ShortRed));
	StatusText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* StatusSlot = VBox->AddChildToVerticalBox(StatusText);
	StatusSlot->SetHorizontalAlignment(HAlign_Center);
	StatusSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 6.0f));

	// Start Distilling (green).
	StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartButton"));
	FButtonStyle StartStyle = StartButton->GetStyle();
	StartStyle.Normal = MakeRounded(FLinearColor(0.15f, 0.55f, 0.15f, 0.95f), 6.0f);
	StartStyle.Hovered = MakeRounded(FLinearColor(0.20f, 0.70f, 0.20f, 0.95f), 6.0f);
	StartStyle.Pressed = MakeRounded(FLinearColor(0.12f, 0.45f, 0.12f, 0.95f), 6.0f);
	StartButton->SetStyle(StartStyle);
	StartButton->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnStartClicked);
	UTextBlock* StartLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartLabel"));
	StartLabel->SetText(FText::FromString(TEXT("Start Distilling")));
	FSlateFontInfo StartFont = StartLabel->GetFont();
	StartFont.Size = 20;
	StartLabel->SetFont(StartFont);
	StartLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StartLabel->SetJustification(ETextJustify::Center);
	StartButton->AddChild(StartLabel);
	UVerticalBoxSlot* StartSlot = VBox->AddChildToVerticalBox(StartButton);
	StartSlot->SetHorizontalAlignment(HAlign_Fill);
	StartSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 4.0f));

	// Close.
	UButton* CloseButton = MakeButton(TEXT("Close"), TEXT("CloseButton"));
	CloseButton->OnClicked.AddDynamic(this, &UStillInventoryWidget::OnCloseClicked);
	UVerticalBoxSlot* CloseSlot = VBox->AddChildToVerticalBox(CloseButton);
	CloseSlot->SetHorizontalAlignment(HAlign_Center);
	CloseSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	WidgetTree->RootWidget = RootCanvas;
	return Super::RebuildWidget();
}

void UStillInventoryWidget::SetupForStand(AMoonshineCharacter_Simple* InOwner, AStillPartActor* InStand)
{
	Owner = InOwner;
	Stand = InStand;
	bShowShortWarning = false;
	Refresh();
}

void UStillInventoryWidget::RefreshRow(UTextBlock* RowText, FName Ingredient, const FString& DisplayName)
{
	if (!RowText || !Owner.IsValid() || !Stand.IsValid()) return;

	const int32 Stored = Stand->GetStored(Ingredient);
	const int32 Req = Owner->GetIngredientReq(Ingredient);
	const int32 PlayerHas = Owner->GetPlayerIngredientCount(Ingredient);

	RowText->SetText(FText::FromString(
		FString::Printf(TEXT("%s:  %d / %d    (you: %d)"), *DisplayName, Stored, Req, PlayerHas)));
	RowText->SetColorAndOpacity(FSlateColor(Stored >= Req ? MetGreen : ShortRed));
}

void UStillInventoryWidget::Refresh()
{
	if (Owner.IsValid() && Stand.IsValid() && TitleText)
	{
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("STILL %d"), Owner->GetStandNumber(Stand.Get()))));
	}

	RefreshRow(WaterRowText, FName(TEXT("Water")), TEXT("Water"));
	RefreshRow(MashRowText, FName(TEXT("Mash")), TEXT("Mash"));
	RefreshRow(FirewoodRowText, FName(TEXT("Firewood")), TEXT("Firewood"));

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(bShowShortWarning ? TEXT("Not Enough Ingredients") : TEXT("")));
	}
}

void UStillInventoryWidget::OnAddWater()    { if (Owner.IsValid() && Stand.IsValid()) { Owner->TransferIngredientToStill(Stand.Get(), FName(TEXT("Water"))); bShowShortWarning = false; Refresh(); } }
void UStillInventoryWidget::OnTakeWater()   { if (Owner.IsValid() && Stand.IsValid()) { Owner->TransferIngredientToPlayer(Stand.Get(), FName(TEXT("Water"))); Refresh(); } }
void UStillInventoryWidget::OnAddMash()     { if (Owner.IsValid() && Stand.IsValid()) { Owner->TransferIngredientToStill(Stand.Get(), FName(TEXT("Mash"))); bShowShortWarning = false; Refresh(); } }
void UStillInventoryWidget::OnTakeMash()    { if (Owner.IsValid() && Stand.IsValid()) { Owner->TransferIngredientToPlayer(Stand.Get(), FName(TEXT("Mash"))); Refresh(); } }
void UStillInventoryWidget::OnAddFirewood() { if (Owner.IsValid() && Stand.IsValid()) { Owner->TransferIngredientToStill(Stand.Get(), FName(TEXT("Firewood"))); bShowShortWarning = false; Refresh(); } }
void UStillInventoryWidget::OnTakeFirewood(){ if (Owner.IsValid() && Stand.IsValid()) { Owner->TransferIngredientToPlayer(Stand.Get(), FName(TEXT("Firewood"))); Refresh(); } }

void UStillInventoryWidget::OnStartClicked()
{
	if (!Owner.IsValid() || !Stand.IsValid()) return;

	if (Owner->TryStartDistilling(Stand.Get()))
	{
		// Success: the character closes the UI and kicks the batch.
		Owner->CloseStillInventory();
	}
	else
	{
		// Short: stay open, surface the warning (per-row red counts already show what's missing).
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
