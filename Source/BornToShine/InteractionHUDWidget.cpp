// Born To Shine - Interaction HUD: [E] prompt, distill countdown, event toasts

#include "InteractionHUDWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

namespace
{
	// Palette shared with the inventory widgets.
	const FLinearColor PanelDark(0.08f, 0.06f, 0.04f, 0.85f);
	const FLinearColor TextCream(1.0f, 0.95f, 0.8f, 1.0f);
	const FLinearColor KeycapBg(0.9f, 0.85f, 0.7f, 0.9f);
	const FLinearColor KeycapText(0.1f, 0.08f, 0.05f, 1.0f);
	const FLinearColor ToastGreen(0.45f, 0.9f, 0.35f, 1.0f);
	const FLinearColor ToastRed(1.0f, 0.4f, 0.35f, 1.0f);
	const FLinearColor BarFill(0.95f, 0.8f, 0.2f, 1.0f);

	FSlateBrush MakeRoundedBrush(const FLinearColor& Color, float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		return Brush;
	}
}

TSharedRef<SWidget> UInteractionHUDWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));

	// --- Prompt: bottom-center, ~120px above the bottom edge ---
	PromptPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptPanel"));
	PromptPanel->SetBrush(MakeRoundedBrush(PanelDark, 8.0f));
	PromptPanel->SetPadding(FMargin(14.0f, 8.0f));

	UHorizontalBox* PromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PromptRow"));

	UBorder* Keycap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Keycap"));
	Keycap->SetBrush(MakeRoundedBrush(KeycapBg, 4.0f));
	Keycap->SetPadding(FMargin(8.0f, 2.0f));
	UTextBlock* KeycapLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeycapLabel"));
	KeycapLabel->SetText(FText::FromString(TEXT("E")));
	FSlateFontInfo KeyFont = KeycapLabel->GetFont();
	KeyFont.Size = 14;
	KeycapLabel->SetFont(KeyFont);
	KeycapLabel->SetColorAndOpacity(FSlateColor(KeycapText));
	Keycap->SetContent(KeycapLabel);
	UHorizontalBoxSlot* KeySlot = PromptRow->AddChildToHorizontalBox(Keycap);
	KeySlot->SetVerticalAlignment(VAlign_Center);
	KeySlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));

	PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptText"));
	FSlateFontInfo PromptFont = PromptText->GetFont();
	PromptFont.Size = 16;
	PromptText->SetFont(PromptFont);
	PromptText->SetColorAndOpacity(FSlateColor(TextCream));
	UHorizontalBoxSlot* TextSlot = PromptRow->AddChildToHorizontalBox(PromptText);
	TextSlot->SetVerticalAlignment(VAlign_Center);

	PromptPanel->SetContent(PromptRow);
	UCanvasPanelSlot* PromptSlot = RootCanvas->AddChildToCanvas(PromptPanel);
	PromptSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
	PromptSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	PromptSlot->SetAutoSize(true);
	PromptSlot->SetPosition(FVector2D(0.0f, -120.0f));
	PromptPanel->SetVisibility(ESlateVisibility::Collapsed);

	// --- Distill timer: top-center ---
	TimerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TimerBox"));

	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
	FSlateFontInfo TimerFont = TimerText->GetFont();
	TimerFont.Size = 18;
	TimerText->SetFont(TimerFont);
	TimerText->SetColorAndOpacity(FSlateColor(TextCream));
	TimerText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TimerTextSlot = TimerBox->AddChildToVerticalBox(TimerText);
	TimerTextSlot->SetHorizontalAlignment(HAlign_Center);
	TimerTextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TimerBarSize"));
	BarSize->SetWidthOverride(320.0f);
	BarSize->SetHeightOverride(8.0f);
	TimerBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TimerBar"));
	TimerBar->SetFillColorAndOpacity(BarFill);
	BarSize->AddChild(TimerBar);
	UVerticalBoxSlot* BarSlot = TimerBox->AddChildToVerticalBox(BarSize);
	BarSlot->SetHorizontalAlignment(HAlign_Center);

	UCanvasPanelSlot* TimerSlot = RootCanvas->AddChildToCanvas(TimerBox);
	TimerSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
	TimerSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	TimerSlot->SetAutoSize(true);
	TimerSlot->SetPosition(FVector2D(0.0f, TimerListTopOffset)); // below the suspicion star meter
	TimerBox->SetVisibility(ESlateVisibility::Collapsed);

	// --- Autosave indicator: bottom-right, faded in/out from code ---
	SaveIndicatorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SaveIndicatorText"));
	SaveIndicatorText->SetText(FText::FromString(TEXT("Saving…")));
	FSlateFontInfo SaveFont = SaveIndicatorText->GetFont();
	SaveFont.Size = 12;
	SaveIndicatorText->SetFont(SaveFont);
	SaveIndicatorText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f)));
	UCanvasPanelSlot* SaveSlot = RootCanvas->AddChildToCanvas(SaveIndicatorText);
	SaveSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
	SaveSlot->SetAlignment(FVector2D(1.0f, 1.0f));
	SaveSlot->SetAutoSize(true);
	SaveSlot->SetPosition(FVector2D(-20.0f, -20.0f));
	SaveIndicatorText->SetVisibility(ESlateVisibility::Collapsed);

	// --- Toast stack: just above the prompt slot ---
	ToastBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ToastBox"));
	UCanvasPanelSlot* ToastSlot = RootCanvas->AddChildToCanvas(ToastBox);
	ToastSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
	ToastSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	ToastSlot->SetAutoSize(true);
	ToastSlot->SetPosition(FVector2D(0.0f, -180.0f));

	WidgetTree->RootWidget = RootCanvas;

	return Super::RebuildWidget();
}

void UInteractionHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Fade the autosave indicator out.
	if (SaveIndicatorAge >= 0.0f && SaveIndicatorText)
	{
		SaveIndicatorAge += InDeltaTime;
		if (SaveIndicatorAge >= SaveIndicatorLifetime)
		{
			SaveIndicatorAge = -1.0f;
			SaveIndicatorText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			SaveIndicatorText->SetRenderOpacity(1.0f - SaveIndicatorAge / SaveIndicatorLifetime);
		}
	}

	// Age toasts: hold full opacity, then fade out, then drop.
	for (int32 i = Toasts.Num() - 1; i >= 0; --i)
	{
		FToastEntry& Toast = Toasts[i];
		Toast.Age += InDeltaTime;

		UBorder* Panel = Toast.Panel.Get();
		if (!Panel || Toast.Age >= ToastLifetime)
		{
			if (Panel)
			{
				Panel->RemoveFromParent();
			}
			Toasts.RemoveAt(i);
			continue;
		}

		if (Toast.Age > ToastFadeStart)
		{
			const float Fade = 1.0f - (Toast.Age - ToastFadeStart) / (ToastLifetime - ToastFadeStart);
			Panel->SetRenderOpacity(FMath::Clamp(Fade, 0.0f, 1.0f));
		}
	}
}

void UInteractionHUDWidget::ShowSaveIndicator()
{
	if (!SaveIndicatorText) return;
	SaveIndicatorAge = 0.0f;
	SaveIndicatorText->SetRenderOpacity(1.0f);
	SaveIndicatorText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UInteractionHUDWidget::SetPrompt(const FString& ActionText)
{
	if (!PromptPanel || !PromptText) return;
	PromptText->SetText(FText::FromString(ActionText));
	PromptPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UInteractionHUDWidget::ClearPrompt()
{
	if (!PromptPanel) return;
	PromptPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UInteractionHUDWidget::ShowTimer(const FString& Text, float BarPercent)
{
	if (!TimerBox || !TimerText || !TimerBar) return;

	TimerText->SetText(FText::FromString(Text));
	TimerBar->SetPercent(FMath::Clamp(BarPercent, 0.0f, 1.0f));

	TimerBox->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UInteractionHUDWidget::HideTimer()
{
	if (!TimerBox) return;
	TimerBox->SetVisibility(ESlateVisibility::Collapsed);
}

bool UInteractionHUDWidget::AddToast(const FString& Text, bool bSuccess)
{
	if (!ToastBox || !WidgetTree) return false;

	// Refresh instead of duplicating when the same message is re-issued while still fresh
	// (also protects against per-tick callers stacking copies).
	for (FToastEntry& Toast : Toasts)
	{
		if (Toast.Text == Text && Toast.Age < ToastFadeStart)
		{
			Toast.Age = 0.0f;
			if (UBorder* Panel = Toast.Panel.Get())
			{
				Panel->SetRenderOpacity(1.0f);
			}
			return false;
		}
	}

	// Oldest drops first beyond the cap.
	while (Toasts.Num() >= MaxToasts)
	{
		if (UBorder* Panel = Toasts[0].Panel.Get())
		{
			Panel->RemoveFromParent();
		}
		Toasts.RemoveAt(0);
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrush(MakeRoundedBrush(PanelDark, 6.0f));
	Panel->SetPadding(FMargin(12.0f, 6.0f));

	UTextBlock* ToastText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ToastText->SetText(FText::FromString(Text));
	FSlateFontInfo ToastFont = ToastText->GetFont();
	ToastFont.Size = 14;
	ToastText->SetFont(ToastFont);
	ToastText->SetColorAndOpacity(FSlateColor(bSuccess ? ToastGreen : ToastRed));
	ToastText->SetJustification(ETextJustify::Center);
	Panel->SetContent(ToastText);

	UVerticalBoxSlot* PanelSlot = ToastBox->AddChildToVerticalBox(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetPadding(FMargin(0.0f, 2.0f));

	FToastEntry Entry;
	Entry.Panel = Panel;
	Entry.Text = Text;
	Toasts.Add(Entry);
	return true;
}
