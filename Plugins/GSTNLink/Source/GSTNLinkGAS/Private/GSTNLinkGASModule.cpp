// Links, ability-system half (no Network Prediction, no Mover).

#include "Data/GSTN_LinkKitExtension.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkTags.h"
#include "Modules/ModuleManager.h"

/**
 * The link registry is an engine subsystem that builds itself once the asset manager finished its first scan, and plugins append their
 * break reasons through UGSTN_LinkRegistry::RegisterPluginReason from their own startup. The one startup job here is the grab's interact
 * offer (M7 G1): the grab's assets predate the definition's InteractOffer field, so the plugin registers the offer they always had by code
 * (Interact.Target.Grab, the default meaning priority 100, kits with a link extension); a grab asset that names an offer itself wins.
 */
class FGSTNLinkGASModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LinkInteractOfferDef Grab;
		Grab.Meaning = GSTNLinkTags::Interact_Target_Grab;
		Grab.Priority = 100;
		Grab.RequiredKitExtension = FGSTN_LinkKitExtension::StaticStruct();
		UGSTN_LinkRegistry::RegisterInteractOffer(GSTNLinkTags::Type_Grab.GetTag().GetTagName(), Grab);
	}

	virtual void ShutdownModule() override
	{
		UGSTN_LinkRegistry::UnregisterInteractOffer(GSTNLinkTags::Type_Grab.GetTag().GetTagName());
	}
};

IMPLEMENT_MODULE(FGSTNLinkGASModule, GSTNLinkGAS)
