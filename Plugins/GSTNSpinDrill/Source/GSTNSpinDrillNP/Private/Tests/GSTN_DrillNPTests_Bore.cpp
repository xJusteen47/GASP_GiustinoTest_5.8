// Spin and drill, Network Prediction half: the bore of D9 part 2 (timeline, hold hook, adoption, crouch stamp, anchor, launch and pop).

#include "Data/GSTN_SpinDrillTracks.h"
#include "Data/GSTN_SpinDrillTypes.h"
#include "Data/GSTN_SpinTimelines.h"
#include "Misc/AutomationTest.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Sim/GSTN_DrillTickExtension.h"
#include "Sim/GSTN_DrillTrackHook.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "State/GSTN_LinkState.h"
#include "Timeline/GSTN_TimelinePlayer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNDrillBoreTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FGSTN_DrillFragment MakeLatched(EGSTN_DrillPhase Phase, int32 LatchFrame)
	{
		FGSTN_DrillFragment Fragment;
		Fragment.Phase = Phase;
		Fragment.PhaseStartFrame = LatchFrame;
		Fragment.LatchFrame = LatchFrame;
		Fragment.SeatFrames = 12;
		Fragment.MaxBoreFrames = 90;
		return Fragment;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillBoreTimelineTest, "GSTNSpinDrill.NP.Bore.Timeline", GSTNDrillBoreTest::Flags)
bool FGSTN_DrillBoreTimelineTest::RunTest(const FString& Parameters)
{
	FGSTN_BoreTimelineParams Params;
	Params.RehitIntervalFrames = 5;
	const FGSTN_CombatTimelineData Data = FGSTN_SpinTimelines::MakeBore(Params);
	const FGSTN_BoreTrack* Bore = FGSTN_BoreTrack::Find(Data);
	if (!TestNotNull(TEXT("the bore timeline carries the bore track"), Bore))
	{
		return false;
	}
	TestTrue(TEXT("seat frame 0, bore frame 1, crouch"), Bore->StartFrame == 0 && Bore->BoreFrame == 1 && Bore->HoldButton.IsValid());
	TestTrue(TEXT("the recovery follows the bore frame"), Data.TotalFrames == 2 + Params.RecoveryFrames);
	const FGSTN_TimelineTrack_Hitbox* Tip = nullptr;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Data.Tracks)
	{
		if (const FGSTN_TimelineTrack_Hitbox* Hitbox = Track.GetPtr<FGSTN_TimelineTrack_Hitbox>())
		{
			Tip = Hitbox;
		}
	}
	if (!TestNotNull(TEXT("the tip hitbox"), Tip) || !TestEqual(TEXT("one tip"), Tip->Hitboxes.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("open on the bore frame only"), Tip->StartFrame == 1 && Tip->EndFrame == 2);
	TestEqual(TEXT("the re-hit interval of the params (seam S1)"), Tip->Hitboxes[0].RehitIntervalFrames, 5);
	TestFalse(TEXT("no link request: the latch holds already"), Tip->Hitboxes[0].Data.LinkType.IsValid());
	TestNull(TEXT("the dive is not a bore"), FGSTN_BoreTrack::Find(FGSTN_SpinTimelines::MakeDive(FGSTN_DiveTimelineParams())));
	TestNull(TEXT("no table set: no bore ability"), FGSTN_DrillTickExtension::FindBoreAbility(nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillBoreHoldTest, "GSTNSpinDrill.NP.Bore.HoldAndStamp", GSTNDrillBoreTest::Flags)
bool FGSTN_DrillBoreHoldTest::RunTest(const FString& Parameters)
{
	using namespace GSTNDrillBoreTest;
	const FGSTN_DrillFragment Seating = MakeLatched(EGSTN_DrillPhase::Seating, 100);
	const FGSTN_DrillFragment Boring = MakeLatched(EGSTN_DrillPhase::Boring, 100);
	const FGSTN_DrillFragment Idle;
	TestTrue(TEXT("seating: the seat frame holds"), FGSTN_BoreTrackHook::ShouldStall(&Seating, 0, 0, 1));
	TestFalse(TEXT("boring: the seat frame moves on to the bore frame"), FGSTN_BoreTrackHook::ShouldStall(&Boring, 0, 0, 1));
	TestTrue(TEXT("boring: the bore frame holds"), FGSTN_BoreTrackHook::ShouldStall(&Boring, 1, 0, 1));
	TestFalse(TEXT("latch over: the recovery plays"), FGSTN_BoreTrackHook::ShouldStall(&Idle, 1, 0, 1));
	TestFalse(TEXT("no fragment: never"), FGSTN_BoreTrackHook::ShouldStall(nullptr, 1, 0, 1));
	TestFalse(TEXT("past the bore frame: never"), FGSTN_BoreTrackHook::ShouldStall(&Boring, 2, 0, 1));

	bool bHeld = true;
	TestFalse(TEXT("no timeline output: unset"), FGSTN_DrillTickExtension::ReadCrouchStamp(nullptr, bHeld));
	FGSTN_TimelineOutput Output;
	TestFalse(TEXT("no stamp: unset"), FGSTN_DrillTickExtension::ReadCrouchStamp(&Output, bHeld));
	TestTrue(TEXT("unset leaves the value"), bHeld);
	FGSTN_StampedWindow Other;
	Other.Profile = 3;
	Other.EndFrame = 120;
	Output.StampedWindows.Add(Other);
	FGSTN_StampedWindow Crouch;
	Crouch.Profile = GSTNDrillStamps::CrouchMarker;
	Crouch.StartFrame = 100;
	Crouch.EndFrame = INDEX_NONE;
	Output.StampedWindows.Add(Crouch);
	TestTrue(TEXT("held stamp read past another profile"), FGSTN_DrillTickExtension::ReadCrouchStamp(&Output, bHeld) && bHeld);
	Output.StampedWindows[1].EndFrame = 121;
	TestTrue(TEXT("released stamp"), FGSTN_DrillTickExtension::ReadCrouchStamp(&Output, bHeld) && !bHeld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillBoreAdoptionTest, "GSTNSpinDrill.NP.Bore.Adoption", GSTNDrillBoreTest::Flags)
bool FGSTN_DrillBoreAdoptionTest::RunTest(const FString& Parameters)
{
	using namespace GSTNDrillBoreTest;
	const FGSTN_DrillFragment Seating = MakeLatched(EGSTN_DrillPhase::Seating, 100);
	FGSTN_CombatSyncCore Core;
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.TimelineStartFrame = 90;
	TestFalse(TEXT("the latch frame (F): the seat starts next frame"), FGSTN_DrillTickExtension::WantsBoreAdoption(Seating, 99, Core));
	TestTrue(TEXT("the seat's first frame, the dive still runs: adopt"), FGSTN_DrillTickExtension::WantsBoreAdoption(Seating, 100, Core));
	TestFalse(TEXT("asked once"), FGSTN_DrillTickExtension::WantsBoreAdoption(Seating, 101, Core));
	Core.TimelineStartFrame = 100;
	TestFalse(TEXT("the bore runs already (re-simulated frame)"), FGSTN_DrillTickExtension::WantsBoreAdoption(Seating, 100, Core));
	Core.ActionState = EGSTN_ActionState::Actionable;
	TestTrue(TEXT("no timeline (the dive ended): adopt"), FGSTN_DrillTickExtension::WantsBoreAdoption(Seating, 100, Core));
	TestFalse(TEXT("not latched: never"), FGSTN_DrillTickExtension::WantsBoreAdoption(FGSTN_DrillFragment(), 100, Core));

	// The bore holds the crouch from its own start: the fallback rule keeps a timeline that started on the latch frame
	FGSTN_DrillLinkView Active;
	Active.bActive = true;
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.TimelineStartFrame = 100;
	FGSTN_DrillFrameInputs In;
	FGSTN_DrillTickExtension::ReadLatchEnd(MakeLatched(EGSTN_DrillPhase::Boring, 100), Core, Active, In);
	TestTrue(TEXT("the bore's timeline counts as held"), In.bCrouchHeld);
	Core.TimelineStartFrame = 130;
	In = FGSTN_DrillFrameInputs();
	FGSTN_DrillTickExtension::ReadLatchEnd(MakeLatched(EGSTN_DrillPhase::Boring, 100), Core, Active, In);
	TestFalse(TEXT("a later timeline is not the drill's"), In.bCrouchHeld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillBoreAnchorTest, "GSTNSpinDrill.NP.Bore.Anchor", GSTNDrillBoreTest::Flags)
bool FGSTN_DrillBoreAnchorTest::RunTest(const FString& Parameters)
{
	constexpr int32 DrillIndex = 4;
	FGSTN_LinkState Body;
	Body.LinkTypeIndex = DrillIndex;
	Body.Role = 1;
	Body.Phase = EGSTN_LinkPhase::Active;
	const FTransform Partner(FRotator(0.0, 90.0, 0.0), FVector(1000.0, 0.0, 0.0));
	const FTransform Self(FVector(1000.0, 30.0, 150.0));
	TestFalse(TEXT("not the drill type: no anchor"), FGSTN_DrillTickExtension::WriteLatchAnchor(Body, DrillIndex + 1, Self, Partner));
	TestTrue(TEXT("the driller's Active drill link: written"), FGSTN_DrillTickExtension::WriteLatchAnchor(Body, DrillIndex, Self, Partner));
	FVector Local = FVector::ZeroVector;
	TestTrue(TEXT("the flag is set"), Body.GetAnchorOverride(Local));
	// The partner turned 90 degrees: world +Y 30 is its local +X 30
	TestTrue(TEXT("in the partner's root space, whole cm"), Local.Equals(FVector(30.0, 0.0, 150.0), 1.0));
	TestTrue(TEXT("placed back where the driller met it"), Partner.TransformPosition(Local).Equals(Self.GetLocation(), 1.0));
	TestFalse(TEXT("written once per latch"), FGSTN_DrillTickExtension::WriteLatchAnchor(Body, DrillIndex, FTransform::Identity, Partner));

	FGSTN_LinkState Target = Body;
	Target.ClearAnchorOverride();
	Target.Role = 2;
	TestFalse(TEXT("the drilled side never writes"), FGSTN_DrillTickExtension::WriteLatchAnchor(Target, DrillIndex, Self, Partner));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillBoreEndTest, "GSTNSpinDrill.NP.Bore.LaunchAndPop", GSTNDrillBoreTest::Flags)
bool FGSTN_DrillBoreEndTest::RunTest(const FString& Parameters)
{
	using namespace GSTNDrillBoreTest;
	const FGSTN_DrillSimTuning Tuning = FGSTN_DrillSimTuning::MakeDefault();
	TestTrue(TEXT("the launch and pop speeds come from the Core tuning"), Tuning.LaunchSpeed == FGSTN_DrillLaunchTuning().LaunchSpeed
		&& Tuning.DetachPopSpeed == FGSTN_BoreTuning().DetachPopSpeed);

	// Target gone while boring: the launch, which lasts as long as the bore's timeline
	FGSTN_DrillFragment Launch = MakeLatched(EGSTN_DrillPhase::Boring, 100);
	FGSTN_DrillFrameInputs Gone;
	Gone.bTargetGone = true;
	FGSTN_DrillTickExtension::StepAfterExecute(Launch, 140, Gone, Tuning);
	TestTrue(TEXT("target gone: launching"), Launch.Phase == EGSTN_DrillPhase::Launching && Launch.PhaseStartFrame == 140);
	FGSTN_CombatSyncCore Core;
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.TimelineStartFrame = 100;
	TestFalse(TEXT("the bore's recovery runs: still launching"), FGSTN_DrillTickExtension::HasPhaseTimelineEnded(Launch, Core));
	Core.ActionState = EGSTN_ActionState::Actionable;
	TestTrue(TEXT("the bore's timeline over: the launch ends"), FGSTN_DrillTickExtension::HasPhaseTimelineEnded(Launch, Core));

	// Released before the launch is earned: the pop, flagged for its frame only
	FGSTN_DrillFragment Pop = MakeLatched(EGSTN_DrillPhase::Boring, 100);
	FGSTN_DrillFrameInputs Released;
	Released.bCrouchHeld = false;
	FGSTN_DrillTickExtension::StepAfterExecute(Pop, 110, Released, Tuning);
	TestTrue(TEXT("pop: no phase, the flag"), Pop.Phase == EGSTN_DrillPhase::None && Pop.Flags == GSTNDrillFlags::Popped);
	FGSTN_DrillTickExtension::StepAfterClocks(Pop, 111, FGSTN_DrillFrameInputs(), Tuning);
	TestTrue(TEXT("cleared the next frame"), Pop.IsDefault());

	// A bore hit counts on the latched fragment
	FGSTN_DrillFragment Hits = MakeLatched(EGSTN_DrillPhase::Boring, 100);
	FGSTN_DrillFrameInputs Hit;
	Hit.bHitLanded = true;
	FGSTN_DrillTickExtension::StepAfterExecute(Hits, 112, Hit, Tuning);
	FGSTN_DrillTickExtension::StepAfterExecute(Hits, 118, Hit, Tuning);
	TestEqual(TEXT("two bore hits"), static_cast<int32>(Hits.HitsLanded), 2);
	return true;
}

#endif
