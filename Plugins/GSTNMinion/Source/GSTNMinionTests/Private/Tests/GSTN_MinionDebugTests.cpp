// Automation tests of the minion debug draw (M8 G12a): with every variable off it does no work. Pure: no world needed, the draw subsystem
// ticks only while the flag holds, so the flag and the pass counter are what "zero cost off" means.

#include "Debug/GSTN_MinionDebugDraw.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionDebugOffTest, "GSTNMinion.Runtime.G12.DebugDrawOffIsFree", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_MinionDebugOffTest::RunTest(const FString& Parameters)
{
	static const TCHAR* const Names[] = { TEXT("gstn.Minion.Debug.Capsules"), TEXT("gstn.Minion.Debug.Hitboxes"), TEXT("gstn.Minion.Debug.LOD"), TEXT("gstn.Minion.Debug.History"), TEXT("gstn.Minion.Debug.Intent") };
	int32 Saved[UE_ARRAY_COUNT(Names)];
	IConsoleVariable* Vars[UE_ARRAY_COUNT(Names)];
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		Vars[Index] = IConsoleManager::Get().FindConsoleVariable(Names[Index]);
		if (!TestNotNull(Names[Index], Vars[Index]))
		{
			return false;
		}
		Saved[Index] = Vars[Index]->GetInt();
		Vars[Index]->Set(0, ECVF_SetByCode);
	}

	TestFalse(TEXT("every variable off: not enabled"), GSTNMinionDebug::IsAnyEnabled());
	const int32 Passes = GSTNMinionDebug::GetPassCount();

	Vars[3]->Set(8, ECVF_SetByCode);
	TestTrue(TEXT("one variable on: enabled"), GSTNMinionDebug::IsAnyEnabled());
	Vars[3]->Set(0, ECVF_SetByCode);
	TestFalse(TEXT("back off: not enabled"), GSTNMinionDebug::IsAnyEnabled());
	TestEqual(TEXT("no draw pass ran while off"), GSTNMinionDebug::GetPassCount(), Passes);

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		Vars[Index]->Set(Saved[Index], ECVF_SetByCode);
	}
	return true;
}

#endif
