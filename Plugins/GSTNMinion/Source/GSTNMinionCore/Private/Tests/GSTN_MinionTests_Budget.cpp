// Tests of the pure host budget pieces of G11: the farthest-first selection of the T0 cap and the T0 replication rate fallback. No world.

#include "GSTN_MinionTestUtils.h"
#include "LOD/GSTN_MinionLOD.h"
#include "Net/GSTN_MinionNetRates.h"
#include "Tokens/GSTN_MinionAttackTokens.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_MinionCapCandidate MakeCandidate(int32 Slot, float Distance)
	{
		FGSTN_MinionCapCandidate Candidate;
		Candidate.Slot = Slot;
		Candidate.Distance = Distance;
		return Candidate;
	}

	/** Puts the process-wide T0 rate back whatever the test does */
	struct FCombatRateGuard
	{
		~FCombatRateGuard() { GSTNMinionNetRate::SetCombatRateHz(GSTNMinionNetRate::DefaultCombatRateHz); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBudgetSelectFarthestTest, "GSTNMinion.Core.Budget.SelectFarthestWithSlotTieBreak", GSTNMinionTest::Flags)
bool FGSTN_MinionBudgetSelectFarthestTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_MinionCapCandidate> Candidates = {
		MakeCandidate(0, 100.f), MakeCandidate(1, 900.f), MakeCandidate(2, 500.f), MakeCandidate(3, 900.f), MakeCandidate(4, 50.f), MakeCandidate(5, 700.f) };
	TestEqual(TEXT("two picked"), GSTNMinionLODCap::SelectFarthest(Candidates, 2), 2);
	// 900 twice: the higher slot goes first, so slot 3 then slot 1
	TestEqual(TEXT("the farthest, higher slot first on a tie"), Candidates[0].Slot, 3);
	TestEqual(TEXT("then the other of the tie"), Candidates[1].Slot, 1);

	// Same input in another order gives the same pick (a replay or another machine walks the slots in a different order)
	TArray<FGSTN_MinionCapCandidate> Shuffled = { MakeCandidate(5, 700.f), MakeCandidate(3, 900.f), MakeCandidate(4, 50.f), MakeCandidate(1, 900.f), MakeCandidate(0, 100.f), MakeCandidate(2, 500.f) };
	GSTNMinionLODCap::SelectFarthest(Shuffled, 3);
	TestEqual(TEXT("order independent, first"), Shuffled[0].Slot, 3);
	TestEqual(TEXT("order independent, second"), Shuffled[1].Slot, 1);
	TestEqual(TEXT("order independent, third"), Shuffled[2].Slot, 5);

	TestEqual(TEXT("count clamps to the array"), GSTNMinionLODCap::SelectFarthest(Candidates, 99), Candidates.Num());
	TestEqual(TEXT("a zero or negative count picks none"), GSTNMinionLODCap::SelectFarthest(Candidates, -1), 0);
	TArray<FGSTN_MinionCapCandidate> None;
	TestEqual(TEXT("empty is fine"), GSTNMinionLODCap::SelectFarthest(None, 3), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBudgetT0RateTest, "GSTNMinion.Core.Budget.T0RateFallback", GSTNMinionTest::Flags)
bool FGSTN_MinionBudgetT0RateTest::RunTest(const FString& Parameters)
{
	FCombatRateGuard Guard;
	TestEqual(TEXT("default T0 15 Hz"), GSTNMinionNetRate::RateHz(EGSTN_MinionLODTier::Combat), 15);
	const int32 Period15 = GSTNMinionNetRate::PeriodMilliFrames(GSTNMinionNetRate::RateHz(EGSTN_MinionLODTier::Combat));

	GSTNMinionNetRate::SetCombatRateHz(10);
	TestEqual(TEXT("T0 10 Hz"), GSTNMinionNetRate::RateHz(EGSTN_MinionLODTier::Combat), 10);
	TestTrue(TEXT("a longer period"), GSTNMinionNetRate::PeriodMilliFrames(10) > Period15);
	TestEqual(TEXT("T1 untouched"), GSTNMinionNetRate::RateHz(EGSTN_MinionLODTier::Near), 5);
	TestEqual(TEXT("a pack of T0 members follows"), GSTNMinionNetRate::PackCeilingHz(TArray<uint8>{ 0, 1 }), 10);

	// A continuously dirty T0 item sends about 10 times a second: 62 frames hold at most 10 + 1 sends, and well over 8
	FGSTN_MinionNetRateLimiter Limiter;
	int32 Sends = 0;
	for (int32 Frame = 0; Frame < 125; ++Frame)
	{
		if (Limiter.CanSend(Frame, 10))
		{
			Limiter.OnSent(Frame, 10);
			++Sends;
		}
	}
	TestTrue(TEXT("about 20 sends in two seconds at 10 Hz"), Sends >= 19 && Sends <= 21);

	GSTNMinionNetRate::SetCombatRateHz(2);
	TestEqual(TEXT("clamped up to 5"), GSTNMinionNetRate::GetCombatRateHz(), 5);
	GSTNMinionNetRate::SetCombatRateHz(40);
	TestEqual(TEXT("clamped down to 15"), GSTNMinionNetRate::GetCombatRateHz(), 15);
	GSTNMinionNetRate::SetCombatRateHz(15);
	TestEqual(TEXT("15 restores the old period"), GSTNMinionNetRate::PeriodMilliFrames(GSTNMinionNetRate::RateHz(EGSTN_MinionLODTier::Combat)), Period15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBudgetHoldsAnyTest, "GSTNMinion.Core.Budget.TokenHoldsAny", GSTNMinionTest::Flags)
bool FGSTN_MinionBudgetHoldsAnyTest::RunTest(const FString& Parameters)
{
	FGSTN_MinionAttackTokens Tokens;
	Tokens.SetMaxPerTarget(1);
	Tokens.Request(7, 3);
	Tokens.Request(7, 4);
	TestFalse(TEXT("a request alone is not a token"), Tokens.HoldsAny(3));
	Tokens.Resolve();
	TestTrue(TEXT("the lowest slot got the token"), Tokens.HoldsAny(3));
	TestFalse(TEXT("the waiting slot holds none"), Tokens.HoldsAny(4));
	Tokens.ReleaseSlot(3);
	TestFalse(TEXT("released"), Tokens.HoldsAny(3));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
