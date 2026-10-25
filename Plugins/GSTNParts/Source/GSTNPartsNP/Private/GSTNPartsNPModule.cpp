// Destructible parts, Network Prediction half.

#include "GSTN_PartsFragmentRegistration.h"

#include "Modules/ModuleManager.h"
#include "State/GSTN_PartsFragment.h"

bool FGSTN_PartsFragmentRegistration::Register()
{
	return FGSTN_SyncFragmentRegistry::Register<FGSTN_PartsFragment>(GSTNPartsFeatureIds::Parts);
}

void FGSTN_PartsFragmentRegistration::Unregister()
{
	FGSTN_SyncFragmentRegistry::Unregister(GSTNPartsFeatureIds::Parts);
}

/**
 * Registers the parts fragment before the first simulation tick, so a machine that has the plugin reads and writes the fragment and
 * a machine without it skips the bytes. An entity gets a fragment only when it is given a part set (P7), so a project that enables
 * the plugin and attaches no set pays nothing on the wire.
 */
class FGSTNPartsNPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_PartsFragmentRegistration::Register();
	}

	virtual void ShutdownModule() override
	{
		FGSTN_PartsFragmentRegistration::Unregister();
	}
};

IMPLEMENT_MODULE(FGSTNPartsNPModule, GSTNPartsNP)
