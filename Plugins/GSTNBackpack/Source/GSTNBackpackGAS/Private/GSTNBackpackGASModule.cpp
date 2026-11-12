// Backpacks, ability layer: registers the loadout extension and the attachment provider.

#include "GSTN_BackpackLoadout.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Modules/ModuleManager.h"

class FGSTNBackpackGASModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LoadoutExtension Extension;
		Extension.Priority = FGSTN_BackpackLoadout::ExtensionPriority;
		Extension.ContributeCompileInput = &FGSTN_BackpackLoadout::ContributeCompileInput;
		Extension.ContributeRulesView = &FGSTN_BackpackLoadout::ContributeRulesView;
		Extension.OnGranted = &FGSTN_BackpackLoadout::OnGranted;
		Extension.OnCleared = &FGSTN_BackpackLoadout::OnCleared;
		FGSTN_LoadoutExtensionRegistry::Register(FGSTN_BackpackLoadout::ExtensionId, MoveTemp(Extension));
		FGSTN_AttachmentProviderRegistry::Register(FGSTN_BackpackLoadout::AttachmentProviderId, MakeShared<FGSTN_BackpackAttachmentProvider>(), 100);
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionRegistry::Unregister(FGSTN_BackpackLoadout::ExtensionId);
		FGSTN_AttachmentProviderRegistry::Unregister(FGSTN_BackpackLoadout::AttachmentProviderId);
	}
};

IMPLEMENT_MODULE(FGSTNBackpackGASModule, GSTNBackpackGAS);
