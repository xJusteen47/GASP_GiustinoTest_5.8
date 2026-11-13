// Piggyback, editor part (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"

GSTNPIGGYBACKEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogGSTNPiggybackEditor, Log, All);

/** Starts the PIE link debug ticker (GSTN_PiggybackLinkDebug.h) and stops it on shutdown */
class FGSTNPiggybackEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FTSTicker::FDelegateHandle TickerHandle;
};
