// Automation tests of tumble air recovery and balloon knockback (owner 2026-11-16). Pure: structs only, no world, no asset.
// The balloon's hand values follow the public Ultimate example (hitstun 40 -> first frame x2, second x1.857), worked out in the comments.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Hits/GSTN_BalloonKnockback.h"
#include "Hits/GSTN_TumbleRecovery.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Rules/GSTN_CombatRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BalloonOffByDefaultTest, "GSTNCombat.Core.Balloon.OffByDefault", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BalloonOffByDefaultTest::RunTest(const FString& Parameters)
{
	const FGSTN_ReactionTuning Tuning;
	TestFalse(TEXT("the reaction tuning has the balloon off"), Tuning.Balloon.bEnabled);
	TestTrue(TEXT("tumble only and set knockback exempt, as in Ultimate"), Tuning.Balloon.bTumbleOnly && !Tuning.Balloon.bApplyToFixedKnockback);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BalloonMultiplierTest, "GSTNCombat.Core.Balloon.Multiplier", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BalloonMultiplierTest::RunTest(const FString& Parameters)
{
	FGSTN_BalloonKnockbackSettings Settings;
	Settings.bEnabled = true;
	// 30..80 -> 1..6: hitstun 40 is a fifth of the way, M0 = 1 + 5 x 0.2 = 2
	TestEqual(TEXT("no speed-up at or under the low end"), FGSTN_BalloonMath::InitialMultiplier(Settings, 30), 1.0);
	TestEqual(TEXT("M0 at 40"), FGSTN_BalloonMath::InitialMultiplier(Settings, 40), 2.0, 1e-9);
	TestEqual(TEXT("M0 at 80"), FGSTN_BalloonMath::InitialMultiplier(Settings, 80), 6.0, 1e-9);
	TestEqual(TEXT("clamped past 80"), FGSTN_BalloonMath::InitialMultiplier(Settings, 200), 6.0, 1e-9);

	// Second frame: 2 flight frames flown, left 38/40 = 0.95, M = 2 x (0.95 - 0.3) / 0.7 = 1.857
	double Consumed = 0.0;
	double M = 0.0;
	FGSTN_BalloonMath::Evaluate(2.0, 0.3, 40, 0, Consumed, M);
	TestEqual(TEXT("frame 0: nothing flown, M0"), M, 2.0, 1e-9);
	FGSTN_BalloonMath::Evaluate(2.0, 0.3, 40, 1, Consumed, M);
	TestEqual(TEXT("frame 1: 2 flown"), Consumed, 2.0, 1e-9);
	TestEqual(TEXT("frame 1: x1.857 as in the Ultimate example"), M, 2.0 * 0.65 / 0.7, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BalloonSamePathLessTimeTest, "GSTNCombat.Core.Balloon.SamePathLessTime", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BalloonSamePathLessTimeTest::RunTest(const FString& Parameters)
{
	FGSTN_BalloonKnockbackSettings Settings;
	Settings.bEnabled = true;
	for (const int32 Flight : { 40, 60, 80, 120, 255 })
	{
		const double M0 = FGSTN_BalloonMath::InitialMultiplier(Settings, Flight);
		const int32 Game = FGSTN_BalloonMath::GameFrames(M0, Settings.EndFraction, Flight);
		const int32 WarpEnd = FGSTN_BalloonMath::WarpEndFrame(M0, Settings.EndFraction, Flight);
		const FString What = FString::Printf(TEXT("flight %d"), Flight);
		TestTrue(*(What + TEXT(": fewer game frames than flight frames")), Game < Flight);
		TestTrue(*(What + TEXT(": the warp ends before the lock does")), WarpEnd > 0 && WarpEnd < Game);

		// The warped clock reaches the whole flight on the last game frame: the lock ends at the same point of the path
		double Consumed = 0.0;
		double M = 0.0;
		FGSTN_BalloonMath::Evaluate(M0, Settings.EndFraction, Flight, Game, Consumed, M);
		TestTrue(*(What + TEXT(": the clock covers the flight")), Consumed >= Flight && Consumed < Flight + M0);
		FGSTN_BalloonMath::Evaluate(M0, Settings.EndFraction, Flight, WarpEnd, Consumed, M);
		TestEqual(*(What + TEXT(": M is 1 from the warp end")), M, 1.0);
		// The multiplier only falls
		double Previous = M0 + 1.0;
		for (int32 Frame = 0; Frame <= WarpEnd; ++Frame)
		{
			FGSTN_BalloonMath::Evaluate(M0, Settings.EndFraction, Flight, Frame, Consumed, M);
			TestTrue(*(What + TEXT(": M never grows")), M <= Previous);
			Previous = M;
		}
	}
	// Deterministic: the same input twice gives the same bits
	const int32 A = FGSTN_BalloonMath::GameFrames(4.2, 0.3, 67);
	const int32 B = FGSTN_BalloonMath::GameFrames(4.2, 0.3, 67);
	TestEqual(TEXT("same input, same frames"), A, B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TumbleRecoveryResolveTest, "GSTNCombat.Core.TumbleRecovery.Resolve", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TumbleRecoveryResolveTest::RunTest(const FString& Parameters)
{
	const FGSTN_TumbleRecoveryRules Defaults;
	TestTrue(TEXT("the rules default to recovery after hitstun (owner 2026-11-16, option B)"), Defaults.Mode == EGSTN_TumbleRecovery::AfterHitstun);

	FGSTN_TumbleRecoveryRules Rules;
	Rules.Mode = EGSTN_TumbleRecovery::AfterHitstun;
	TestTrue(TEXT("UseRules follows the rules"), FGSTN_TumbleRecoveryResolved::Resolve(&Rules, nullptr, EGSTN_TumbleRecoveryOverride::UseRules).bAfterHitstun);
	TestFalse(TEXT("a dummy's override wins"), FGSTN_TumbleRecoveryResolved::Resolve(&Rules, nullptr, EGSTN_TumbleRecoveryOverride::UntilGround).bAfterHitstun);
	Rules.Mode = EGSTN_TumbleRecovery::UntilGround;
	TestTrue(TEXT("and a player's"), FGSTN_TumbleRecoveryResolved::Resolve(&Rules, nullptr, EGSTN_TumbleRecoveryOverride::AfterHitstun).bAfterHitstun);
	TestFalse(TEXT("no rules: locked until ground (the old behaviour)"), FGSTN_TumbleRecoveryResolved::Resolve(nullptr, nullptr, EGSTN_TumbleRecoveryOverride::UseRules).bAfterHitstun);

	// Commands: empty = all; the kit's list replaces the rules'; hierarchical match
	const FGameplayTag Jump = FGameplayTag::RequestGameplayTag(TEXT("Command.Movement.Jump"), false);
	const FGameplayTag Neutral = FGameplayTag::RequestGameplayTag(TEXT("Command.Attack.Neutral"), false);
	const FGameplayTag Attack = FGameplayTag::RequestGameplayTag(TEXT("Command.Attack"), false);
	if (!Jump.IsValid() || !Neutral.IsValid() || !Attack.IsValid())
	{
		AddWarning(TEXT("the standard command tags are not registered in this run: command checks skipped"));
		return true;
	}
	TestTrue(TEXT("empty list: every command"), FGSTN_TumbleRecoveryResolved::Resolve(&Rules, nullptr, EGSTN_TumbleRecoveryOverride::UseRules).IsCommandAllowed(Jump));
	Rules.AllowedCommands.AddTag(Jump);
	FGSTN_TumbleRecoveryExtension Kit;
	Kit.AllowedCommands.AddTag(Attack);
	const FGSTN_TumbleRecoveryResolved FromRules = FGSTN_TumbleRecoveryResolved::Resolve(&Rules, nullptr, EGSTN_TumbleRecoveryOverride::UseRules);
	TestTrue(TEXT("rules: jump admitted"), FromRules.IsCommandAllowed(Jump));
	TestFalse(TEXT("rules: attack refused"), FromRules.IsCommandAllowed(Neutral));
	const FGSTN_TumbleRecoveryResolved FromKit = FGSTN_TumbleRecoveryResolved::Resolve(&Rules, &Kit, EGSTN_TumbleRecoveryOverride::UseRules);
	TestTrue(TEXT("kit: Command.Attack admits the neutral attack"), FromKit.IsCommandAllowed(Neutral));
	TestFalse(TEXT("kit: its list replaces the rules'"), FromKit.IsCommandAllowed(Jump));
	return true;
}

#endif
