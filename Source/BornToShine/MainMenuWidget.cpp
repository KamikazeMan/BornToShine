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
	const FLinearColor MmGold(0.95f, 0.8f, 0.2f, 1.0f);
	const FLinearColor MmCream(0.9f, 0.85f, 0.7f, 1.0f);
	const FLinearColor MmButtonNormal(0.15f, 0.12f, 0.08f, 0.95f);
	const FLinearColor MmButtonHover(0.25f, 0.2f, 0.12f, 0.95f);
	const FLinearColor MmButtonDisabled(0.1f, 0.08f, 0.06f, 0.5f);

	FSlateBrush MmMakeRounded(const FLinearColor& Color, float Radius)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Color);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		B.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		return B;
	}

	UButton* MmMakeMenuButton(UWidgetTree* Tree, const FString& Label, const FName& WidgetName)
	{
		UButton* Btn = Tree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
		{
			FButtonStyle Style = Btn->GetStyle();
			Style.SetNormal(MmMakeRounded(MmButtonNormal, 6.0f));
			Style.SetHovered(MmMakeRounded(MmButtonHover, 6.0f));
			Style.SetPressed(MmMakeRounded(FLinearColor(0.3f, 0.25f, 0.15f, 0.95f), 6.0f));
			Style.SetDisabled(MmMakeRounded(MmButtonDisabled, 6.0f));
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

	// Full-screen background image (or solid dark fallback)
	BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MmBgImage"));
	{
		FSlateBrush Brush;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		BackgroundImage->SetBrush(Brush);
		BackgroundImage->SetColorAndOpacity(FLinearColor(0.05f, 0.03f, 0.01f, 1.0f));
	}
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BackgroundImage);
	BgSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BgSlot->SetOffsets(FMargin(0));

	// Button column — anchored lower-left so it sits over the dark forest area of the splash
	UVerticalBox* BtnBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MmBtnBox"));

	auto AddButton = [&](UButton* Btn, float TopPad)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("MmSizeBox_%s"), *Btn->GetName()));
		SizeBox->SetWidthOverride(280.0f);
		SizeBox->SetHeightOverride(52.0f);
		SizeBox->AddChild(Btn);
		UVerticalBoxSlot* Slot = BtnBox->AddChildToVerticalBox(SizeBox);
		Slot->SetPadding(FMargin(0.0f, TopPad, 0.0f, 0.0f));
		Slot->SetHorizontalAlignment(HAlign_Left);
	};

	ContinueButton = MmMakeMenuButton(WidgetTree, TEXT("Continue"), TEXT("MmContinueBtn"));
	AddButton(ContinueButton, 0.0f);

	NewGameButton = MmMakeMenuButton(WidgetTree, TEXT("New Game"), TEXT("MmNewGameBtn"));
	AddButton(NewGameButton, 12.0f);

	LoadGameButton = MmMakeMenuButton(WidgetTree, TEXT("Load Game"), TEXT("MmLoadGameBtn"));
	AddButton(LoadGameButton, 12.0f);

	QuitButton = MmMakeMenuButton(WidgetTree, TEXT("Quit"), TEXT("MmQuitBtn"));
	AddButton(QuitButton, 28.0f);

	UCanvasPanelSlot* BtnSlot = RootCanvas->AddChildToCanvas(BtnBox);
	BtnSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
	BtnSlot->SetAlignment(FVector2D(0.0f, 1.0f));
	BtnSlot->SetAutoSize(true);
	BtnSlot->SetPosition(FVector2D(80.0f, -100.0f));

	// Version label — bottom-right corner
	VersionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MmVersion"));
	VersionText->SetText(FText::FromString(TEXT("Early Access")));
	{
		FSlateFontInfo Font = VersionText->GetFont();
		Font.Size = 11;
		VersionText->SetFont(Font);
	}
	VersionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.6f)));
	UCanvasPanelSlot* VerSlot = RootCanvas->AddChildToCanvas(VersionText);
	VerSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
	VerSlot->SetAlignment(FVector2D(1.0f, 1.0f));
	VerSlot->SetAutoSize(true);
	VerSlot->SetPosition(FVector2D(-20.0f, -16.0f));

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
	if (TitleBackground && BackgroundImage)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(TitleBackground);
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

	// Show mouse cursor + UI input mode
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
