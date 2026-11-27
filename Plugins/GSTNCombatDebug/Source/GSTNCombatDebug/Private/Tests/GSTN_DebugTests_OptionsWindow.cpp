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
	TestTrue(TEXT("the skid motion switch of GSTNCombatNP is listed (skid-physics-1)"), Names.Contains(TEXT("gstn.Skid.Motion")));
	TestTrue(TEXT("the stock / our skid switch is listed"), Names.Contains(TEXT("gstn.Skid.Sim")));
	TestTrue(TEXT("the crouch mesh pop switch is listed (crouch-mesh-pop-1)"), Names.Contains(TEXT("gstn.Mesh.CapsuleShiftBeforeAnim")));
	TestTrue(TEXT("the readable run stage steps switch is listed (run-stages-leggibili S1)"), Names.Contains(TEXT("gstn.RunStage.Readable")));
	if (const IConsoleVariable* Readable = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.RunStage.Readable")))
	{
		const FString Help = Readable->GetHelp();
		TestFalse(TEXT("its help names no level 2 or more: one check box, no number field"), Help.Contains(TEXT("2 =")) || Help.Contains(TEXT("3 =")));
	}
	for (const TCHAR* Surface : {TEXT("gstn.Audio.Surface"), TEXT("gstn.Audio.FootwearOverride"), TEXT("gstn.Audio.LayerMode"),
		TEXT("gstn.Audio.SurfaceDebug"), TEXT("gstn.Haptics.Enable"), TEXT("gstn.Haptics.Intensity")})
	{
		TestTrue(FString::Printf(TEXT("the surface feedback switch %s is listed"), Surface), Names.Contains(Surface));
	}
	// debug-wall-switch-1: the wall slide draw, the ground contact draw and the platform draw are reachable from the window and off by default
	for (const TCHAR* Draw : {TEXT("gstn.Wall.Debug.Draw"), TEXT("gstn.Anim.Debug.GroundContactFix"), TEXT("gstn.Platform.Debug.Draw")})
	{
		TestTrue(FString::Printf(TEXT("the debug draw switch %s is listed"), Draw), Names.Contains(Draw));
		if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Draw))
		{
			TestEqual(FString::Printf(TEXT("%s is off by default"), Draw), Var->GetInt(), 0);
		}
	}
	TestEqual(TEXT("the group is the second part of the name"), SGSTN_DebugOptionsPanel::GroupOf(TEXT("gstn.Minion.Debug.LOD")), FString(TEXT("Minion")));
	TestEqual(TEXT("a short name falls in Other"), SGSTN_DebugOptionsPanel::GroupOf(TEXT("gstn.Debug")), FString(TEXT("Other")));

	IConsoleManager::Get().UnregisterConsoleObject(Probe, false);
	IConsoleManager::Get().UnregisterConsoleObject(NotDebug, false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
