// Automation tests of the minion Core: LOD tiers, hysteresis and dwell.

#include "GSTN_MinionTestUtils.h"
#include "LOD/GSTN_MinionLOD.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Tier = EGSTN_MinionLODTier;

	/** A tier name for failure messages */
	const TCHAR* TierName(EGSTN_MinionLODTier Value)
	{
		switch (Value)
		{
		case Tier::Combat: return TEXT("Combat");
		case Tier::Near: return TEXT("Near");
		case Tier::Far: return TEXT("Far");
		default: return TEXT("Dormant");
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionLODThresholdsTest, "GSTNMinion.Core.LOD.Thresholds", GSTNMinionTest::Flags)
bool FGSTN_MinionLODThresholdsTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionLODParams Params;
	TestTrue(TEXT("The defaults are consistent"), Params.IsConsistent());

	struct FCase { float Distance; Tier Expected; };
	const FCase Cases[] = {
		{ 0.f, Tier::Combat }, { 1500.f, Tier::Combat }, { 1500.5f, Tier::Near }, { 3500.f, Tier::Near },
		{ 3500.5f, Tier::Far }, { 8000.f, Tier::Far }, { 8000.5f, Tier::Dormant }, { 1.0e6f, Tier::Dormant },
	};
	for (const FCase& Case : Cases)
	{
		const Tier Got = FGSTN_MinionLODMath::TierForDistance(Params, Case.Distance);
		TestEqual(FString::Printf(TEXT("Tier at %.1f cm"), Case.Distance), FString(TierName(Got)), FString(TierName(Case.Expected)));
	}
	TestEqual(TEXT("An unknown distance simulates fully"), static_cast<int32>(FGSTN_MinionLODMath::TierForDistance(Params, -1.f)), static_cast<int32>(Tier::Combat));
	TestEqual(TEXT("NaN simulates fully"), static_cast<int32>(FGSTN_MinionLODMath::TierForDistance(Params, std::numeric_limits<float>::quiet_NaN())), static_cast<int32>(Tier::Combat));

	TestEqual(TEXT("Interval of T1"), Params.GetInterval(Tier::Near), 2);
	TestEqual(TEXT("Interval of T3"), Params.GetInterval(Tier::Dormant), 16);

	FGSTN_MinionLODParams Bad = Params;
	Bad.NearDistance = Bad.CombatDistance;
	TestFalse(TEXT("Borders must strictly increase"), Bad.IsConsistent());
	Bad = Params;
	Bad.HysteresisFraction = 0.5f;
	TestFalse(TEXT("Hysteresis must stay below half"), Bad.IsConsistent());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionLODHysteresisTest, "GSTNMinion.Core.LOD.Hysteresis", GSTNMinionTest::Flags)
bool FGSTN_MinionLODHysteresisTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionLODParams Params;
	constexpr int32 Settled = 1000;
	const auto Eval = [&Params](Tier Current, float Distance)
	{
		return static_cast<int32>(FGSTN_MinionLODMath::Evaluate(Params, Current, Distance, Settled, false));
	};

	// Outward needs border x 1.1
	TestEqual(TEXT("Just past the border stays"), Eval(Tier::Combat, 1600.f), static_cast<int32>(Tier::Combat));
	TestEqual(TEXT("Beyond border + 10 % moves out"), Eval(Tier::Combat, 1651.f), static_cast<int32>(Tier::Near));
	TestEqual(TEXT("Near stays just past its border"), Eval(Tier::Near, 3800.f), static_cast<int32>(Tier::Near));
	TestEqual(TEXT("Near moves out beyond 3850"), Eval(Tier::Near, 3851.f), static_cast<int32>(Tier::Far));

	// Inward needs border x 0.9
	TestEqual(TEXT("Just inside the border stays"), Eval(Tier::Near, 1400.f), static_cast<int32>(Tier::Near));
	TestEqual(TEXT("Below border - 10 % moves in"), Eval(Tier::Near, 1349.f), static_cast<int32>(Tier::Combat));
	TestEqual(TEXT("Dormant wakes into Far below 7200"), Eval(Tier::Dormant, 7199.f), static_cast<int32>(Tier::Far));
	TestEqual(TEXT("Dormant stays at 7500"), Eval(Tier::Dormant, 7500.f), static_cast<int32>(Tier::Dormant));

	// Several borders crossed at once land in the right tier in one step
	TestEqual(TEXT("A far jump goes straight out"), Eval(Tier::Combat, 20000.f), static_cast<int32>(Tier::Dormant));
	TestEqual(TEXT("A close jump goes straight in"), Eval(Tier::Dormant, 100.f), static_cast<int32>(Tier::Combat));

	// An oscillation inside the hysteresis band never flips the tier
	FGSTN_MinionLODState State;
	int32 Changes = 0;
	for (int32 Frame = 0; Frame < 600; ++Frame)
	{
		const float Distance = (Frame % 2 == 0) ? 1450.f : 1550.f;
		Changes += FGSTN_MinionLODMath::Step(Params, State, Distance, false) ? 1 : 0;
	}
	TestEqual(TEXT("A minion on the border does not flip"), Changes, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionLODDwellTest, "GSTNMinion.Core.LOD.Dwell", GSTNMinionTest::Flags)
bool FGSTN_MinionLODDwellTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionLODParams Params;

	TestEqual(TEXT("Under the dwell the tier is kept"),
		static_cast<int32>(FGSTN_MinionLODMath::Evaluate(Params, Tier::Combat, 5000.f, Params.MinDwellFrames - 1, false)), static_cast<int32>(Tier::Combat));
	TestEqual(TEXT("At the dwell the tier may change"),
		static_cast<int32>(FGSTN_MinionLODMath::Evaluate(Params, Tier::Combat, 5000.f, Params.MinDwellFrames, false)), static_cast<int32>(Tier::Far));
	TestEqual(TEXT("A forced Combat ignores the dwell"),
		static_cast<int32>(FGSTN_MinionLODMath::Evaluate(Params, Tier::Far, 5000.f, 0, true)), static_cast<int32>(Tier::Combat));

	// Step: the minion starts in Combat, a fighter is far away, it leaves after the dwell and the counter restarts
	FGSTN_MinionLODState State;
	int32 ChangeFrame = INDEX_NONE;
	for (int32 Frame = 1; Frame <= 100 && ChangeFrame == INDEX_NONE; ++Frame)
	{
		if (FGSTN_MinionLODMath::Step(Params, State, 5000.f, false))
		{
			ChangeFrame = Frame;
		}
	}
	TestEqual(TEXT("Leaves Combat on the frame the dwell is reached"), ChangeFrame, Params.MinDwellFrames);
	TestEqual(TEXT("Now in Far"), static_cast<int32>(State.Tier), static_cast<int32>(Tier::Far));
	TestEqual(TEXT("The dwell counter restarted"), State.FramesInTier, 0);

	// A fighter walks in right after: the dwell holds it in Far, then it returns
	int32 BackFrame = INDEX_NONE;
	for (int32 Frame = 1; Frame <= 100 && BackFrame == INDEX_NONE; ++Frame)
	{
		if (FGSTN_MinionLODMath::Step(Params, State, 300.f, false))
		{
			BackFrame = Frame;
		}
	}
	TestEqual(TEXT("Returns after one dwell"), BackFrame, Params.MinDwellFrames);
	TestEqual(TEXT("Back in Combat"), static_cast<int32>(State.Tier), static_cast<int32>(Tier::Combat));

	// A hit forces Combat at once
	State.Tier = Tier::Dormant;
	State.FramesInTier = 3;
	TestTrue(TEXT("Forced change reported"), FGSTN_MinionLODMath::Step(Params, State, 9000.f, true));
	TestEqual(TEXT("A hit wakes a dormant minion"), static_cast<int32>(State.Tier), static_cast<int32>(Tier::Combat));

	// A multi-frame step counts the frames (a minion stepped every 4th frame advances by 4)
	FGSTN_MinionLODState Slow;
	FGSTN_MinionLODMath::Step(Params, Slow, 100.f, false, 4);
	TestEqual(TEXT("Delta frames accumulate"), Slow.FramesInTier, 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
