// Interaction runtime module: log category and console variables. Game thread, presentation and tooling only.

#include "GSTN_InteractRuntimeTypes.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNInteract);

namespace GSTNInteractCVars
{
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("gstn.Interact.Enable"), 1,
		TEXT("0 stops every game thread interactor from probing (debugging switch). Default 1."), ECVF_Default);

	static TAutoConsoleVariable<int32> CVarPresentation(TEXT("gstn.Interact.Presentation"), 1,
		TEXT("0 hides the prompt and the highlight; the focus itself still runs. Local only. Default 1."), ECVF_Default);

	static TAutoConsoleVariable<int32> CVarDebug(TEXT("gstn.Interact.Debug"), 0,
		TEXT("1 draws every record near each local interactor. Default 0."), ECVF_Default);

	bool IsEnabled() { return CVarEnable.GetValueOnGameThread() != 0; }
	bool IsPresentationEnabled() { return CVarPresentation.GetValueOnGameThread() != 0; }
	bool IsDebugDrawEnabled() { return CVarDebug.GetValueOnGameThread() != 0; }
}

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNInteractRuntime);
