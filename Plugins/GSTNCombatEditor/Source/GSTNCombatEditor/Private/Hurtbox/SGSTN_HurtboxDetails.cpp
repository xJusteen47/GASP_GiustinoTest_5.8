// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Hurtbox/SGSTN_HurtboxDetails.h"

#include "Hurtbox/GSTN_HurtboxEditModel.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "UObject/StructOnScope.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

void SGSTN_HurtboxDetails::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.bLockable = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	View = PropertyEditor.CreateStructureDetailView(DetailsArgs, FStructureDetailsViewArgs(), nullptr);
	View->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SGSTN_HurtboxDetails::HandleFinishedChanging);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.f))
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return ShownIndex == INDEX_NONE ? EVisibility::Visible : EVisibility::Collapsed; })
			.AutoWrapText(true)
			.Text_Lambda([this]() -> FText
			{
				return Model->GetSelection().IsEmpty()
					? LOCTEXT("HurtboxDetailsNothing", "Select a hurtbox to edit its properties.")
					: LOCTEXT("HurtboxDetailsSeveral", "Select a single hurtbox to edit its properties.");
			})
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			View->GetWidget().ToSharedRef()
		]
	];

	ChangedHandle = Model->OnChanged.AddSP(this, &SGSTN_HurtboxDetails::Refresh);
	SelectionHandle = Model->OnSelectionChanged.AddSP(this, &SGSTN_HurtboxDetails::Refresh);
	Refresh();
}

SGSTN_HurtboxDetails::~SGSTN_HurtboxDetails()
{
	if (Model.IsValid())
	{
		Model->OnChanged.Remove(ChangedHandle);
		Model->OnSelectionChanged.Remove(SelectionHandle);
		// A slider released outside the panel must not leave the model's session open
		Model->EndEdit();
	}
}

void SGSTN_HurtboxDetails::Refresh()
{
	if (bCommitting)
	{
		return;
	}
	const int32 Wanted = Model->GetSingleSelection();
	const TArray<FGSTN_Hurtbox>* Hurtboxes = Model->GetHurtboxes();
	if (Wanted == INDEX_NONE || Hurtboxes == nullptr || !Hurtboxes->IsValidIndex(Wanted))
	{
		if (ShownIndex != INDEX_NONE)
		{
			ShownIndex = INDEX_NONE;
			View->SetStructureData(nullptr);
		}
		return;
	}
	*Copy = (*Hurtboxes)[Wanted];
	ShownIndex = Wanted;
	View->SetStructureData(MakeShared<FStructOnScope>(FGSTN_Hurtbox::StaticStruct(), reinterpret_cast<uint8*>(&Copy.Get())));
}

void SGSTN_HurtboxDetails::HandleFinishedChanging(const FPropertyChangedEvent& Event)
{
	if (ShownIndex == INDEX_NONE)
	{
		return;
	}
	const bool bInteractive = Event.ChangeType == EPropertyChangeType::Interactive;
	bCommitting = true;
	if (bInteractive && !Model->IsEditing())
	{
		Model->BeginEdit(LOCTEXT("EditHurtbox", "Edit hurtbox"));
	}
	Model->SetHurtbox(ShownIndex, *Copy, LOCTEXT("EditHurtbox", "Edit hurtbox"));
	if (!bInteractive)
	{
		Model->EndEdit();
	}
	bCommitting = false;
}

#undef LOCTEXT_NAMESPACE
