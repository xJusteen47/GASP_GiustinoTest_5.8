// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "GSTNCombatEditorModule.h"

#include "Modules/ModuleManager.h"
#include "Timeline/GSTN_TimelineEditorCommands.h"
#include "Timeline/GSTN_TimelinePanelTest.h"

DEFINE_LOG_CATEGORY(LogGSTNCombatEditor);

void FGSTN_CombatEditorModule::StartupModule()
{
	// The commands first: the panel reads them when it is built. Commands need Slate's binding manager, which a commandlet has too
	FGSTN_TimelineEditorCommands::Register();
	GSTNTimelinePanelTest::Register();
	UE_LOG(LogGSTNCombatEditor, Verbose, TEXT("GSTN Combat Editor module started"));
}

void FGSTN_CombatEditorModule::ShutdownModule()
{
	// Reverse order of startup
	GSTNTimelinePanelTest::Unregister();
	FGSTN_TimelineEditorCommands::Unregister();
}

FGSTN_CombatEditorModule* FGSTN_CombatEditorModule::Get()
{
	return FModuleManager::GetModulePtr<FGSTN_CombatEditorModule>(TEXT("GSTNCombatEditor"));
}

IMPLEMENT_MODULE(FGSTN_CombatEditorModule, GSTNCombatEditor)
