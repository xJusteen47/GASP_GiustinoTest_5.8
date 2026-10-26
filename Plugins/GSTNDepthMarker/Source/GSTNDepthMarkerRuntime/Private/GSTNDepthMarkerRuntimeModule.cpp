// Depth marker runtime module: registers the built-in adapters. Presentation only.

#include "GSTN_DepthMarkerRuntimeTypes.h"
#include "GSTN_DepthSubject.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNDepthMarker);

class FGSTNDepthMarkerRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// The runtime knows no movement type besides the engine ones; a Mover adapter registers itself from its own plugin
		FGSTN_DepthAdapterRegistry::Register(UGSTN_DepthSubjectAdapter_Actor::StaticClass());
		FGSTN_DepthAdapterRegistry::Register(UGSTN_DepthSubjectAdapter_CharacterMovement::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		FGSTN_DepthAdapterRegistry::Unregister(UGSTN_DepthSubjectAdapter_CharacterMovement::StaticClass());
		FGSTN_DepthAdapterRegistry::Unregister(UGSTN_DepthSubjectAdapter_Actor::StaticClass());
	}
};

IMPLEMENT_MODULE(FGSTNDepthMarkerRuntimeModule, GSTNDepthMarkerRuntime);
