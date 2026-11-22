// Automation tests of the backpack's NP half (M11 L7b): the dome ability's default timeline and the tick extension's reads of the combat state.

#include "Misc/AutomationTest.h"
#include "GSTN_BackpackSim.h"
#include "GSTN_BackpackTickExtension.h"
#include "GSTN_GA_BackpackDome.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Sync/GSTN_SyncFragment.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackNPDomeAbilityTest, "GSTNBackpack.NP.DomeAbility", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTNBackpackNPDomeAbilityTest::RunTest(const FString& Parameters)
{
	const UGSTN_GA_BackpackDome* Ability = GetDefault<UGSTN_GA_BackpackDome>();
	const FGSTN_CombatTimelineData& Data = Ability->GetTimelineData();
	TestTrue(TEXT("held: the release ends it"), Ability->bEndOnTriggerRelease);
	TestNotNull(TEXT("a charge track"), FGSTN_BackpackChargeTrack::Find(Data));
	const FGSTN_BackpackDomeTrack* DomeTrack = FGSTN_BackpackDomeTrack::Find(Data);
	if (!TestNotNull(TEXT("a dome track"), DomeTrack))
	{
		return false;
	}
	TestTrue(TEXT("the timeline caps the hold"), Data.TotalFrames > 0);
	TestTrue(TEXT("the fragment is registered under its feature id"), FGSTN_SyncFragmentRegistry::FindStruct(GSTNFeatureIds::Backpack) == FGSTN_BackpackFragment::StaticStruct());

	// The tick extension's reads
	FGSTN_CombatSyncCore In;
	FGSTN_CombatSyncCore Out;
	In.Jump.Phase = EGSTN_JumpPhase::Falling;
	TestTrue(TEXT("falling then grounded is a landing"), FGSTN_BackpackTickExtension::HasLanded(In, Out));
	TestFalse(TEXT("grounded twice is not"), FGSTN_BackpackTickExtension::HasLanded(Out, Out));

	FGSTN_BackpackFragment Fragment;
	FGSTN_BackpackStep::RaiseDome(Fragment, 40, *DomeTrack, FVector::ZeroVector, 5, Data.TotalFrames);
	Out.ActiveTimeline = 5;
	Out.TimelineStartFrame = 40;
	TestTrue(TEXT("the dome's move runs"), FGSTN_BackpackTickExtension::IsDomeTimelineRunning(Fragment, Out));
	Out.TimelineStartFrame = 55;
	TestFalse(TEXT("the same move started again is another hold"), FGSTN_BackpackTickExtension::IsDomeTimelineRunning(Fragment, Out));
	Out.ActiveTimeline = INDEX_NONE;
	TestFalse(TEXT("no timeline: released"), FGSTN_BackpackTickExtension::IsDomeTimelineRunning(Fragment, Out));
	return true;
}

#endif
