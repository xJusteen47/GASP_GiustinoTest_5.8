#include "SGSTN_MotionToolsPanel.h"

#include "GSTN_MotionToolsPython.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNMotionTools"

namespace
{
	TSharedRef<SWidget> Heading(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(FAppStyle::GetFontStyle("BoldFont"));
	}

	TSharedRef<SWidget> Labelled(const FText& Label, const TSharedRef<SWidget>& Widget)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center).Padding(0, 0, 6, 0)[SNew(STextBlock).Text(Label)]
			+ SHorizontalBox::Slot().FillWidth(0.65f)[Widget];
	}

	FString ModeLabel(const FString& Id)
	{
		return Id == TEXT("offset") ? TEXT("Sfasamento") : TEXT("Molla");
	}
}

void SGSTN_MotionToolsPanel::Construct(const FArguments& InArgs)
{
	ModeIds = {TEXT("spring"), TEXT("offset")};
	ModeOptions = {MakeShared<FString>(TEXT("Molla")), MakeShared<FString>(TEXT("Sfasamento"))};

	auto Button = [](const FText& Text, const FText& Tip, TFunction<void()> Action)
	{
		return SNew(SButton).Text(Text).ToolTipText(Tip).OnClicked_Lambda([Action]() { Action(); return FReply::Handled(); });
	};

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(8)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([this]() { return SequenceText; }).AutoWrapText(true)]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Refresh", "Aggiorna"), LOCTEXT("RefreshTip", "Rilegge la sequenza aperta, le catene del rig e i layer."), [this]() { Refresh(); })]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[SNew(STextBlock).Text_Lambda([this]() { return LayersText; }).AutoWrapText(true)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)[SNew(SSeparator)]

			// ---- Overlap ---------------------------------------------------------------------------------------
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[Heading(LOCTEXT("OverlapHead", "Overlap (follow-through)"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[Labelled(LOCTEXT("Chain", "Catena"), MakeCombo(ChainOptions, ChainIndex, ChainCombo, nullptr))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[Labelled(LOCTEXT("Preset", "Preset"), MakeCombo(PresetOptions, PresetIndex, PresetCombo, [this]() { ApplyPreset(); }))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[Labelled(LOCTEXT("Mode", "Modo"), MakeCombo(ModeOptions, ModeIndex, ModeCombo, [this]() { if (bLive) { ApplyOverlap(); } }))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[MakeSlider(LOCTEXT("Delay", "Ritardo (frame)"), LOCTEXT("DelayTip", "Quanti frame la punta della catena arriva dopo. Gli anelli in mezzo in proporzione."), Delay, 0.f, 12.f, 0.5f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[MakeSlider(LOCTEXT("Softness", "Morbidezza"), LOCTEXT("SoftnessTip", "Arrotonda il movimento della catena (0 = come la base)."), Softness, 0.f, 1.f, 0.05f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[MakeSlider(LOCTEXT("Bounce", "Rimbalzo"), LOCTEXT("BounceTip", "Quanto la catena supera la posa e torna quando il movimento si ferma (0 = si assesta)."), Bounce, 0.f, 1.f, 0.05f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[MakeSlider(LOCTEXT("Weight", "Peso"), LOCTEXT("WeightTip", "Quantità di overlap su questa catena (1 = calcolato)."), Weight, 0.f, 1.5f, 0.05f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[MakeCheck(LOCTEXT("Protect", "Proteggi le chiavi"), LOCTEXT("ProtectTip", "Peso 0 sui frame chiave della base: le pose chiave restano identiche. Spento: solo primo e ultimo frame."), bProtectKeys)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				Labelled(LOCTEXT("KeyFrames", "Frame chiave"),
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("KeyFramesHint", "vuoto = chiavi della base; clip cotta: es. 0, 6, 12"))
					.ToolTipText(LOCTEXT("KeyFramesTip", "Frame delle pose chiave da proteggere (frame del Sequencer). Serve quando la base ha una chiave per frame."))
					.Text_Lambda([this]() { return FText::FromString(KeyFramesText); })
					.OnTextChanged_Lambda([this](const FText& NewText) { KeyFramesText = NewText.ToString(); }))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.5f)[MakeCheck(LOCTEXT("Floor", "Pavimento solido"), LOCTEXT("FloorTip", "L'overlap non spinge la catena sotto il piano Z indicato (sfere come il ground-contact del gioco). Non corregge la base."), bFloor)]
				+ SHorizontalBox::Slot().FillWidth(0.5f)[MakeSlider(LOCTEXT("FloorZ", "Z"), LOCTEXT("FloorZTip", "Altezza del pavimento nel mondo (cm)."), FloorZ, -500.f, 500.f, 1.f)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[MakeCheck(LOCTEXT("Live", "Aggiorna dal vivo"), LOCTEXT("LiveTip", "Riapplica quando rilasci un cursore."), bLive)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[Button(LOCTEXT("ApplyOverlap", "Applica overlap"), LOCTEXT("ApplyOverlapTip", "Scrive la catena sul layer Overlap (riscrive se c'è già)."), [this]() { ApplyOverlap(); })]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[Button(LOCTEXT("RemoveChain", "Togli catena"), LOCTEXT("RemoveChainTip", "Toglie questa catena dal layer Overlap."), [this]() { RemoveChain(); })]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[Button(LOCTEXT("OverlapOff", "Spento"), LOCTEXT("OverlapOffTip", "Spegne il layer Overlap (confronto A/B)."), [this]() { SetLayerActive(TEXT("Overlap"), false); })]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("OverlapOn", "Acceso"), LOCTEXT("OverlapOnTip", "Riaccende il layer Overlap."), [this]() { SetLayerActive(TEXT("Overlap"), true); })]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)[SNew(SSeparator)]

			// ---- Easing ----------------------------------------------------------------------------------------
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[Heading(LOCTEXT("EasingHead", "Easing (sui controlli selezionati)"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[Labelled(LOCTEXT("EasePreset", "Preset"), MakeCombo(EasingOptions, EasingIndex, EasingCombo, nullptr))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[MakeSlider(LOCTEXT("Intensity", "Intensità"), LOCTEXT("IntensityTip", "Forza di anticipo, overshoot e assestamento (1 = sheet delle curve)."), EaseIntensity, 0.25f, 2.f, 0.05f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				Button(LOCTEXT("EaseAround", "Easing tra le chiavi attorno al cursore"), LOCTEXT("EaseAroundTip", "Usa le due chiavi della base prima e dopo il cursore del Sequencer."), [this]() { ApplyEasing(true); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.4f)[MakeSlider(LOCTEXT("EaseStart", "Da"), FText::GetEmpty(), EaseStart, -1000.f, 10000.f, 1.f)]
				+ SHorizontalBox::Slot().FillWidth(0.4f)[MakeSlider(LOCTEXT("EaseEnd", "A"), FText::GetEmpty(), EaseEnd, -1000.f, 10000.f, 1.f)]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("EaseRange", "Applica"), LOCTEXT("EaseRangeTip", "Easing sull'intervallo scritto (frame del Sequencer)."), [this]() { ApplyEasing(false); })]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[Button(LOCTEXT("EaseOff", "Spento"), FText::GetEmpty(), [this]() { SetLayerActive(TEXT("Easing"), false); })]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[Button(LOCTEXT("EaseOn", "Acceso"), FText::GetEmpty(), [this]() { SetLayerActive(TEXT("Easing"), true); })]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("EaseRemove", "Rimuovi layer Easing"), FText::GetEmpty(), [this]() { RemoveLayer(TEXT("Easing")); })]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)[SNew(SSeparator)]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return StatusText; })
				.ColorAndOpacity_Lambda([this]() { return bLastOk ? FSlateColor::UseForeground() : FSlateColor(FLinearColor(1.f, 0.35f, 0.3f)); })
				.AutoWrapText(true)
			]
		]
	];

	Refresh();
}

TSharedRef<SWidget> SGSTN_MotionToolsPanel::MakeCombo(FOptions& Options, int32& Index, TSharedPtr<SComboBox<TSharedPtr<FString>>>& OutCombo, TFunction<void()> OnChanged)
{
	FOptions* OptionsPtr = &Options;
	int32* IndexPtr = &Index;
	return SAssignNew(OutCombo, SComboBox<TSharedPtr<FString>>)
		.OptionsSource(OptionsPtr)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString())); })
		.OnSelectionChanged_Lambda([OptionsPtr, IndexPtr, OnChanged](TSharedPtr<FString> Item, ESelectInfo::Type Type)
		{
			const int32 NewIndex = OptionsPtr->IndexOfByKey(Item);
			if (NewIndex != INDEX_NONE && Type != ESelectInfo::Direct)
			{
				*IndexPtr = NewIndex;
				if (OnChanged)
				{
					OnChanged();
				}
			}
		})
		[
			SNew(STextBlock).Text_Lambda([OptionsPtr, IndexPtr]()
			{
				return OptionsPtr->IsValidIndex(*IndexPtr) ? FText::FromString(*(*OptionsPtr)[*IndexPtr]) : LOCTEXT("None", "-");
			})
		];
}

TSharedRef<SWidget> SGSTN_MotionToolsPanel::MakeSlider(const FText& Label, const FText& Tooltip, float& Value, float Min, float Max, float Delta)
{
	float* ValuePtr = &Value;
	return SNew(SHorizontalBox)
		.ToolTipText(Tooltip)
		+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center).Padding(0, 0, 6, 0)[SNew(STextBlock).Text(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.65f)
		[
			SNew(SSpinBox<float>)
			.MinValue(Min).MaxValue(Max).MinSliderValue(Min).MaxSliderValue(Max).Delta(Delta)
			.Value_Lambda([ValuePtr]() { return *ValuePtr; })
			.OnValueChanged_Lambda([ValuePtr](float NewValue) { *ValuePtr = NewValue; })
			.OnValueCommitted_Lambda([this, ValuePtr](float NewValue, ETextCommit::Type) { *ValuePtr = NewValue; OnSliderReleased(); })
			.OnEndSliderMovement_Lambda([this, ValuePtr](float NewValue) { *ValuePtr = NewValue; OnSliderReleased(); })
		];
}

TSharedRef<SWidget> SGSTN_MotionToolsPanel::MakeCheck(const FText& Label, const FText& Tooltip, bool& Value)
{
	bool* ValuePtr = &Value;
	return SNew(SCheckBox)
		.ToolTipText(Tooltip)
		.IsChecked_Lambda([ValuePtr]() { return *ValuePtr ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([ValuePtr](ECheckBoxState State) { *ValuePtr = State == ECheckBoxState::Checked; })
		[
			SNew(STextBlock).Text(Label)
		];
}

TSharedRef<FJsonObject> SGSTN_MotionToolsPanel::MakeRequest(const FString& Op) const
{
	TSharedRef<FJsonObject> Request = MakeShared<FJsonObject>();
	Request->SetStringField(TEXT("op"), Op);
	return Request;
}

void SGSTN_MotionToolsPanel::AddChainFields(const TSharedRef<FJsonObject>& Request) const
{
	const FString ChainId = ChainIds.IsValidIndex(ChainIndex) ? ChainIds[ChainIndex] : FString();
	if (ChainId.IsEmpty())
	{
		Request->SetBoolField(TEXT("use_selection"), true);
	}
	else
	{
		Request->SetStringField(TEXT("chain"), ChainId);
	}
}

void SGSTN_MotionToolsPanel::Refresh()
{
	const TSharedRef<FJsonObject> Reply = GSTNMotionToolsPython::Call(MakeRequest(TEXT("state")));
	const FString PreviousChain = ChainIds.IsValidIndex(ChainIndex) ? ChainIds[ChainIndex] : FString();

	// Presets (overlap, easing) come from Python so the panel never disagrees with what agents use.
	const TSharedPtr<FJsonObject>* PresetsObj = nullptr;
	if (Reply->TryGetObjectField(TEXT("presets"), PresetsObj))
	{
		Presets.Reset();
		PresetOptions.Reset();
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if ((*PresetsObj)->TryGetArrayField(TEXT("overlap"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FOverlapPreset& P = Presets.AddDefaulted_GetRef();
				P.Id = O->GetStringField(TEXT("id"));
				P.Delay = static_cast<float>(O->GetNumberField(TEXT("delay")));
				P.Softness = static_cast<float>(O->GetNumberField(TEXT("softness")));
				P.Bounce = static_cast<float>(O->GetNumberField(TEXT("bounce")));
				P.Mode = O->GetStringField(TEXT("mode"));
				PresetOptions.Add(MakeShared<FString>(O->GetStringField(TEXT("label"))));
			}
		}
		EasingIds.Reset();
		EasingOptions.Reset();
		if ((*PresetsObj)->TryGetArrayField(TEXT("easing"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				EasingIds.Add(O->GetStringField(TEXT("id")));
				EasingOptions.Add(MakeShared<FString>(O->GetStringField(TEXT("label"))));
			}
		}
		PresetIndex = FMath::Clamp(PresetIndex, 0, FMath::Max(0, PresetOptions.Num() - 1));
		EasingIndex = FMath::Clamp(EasingIndex, 0, FMath::Max(0, EasingOptions.Num() - 1));
	}

	// Chains: only those the rig has, then "selection".
	ChainIds.Reset();
	ChainOptions.Reset();
	const TSharedPtr<FJsonObject>* Chains = nullptr;
	if (Reply->TryGetObjectField(TEXT("chains"), Chains))
	{
		const TArray<TSharedPtr<FJsonValue>>* AllChains = nullptr;
		if (Reply->TryGetObjectField(TEXT("presets"), PresetsObj) && (*PresetsObj)->TryGetArrayField(TEXT("chains"), AllChains))
		{
			for (const TSharedPtr<FJsonValue>& V : *AllChains)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				const FString Id = O->GetStringField(TEXT("id"));
				const TArray<TSharedPtr<FJsonValue>>* Controls = nullptr;
				if ((*Chains)->TryGetArrayField(Id, Controls))
				{
					ChainIds.Add(Id);
					ChainOptions.Add(MakeShared<FString>(FString::Printf(TEXT("%s (%d)"), *O->GetStringField(TEXT("label")), Controls->Num())));
				}
			}
		}
	}
	ChainIds.Add(FString());
	ChainOptions.Add(MakeShared<FString>(TEXT("Selezione nel viewport")));
	ChainIndex = FMath::Max(0, ChainIds.IndexOfByKey(PreviousChain));

	for (const TSharedPtr<SComboBox<TSharedPtr<FString>>>& Combo : {ChainCombo, PresetCombo, ModeCombo, EasingCombo})
	{
		if (Combo.IsValid())
		{
			Combo->RefreshOptions();
		}
	}

	FString Sequence;
	if (Reply->TryGetStringField(TEXT("sequence"), Sequence))
	{
		SequenceText = FText::FromString(Sequence);
		FString Layers;
		const TSharedPtr<FJsonObject>* LayerObj = nullptr;
		if (Reply->TryGetObjectField(TEXT("layers"), LayerObj))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*LayerObj)->Values)
			{
				Layers += FString::Printf(TEXT("%s: %s   "), *Pair.Key, Pair.Value->AsBool() ? TEXT("c'è") : TEXT("-"));
			}
		}
		LayersText = FText::FromString(TEXT("Layer  ") + Layers);
		double Playhead = 0.0;
		if (Reply->TryGetNumberField(TEXT("playhead"), Playhead))
		{
			EaseStart = static_cast<float>(Playhead);
			EaseEnd = static_cast<float>(Playhead) + 6.f;
		}
	}
	else
	{
		FString Message;
		Reply->TryGetStringField(TEXT("message"), Message);
		SequenceText = FText::FromString(Message.IsEmpty() ? TEXT("Nessuna sequenza aperta") : Message);
		LayersText = FText::GetEmpty();
	}
	ShowReply(Reply);
	if (bLastOk && !Sequence.IsEmpty())
	{
		StatusText = LOCTEXT("Ready", "Pronto. Scegli catena e preset, poi Applica overlap.");
	}
}

void SGSTN_MotionToolsPanel::ApplyPreset()
{
	if (!Presets.IsValidIndex(PresetIndex))
	{
		return;
	}
	const FOverlapPreset& P = Presets[PresetIndex];
	Delay = P.Delay;
	Softness = P.Softness;
	Bounce = P.Bounce;
	ModeIndex = FMath::Max(0, ModeIds.IndexOfByKey(P.Mode));
	if (bLive)
	{
		ApplyOverlap();
	}
}

void SGSTN_MotionToolsPanel::ApplyOverlap()
{
	TSharedRef<FJsonObject> Request = MakeRequest(TEXT("overlap"));
	AddChainFields(Request);
	Request->SetStringField(TEXT("preset"), Presets.IsValidIndex(PresetIndex) ? Presets[PresetIndex].Id : TEXT("leggero"));
	Request->SetStringField(TEXT("mode"), ModeIds.IsValidIndex(ModeIndex) ? ModeIds[ModeIndex] : TEXT("spring"));
	Request->SetNumberField(TEXT("delay"), Delay);
	Request->SetNumberField(TEXT("softness"), Softness);
	Request->SetNumberField(TEXT("bounce"), Bounce);
	Request->SetNumberField(TEXT("weight"), Weight);
	Request->SetStringField(TEXT("protect"), bProtectKeys ? TEXT("keys") : TEXT("ends"));
	Request->SetStringField(TEXT("key_frames"), KeyFramesText);
	Request->SetBoolField(TEXT("floor"), bFloor);
	Request->SetNumberField(TEXT("floor_z"), FloorZ);
	const TSharedRef<FJsonObject> Reply = GSTNMotionToolsPython::Call(Request);
	ShowReply(Reply);
}

void SGSTN_MotionToolsPanel::RemoveChain()
{
	TSharedRef<FJsonObject> Request = MakeRequest(TEXT("remove"));
	Request->SetStringField(TEXT("layer"), TEXT("Overlap"));
	AddChainFields(Request);
	ShowReply(GSTNMotionToolsPython::Call(Request));
}

void SGSTN_MotionToolsPanel::SetLayerActive(const FString& Layer, bool bActive)
{
	TSharedRef<FJsonObject> Request = MakeRequest(TEXT("layer"));
	Request->SetStringField(TEXT("layer"), Layer);
	Request->SetBoolField(TEXT("active"), bActive);
	ShowReply(GSTNMotionToolsPython::Call(Request));
}

void SGSTN_MotionToolsPanel::RemoveLayer(const FString& Layer)
{
	TSharedRef<FJsonObject> Request = MakeRequest(TEXT("remove"));
	Request->SetStringField(TEXT("layer"), Layer);
	ShowReply(GSTNMotionToolsPython::Call(Request));
}

void SGSTN_MotionToolsPanel::ApplyEasing(bool bAroundPlayhead)
{
	TSharedRef<FJsonObject> Request = MakeRequest(bAroundPlayhead ? TEXT("easing_around_playhead") : TEXT("easing"));
	Request->SetStringField(TEXT("preset"), EasingIds.IsValidIndex(EasingIndex) ? EasingIds[EasingIndex] : TEXT("morbido"));
	Request->SetNumberField(TEXT("intensity"), EaseIntensity);
	Request->SetNumberField(TEXT("start"), FMath::RoundToInt(EaseStart));
	Request->SetNumberField(TEXT("end"), FMath::RoundToInt(EaseEnd));
	ShowReply(GSTNMotionToolsPython::Call(Request));
}

void SGSTN_MotionToolsPanel::OnSliderReleased()
{
	if (bLive)
	{
		ApplyOverlap();
	}
}

void SGSTN_MotionToolsPanel::ShowReply(const TSharedRef<FJsonObject>& Reply)
{
	bool bOk = false;
	Reply->TryGetBoolField(TEXT("ok"), bOk);
	bLastOk = bOk;
	if (!bOk)
	{
		FString Error;
		Reply->TryGetStringField(TEXT("error"), Error);
		StatusText = FText::FromString(TEXT("Errore: ") + Error);
		return;
	}
	FString Op;
	Reply->TryGetStringField(TEXT("op"), Op);
	double Peak = 0.0;
	Reply->TryGetNumberField(TEXT("peak_delta"), Peak);
	if (Op == TEXT("overlap"))
	{
		FString Chain;
		Reply->TryGetStringField(TEXT("chain"), Chain);
		FString Text = FString::Printf(TEXT("Overlap su %s scritto. Scarto massimo %.1f gradi."), *Chain, Peak);
		const TSharedPtr<FJsonObject>* Floor = nullptr;
		if (Reply->TryGetObjectField(TEXT("floor"), Floor))
		{
			Text += FString::Printf(TEXT(" Pavimento: %d frame ridotti, resto %.1f cm."),
				static_cast<int32>((*Floor)->GetNumberField(TEXT("frames_reduced"))), (*Floor)->GetNumberField(TEXT("residual_cm")));
		}
		const TArray<TSharedPtr<FJsonValue>>* Warnings = nullptr;
		if (Reply->TryGetArrayField(TEXT("warnings"), Warnings))
		{
			for (const TSharedPtr<FJsonValue>& W : *Warnings)
			{
				Text += TEXT("\nAttenzione: ") + W->AsString();
			}
		}
		StatusText = FText::FromString(Text);
	}
	else if (Op == TEXT("easing"))
	{
		const TArray<TSharedPtr<FJsonValue>>* Frames = nullptr;
		FString Range;
		if (Reply->TryGetArrayField(TEXT("frames"), Frames) && Frames->Num() == 2)
		{
			Range = FString::Printf(TEXT("%d-%d"), static_cast<int32>((*Frames)[0]->AsNumber()), static_cast<int32>((*Frames)[1]->AsNumber()));
		}
		StatusText = FText::FromString(FString::Printf(TEXT("Easing scritto sui frame %s. Scarto massimo %.1f gradi."), *Range, Peak));
	}
	else if (Op == TEXT("remove") || Op == TEXT("set_layer"))
	{
		StatusText = LOCTEXT("Done", "Fatto.");
	}
}

#undef LOCTEXT_NAMESPACE
