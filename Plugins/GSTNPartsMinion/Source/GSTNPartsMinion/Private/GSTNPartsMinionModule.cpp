// Bridge between parts and minions (plan P15 interface, P16 hit flow).

#include "GSTNPartsMinionModule.h"

#include "GSTN_HitStep_PartResolveMinion.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogGSTNPartsMinion);

/**
 * The glue is UGSTN_PartsMinionSubsystem, a world subsystem of this module: it exists only when this plugin is enabled (which needs both
 * GSTNParts and GSTNMinion), binds to the world's minion subsystem in Initialize and unbinds in Deinitialize. Neither plugin knows this
 * one, so without it the minion hit flow runs exactly as M8 ships it.
 *
 * The one startup job (P16-E): the hit step's cache is keyed by raw object pointers, which is safe because it is dropped after every
 * garbage collection, the only moment an object is freed.
 */
class FGSTNPartsMinionModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GCHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddStatic(&UGSTN_HitStep_PartResolveMinion::InvalidateStepCache);
	}

	virtual void ShutdownModule() override
	{
		FCoreUObjectDelegates::GetPostGarbageCollect().Remove(GCHandle);
		GCHandle.Reset();
		UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();
	}

private:
	FDelegateHandle GCHandle;
};

IMPLEMENT_MODULE(FGSTNPartsMinionModule, GSTNPartsMinion)
