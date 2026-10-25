// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "GSTNCombatEditorModule.h"

#include "Bake/GSTN_BakeAll.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Timeline/Details/GSTN_HitDataCustomization.h"
#include "Timeline/Details/GSTN_TimelineDataCustomization.h"
#include "Timeline/GSTN_TimelineEditorCommands.h"
#include "Timeline/GSTN_TrackEditors.h"
#include "Validation/GSTN_ValidationMessageLog.h"
#include "Validation/GSTN_ValidationTab.h"

DEFINE_LOG_CATEGORY(LogGSTNCombatEditor);

void FGSTN_CombatEditorModule::StartupModule()
{
	// The commands first: the panel reads them when it is built. Commands need Slate's binding manager, which a commandlet has too
	FGSTN_TimelineEditorCommands::Register();

	// The track editors of the M3 kinds. A plugin that adds a track registers its own the same way, from its own startup. The timeline
	// asset's definition (UGSTN_AssetDefinition_CombatTimeline) and the toolkit register by reflection and on demand: nothing to do here
	GSTNTrackEditors::RegisterBuiltIns();

	// The Message Log tab the findings of the Validate button go to. The validators themselves need no registration: Data Validation finds
	// UEditorValidatorBase classes by reflection when the editor starts
	GSTNValidationLog::RegisterListing();

	// Window > GSTN Combat > Validation, and gstn.Combat.ValidateAll (E11)
	GSTNValidationTab::Register();

	// gstn.Combat.BakeAll (E8)
	GSTNBakeAll::RegisterConsoleCommand();

	// The Open Timeline Editor row of an FGSTN_CombatTimelineData, in the Class Defaults of an ability Blueprint and on a timeline asset (inline mode, E13)
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditor.RegisterCustomPropertyTypeLayout(FGSTN_TimelineDataCustomization::GetStructName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGSTN_TimelineDataCustomization::MakeInstance));
	// The groups, the angle dial and the knockback preview of a hit (E12), wherever an FGSTN_HitData is edited
	PropertyEditor.RegisterCustomPropertyTypeLayout(FGSTN_HitDataCustomization::GetStructName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGSTN_HitDataCustomization::MakeInstance));
	UE_LOG(LogGSTNCombatEditor, Verbose, TEXT("GSTN Combat Editor module started"));
}

void FGSTN_CombatEditorModule::ShutdownModule()
{
	// Reverse order of startup
	if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
	{
		PropertyEditor->UnregisterCustomPropertyTypeLayout(FGSTN_HitDataCustomization::GetStructName());
		PropertyEditor->UnregisterCustomPropertyTypeLayout(FGSTN_TimelineDataCustomization::GetStructName());
	}
	GSTNBakeAll::UnregisterConsoleCommand();
	GSTNValidationTab::Unregister();
	GSTNValidationLog::UnregisterListing();
	GSTNTrackEditors::UnregisterBuiltIns();
	FGSTN_TimelineEditorCommands::Unregister();
}

FGSTN_CombatEditorModule* FGSTN_CombatEditorModule::Get()
{
	return FModuleManager::GetModulePtr<FGSTN_CombatEditorModule>(TEXT("GSTNCombatEditor"));
}

IMPLEMENT_MODULE(FGSTN_CombatEditorModule, GSTNCombatEditor)
