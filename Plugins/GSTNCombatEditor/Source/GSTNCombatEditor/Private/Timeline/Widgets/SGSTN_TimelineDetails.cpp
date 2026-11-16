// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelineDetails.h"

#include "Timeline/GSTN_TimelineSelectionProxy.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "Timeline/Widgets/GSTN_TimelineWidgetCommon.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

int32 SGSTN_TimelineDetails::ResolveShownTrack(const TArray<FGSTN_Selection>& Selection)
{
	int32 Shown = INDEX_NONE;
	for (const FGSTN_Selection& Item : Selection)
	{
		if (Shown != INDEX_NONE && Shown != Item.TrackIndex)
		{
			return INDEX_NONE;
		}
		Shown = Item.TrackIndex;
	}
	return Shown;
}

void SGSTN_TimelineDetails::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());

	Proxy.Reset(NewObject<UGSTN_TimelineSelectionProxy>(GetTransientPackage()));

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.bLockable = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	View = PropertyEditor.CreateStructureDetailView(DetailsArgs, FStructureDetailsViewArgs(), nullptr);
	View->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SGSTN_TimelineDetails::HandleFinishedChanging);

	ChildSlot
	[
		SNew(SVerticalBox)

		// Which track this is, and what it says (the same words as its bar)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(FMargin(8.f, 4.f))
			.Visibility_Lambda([this]() { return Proxy->IsBound() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					const UScriptStruct* Struct = Proxy->GetTrackStruct();
					const FGSTN_TimelineTrack* Track = Proxy->GetTrackCopy();
					if (Struct == nullptr || Track == nullptr)
					{
						return FText::GetEmpty();
					}
					const TSharedRef<const IGSTN_TrackEditor> Editor = FGSTN_TrackEditorRegistry::Get().FindOrGeneric(Struct);
					return FText::Format(LOCTEXT("DetailsHeader", "{0}: {1}"), Editor->GetDisplayName(), Editor->GetRowLabel(*Track));
				})
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.f, 8.f))
			[
				SNew(STextBlock)
				.Visibility_Lambda([this]() { return Proxy->IsBound() ? EVisibility::Collapsed : EVisibility::Visible; })
				.ColorAndOpacity(FSlateColor(GSTNTimelineWidgets::TextColorDim()))
				.AutoWrapText(true)
				.Text_Lambda([this]() -> FText
				{
					return Model->GetSelection().IsEmpty()
						? LOCTEXT("DetailsNothingSelected", "Select a track to edit its properties.")
						: LOCTEXT("DetailsSeveralSelected", "Select a single track to edit its properties.");
				})
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				View->GetWidget().ToSharedRef()
			]
		]
	];

	DataHandle = Model->OnDataChanged.AddSP(this, &SGSTN_TimelineDetails::Refresh);
	SelectionHandle = Model->OnSelectionChanged.AddSP(this, &SGSTN_TimelineDetails::Refresh);
	Refresh();
}

SGSTN_TimelineDetails::~SGSTN_TimelineDetails()
{
	if (Model.IsValid())
	{
		Model->OnDataChanged.Remove(DataHandle);
		Model->OnSelectionChanged.Remove(SelectionHandle);
	}
	if (Proxy.IsValid())
	{
		// Closes a drag an interactive edit left open
		Proxy->Unbind();
	}
}

void SGSTN_TimelineDetails::Refresh()
{
	const int32 Wanted = ResolveShownTrack(Model->GetSelection());
	if (Wanted == INDEX_NONE)
	{
		if (Proxy->IsBound())
		{
			Proxy->Unbind();
			ShowProxy();
		}
		return;
	}

	if (!Proxy->IsBound() || Proxy->GetTrackIndex() != Wanted)
	{
		Proxy->Bind(Model, Wanted);
		ShowProxy();
	}
	else if (Proxy->Pull())
	{
		ShowProxy();
	}
}

void SGSTN_TimelineDetails::HandleFinishedChanging(const FPropertyChangedEvent& Event)
{
	// The change is written to the track by the model; the data-changed broadcast that follows finds the copy equal and leaves the view alone
	Proxy->Commit(Event.ChangeType);
}

void SGSTN_TimelineDetails::ShowProxy()
{
	View->SetStructureData(Proxy->MakeStructData());
}

#undef LOCTEXT_NAMESPACE
