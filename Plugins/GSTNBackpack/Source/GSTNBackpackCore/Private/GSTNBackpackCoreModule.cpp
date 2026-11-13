// Backpacks, pure layer: registers the loadout extension's wire id so a saved or sent loadout keeps it across a struct rename.

#include "GSTN_BackpackTypes.h"
#include "Modules/ModuleManager.h"

class FGSTNBackpackCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LoadoutExtensionTypes::Register(FGSTN_LoadoutBackpackExt::WireId, FGSTN_LoadoutBackpackExt::StaticStruct());
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionTypes::Unregister(FGSTN_LoadoutBackpackExt::WireId);
	}
};

IMPLEMENT_MODULE(FGSTNBackpackCoreModule, GSTNBackpackCore);
