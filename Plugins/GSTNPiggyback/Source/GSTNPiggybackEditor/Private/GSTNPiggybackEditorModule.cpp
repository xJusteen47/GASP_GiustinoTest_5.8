// Piggyback, editor part (editor only; the runtime never depends on it).

#include "GSTNPiggybackEditorModule.h"

#include "GSTN_PiggybackLinkDebug.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNPiggybackEditor);

void FGSTNPiggybackEditorModule::StartupModule()
{
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&FGSTN_PiggybackLinkDebug::Tick));
}

void FGSTNPiggybackEditorModule::ShutdownModule()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	FGSTN_PiggybackLinkDebug::Reset();
}

IMPLEMENT_MODULE(FGSTNPiggybackEditorModule, GSTNPiggybackEditor)
