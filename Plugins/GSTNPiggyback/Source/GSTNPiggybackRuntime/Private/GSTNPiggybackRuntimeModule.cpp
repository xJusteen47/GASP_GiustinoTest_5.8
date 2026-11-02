// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_LinkRegistry.h"
#include "GSTN_MountLinkType.h"
#include "Engine/AssetManager.h"
#include "GSTN_MountLoadout.h"
#include "GSTN_PiggybackContentLibrary.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the plugin with GSTNLink and GSTNCombat from outside, through their public API only:
 * - its break reasons (Dismount, Drop, ...) before any link registry is built, so their indices follow EGSTN_LinkBreakReason::FirstPlugin by name;
 * - the mount's loadout extension (bindings and disallowed commands of a kit's mount data);
 * - the mount link type, once the engine exists (the registry is an engine subsystem): code-added definitions survive the registry's rebuilds.
 * Disabled, none of this happens and grabs, latches and every binding are as without the plugin.
 */
class FGSTNPiggybackRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		for (const FName Reason : FGSTN_MountLinkType::GetBreakReasonNames())
		{
			UGSTN_LinkRegistry::RegisterPluginReason(Reason);
		}

		FGSTN_LoadoutExtension Loadout;
		Loadout.ContributeCompileInput = &FGSTN_MountLoadout::Contribute;
		FGSTN_LoadoutExtensionRegistry::Register(FGSTN_MountLoadout::GetLoadoutExtensionId(), MoveTemp(Loadout));

		// The plugin's test kits (Content/Test) are found by their primary asset id once the asset manager exists
		UAssetManager::CallOrRegister_OnAssetManagerCreated(FSimpleMulticastDelegate::FDelegate::CreateLambda([]() { UGSTN_PiggybackContentLibrary::ScanPluginKits(); }));

		if (GEngine)
		{
			FGSTN_MountLinkType::Register();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddStatic(&FGSTN_MountLinkType::Register);
		}
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		FGSTN_MountLinkType::Unregister();
		FGSTN_LoadoutExtensionRegistry::Unregister(FGSTN_MountLoadout::GetLoadoutExtensionId());
		for (const FName Reason : FGSTN_MountLinkType::GetBreakReasonNames())
		{
			UGSTN_LinkRegistry::UnregisterPluginReason(Reason);
		}
	}

private:
	FDelegateHandle PostEngineInitHandle;
};

IMPLEMENT_MODULE(FGSTNPiggybackRuntimeModule, GSTNPiggybackRuntime)
