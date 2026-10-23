// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "GSTNCombatEditorModule.h"

#include "Modules/ModuleManager.h"
#include "Timeline/GSTN_TimelineEditorCommands.h"
#include "Timeline/GSTN_TrackEditors.h"
#include "Validation/GSTN_ValidationMessageLog.h"

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
	UE_LOG(LogGSTNCombatEditor, Verbose, TEXT("GSTN Combat Editor module started"));
}

void FGSTN_CombatEditorModule::ShutdownModule()
{
	// Reverse order of startup
	GSTNValidationLog::UnregisterListing();
	GSTNTrackEditors::UnregisterBuiltIns();
	FGSTN_TimelineEditorCommands::Unregister();
}

FGSTN_CombatEditorModule* FGSTN_CombatEditorModule::Get()
{
	return FModuleManager::GetModulePtr<FGSTN_CombatEditorModule>(TEXT("GSTNCombatEditor"));
}

IMPLEMENT_MODULE(FGSTN_CombatEditorModule, GSTNCombatEditor)
