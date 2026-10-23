// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "GSTNCombatEditorModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNCombatEditor);

void FGSTN_CombatEditorModule::StartupModule()
{
	UE_LOG(LogGSTNCombatEditor, Verbose, TEXT("GSTN Combat Editor module started"));
}

void FGSTN_CombatEditorModule::ShutdownModule()
{
}

FGSTN_CombatEditorModule* FGSTN_CombatEditorModule::Get()
{
	return FModuleManager::GetModulePtr<FGSTN_CombatEditorModule>(TEXT("GSTNCombatEditor"));
}

IMPLEMENT_MODULE(FGSTN_CombatEditorModule, GSTNCombatEditor)
