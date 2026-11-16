// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelineDetails.h"

#include "Timeline/GSTN_TimelineSelectionProxy.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "Timeline/Widgets/GSTN_TimelineWidgetCommon.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Styling/AppStyle.h"
#include "Timeline/GSTN_HitboxKeyedAccess.h"
#include "Timeline/GSTN_HitboxOutline.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
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
	View = MakeView(PropertyEditor);
	HitboxView = MakeView(PropertyEditor);
	KeyView = MakeView(PropertyEditor);

	// The group's view hides its Hitboxes array while one hitbox has views of its own; the hitbox's view hides its Keys (the timeline and the Key view hold them)
	View->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([this](const FPropertyAndParent& PropertyAndParent)
	{
		return !(Proxy->GetFocusedSpec() != nullptr && PropertyAndParent.ParentProperties.IsEmpty()
			&& PropertyAndParent.Property.GetFName() == GET_MEMBER_NAME_CHECKED(FGSTN_TimelineTrack_Hitbox, Hitboxes));
	}));
	HitboxView->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& PropertyAndParent)
	{
		return !(PropertyAndParent.ParentProperties.IsEmpty() && PropertyAndParent.Property.GetFName() == GET_MEMBER_NAME_CHECKED(FGSTN_HitboxSpec, Keys));
	}));

	const auto SectionHeader = [](TFunction<FText()> Label, FName Icon) -> TSharedRef<SWidget>
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
			.Padding(FMargin(6.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[
					SNew(SImage).Image(FAppStyle::Get().GetBrush(Icon)).ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle")).Text_Lambda(Label)
				]
			];
	};
	const auto HitboxVisibility = [this]() { return Proxy->GetFocusedSpec() != nullptr ? EVisibility::Visible : EVisibility::Collapsed; };
	const auto KeyVisibility = [this]() { const FGSTN_HitboxSpec* Spec = Proxy->GetFocusedSpec(); return Spec != nullptr && Spec->Keys.IsValidIndex(Proxy->GetFocusKey()) ? EVisibility::Visible : EVisibility::Collapsed; };

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
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.35f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SectionHeader([this]() { return LOCTEXT("DetailsGroupSection", "Track / Group"); }, "Icons.FolderOpen")
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						View->GetWidget().ToSharedRef()
					]
				]
				+ SSplitter::Slot()
				.Value(0.45f)
				[
					SNew(SVerticalBox)
					.Visibility_Lambda(HitboxVisibility)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SectionHeader([this]() -> FText
						{
							const TSharedPtr<FGSTN_TimelineEditorModel> M = Model;
							const FGSTN_CombatTimelineData* Data = M->GetData();
							return Data != nullptr && Proxy->GetFocusedSpec() != nullptr
								? FText::Format(LOCTEXT("DetailsHitboxSection", "Hitbox: {0}"), GSTNHitboxOutline::GetName(*Data, FGSTN_HitboxOutlineItem(Proxy->GetTrackIndex(), Proxy->GetFocusSpec())))
								: FText::GetEmpty();
						}, "Icons.Target")
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						HitboxView->GetWidget().ToSharedRef()
					]
				]
				+ SSplitter::Slot()
				.Value(0.2f)
				[
					SNew(SVerticalBox)
					.Visibility_Lambda(KeyVisibility)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SectionHeader([this]() -> FText
						{
							const FGSTN_HitboxSpec* Spec = Proxy->GetFocusedSpec();
							const FGSTN_TimelineTrack* Track = Proxy->GetTrackCopy();
							if (Spec == nullptr || Track == nullptr || !Spec->Keys.IsValidIndex(Proxy->GetFocusKey()))
							{
								return FText::GetEmpty();
							}
							// The key stores its frame from the start of the group's window; the timeline shows the absolute frame
							const int32 Relative = Spec->Keys[Proxy->GetFocusKey()].Frame;
							return FText::Format(LOCTEXT("DetailsKeySection", "Key {0} of {1}: timeline frame {2} (window frame {3})"),
								FText::AsNumber(Proxy->GetFocusKey() + 1), FText::AsNumber(Spec->Keys.Num()), FText::AsNumber(Track->StartFrame + Relative), FText::AsNumber(Relative));
						}, "Icons.Edit")
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						KeyView->GetWidget().ToSharedRef()
					]
				]
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

TSharedRef<IStructureDetailsView> SGSTN_TimelineDetails::MakeView(FPropertyEditorModule& PropertyEditor)
{
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.bLockable = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	TSharedRef<IStructureDetailsView> NewView = PropertyEditor.CreateStructureDetailView(DetailsArgs, FStructureDetailsViewArgs(), nullptr);
	NewView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SGSTN_TimelineDetails::HandleFinishedChanging);
	return NewView;
}

void SGSTN_TimelineDetails::ResolveFocus(int32 TrackIndex, int32& OutSpec, int32& OutKey) const
{
	OutSpec = INDEX_NONE;
	OutKey = INDEX_NONE;
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	const FGSTN_TimelineTrack_Hitbox* Hitbox = Data != nullptr ? Data->GetHitboxTrack(TrackIndex) : nullptr;
	if (Hitbox == nullptr)
	{
		return;
	}
	for (const FGSTN_Selection& Item : Model->GetSelection())
	{
		if (Item.TrackIndex == TrackIndex && Item.IsKey() && FGSTN_HitboxKeyedAccess::Locate(*Hitbox, Item.KeyIndex, OutSpec, OutKey))
		{
			return;
		}
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

	int32 WantedSpec = INDEX_NONE;
	int32 WantedKey = INDEX_NONE;
	ResolveFocus(Wanted, WantedSpec, WantedKey);

	if (!Proxy->IsBound() || Proxy->GetTrackIndex() != Wanted)
	{
		Proxy->Bind(Model, Wanted);
		Proxy->SetFocus(WantedSpec, WantedKey);
		ShowProxy();
	}
	else if (Proxy->Pull() || Proxy->GetFocusSpec() != WantedSpec || Proxy->GetFocusKey() != WantedKey)
	{
		Proxy->SetFocus(WantedSpec, WantedKey);
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
	// The hitbox and key views point into the copy, so they are re-pointed whenever the copy is
	View->SetStructureData(Proxy->MakeStructData());
	HitboxView->SetStructureData(Proxy->MakeSpecStructData());
	KeyView->SetStructureData(Proxy->MakeKeyStructData());
	// The group's view hides or shows its Hitboxes array with the focus
	View->GetDetailsView()->ForceRefresh();
}

#undef LOCTEXT_NAMESPACE
