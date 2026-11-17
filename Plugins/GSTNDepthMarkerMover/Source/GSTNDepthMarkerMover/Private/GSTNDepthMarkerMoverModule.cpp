// Mover adapter module of the depth marker: registers the adapter with the runtime's registry. Presentation only.

#include "GSTN_DepthSubject.h"
#include "GSTN_DepthSubjectAdapter_Mover.h"
#include "Modules/ModuleManager.h"

class FGSTNDepthMarkerMoverModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_DepthAdapterRegistry::Register(UGSTN_DepthSubjectAdapter_Mover::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		FGSTN_DepthAdapterRegistry::Unregister(UGSTN_DepthSubjectAdapter_Mover::StaticClass());
	}
};

IMPLEMENT_MODULE(FGSTNDepthMarkerMoverModule, GSTNDepthMarkerMover);
