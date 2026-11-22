// Ground rules and Landing track (owner 2026-11-16): FGSTN_LandingRules, pure.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_Landing.h"
#include "Timeline/GSTN_TimelineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNLandingTests
{
	FGSTN_CombatTimelineData MakeMove(EGSTN_MoveGroundRule Rule, int32 TakeoffFrom = -1, int32 TakeoffUntil = -1)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 60;
		Data.FirstActionableFrame = 50;
		Data.GroundRule = Rule;
		Data.TakeoffFromFrame = TakeoffFrom;
		Data.TakeoffUntilFrame = TakeoffUntil;
		return Data;
	}

	void AddWindow(FGSTN_CombatTimelineData& Data, int32 Start, int32 End, EGSTN_LandingAction Action)
	{
		FGSTN_TimelineTrack_Landing Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		Track.Action = Action;
		Track.Lag.RecoilFrames = Action == EGSTN_LandingAction::LandingLag ? 12 : 0;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Landing>(Track));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LandingGroundRuleTest, "GSTNCombat.Core.Landing.GroundRule", GSTN_CORE_TEST_FLAGS)
bool FGSTN_LandingGroundRuleTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLandingTests;
	auto Action = [](const FGSTN_CombatTimelineData& Data, int32 Frame, bool bGround, bool bFirst, bool bOk)
	{
		return FGSTN_LandingRules::Resolve(Data, Frame, bGround, bFirst, bOk).Action;
	};

	const FGSTN_CombatTimelineData AnyMove = MakeMove(EGSTN_MoveGroundRule::Any);
	TestTrue(TEXT("Any: the air changes nothing"), Action(AnyMove, 5, false, false, false) == EGSTN_GroundClockAction::None);

	const FGSTN_CombatTimelineData Jab = MakeMove(EGSTN_MoveGroundRule::Ground);
	TestFalse(TEXT("a ground move without take-off cannot start in coyote time"), Jab.AllowsTakeoff());
	TestTrue(TEXT("Ground: on the ground plays on"), Action(Jab, 5, true, false, false) == EGSTN_GroundClockAction::None);
	TestTrue(TEXT("Ground: in the air (ledge, teleport) is cancelled"), Action(Jab, 5, false, false, false) == EGSTN_GroundClockAction::CancelAirborne);
	TestTrue(TEXT("even on the first tick"), Action(Jab, 1, false, true, false) == EGSTN_GroundClockAction::CancelAirborne);

	// The wizard's dropkick: may take off on [10, 14)
	const FGSTN_CombatTimelineData Wizard = MakeMove(EGSTN_MoveGroundRule::Ground, 10, 14);
	TestTrue(TEXT("may take off"), Wizard.AllowsTakeoff());
	TestTrue(TEXT("in the air before the window: cancelled"), Action(Wizard, 6, false, false, false) == EGSTN_GroundClockAction::CancelAirborne);
	const FGSTN_GroundClockDecision Takeoff = FGSTN_LandingRules::Resolve(Wizard, 11, false, false, false);
	TestTrue(TEXT("in the window: allowed"), Takeoff.Action == EGSTN_GroundClockAction::None && Takeoff.bAirborneOk);
	TestTrue(TEXT("after the window, once taken off: still allowed"), Action(Wizard, 30, false, false, true) == EGSTN_GroundClockAction::None);
	TestTrue(TEXT("after the window, never taken off (ledge): cancelled"), Action(Wizard, 30, false, false, false) == EGSTN_GroundClockAction::CancelAirborne);
	const FGSTN_GroundClockDecision Coyote = FGSTN_LandingRules::Resolve(Wizard, 1, false, true, false);
	TestTrue(TEXT("started in coyote time: allowed from the first tick"), Coyote.Action == EGSTN_GroundClockAction::None && Coyote.bAirborneOk);
	TestFalse(TEXT("the ground closes the bit"), FGSTN_LandingRules::Resolve(Wizard, 30, true, false, true).bAirborneOk);
	const FGSTN_CombatTimelineData Open = MakeMove(EGSTN_MoveGroundRule::Ground, 10);
	TestTrue(TEXT("until -1 = to the end"), Open.IsTakeoffFrame(55));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LandingWindowTest, "GSTNCombat.Core.Landing.Windows", GSTN_CORE_TEST_FLAGS)
bool FGSTN_LandingWindowTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLandingTests;
	// An aerial: startup continues on the ground, active = landing lag, late recovery autocancels, the middle plays on
	FGSTN_CombatTimelineData Air = MakeMove(EGSTN_MoveGroundRule::Air);
	AddWindow(Air, 0, 8, EGSTN_LandingAction::ContinueWithMove);
	AddWindow(Air, 8, 20, EGSTN_LandingAction::LandingLag);
	AddWindow(Air, 35, 60, EGSTN_LandingAction::AutoCancel);
	TestTrue(TEXT("has Landing tracks"), FGSTN_LandingRules::HasLandingTracks(Air));

	const FGSTN_GroundClockDecision Startup = FGSTN_LandingRules::Resolve(Air, 3, true, false, false);
	TestTrue(TEXT("landing in the startup: Land"), Startup.Action == EGSTN_GroundClockAction::Land);
	TestEqual(TEXT("on the continue window"), Startup.TrackIndex, 0);
	TestEqual(TEXT("landing in the active part: the lag window"), FGSTN_LandingRules::Resolve(Air, 10, true, false, false).TrackIndex, 1);
	TestTrue(TEXT("between windows: plays on"), FGSTN_LandingRules::Resolve(Air, 25, true, false, false).Action == EGSTN_GroundClockAction::None);
	TestEqual(TEXT("late: autocancel"), FGSTN_LandingRules::Resolve(Air, 40, true, false, false).TrackIndex, 2);
	TestTrue(TEXT("in the air: nothing"), FGSTN_LandingRules::Resolve(Air, 10, false, false, false).Action == EGSTN_GroundClockAction::None);

	// The lag is the track's own response, never the move's
	TestTrue(TEXT("the lag window gives its lag as the recoil response"), Air.GetRicochetResponse(1) && Air.GetRicochetResponse(1)->RecoilFrames == 12);
	TestNull(TEXT("an autocancel window gives none"), Air.GetRicochetResponse(2));

	// A ground move that took off lands only after the take-off
	FGSTN_CombatTimelineData Wizard = MakeMove(EGSTN_MoveGroundRule::Ground, 10, 14);
	AddWindow(Wizard, 0, 60, EGSTN_LandingAction::AutoCancel);
	TestTrue(TEXT("on the ground before the take-off: no landing"), FGSTN_LandingRules::Resolve(Wizard, 5, true, false, false).Action == EGSTN_GroundClockAction::None);
	TestTrue(TEXT("after it: lands"), FGSTN_LandingRules::Resolve(Wizard, 20, true, false, true).Action == EGSTN_GroundClockAction::Land);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LandingAuthoringTest, "GSTNCombat.Core.Landing.AuthoringHelpers", GSTN_CORE_TEST_FLAGS)
bool FGSTN_LandingAuthoringTest::RunTest(const FString& Parameters)
{
	// The kit-script path of an aerial's landing windows (dair V10: light lag, heavy lag, light lag): added in order, found by frame, removable
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 43;
	FGSTN_RicochetResponse Light;
	Light.RecoilFrames = 4;
	FGSTN_RicochetResponse Heavy;
	Heavy.RecoilFrames = 16;
	Data = UGSTN_TimelineUtils::AddLandingTrack(Data, 0, 3, EGSTN_LandingAction::LandingLag, Light, nullptr);
	Data = UGSTN_TimelineUtils::AddLandingTrack(Data, 3, 31, EGSTN_LandingAction::LandingLag, Heavy, nullptr);
	Data = UGSTN_TimelineUtils::AddLandingTrack(Data, 31, 43, EGSTN_LandingAction::LandingLag, Light, nullptr);
	int32 Index = INDEX_NONE;
	const FGSTN_TimelineTrack_Landing* Early = FGSTN_LandingRules::FindWindow(Data, 2, &Index);
	TestTrue(TEXT("frame 2: the light window"), Early && Early->Lag.RecoilFrames == 4 && Index == 0);
	const FGSTN_TimelineTrack_Landing* Mid = FGSTN_LandingRules::FindWindow(Data, 3);
	TestTrue(TEXT("frame 3: the heavy window"), Mid && Mid->Action == EGSTN_LandingAction::LandingLag && Mid->Lag.RecoilFrames == 16);
	const FGSTN_TimelineTrack_Landing* Late = FGSTN_LandingRules::FindWindow(Data, 42);
	TestTrue(TEXT("frame 42: the light window again"), Late && Late->Lag.RecoilFrames == 4);
	Data = UGSTN_TimelineUtils::RemoveLandingTracks(Data);
	TestNull(TEXT("removed"), FGSTN_LandingRules::FindWindow(Data, 10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LandingTouchdownTest, "GSTNCombat.Core.Landing.TouchdownThisFrame", GSTN_CORE_TEST_FLAGS)
bool FGSTN_LandingTouchdownTest::RunTest(const FString& Parameters)
{
	constexpr double Step = 1.0 / 62.5;
	// Falling at 800 cm/s under 980 cm/s^2: 12.8 + 0.125 = 12.93 cm this frame
	TestEqual(TEXT("fall of the frame"), FGSTN_LandingRules::FallThisFrame(-800.0, 980.0, Step), 12.9254, 0.001);
	TestTrue(TEXT("floor 10 cm below: touches this frame"), FGSTN_LandingRules::ReachesFloorThisFrame(-800.0, 980.0, Step, 10.0, 3.0));
	TestTrue(TEXT("floor 15 cm below: inside the margin"), FGSTN_LandingRules::ReachesFloorThisFrame(-800.0, 980.0, Step, 15.0, 3.0));
	TestFalse(TEXT("floor 20 cm below: next frame"), FGSTN_LandingRules::ReachesFloorThisFrame(-800.0, 980.0, Step, 20.0, 3.0));
	TestFalse(TEXT("rising: never"), FGSTN_LandingRules::ReachesFloorThisFrame(300.0, 980.0, Step, 1.0, 3.0));
	TestFalse(TEXT("still: never (standing is Mover's own ground)"), FGSTN_LandingRules::ReachesFloorThisFrame(0.0, 980.0, Step, 0.5, 3.0));
	TestFalse(TEXT("a floor above the capsule bottom is not one to land on"), FGSTN_LandingRules::ReachesFloorThisFrame(-800.0, 980.0, Step, -10.0, 3.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
