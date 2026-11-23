#include "GSTN_AnimBlueprintToolset.h"
#include "GSTN_ChooserTableToolset.h"
#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

/** Registers the GSTN toolsets with the ToolsetRegistry, so the editor MCP lists them next to Epic's. */
class FGSTNAnimToolsetsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Plugin discovery does not guarantee that the registry started first.
		FModuleManager::Get().LoadModuleChecked(TEXT("ToolsetRegistry"));
		UToolsetRegistry::RegisterToolsetClass(UGSTN_AnimBlueprintToolset::StaticClass());
		UToolsetRegistry::RegisterToolsetClass(UGSTN_ChooserTableToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		UToolsetRegistry::UnregisterToolsetClass(UGSTN_ChooserTableToolset::StaticClass());
		UToolsetRegistry::UnregisterToolsetClass(UGSTN_AnimBlueprintToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FGSTNAnimToolsetsModule, GSTNAnimToolsets)
