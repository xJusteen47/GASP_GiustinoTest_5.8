#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Window/SGSTN_DebugOptionsPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

// The Debug Options window lists the variables the console has, by rule, never by a hand-written list: a variable registered here appears,
// and the hitbox switch of GSTNCombatNP (a plugin this one depends on) is there without being named in the panel's code
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DebugOptionsWindowTest, "GSTNCombat.Debug.OptionsWindow.ListsDebugVariables",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DebugOptionsWindowTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* Probe = IConsoleManager::Get().RegisterConsoleVariable(TEXT("gstn.TestProbe.Debug.Switch"), 0, TEXT("test only"), ECVF_Default);
	IConsoleVariable* NotDebug = IConsoleManager::Get().RegisterConsoleVariable(TEXT("gstn.TestProbe.Speed"), 0, TEXT("test only"), ECVF_Default);

	const TArray<FString> Names = SGSTN_DebugOptionsPanel::CollectVariableNames();
	TestTrue(TEXT("a gstn.*Debug* variable registered now is listed"), Names.Contains(TEXT("gstn.TestProbe.Debug.Switch")));
	TestFalse(TEXT("a gstn variable without Debug in its name is not"), Names.Contains(TEXT("gstn.TestProbe.Speed")));
	TestTrue(TEXT("the hitbox switch of GSTNCombatNP is listed"), Names.Contains(TEXT("gstn.Combat.Debug.Hitboxes")));
	TestEqual(TEXT("the group is the second part of the name"), SGSTN_DebugOptionsPanel::GroupOf(TEXT("gstn.Minion.Debug.LOD")), FString(TEXT("Minion")));
	TestEqual(TEXT("a short name falls in Other"), SGSTN_DebugOptionsPanel::GroupOf(TEXT("gstn.Debug")), FString(TEXT("Other")));

	IConsoleManager::Get().UnregisterConsoleObject(Probe, false);
	IConsoleManager::Get().UnregisterConsoleObject(NotDebug, false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
