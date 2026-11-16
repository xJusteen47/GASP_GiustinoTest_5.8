// Ground rules and Landing track (owner 2026-11-16): FGSTN_LandingRules, pure.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_Landing.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
