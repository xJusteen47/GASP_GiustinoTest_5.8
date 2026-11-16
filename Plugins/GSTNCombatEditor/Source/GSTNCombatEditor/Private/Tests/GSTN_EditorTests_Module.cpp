#include "GSTNCombatEditorModule.h"
#include "Settings/GSTN_CombatEditorSettings.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EditorNoDebugDependencyTest, "GSTNCombat.Editor.Debug.NoDependencyOnDebugModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_EditorNoDebugDependencyTest::RunTest(const FString& Parameters)
{
	// The input display tab is registered by the debug module itself (plan m5 E14), so the editor plugin must not reach the debug
	// plugin in any way: not a module dependency, not a plugin dependency, not an include. Built from pieces so this file does not match its own search.
	const FString DebugName = FString(TEXT("GSTNCombat")) + TEXT("Debug");

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNCombatEditor"));
	if (!TestTrue(TEXT("the plugin is found"), Plugin.IsValid()))
	{
		return false;
	}
	TestFalse(TEXT("the debug module is not a module of the editor plugin"),
		Plugin->GetDescriptor().Modules.ContainsByPredicate([&DebugName](const FModuleDescriptor& Module) { return Module.Name == *DebugName; }));
	TestFalse(TEXT("the debug plugin is not a plugin dependency of the editor plugin"),
		Plugin->GetDescriptor().Plugins.ContainsByPredicate([&DebugName](const FPluginReferenceDescriptor& Reference) { return Reference.Name == DebugName; }));

	// The Build.cs: no quoted module name
	const FString SourceDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/GSTNCombatEditor"));
	FString BuildCs;
	if (TestTrue(TEXT("Build.cs can be read"), FFileHelper::LoadFileToString(BuildCs, *FPaths::Combine(SourceDir, TEXT("GSTNCombatEditor.Build.cs")))))
	{
		TestFalse(TEXT("Build.cs names no debug module"), BuildCs.Contains(TEXT("\"") + DebugName + TEXT("\"")));
	}

	// No source file includes a header of the debug plugin
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *SourceDir, TEXT("*.*"), true, false);
	int32 NumChecked = 0;
	for (const FString& File : Files)
	{
		if (!File.EndsWith(TEXT(".h")) && !File.EndsWith(TEXT(".cpp")))
		{
			continue;
		}
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *File))
		{
			++NumChecked;
			TestFalse(FString::Printf(TEXT("%s includes no debug header"), *FPaths::GetCleanFilename(File)), Text.Contains(TEXT("#include \"") + DebugName));
		}
	}
	TestTrue(TEXT("the sources were scanned"), NumChecked > 10);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
