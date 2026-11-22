// Kit mods, pure layer: registers the loadout extension's wire id so a saved or sent loadout keeps it across a struct rename.

#include "GSTN_KitModTypes.h"
#include "Modules/ModuleManager.h"

class FGSTNKitModsCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LoadoutExtensionTypes::Register(FGSTN_LoadoutModsExt::WireId, FGSTN_LoadoutModsExt::StaticStruct());
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionTypes::Unregister(FGSTN_LoadoutModsExt::WireId);
	}
};

IMPLEMENT_MODULE(FGSTNKitModsCoreModule, GSTNKitModsCore);
