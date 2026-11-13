// Camera runtime module: registers the built-in subject adapters. Presentation only.

#include "GSTN_CameraRuntimeTypes.h"
#include "GSTN_CameraSubjectAdapters.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

class FGSTNCameraRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// The runtime knows no movement type besides the engine ones; a Mover adapter registers itself from its own plugin
		FGSTN_CameraAdapterRegistry::Register(UGSTN_CameraSubjectAdapter_Actor::StaticClass());
		FGSTN_CameraAdapterRegistry::Register(UGSTN_CameraSubjectAdapter_Character::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		// On engine exit the UObject system is already torn down: StaticClass() / weak-pointer lookups would assert (exit code 3).
		// The registry holds only weak entries and dies with the process, so skipping the unregister is safe there.
		if (IsEngineExitRequested() || !UObjectInitialized())
		{
			return;
		}
		FGSTN_CameraAdapterRegistry::Unregister(UGSTN_CameraSubjectAdapter_Character::StaticClass());
		FGSTN_CameraAdapterRegistry::Unregister(UGSTN_CameraSubjectAdapter_Actor::StaticClass());
	}
};

IMPLEMENT_MODULE(FGSTNCameraRuntimeModule, GSTNCameraRuntime);
