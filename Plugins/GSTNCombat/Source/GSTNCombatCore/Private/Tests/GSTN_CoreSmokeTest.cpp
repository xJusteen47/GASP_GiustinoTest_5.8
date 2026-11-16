#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

/** The Core module is loaded: a failure here means the plugin is not enabled or the module did not build. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CoreSmokeTest, "GSTNCombat.Core.Smoke",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGSTN_CoreSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("GSTNCombatCore module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("GSTNCombatCore")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
