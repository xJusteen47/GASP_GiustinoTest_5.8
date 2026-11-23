// Development tooling of GSTN Input NP (PIE and development builds, never Shipping).

#include "GSTN_InputSimComponentSource.h"
#include "Modules/ModuleManager.h"
#include "Sources/GSTN_InputSourceRegistry.h"

namespace
{
	/** The key this module registers under in the input debug registry */
	const FName RegistryKey(TEXT("GSTNInputNPDebug"));
}

/**
 * Teaches the input display of GSTNInputDebug to read UGSTN_InputSimComponent. Registered at startup, removed at shutdown. A pawn that has
 * the combat component too is read through the combat finder if that plugin is loaded (the input component stays inert next to it, see its
 * header), so the order of the finders cannot show the wrong one.
 */
class FGSTN_InputNPDebugModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_InputSourceRegistry::Get().RegisterFinder(RegistryKey, [](APawn& Pawn) { return FGSTN_InputSimComponentSource::Find(Pawn); });
	}

	virtual void ShutdownModule() override
	{
		FGSTN_InputSourceRegistry::Get().UnregisterFinder(RegistryKey);
	}
};

IMPLEMENT_MODULE(FGSTN_InputNPDebugModule, GSTNInputNPDebug)
