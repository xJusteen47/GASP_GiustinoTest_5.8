// Spin and drill plugin, module GSTNSpinDrillGAS.

#include "GSTN_DrillLinkType.h"
#include "GSTN_SpinDrillKitExtension.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Misc/CoreDelegates.h"
#include "Engine/Engine.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the plugin's loadout extension with the combat GAS layer: a kit's FGSTN_SpinDrillKitExtension becomes the pawn's attached tuning
 * wherever the loadout is compiled. Also adds the drill latch link type (FGSTN_DrillLinkType) to GSTNLink's registry once the engine exists
 * (the registry is an engine subsystem; code-added definitions survive its rebuilds). A build with the plugin disabled registers neither, so the
 * loadout is exactly the combat plugin's and the link table GSTNLink's own.
 */
class FGSTNSpinDrillGASModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LoadoutExtension Extension;
		Extension.ContributeCompileInput = &FGSTN_SpinDrillLoadout::AttachFromLoadout;
		FGSTN_LoadoutExtensionRegistry::Register(FGSTN_SpinDrillLoadout::GetLoadoutExtensionId(), MoveTemp(Extension));

		if (GEngine)
		{
			FGSTN_DrillLinkType::Register();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddStatic(&FGSTN_DrillLinkType::Register);
		}
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		FGSTN_DrillLinkType::Unregister();
		FGSTN_LoadoutExtensionRegistry::Unregister(FGSTN_SpinDrillLoadout::GetLoadoutExtensionId());
	}

private:
	FDelegateHandle PostEngineInitHandle;
};

IMPLEMENT_MODULE(FGSTNSpinDrillGASModule, GSTNSpinDrillGAS)
