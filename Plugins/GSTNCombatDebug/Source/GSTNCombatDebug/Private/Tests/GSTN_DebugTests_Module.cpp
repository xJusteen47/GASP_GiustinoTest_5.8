#include "GSTNCombatDebugModule.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DebugModuleTest, "GSTNCombat.Debug.Module.LoadsAndExposesInterface",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DebugModuleTest::RunTest(const FString& Parameters)
{
	// The test runs inside a module of the plugin, so the module is loaded; the point is that an outsider reaches it through
	// the interface by name, without linking
	TestTrue(TEXT("the module is available"), IGSTN_CombatDebugModule::IsAvailable());
	IGSTN_CombatDebugModule* Module = IGSTN_CombatDebugModule::Get();
	if (TestNotNull(TEXT("the interface is reachable by module name"), Module))
	{
		// Null is a legal answer until E14 implements the widget: callers treat it like an absent module. Asking must be safe.
		Module->CreateInputDisplay();
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
