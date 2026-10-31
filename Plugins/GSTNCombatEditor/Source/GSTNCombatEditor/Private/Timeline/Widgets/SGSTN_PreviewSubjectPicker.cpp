// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_PreviewSubjectPicker.h"

#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Preview/GSTN_PreviewController.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

void SGSTN_PreviewSubjectPicker::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	check(Controller.IsValid());
	const TSharedPtr<FGSTN_PreviewController> Control = Controller;

	ChildSlot
	[
		SNew(SComboButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ToolTipText_Lambda([Control]()
		{
			FText Tip = LOCTEXT("SubjectTip", "What the preview shows: a player kit with a skin, or any skeletal mesh (an enemy), plus extra meshes such as a weapon. Stored for this asset on this machine.");
			for (const FText& Problem : Control->GetProblems())
			{
				Tip = FText::Format(LOCTEXT("SubjectTipProblem", "{0}\n\n{1}"), Tip, Problem);
			}
			return Tip;
		})
		.OnGetMenuContent(this, &SGSTN_PreviewSubjectPicker::MakeMenuContent)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Lambda([Control]()
			{
				return FText::Format(LOCTEXT("SubjectButton", "Preview: {0}"), Control->GetSubjectLabel());
			})
		]
	];
}

TSharedRef<SWidget> SGSTN_PreviewSubjectPicker::MakeMenuContent()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	FStructureDetailsViewArgs StructArgs;

	// The view edits the controller's subject in place: nothing is copied, and SubjectEdited applies and stores each change
	const TSharedRef<FStructOnScope> Data = MakeShared<FStructOnScope>(FGSTN_PreviewSubject::StaticStruct(), reinterpret_cast<uint8*>(&Controller->EditSubject()));
	DetailsView = PropertyEditor.CreateStructureDetailView(DetailsArgs, StructArgs, Data);
	DetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(Controller.ToSharedRef(), &FGSTN_PreviewController::SubjectEditedFromDetails);

	return SNew(SBox)
		.WidthOverride(440.f)
		.MaxDesiredHeight(560.f)
		[
			DetailsView->GetWidget().ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
