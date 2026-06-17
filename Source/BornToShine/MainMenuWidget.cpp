// Born To Shine - Main menu widget

#include "MainMenuWidget.h"
#include "BornToShineGameInstance.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	const FLinearColor MmPanelDark(0.06f, 0.04f, 0.02f, 0.92f);
	const FLinearColor MmGold(0.95f, 0.8f, 0.2f, 1.0f);
	const FLinearColor MmCream(0.9f, 0.85f, 0.7f, 1.0f);
	const FLinearColor MmButtonNormal(0.15f, 0.12f, 0.08f, 1.0f);
	const FLinearColor MmButtonHover(0.25f, 0.2f, 0.12f, 1.0f);
	const FLinearColor MmButtonDisabled(0.1f, 0.08f, 0.06f, 0.6f);

	UButton* MmMakeMenuButton(UWidgetTree* Tree, const FString& Label, const FName& WidgetName)
	{
		UButton* Btn = Tree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
		{
			FButtonStyle Style = Btn->GetStyle();

			FSlateBrush Normal;
			Normal.DrawAs = ESlateBrushDrawType::RoundedBox;
			Normal.TintColor = FSlateColor(MmButtonNormal);
			Normal.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Normal.OutlineSettings.CornerRadii = FVector4(6.0f, 6.0f, 6.0f, 6.0f);
			Normal.OutlineSettings.Color = FSlateColor(MmGold);
			Normal.OutlineSettings.Width = 1.5f;

			FSlateBrush Hovered = Normal;
			Hovered.TintColor = FSlateColor(MmButtonHover);

			FSlateBrush Pressed = Normal;
			Pressed.TintColor = FSlateColor(FLinearColor(0.3f, 0.25f, 0.15f, 1.0f));

			FSlateBrush Disabled = Normal;
			Disabled.TintColor = FSlateColor(MmButtonDisabled);
			Disabled.OutlineSettings.Color = FSlateColor(FLinearColor(0.4f, 0.35f, 0.25f, 0.5f));

			Style.SetNormal(Normal);
			Style.SetHovered(Hovered);
			Style.SetPressed(Pressed);
			Style.SetDisabled(Disabled);
			Btn->SetStyle(Style);
		}

		UTextBlock* BtnLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%s_Label"), *WidgetName.ToString()));
		BtnLabel->SetText(FText::FromString(Label));
		FSlateFontInfo Font = BtnLabel->GetFont();
		Font.Size = 22;
		BtnLabel->SetFont(Font);
		BtnLabel->SetColorAndOpacity(FSlateColor(MmCream));
		BtnLabel->SetJustification(ETextJustify::Center);
		Btn->AddChild(BtnLabel);

		return Btn;
	}
}

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MmRootCanvas"));

	// Full-screen overlay
	UOverlay* MainOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MmOverlay"));
	UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(MainOverlay);
	OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	OverlaySlot->SetOffsets(FMargin(0));

	// Background image (or solid dark fallback)
	BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MmBgImage"));
	{
		FSlateBrush Brush;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		BackgroundImage->SetBrush(Brush);
		BackgroundImage->SetColorAndOpacity(FLinearColor(0.05f, 0.03f, 0.01f, 1.0f));
	}
	UOverlaySlot* BgSlot = MainOverlay->AddChildToOverlay(BackgroundImage);
	BgSlot->SetHorizontalAlignment(HAlign_Fill);
	BgSlot->SetVerticalAlignment(VAlign_Fill);

	// Center column with title + buttons
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MmVBox"));

	// Top spacer pushes content to upper-center
	USizeBox* TopSpacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MmTopSpacer"));
	TopSpacer->SetHeightOverride(1.0f);
	UVerticalBoxSlot* TopSpacerSlot = VBox->AddChildToVerticalBox(TopSpacer);
	TopSpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Title
	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MmTitle"));
	TitleText->SetText(FText::FromString(TEXT("BORN TO SHINE")));
	{
		FSlateFontInfo Font = TitleText->GetFont();
		Font.Size = 72;
		TitleText->SetFont(Font);
	}
	TitleText->SetColorAndOpacity(FSlateColor(MmGold));
	TitleText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 60.0f));
	TitleSlot->SetHorizontalAlignment(HAlign_Center);

	// Buttons — wrapped in SizeBoxes for consistent width
	auto AddButton = [&](UButton* Btn, float TopPad) -> UVerticalBoxSlot*
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("MmSizeBox_%s"), *Btn->GetName()));
		SizeBox->SetWidthOverride(320.0f);
		SizeBox->SetHeightOverride(56.0f);
		SizeBox->AddChild(Btn);
		UVerticalBoxSlot* Slot = VBox->AddChildToVerticalBox(SizeBox);
		Slot->SetPadding(FMargin(0.0f, TopPad, 0.0f, 0.0f));
		Slot->SetHorizontalAlignment(HAlign_Center);
		return Slot;
	};

	ContinueButton = MmMakeMenuButton(WidgetTree, TEXT("Continue"), TEXT("MmContinueBtn"));
	AddButton(ContinueButton, 0.0f);

	NewGameButton = MmMakeMenuButton(WidgetTree, TEXT("New Game"), TEXT("MmNewGameBtn"));
	AddButton(NewGameButton, 16.0f);

	LoadGameButton = MmMakeMenuButton(WidgetTree, TEXT("Load Game"), TEXT("MmLoadGameBtn"));
	AddButton(LoadGameButton, 16.0f);

	QuitButton = MmMakeMenuButton(WidgetTree, TEXT("Quit"), TEXT("MmQuitBtn"));
	AddButton(QuitButton, 32.0f);

	// Bottom spacer
	USizeBox* BotSpacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MmBotSpacer"));
	BotSpacer->SetHeightOverride(1.0f);
	UVerticalBoxSlot* BotSpacerSlot = VBox->AddChildToVerticalBox(BotSpacer);
	BotSpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Version label at the bottom
	VersionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MmVersion"));
	VersionText->SetText(FText::FromString(TEXT("Early Access")));
	{
		FSlateFontInfo Font = VersionText->GetFont();
		Font.Size = 11;
		VersionText->SetFont(Font);
	}
	VersionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.6f)));
	VersionText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* VerSlot = VBox->AddChildToVerticalBox(VersionText);
	VerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	VerSlot->SetHorizontalAlignment(HAlign_Center);

	UOverlaySlot* VBoxSlot = MainOverlay->AddChildToOverlay(VBox);
	VBoxSlot->SetHorizontalAlignment(HAlign_Fill);
	VBoxSlot->SetVerticalAlignment(VAlign_Fill);

	WidgetTree->RootWidget = RootCanvas;
	return Super::RebuildWidget();
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Wire button callbacks
	if (ContinueButton)  ContinueButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnContinueClicked);
	if (NewGameButton)   NewGameButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnNewGameClicked);
	if (LoadGameButton)  LoadGameButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnLoadGameClicked);
	if (QuitButton)      QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnQuitClicked);

	// Apply background texture if assigned
	if (BackgroundTexture && BackgroundImage)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(BackgroundTexture);
		Brush.ImageSize = FVector2D(1920.0f, 1080.0f);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		BackgroundImage->SetBrush(Brush);
		BackgroundImage->SetColorAndOpacity(FLinearColor::White);
	}

	// Disable Continue and Load if no save exists
	UBornToShineGameInstance* GI = Cast<UBornToShineGameInstance>(GetGameInstance());
	const bool bSaveExists = GI ? GI->HasExistingSave() : false;

	if (ContinueButton) ContinueButton->SetIsEnabled(bSaveExists);
	if (LoadGameButton) LoadGameButton->SetIsEnabled(bSaveExists);

	// Show mouse cursor
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(true);
		PC->SetInputMode(FInputModeUIOnly());
	}
}

void UMainMenuWidget::OnContinueClicked()
{
	OpenGameplayLevel(true);
}

void UMainMenuWidget::OnNewGameClicked()
{
	OpenGameplayLevel(false);
}

void UMainMenuWidget::OnLoadGameClicked()
{
	OpenGameplayLevel(true);
}

void UMainMenuWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::OpenGameplayLevel(bool bLoadExistingSave)
{
	UBornToShineGameInstance* GI = Cast<UBornToShineGameInstance>(GetGameInstance());
	if (GI)
	{
		GI->bShouldLoadSave = bLoadExistingSave;
		GI->PendingLoadSlot.Empty();
	}

	UGameplayStatics::OpenLevel(this, GameplayMapName);
}
