#include "GSTNCombatDebugModule.h"
#include "Misc/AutomationTest.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

// Review 2026-10-23 C.1: the interface was documented as the way in for other plugins, but Get() and IsAvailable() could not link from
// another module. A test inside this module cannot see that, so it reads the declaration; the built DLL shows the export too
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DebugModuleExportTest, "GSTNCombat.Debug.Module.InterfaceIsExported",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DebugModuleExportTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNCombatDebug"));
	if (!TestTrue(TEXT("the plugin is found"), Plugin.IsValid()))
	{
		return false;
	}
	FString Header;
	if (!TestTrue(TEXT("the public header can be read"), FFileHelper::LoadFileToString(Header, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/GSTNCombatDebug/Public/GSTNCombatDebugModule.h")))))
	{
		return false;
	}
	TestTrue(TEXT("the interface class carries the module export macro"), Header.Contains(TEXT("class GSTNCOMBATDEBUG_API IGSTN_CombatDebugModule")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
