// Born To Shine - Interaction HUD: [E] prompt, distill countdown, event toasts

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionHUDWidget.generated.h"

class UBorder;
class UTextBlock;
class UProgressBar;
class UVerticalBox;

/**
 * Code-built HUD overlay (same style as the inventory widgets):
 *  - bottom-center contextual prompt: keycap "[E]" + action text
 *  - top-center distill countdown text + progress bar (only while Running)
 *  - toast stack above the prompt: short fading event messages (green success / red failure)
 */
UCLASS()
class BORNTOSHINE_API UInteractionHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Shows the bottom-center prompt as "[E] <ActionText>".
	void SetPrompt(const FString& ActionText);
	void ClearPrompt();

	// Shows/updates the top-center countdown block. Text may be multi-line (one line per running
	// still); the fill bar shows BarPercent (0..1) for the primary (nearest) still.
	void ShowTimer(const FString& Text, float BarPercent);
	void HideTimer();

	// Pushes a fading event message (green when bSuccess, red otherwise). Re-issuing the same
	// text while it is still fresh refreshes it instead of stacking a duplicate. Returns true
	// when a NEW toast was added (false on refresh) so callers can gate one-shot audio.
	bool AddToast(const FString& Text, bool bSuccess);

	// Flashes the small bottom-right "Saving…" indicator (fades out over ~1.5s). Silent.
	void ShowSaveIndicator();

protected:
	UPROPERTY() UBorder* PromptPanel = nullptr;
	UPROPERTY() UTextBlock* PromptText = nullptr;

	UPROPERTY() UVerticalBox* TimerBox = nullptr;
	UPROPERTY() UTextBlock* TimerText = nullptr;
	UPROPERTY() UProgressBar* TimerBar = nullptr;

	UPROPERTY() UVerticalBox* ToastBox = nullptr;

	UPROPERTY() UTextBlock* SaveIndicatorText = nullptr;
	float SaveIndicatorAge = -1.0f; // < 0 = hidden
	static constexpr float SaveIndicatorLifetime = 1.5f;

	struct FToastEntry
	{
		TWeakObjectPtr<UBorder> Panel;
		FString Text;
		float Age = 0.0f;
	};
	TArray<FToastEntry> Toasts; // panels are owned/GC-rooted by ToastBox

	static constexpr float ToastLifetime = 2.5f;  // total seconds on screen
	static constexpr float ToastFadeStart = 1.5f; // opacity ramps down after this age
	static constexpr int32 MaxToasts = 3;
};
