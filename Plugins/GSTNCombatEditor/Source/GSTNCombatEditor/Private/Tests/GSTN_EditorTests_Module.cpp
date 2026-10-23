#include "GSTNCombatEditorModule.h"
#include "Settings/GSTN_CombatEditorSettings.h"

#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EditorModuleTest, "GSTNCombat.Editor.Module.LoadsAndRegisters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_EditorModuleTest::RunTest(const FString& Parameters)
{
	// The editor has no combat content here: the module must come up on its own and register what it owns
	TestTrue(TEXT("the module is loaded"), FModuleManager::Get().IsModuleLoaded("GSTNCombatEditor"));
	TestNotNull(TEXT("the module is reachable through its class"), FGSTN_CombatEditorModule::Get());

	const UGSTN_CombatEditorSettings* Settings = GetDefault<UGSTN_CombatEditorSettings>();
	if (TestNotNull(TEXT("the settings object exists"), Settings))
	{
		// UDeveloperSettings objects register themselves with Project Settings by category and section
		TestEqual(TEXT("settings category"), Settings->GetCategoryName(), FName(TEXT("Plugins")));
		TestEqual(TEXT("settings section"), Settings->GetSectionName(), FName(TEXT("GSTN Combat Editor")));
		TestFalse(TEXT("the settings have a title in Project Settings"), Settings->GetSectionText().IsEmpty());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
