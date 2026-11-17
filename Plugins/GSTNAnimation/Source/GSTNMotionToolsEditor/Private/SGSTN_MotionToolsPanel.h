#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

// Hand UI of GSTN Motion Tools, on the Level Sequence open in Sequencer.
// Overlap: chain (from the rig, or the viewport selection), preset, mode, three sliders (Ritardo, Morbidezza,
// Rimbalzo), weight, key protection, optional floor guard. A preset fills the sliders; any slider can then be moved.
// With "Aggiorna dal vivo" every slider release re-applies (the operation rewrites the same layer channels).
// Easing: preset + intensity on the selected controls, between the two base keys around the playhead or on a typed
// interval. Each operation only writes its own additive layer; the base keys never change.
class SGSTN_MotionToolsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_MotionToolsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	using FOptions = TArray<TSharedPtr<FString>>;

	struct FOverlapPreset
	{
		FString Id;
		float Delay = 2.f;
		float Softness = 0.2f;
		float Bounce = 0.3f;
		FString Mode;
	};

	TSharedRef<SWidget> MakeCombo(FOptions& Options, int32& Index, TSharedPtr<SComboBox<TSharedPtr<FString>>>& OutCombo, TFunction<void()> OnChanged);
	TSharedRef<SWidget> MakeSlider(const FText& Label, const FText& Tooltip, float& Value, float Min, float Max, float Delta);
	TSharedRef<SWidget> MakeCheck(const FText& Label, const FText& Tooltip, bool& Value);

	void Refresh();
	void ApplyPreset();
	void ApplyOverlap();
	void RemoveChain();
	void SetLayerActive(const FString& Layer, bool bActive);
	void RemoveLayer(const FString& Layer);
	void ApplyEasing(bool bAroundPlayhead);
	void OnSliderReleased();
	TSharedRef<FJsonObject> MakeRequest(const FString& Op) const;
	void AddChainFields(const TSharedRef<FJsonObject>& Request) const;
	void ShowReply(const TSharedRef<FJsonObject>& Reply);

	FOptions ChainOptions;
	TArray<FString> ChainIds;          // "" = viewport selection
	int32 ChainIndex = 0;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ChainCombo;

	FOptions PresetOptions;
	TArray<FOverlapPreset> Presets;
	int32 PresetIndex = 0;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PresetCombo;

	FOptions ModeOptions;
	TArray<FString> ModeIds;
	int32 ModeIndex = 0;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ModeCombo;

	FOptions EasingOptions;
	TArray<FString> EasingIds;
	int32 EasingIndex = 0;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> EasingCombo;

	float Delay = 2.f;
	float Softness = 0.2f;
	float Bounce = 0.3f;
	float Weight = 1.f;
	bool bProtectKeys = true;
	FString KeyFramesText;           // owner key frames, '0, 6, 12'; empty = the base keys
	bool bFloor = false;
	float FloorZ = 0.f;
	bool bLive = true;
	float EaseIntensity = 1.f;
	float EaseStart = 0.f;
	float EaseEnd = 10.f;

	FText SequenceText;
	FText LayersText;
	FText StatusText;
	bool bLastOk = true;
};
