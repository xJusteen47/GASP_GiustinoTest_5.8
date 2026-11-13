// Automation tests of the reaction model (checkpoint H2): the knockback models, the stagger / tier / hitstun / hitstop rules, the
// baked curve and the validators. Pure: in-memory structs, no world and no content asset. Hand values are worked out in the
// comments so a changed formula is a visible decision, not a silent one.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "Hits/GSTN_KnockbackModels.h"
#include "Hits/GSTN_ReactionValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNReactionTest
{
	/** Starting values written out, so the tests do not move when the C++ defaults are tuned */
	FGSTN_ReactionTuning MakeTuning()
	{
		FGSTN_ReactionTuning Tuning;
		Tuning.StaggerPerDamage = 2.f;
		Tuning.StaggerPerKnockback = 0.25f;
		Tuning.HitstopPerDamage = 0.5f;
		Tuning.HitstopPerKnockback = 0.f;
		Tuning.HitstopBase = 2.f;
		Tuning.MaxHitstop = 20;
		Tuning.PushBackThreshold = 0.f;
		Tuning.LaunchThreshold = 0.f;
		Tuning.TumbleThreshold = 1800.f;
		Tuning.HitstunFactor = 0.0625f;
		Tuning.MinHitstunFrames = 6;
		Tuning.PushBackSpeedScale = 0.5f;
		Tuning.MaxPushBackSpeed = 300.f;
		Tuning.WeightReference = 100.f;
		Tuning.MinWeight = 1.f;
		return Tuning;
	}

	template <typename T>
	TStrongObjectPtr<T> MakeModel(const FGSTN_ReactionTuning& Tuning)
	{
		TStrongObjectPtr<T> Model(NewObject<T>(GetTransientPackage()));
		Model->Tuning = Tuning;
		return Model;
	}

	/** Damage 10, base 300, growth 20: the authored bracket is 300 + 20 x 10 = 500 */
	FGSTN_KnockbackInput MakeInput()
	{
		FGSTN_KnockbackInput Input;
		Input.Hit.Damage = 10.f;
		Input.Hit.BaseKnockback = 300.f;
		Input.Hit.KnockbackGrowth = 20.f;
		Input.LaunchDirection = FVector(1.0, 0.0, 1.0).GetSafeNormal();
		Input.Up = FVector::UpVector;
		Input.Weight = 100.f;
		return Input;
	}

	/** Every field, exactly: the determinism check is bitwise on values, not near */
	bool Same(const FGSTN_KnockbackResult& A, const FGSTN_KnockbackResult& B)
	{
		return A.Reaction == B.Reaction && A.bStaggered == B.bStaggered && A.StaggerForce == B.StaggerForce && A.Knockback == B.Knockback
			&& A.LaunchVelocity.X == B.LaunchVelocity.X && A.LaunchVelocity.Y == B.LaunchVelocity.Y && A.LaunchVelocity.Z == B.LaunchVelocity.Z
			&& A.HitstunFrames == B.HitstunFrames && A.bTumble == B.bTumble && A.HitstopFrames == B.HitstopFrames
			&& A.AttackerHitstopFrames == B.AttackerHitstopFrames;
	}

	bool HasMessage(const TArray<FGSTN_ReactionMessage>& Messages, bool bError, const TCHAR* Fragment)
	{
		return Messages.ContainsByPredicate([&](const FGSTN_ReactionMessage& Message) { return Message.bError == bError && Message.Text.Contains(Fragment); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionStaggerForceDerivedTest, "GSTNCombat.Core.Reaction.StaggerForceDerived", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionStaggerForceDerivedTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput();

	// 10 x 2 + 300 x 0.25 = 95: authored values only, no weight and no percent in it
	TestEqual(TEXT("force from damage and base knockback"), Model->Compute(Input).StaggerForce, 95.f);

	// A fixed knockback replaces the base in the nominal value: 10 x 2 + 100 x 0.25 = 45
	Input.Hit.FixedKnockback = 100.f;
	TestEqual(TEXT("force from the fixed knockback"), Model->Compute(Input).StaggerForce, 45.f);

	// The victim changes nothing about the attack's force
	Input.Weight = 400.f;
	Input.DamagePercent = 250.f;
	Input.Reaction.WeightScale = 0.5f;
	TestEqual(TEXT("force independent of the victim"), Model->Compute(Input).StaggerForce, 45.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionStaggerForceOverrideTest, "GSTNCombat.Core.Reaction.StaggerForceOverride", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionStaggerForceOverrideTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput();
	Input.Hit.StaggerForceMode = EGSTN_StaggerForceMode::Override;
	Input.Hit.StaggerForce = 50.f;
	TestEqual(TEXT("override replaces the bracket"), Model->Compute(Input).StaggerForce, 50.f);

	Input.Hit.StaggerForceScale = 0.5f;
	TestEqual(TEXT("the scale still multiplies an override"), Model->Compute(Input).StaggerForce, 25.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionStaggerForceScaleTest, "GSTNCombat.Core.Reaction.StaggerForceScale", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionStaggerForceScaleTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput();
	Input.Hit.StaggerForceScale = 2.f;
	TestEqual(TEXT("derived force doubles"), Model->Compute(Input).StaggerForce, 190.f);

	// The victim's StaggerScale multiplies the force in the test only: the reported force is the attack's
	Input.Hit.StaggerForceScale = 1.f;
	Input.Reaction.StaggerScale = 0.5f;
	Input.Reaction.StaggerThreshold.Value = 60.f;
	const FGSTN_KnockbackResult Result = Model->Compute(Input);
	TestEqual(TEXT("reported force is the attack's"), Result.StaggerForce, 95.f);
	TestFalse(TEXT("95 x 0.5 = 47.5 is below 60"), Result.bStaggered);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionHitstunBoundaryTest, "GSTNCombat.Core.Reaction.HitstunBoundary", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionHitstunBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput(); // force 95

	Input.Reaction.StaggerThreshold.Value = 95.f;
	TestTrue(TEXT("a force equal to the threshold staggers"), Model->Compute(Input).bStaggered);
	Input.Reaction.StaggerThreshold.Value = 96.f;
	TestFalse(TEXT("a threshold one unit above does not"), Model->Compute(Input).bStaggered);
	Input.Reaction.StaggerThreshold.Value = 94.f;
	TestTrue(TEXT("one unit below staggers"), Model->Compute(Input).bStaggered);

	// Unset (negative) staggers on every hit, even a zero force
	Input.Reaction.StaggerThreshold = FGSTN_StaggerThreshold();
	Input.Hit.Damage = 0.f;
	Input.Hit.BaseKnockback = 0.f;
	TestTrue(TEXT("an unset threshold staggers on every hit"), Model->Compute(Input).bStaggered);

	// Not staggered: no hitstun, no displacement
	Input = MakeInput();
	Input.Reaction.StaggerThreshold.Value = 96.f;
	const FGSTN_KnockbackResult Below = Model->Compute(Input);
	TestEqual(TEXT("no hitstun below the threshold"), Below.HitstunFrames, 0);
	TestTrue(TEXT("no launch below the threshold"), Below.LaunchVelocity.IsNearlyZero());
	TestFalse(TEXT("no tumble below the threshold"), Below.bTumble);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionThresholdInfiniteTest, "GSTNCombat.Core.Reaction.ThresholdInfinite", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionThresholdInfiniteTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput();
	Input.Hit.StaggerForceMode = EGSTN_StaggerForceMode::Override;
	Input.Hit.StaggerForce = 1.0e9f;
	Input.Reaction.StaggerThreshold.bInfinite = true;

	const FGSTN_KnockbackResult Result = Model->Compute(Input);
	TestFalse(TEXT("super armor never staggers"), Result.bStaggered);
	TestEqual(TEXT("no hitstun"), Result.HitstunFrames, 0);
	TestTrue(TEXT("no displacement"), Result.LaunchVelocity.IsNearlyZero());
	TestTrue(TEXT("hitstop still applies"), Result.HitstopFrames > 0 && Result.AttackerHitstopFrames > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionNoFlinchTest, "GSTNCombat.Core.Reaction.NoFlinch", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionNoFlinchTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput();
	const FGSTN_KnockbackResult Flinch = Model->Compute(Input);
	Input.Reaction.bNoFlinch = true;
	const FGSTN_KnockbackResult Armored = Model->Compute(Input);

	TestFalse(TEXT("no flinch does not stagger"), Armored.bStaggered);
	TestEqual(TEXT("no hitstun"), Armored.HitstunFrames, 0);
	TestTrue(TEXT("no launch"), Armored.LaunchVelocity.IsNearlyZero());
	TestEqual(TEXT("hitstop is the same"), Armored.HitstopFrames, Flinch.HitstopFrames);
	TestEqual(TEXT("attacker hitstop is the same"), Armored.AttackerHitstopFrames, Flinch.AttackerHitstopFrames);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionHitstopAutoHandValuesTest, "GSTNCombat.Core.Reaction.HitstopAutoHandValues", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionHitstopAutoHandValuesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	FGSTN_ReactionTuning Tuning = MakeTuning();
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(Tuning);
	FGSTN_KnockbackInput Input = MakeInput();

	// Base = floor(10 x 0.5 + 0 + 2) = 7 for both sides
	FGSTN_KnockbackResult Result = Model->Compute(Input);
	TestEqual(TEXT("victim hitstop"), Result.HitstopFrames, 7);
	TestEqual(TEXT("attacker hitstop"), Result.AttackerHitstopFrames, 7);

	// Attacker multiplier 0.5: floor(3.5) = 3; HitstopScale 2 doubles both: 14 and floor(7) = 7
	Input.Hit.AttackerHitstopMultiplier = 0.5f;
	Result = Model->Compute(Input);
	TestEqual(TEXT("attacker multiplier"), Result.AttackerHitstopFrames, 3);
	Input.Reaction.HitstopScale = 2.f;
	Result = Model->Compute(Input);
	TestEqual(TEXT("victim with HitstopScale 2"), Result.HitstopFrames, 14);
	TestEqual(TEXT("attacker with HitstopScale 2"), Result.AttackerHitstopFrames, 7);

	// HitstopScale 0 is the boss that must not even pause
	Input.Reaction.HitstopScale = 0.f;
	Result = Model->Compute(Input);
	TestEqual(TEXT("scale 0 victim"), Result.HitstopFrames, 0);
	TestEqual(TEXT("scale 0 attacker"), Result.AttackerHitstopFrames, 0);

	// With a knockback term the REAL K is used: K = 500 (weight 100), 500 / 128 = 3.90625 -> floor(5 + 2 + 3.90625) = 10
	Tuning.HitstopPerKnockback = 0.0078125f;
	Model->Tuning = Tuning;
	Input = MakeInput();
	TestEqual(TEXT("real knockback in the hitstop"), Model->Compute(Input).HitstopFrames, 10);
	// A lighter victim gets a bigger K and a longer hitstop: weight 50 -> K 1000 -> floor(5 + 2 + 7.8125) = 14
	Input.Weight = 50.f;
	TestEqual(TEXT("lighter victim, longer hitstop"), Model->Compute(Input).HitstopFrames, 14);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionHitstopOverrideFinalTest, "GSTNCombat.Core.Reaction.HitstopOverrideFinal", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionHitstopOverrideFinalTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput();
	Input.Hit.HitstopMode = EGSTN_HitstopMode::Override;
	Input.Hit.HitstopFramesOverride = 9;
	Input.Hit.HitstopMultiplier = 3.f;
	Input.Hit.AttackerHitstopMultiplier = 3.f;
	Input.Reaction.HitstopScale = 5.f;

	FGSTN_KnockbackResult Result = Model->Compute(Input);
	TestEqual(TEXT("victim override is final"), Result.HitstopFrames, 9);
	TestEqual(TEXT("attacker defaults to the same value"), Result.AttackerHitstopFrames, 9);

	Input.Hit.AttackerHitstopFramesOverride = 4;
	Result = Model->Compute(Input);
	TestEqual(TEXT("attacker has its own override"), Result.AttackerHitstopFrames, 4);
	TestEqual(TEXT("victim unchanged"), Result.HitstopFrames, 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionHitstopClampTest, "GSTNCombat.Core.Reaction.HitstopClamp", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionHitstopClampTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput();

	Input.Hit.HitstopMode = EGSTN_HitstopMode::Override;
	Input.Hit.HitstopFramesOverride = 99;
	Input.Hit.AttackerHitstopFramesOverride = 50;
	FGSTN_KnockbackResult Result = Model->Compute(Input);
	TestEqual(TEXT("override clamped to MaxHitstop"), Result.HitstopFrames, 20);
	TestEqual(TEXT("attacker override clamped"), Result.AttackerHitstopFrames, 20);

	// Auto: the base and the final value are both clamped
	Input.Hit.HitstopMode = EGSTN_HitstopMode::Auto;
	Input.Hit.Damage = 1000.f;
	Result = Model->Compute(Input);
	TestEqual(TEXT("auto clamped"), Result.HitstopFrames, 20);
	Input.Hit.Damage = 10.f;
	Input.Hit.HitstopMultiplier = 100.f;
	TestEqual(TEXT("multiplied value clamped again"), Model->Compute(Input).HitstopFrames, 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionAttackerHitstopFromSnapshotOnlyTest, "GSTNCombat.Core.Reaction.AttackerHitstopFromSnapshotOnly", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionAttackerHitstopFromSnapshotOnlyTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	FGSTN_ReactionTuning Tuning = MakeTuning();
	Tuning.HitstopPerKnockback = 0.0078125f;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(Tuning);
	const FGSTN_KnockbackInput Reference = MakeInput();
	const int32 Expected = Model->Compute(Reference).AttackerHitstopFrames;

	// What only the victim's own reaction uses (hitstun, tier cap, stagger test, flinch) never reaches the attacker's hitstop
	FGSTN_KnockbackInput Input = Reference;
	Input.Reaction.HitstunScale = 3.f;
	Input.Reaction.MaxReaction = EGSTN_HitReaction::StaggerInPlace;
	Input.Reaction.StaggerThreshold.bInfinite = true;
	Input.Reaction.StaggerScale = 0.1f;
	Input.Reaction.bNoFlinch = true;
	Input.HealthFraction = 0.1f;
	Input.LaunchDirection = FVector::ZeroVector;
	Input.Up = FVector(0.0, 1.0, 0.0);
	TestEqual(TEXT("attacker hitstop ignores the victim's own reaction data"), Model->Compute(Input).AttackerHitstopFrames, Expected);

	// What the snapshot carries (weight, weight scale) does change it, through the real K
	Input = Reference;
	Input.Weight = 50.f;
	TestTrue(TEXT("snapshot weight changes it"), Model->Compute(Input).AttackerHitstopFrames != Expected);
	Input = Reference;
	Input.Reaction.WeightScale = 0.5f;
	TestTrue(TEXT("snapshot weight scale changes it"), Model->Compute(Input).AttackerHitstopFrames != Expected);

	// The packed state is what travels: it round trips the values the model reads
	const FGSTN_VictimReactionState State = FGSTN_VictimReactionState::Make(123.46, 0.6, GSTNVictimReactionFlags::Armor | GSTNVictimReactionFlags::Tumble, FGSTN_StaggerThreshold());
	TestEqual(TEXT("percent in tenths"), State.GetDamagePercent(), 123.5f, 0.001f);
	TestEqual(TEXT("weight scale in 1/64"), State.GetWeightScale(), 38.f / 64.f, 0.0001f);
	TestTrue(TEXT("flags"), State.HasFlag(GSTNVictimReactionFlags::Armor) && State.HasFlag(GSTNVictimReactionFlags::Tumble) && !State.HasFlag(GSTNVictimReactionFlags::Intangible));
	TestFalse(TEXT("an unset threshold stays unset"), State.EffectiveThresholdQ.IsSet());
	FGSTN_StaggerThreshold Infinite;
	Infinite.bInfinite = true;
	TestTrue(TEXT("an infinite threshold stays infinite"), FGSTN_VictimReactionState::Make(0.0, 1.0, 0, Infinite).EffectiveThresholdQ.bInfinite);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionTierBoundariesTest, "GSTNCombat.Core.Reaction.TierBoundaries", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionTierBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	FGSTN_ReactionTuning Tuning = MakeTuning();
	Tuning.PushBackThreshold = 300.f;
	Tuning.LaunchThreshold = 600.f;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(Tuning);

	// Weight equals the reference and a fixed knockback is used, so K is exactly the fixed value
	FGSTN_KnockbackInput Input = MakeInput();
	auto ReactionAt = [&](float K)
	{
		Input.Hit.FixedKnockback = K;
		return Model->Compute(Input);
	};
	TestEqual(TEXT("299 stays in place"), ReactionAt(299.f).Reaction, EGSTN_HitReaction::StaggerInPlace);
	TestEqual(TEXT("300 pushes back"), ReactionAt(300.f).Reaction, EGSTN_HitReaction::PushBack);
	TestEqual(TEXT("599 pushes back"), ReactionAt(599.f).Reaction, EGSTN_HitReaction::PushBack);
	TestEqual(TEXT("600 launches"), ReactionAt(600.f).Reaction, EGSTN_HitReaction::Launch);
	TestFalse(TEXT("1799 does not tumble"), ReactionAt(1799.f).bTumble);
	TestTrue(TEXT("1800 tumbles"), ReactionAt(1800.f).bTumble);

	// The tiers move the victim differently: nothing, a capped horizontal slide, direction x K
	const FGSTN_KnockbackResult InPlace = ReactionAt(100.f);
	TestTrue(TEXT("stagger in place has no displacement"), InPlace.LaunchVelocity.IsNearlyZero());
	TestTrue(TEXT("stagger in place still has hitstun"), InPlace.HitstunFrames >= 6);
	const FGSTN_KnockbackResult Push = ReactionAt(500.f); // min(500 x 0.5, 300) = 250 along the horizontal part of the launch
	TestEqual(TEXT("push back speed"), static_cast<float>(Push.LaunchVelocity.Size()), 250.f, 0.01f);
	TestTrue(TEXT("push back is horizontal"), FMath::IsNearlyZero(Push.LaunchVelocity.Z) && !Push.bTumble);
	TestEqual(TEXT("push back is capped"), static_cast<float>(ReactionAt(590.f).LaunchVelocity.Size()), 295.f, 0.01f);
	Tuning.MaxPushBackSpeed = 100.f;
	Model->Tuning = Tuning;
	TestEqual(TEXT("push back is capped by MaxPushBackSpeed"), static_cast<float>(ReactionAt(590.f).LaunchVelocity.Size()), 100.f, 0.01f);
	const FGSTN_KnockbackResult Launch = ReactionAt(900.f);
	TestEqual(TEXT("launch speed is K"), static_cast<float>(Launch.LaunchVelocity.Size()), 900.f, 0.01f);
	TestTrue(TEXT("launch keeps the direction"), (Launch.LaunchVelocity.GetSafeNormal() - Input.LaunchDirection).IsNearlyZero(1.0e-6));

	// Hitstun: max(6, floor(K x 0.0625 x multipliers)): 900 -> 56, x2 hitstun scale -> 112
	TestEqual(TEXT("hitstun from K"), Launch.HitstunFrames, 56);
	Input.Reaction.HitstunScale = 2.f;
	TestEqual(TEXT("hitstun scale"), ReactionAt(900.f).HitstunFrames, 112);
	Input.Reaction.HitstunScale = 1.f;
	Input.Hit.HitstunMultiplier = 0.5f;
	TestEqual(TEXT("hitstun multiplier of the hit"), ReactionAt(900.f).HitstunFrames, 28);
	TestEqual(TEXT("the floor keeps a stagger readable"), ReactionAt(50.f).HitstunFrames, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionPushBackOnVictimUpTest, "GSTNCombat.Core.Reaction.PushBackOnVictimUp", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionPushBackOnVictimUpTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	FGSTN_ReactionTuning Tuning = MakeTuning();
	Tuning.LaunchThreshold = 100000.f;
	TStrongObjectPtr<UGSTN_KnockbackModel_Fixed> Model = MakeModel<UGSTN_KnockbackModel_Fixed>(Tuning);
	FGSTN_KnockbackInput Input = MakeInput();
	Input.Hit.FixedKnockback = 400.f;
	Input.LaunchDirection = FVector(1.0, 0.0, 1.0).GetSafeNormal();

	// World up: the push is along +X. Gravity along +X (up = -X): the horizontal plane is YZ, so the push is along +Z.
	FGSTN_KnockbackResult Result = Model->Compute(Input);
	TestTrue(TEXT("world up: along X"), (Result.LaunchVelocity.GetSafeNormal() - FVector(1.0, 0.0, 0.0)).IsNearlyZero(1.0e-6));
	Input.Up = FVector(-1.0, 0.0, 0.0);
	Result = Model->Compute(Input);
	TestTrue(TEXT("tilted up: along Z, never world Z logic"), (Result.LaunchVelocity.GetSafeNormal() - FVector(0.0, 0.0, 1.0)).IsNearlyZero(1.0e-6));
	TestTrue(TEXT("no component along the victim's up"), FMath::IsNearlyZero(FVector::DotProduct(Result.LaunchVelocity, Input.Up), 1.0e-9));

	// Straight along up: no horizontal direction, no push
	Input.LaunchDirection = FVector(-1.0, 0.0, 0.0);
	TestTrue(TEXT("no horizontal part, no push"), Model->Compute(Input).LaunchVelocity.IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionMaxReactionCapTest, "GSTNCombat.Core.Reaction.MaxReactionCap", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionMaxReactionCapTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	FGSTN_ReactionTuning Tuning = MakeTuning();
	Tuning.PushBackThreshold = 300.f;
	Tuning.LaunchThreshold = 600.f;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(Tuning);
	FGSTN_KnockbackInput Input = MakeInput();
	Input.Hit.FixedKnockback = 1000.f;

	TestEqual(TEXT("uncapped launches"), Model->Compute(Input).Reaction, EGSTN_HitReaction::Launch);
	Input.Reaction.MaxReaction = EGSTN_HitReaction::PushBack;
	FGSTN_KnockbackResult Result = Model->Compute(Input);
	TestEqual(TEXT("capped at push back"), Result.Reaction, EGSTN_HitReaction::PushBack);
	TestFalse(TEXT("a capped hit does not tumble"), Result.bTumble);
	TestTrue(TEXT("and stays horizontal"), FMath::IsNearlyZero(Result.LaunchVelocity.Z));
	Input.Reaction.MaxReaction = EGSTN_HitReaction::StaggerInPlace;
	Result = Model->Compute(Input);
	TestEqual(TEXT("capped in place"), Result.Reaction, EGSTN_HitReaction::StaggerInPlace);
	TestTrue(TEXT("no displacement"), Result.LaunchVelocity.IsNearlyZero());
	TestTrue(TEXT("the hitstun stays"), Result.HitstunFrames > 0);

	// A cap above the tier changes nothing
	Input.Hit.FixedKnockback = 100.f;
	Input.Reaction.MaxReaction = EGSTN_HitReaction::Launch;
	TestEqual(TEXT("a cap never raises the tier"), Model->Compute(Input).Reaction, EGSTN_HitReaction::StaggerInPlace);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionLighterNeverMilderTest, "GSTNCombat.Core.Reaction.LighterNeverMilder", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionLighterNeverMilderTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	FGSTN_ReactionTuning Tuning = MakeTuning();
	Tuning.PushBackThreshold = 300.f;
	Tuning.LaunchThreshold = 600.f;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> WeightDriven = MakeModel<UGSTN_KnockbackModel_WeightDriven>(Tuning);
	TStrongObjectPtr<UGSTN_KnockbackModel_PercentScaling> Percent = MakeModel<UGSTN_KnockbackModel_PercentScaling>(Tuning);
	TStrongObjectPtr<UGSTN_KnockbackModel_CappedPercent> Capped = MakeModel<UGSTN_KnockbackModel_CappedPercent>(Tuning);
	const UGSTN_KnockbackModel* Models[3] = { WeightDriven.Get(), Percent.Get(), Capped.Get() };

	for (int32 ModelIndex = 0; ModelIndex < 3; ++ModelIndex)
	{
		float LastKnockback = TNumericLimits<float>::Max();
		uint8 LastTier = 255;
		bool bMonotone = true;
		for (float Weight = 10.f; Weight <= 400.f; Weight += 10.f)
		{
			FGSTN_KnockbackInput Input = MakeInput();
			Input.Hit.KnockbackGrowth = 30.f;
			Input.DamagePercent = 40.f;
			Input.Weight = Weight;
			const FGSTN_KnockbackResult Result = Models[ModelIndex]->Compute(Input);
			// Heavier weight: K never rises, so the tier never rises
			bMonotone &= Result.Knockback <= LastKnockback && static_cast<uint8>(Result.Reaction) <= LastTier;
			LastKnockback = Result.Knockback;
			LastTier = static_cast<uint8>(Result.Reaction);
		}
		TestTrue(*FString::Printf(TEXT("model %d: a heavier victim is never launched harder"), ModelIndex), bMonotone);
	}

	// The same through WeightScale (what the health weight curve writes)
	float Last = TNumericLimits<float>::Max();
	bool bScaleMonotone = true;
	for (float Scale = 0.2f; Scale <= 2.f; Scale += 0.1f)
	{
		FGSTN_KnockbackInput Input = MakeInput();
		Input.Reaction.WeightScale = Scale;
		const float Knockback = WeightDriven->Compute(Input).Knockback;
		bScaleMonotone &= Knockback <= Last;
		Last = Knockback;
	}
	TestTrue(TEXT("a larger weight scale is never milder to the victim"), bScaleMonotone);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionZeroThresholdsAlwaysLaunchTest, "GSTNCombat.Core.Reaction.ZeroThresholdsAlwaysLaunch", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionZeroThresholdsAlwaysLaunchTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	// The C++ defaults of both tier thresholds are 0: today's behaviour, every staggered hit launches
	const FGSTN_ReactionTuning Defaults;
	TestEqual(TEXT("push back default"), Defaults.PushBackThreshold, 0.f);
	TestEqual(TEXT("launch default"), Defaults.LaunchThreshold, 0.f);
	TestEqual(TEXT("hitstop per knockback default"), Defaults.HitstopPerKnockback, 0.f);

	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput();
	for (const float Fixed : { 0.f, 1.f, 50.f, 500.f })
	{
		Input.Hit.FixedKnockback = Fixed;
		const FGSTN_KnockbackResult Result = Model->Compute(Input);
		TestEqual(*FString::Printf(TEXT("fixed %.0f launches"), Fixed), Result.Reaction, EGSTN_HitReaction::Launch);
		TestTrue(*FString::Printf(TEXT("fixed %.0f launches along the direction"), Fixed), (Result.LaunchVelocity - Input.LaunchDirection * Fixed).IsNearlyZero(1.0e-9));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionOwnerTwoHitExampleTest, "GSTNCombat.Core.Reaction.OwnerTwoHitExample", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionOwnerTwoHitExampleTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	FGSTN_ReactionTuning Tuning = MakeTuning();
	Tuning.PushBackThreshold = 300.f;
	Tuning.LaunchThreshold = 600.f;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(Tuning);

	// A move with bracket 200; the victim's weight is 100 at full health and 60 after the first hit (WeightScale 0.6)
	FGSTN_KnockbackInput Input = MakeInput();
	Input.Hit.BaseKnockback = 200.f;
	Input.Hit.KnockbackGrowth = 0.f;

	FGSTN_KnockbackResult First = Model->Compute(Input);
	TestEqual(TEXT("hit 1: K = 200"), First.Knockback, 200.f, 0.001f);
	TestTrue(TEXT("hit 1 staggers in place"), First.bStaggered && First.Reaction == EGSTN_HitReaction::StaggerInPlace);

	// The weight of hit 2 comes from the damage taken BEFORE it; this hit's own damage did not lighten hit 1
	Input.Reaction.WeightScale = 0.6f;
	FGSTN_KnockbackResult Second = Model->Compute(Input);
	TestEqual(TEXT("hit 2: K = 200 / 0.6"), Second.Knockback, 200.f / 0.6f, 0.01f);
	TestTrue(TEXT("hit 2 of the same move is still below launch"), Second.bStaggered && Second.Reaction == EGSTN_HitReaction::PushBack);

	// A stronger hit 2 launches: 400 / 0.6 = 667 >= 600
	Input.Hit.BaseKnockback = 400.f;
	Second = Model->Compute(Input);
	TestEqual(TEXT("stronger hit 2 launches"), Second.Reaction, EGSTN_HitReaction::Launch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionModifiersAppliedOnceTest, "GSTNCombat.Core.Reaction.ModifiersAppliedOnce", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionModifiersAppliedOnceTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	const FGSTN_ReactionTuning Tuning = MakeTuning();
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> WeightDriven = MakeModel<UGSTN_KnockbackModel_WeightDriven>(Tuning);
	TStrongObjectPtr<UGSTN_KnockbackModel_PercentScaling> Percent = MakeModel<UGSTN_KnockbackModel_PercentScaling>(Tuning);
	TStrongObjectPtr<UGSTN_KnockbackModel_CappedPercent> Capped = MakeModel<UGSTN_KnockbackModel_CappedPercent>(Tuning);
	TStrongObjectPtr<UGSTN_KnockbackModel_Fixed> Fixed = MakeModel<UGSTN_KnockbackModel_Fixed>(Tuning);
	const UGSTN_KnockbackModel* Models[4] = { WeightDriven.Get(), Percent.Get(), Capped.Get(), Fixed.Get() };
	const TCHAR* Names[4] = { TEXT("WeightDriven"), TEXT("PercentScaling"), TEXT("CappedPercent"), TEXT("Fixed") };

	for (int32 Index = 0; Index < 4; ++Index)
	{
		FGSTN_KnockbackInput Input = MakeInput();
		Input.DamagePercent = 40.f;
		const FGSTN_KnockbackResult Plain = Models[Index]->Compute(Input);

		// KnockbackScale once: exactly twice K (not four times), and the velocity follows
		Input.Reaction.KnockbackScale = 2.f;
		const FGSTN_KnockbackResult Scaled = Models[Index]->Compute(Input);
		TestEqual(*FString::Printf(TEXT("%s: KnockbackScale applied once"), Names[Index]), Scaled.Knockback, Plain.Knockback * 2.f, 0.001f);

		// HitstunScale once, on the K of the plain hit: the model never rescales it elsewhere
		Input = MakeInput();
		Input.DamagePercent = 40.f;
		Input.Reaction.HitstunScale = 3.f;
		const FGSTN_KnockbackResult Stunned = Models[Index]->Compute(Input);
		// K is reported as a float, so the expected frame count may sit one below at an integer boundary
		TestTrue(*FString::Printf(TEXT("%s: HitstunScale applied once"), Names[Index]), FMath::Abs(Stunned.HitstunFrames - FMath::Max(6, FMath::FloorToInt(static_cast<double>(Plain.Knockback) * 0.0625 * 3.0))) <= 1);
		TestEqual(*FString::Printf(TEXT("%s: HitstunScale leaves K alone"), Names[Index]), Stunned.Knockback, Plain.Knockback);

		// HitstopScale once, on both sides
		Input = MakeInput();
		Input.Reaction.HitstopScale = 2.f;
		const FGSTN_KnockbackResult Stopped = Models[Index]->Compute(Input);
		TestEqual(*FString::Printf(TEXT("%s: HitstopScale applied once"), Names[Index]), Stopped.HitstopFrames, 14);
	}

	// WeightScale once, in the models that read the weight: half the weight scale = twice the K (WeightDriven), a lighter victim in PercentScaling
	FGSTN_KnockbackInput Input = MakeInput();
	const float Heavy = WeightDriven->Compute(Input).Knockback;
	Input.Reaction.WeightScale = 0.5f;
	TestEqual(TEXT("WeightDriven: WeightScale applied once"), WeightDriven->Compute(Input).Knockback, Heavy * 2.f, 0.001f);
	Input.Reaction.WeightScale = 1.f;
	Input.Weight = 50.f;
	TestEqual(TEXT("WeightDriven: Weight and WeightScale are the same product"), WeightDriven->Compute(Input).Knockback, Heavy * 2.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionWeightDrivenHandValuesTest, "GSTNCombat.Core.Reaction.WeightDrivenHandValues", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionWeightDrivenHandValuesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput(); // bracket 300 + 20 x 10 = 500

	TestEqual(TEXT("weight at the reference: the bracket"), Model->Compute(Input).Knockback, 500.f, 0.001f);
	Input.Weight = 200.f;
	TestEqual(TEXT("twice the weight, half the knockback"), Model->Compute(Input).Knockback, 250.f, 0.001f);
	Input.Weight = 100.f;
	Input.Reaction.WeightScale = 0.5f;
	TestEqual(TEXT("half the weight scale, twice the knockback"), Model->Compute(Input).Knockback, 1000.f, 0.001f);

	// The victim's accumulated damage is NOT a term of this model
	Input = MakeInput();
	Input.DamagePercent = 300.f;
	TestEqual(TEXT("damage percent does not enter"), Model->Compute(Input).Knockback, 500.f, 0.001f);

	// A fixed knockback replaces the bracket, the weight still applies: 120 x 100 / 200 = 60
	Input.Hit.FixedKnockback = 120.f;
	Input.Weight = 200.f;
	TestEqual(TEXT("fixed knockback replaces the bracket"), Model->Compute(Input).Knockback, 60.f, 0.001f);

	// MinWeight keeps a weightless victim finite: EW = 1 -> 500 x 100 / 1
	Input = MakeInput();
	Input.Weight = 0.f;
	TestEqual(TEXT("the weight floor"), Model->Compute(Input).Knockback, 50000.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionPercentScalingHandValuesTest, "GSTNCombat.Core.Reaction.PercentScalingHandValues", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionPercentScalingHandValuesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_PercentScaling> Model = MakeModel<UGSTN_KnockbackModel_PercentScaling>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput();
	Input.Hit.Damage = 10.f;
	Input.Hit.BaseKnockback = 30.f;
	Input.Hit.KnockbackGrowth = 50.f;
	Input.DamagePercent = 40.f;
	Input.Weight = 100.f;

	// P = 40 + 10 = 50; term = (50/10 + 50 x 10/20) x 200/(100+100) x 1.4 + 18 = 30 x 1 x 1.4 + 18 = 60
	// K = (60 x 50/100 + 30) x 10 = 600
	TestEqual(TEXT("weight 100, 40 percent"), Model->Compute(Input).Knockback, 600.f, 0.01f);

	// Weight 300: 200/400 = 0.5 -> term = 30 x 0.5 x 1.4 + 18 = 39; K = (39 x 0.5 + 30) x 10 = 495
	Input.Weight = 300.f;
	TestEqual(TEXT("heavier victim"), Model->Compute(Input).Knockback, 495.f, 0.01f);

	// No percent, weight 100: P = 10; term = (1 + 5) x 1 x 1.4 + 18 = 26.4; K = (13.2 + 30) x 10 = 432
	Input.Weight = 100.f;
	Input.DamagePercent = 0.f;
	TestEqual(TEXT("fresh victim"), Model->Compute(Input).Knockback, 432.f, 0.01f);

	// More percent, more knockback
	Input.DamagePercent = 120.f;
	TestTrue(TEXT("percent raises K"), Model->Compute(Input).Knockback > 600.f);

	// A fixed knockback is the knockback
	Input.Hit.FixedKnockback = 250.f;
	TestEqual(TEXT("fixed knockback"), Model->Compute(Input).Knockback, 250.f, 0.001f);
	TestTrue(TEXT("the model says it counts the percent"), Model->UsesPercentTerm());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionFixedIgnoresPercentTest, "GSTNCombat.Core.Reaction.FixedIgnoresPercent", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionFixedIgnoresPercentTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_Fixed> Model = MakeModel<UGSTN_KnockbackModel_Fixed>(MakeTuning());
	FGSTN_KnockbackInput Input = MakeInput();

	// No growth, no percent, no weight: the base knockback
	const float Plain = Model->Compute(Input).Knockback;
	TestEqual(TEXT("base knockback"), Plain, 300.f, 0.001f);
	Input.DamagePercent = 900.f;
	Input.Weight = 20.f;
	Input.Reaction.WeightScale = 0.3f;
	TestEqual(TEXT("percent, weight and weight scale are ignored"), Model->Compute(Input).Knockback, Plain, 0.001f);
	Input.Hit.FixedKnockback = 123.f;
	TestEqual(TEXT("a fixed knockback is used when set"), Model->Compute(Input).Knockback, 123.f, 0.001f);
	TestFalse(TEXT("it does not count the percent"), Model->UsesPercentTerm());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionCappedPercentCapTest, "GSTNCombat.Core.Reaction.CappedPercentCap", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionCappedPercentCapTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_CappedPercent> Capped = MakeModel<UGSTN_KnockbackModel_CappedPercent>(MakeTuning());
	TStrongObjectPtr<UGSTN_KnockbackModel_PercentScaling> Uncapped = MakeModel<UGSTN_KnockbackModel_PercentScaling>(MakeTuning());
	Capped->PercentCap = 100.f;
	FGSTN_KnockbackInput Input = MakeInput();

	Input.DamagePercent = 100.f;
	const float AtCap = Capped->Compute(Input).Knockback;
	TestEqual(TEXT("at the cap it is the plain formula"), AtCap, Uncapped->Compute(Input).Knockback, 0.001f);
	Input.DamagePercent = 5000.f;
	TestEqual(TEXT("past the cap nothing grows"), Capped->Compute(Input).Knockback, AtCap, 0.001f);
	TestTrue(TEXT("the uncapped model keeps growing"), Uncapped->Compute(Input).Knockback > AtCap);
	Input.DamagePercent = 50.f;
	TestTrue(TEXT("below the cap it follows the percent"), Capped->Compute(Input).Knockback < AtCap);
	Input.DamagePercent = -30.f;
	FGSTN_KnockbackInput Zero = Input;
	Zero.DamagePercent = 0.f;
	TestEqual(TEXT("a negative percent counts as 0"), Capped->Compute(Input).Knockback, Capped->Compute(Zero).Knockback, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionBakedCurveDeterministicTest, "GSTNCombat.Core.Reaction.BakedCurveDeterministic", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionBakedCurveDeterministicTest::RunTest(const FString& Parameters)
{
	FGSTN_BakedCurve A;
	A.BakeLinear(1.0, 0.6);
	FGSTN_BakedCurve B;
	B.Bake([](double T) { return 1.0 + (0.6 - 1.0) * T; });
	TestTrue(TEXT("authored"), A.IsAuthored() && A.Samples.Num() == FGSTN_BakedCurve::SampleCount);
	TestEqual(TEXT("two bakes of the same function are the same samples"), A.Samples, B.Samples);

	TestEqual(TEXT("1.0 at 0 percent"), A.EvaluateFixed(0.0), 16384);
	TestEqual(TEXT("0.6 at the cap: round(0.6 x 16384) = 9830"), A.EvaluateFixed(1.0), 9830);
	TestEqual(TEXT("clamped below 0"), A.EvaluateFixed(-5.0), 16384);
	TestEqual(TEXT("clamped above 1"), A.EvaluateFixed(7.0), 9830);
	TestEqual(TEXT("NaN reads as 0"), A.EvaluateFixed(std::numeric_limits<double>::quiet_NaN()), 16384);
	TestEqual(TEXT("float view"), A.Evaluate(0.0), 1.f);

	// Between two samples: the midpoint of sample 31 and 32 (input 0.5 -> index 31, fraction 0.5)
	const int32 Mid = A.EvaluateFixed(0.5);
	const int32 S31 = A.Samples[31], S32 = A.Samples[32];
	TestEqual(TEXT("integer interpolation of the midpoint"), Mid, S31 + (S32 - S31) / 2);

	// Non increasing over a fine sweep, identical on a second pass
	TArray<int32> First, Second;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		TArray<int32>& Out = Pass == 0 ? First : Second;
		for (int32 Step = 0; Step <= 2000; ++Step)
		{
			Out.Add(A.EvaluateFixed(Step / 2000.0));
		}
	}
	TestEqual(TEXT("bitwise identical on two runs"), First, Second);
	bool bNonIncreasing = true;
	for (int32 Index = 1; Index < First.Num(); ++Index)
	{
		bNonIncreasing &= First[Index] <= First[Index - 1];
	}
	TestTrue(TEXT("a falling curve never rises"), bNonIncreasing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionFlatCurveNoChangeTest, "GSTNCombat.Core.Reaction.FlatCurveNoChange", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionFlatCurveNoChangeTest::RunTest(const FString& Parameters)
{
	FGSTN_BakedCurve Flat;
	Flat.BakeConstant(1.0);
	TestTrue(TEXT("flat"), Flat.IsFlat());
	bool bAllOne = true;
	for (int32 Step = 0; Step <= 500; ++Step)
	{
		bAllOne &= Flat.EvaluateFixed(Step / 500.0) == FGSTN_BakedCurve::One;
	}
	TestTrue(TEXT("a flat 1.0 curve changes no weight anywhere"), bAllOne);

	// A curve that is not authored is the identity too, but it is not "flat": the next source applies
	const FGSTN_BakedCurve Empty;
	TestFalse(TEXT("not authored"), Empty.IsAuthored());
	TestFalse(TEXT("not authored is not flat"), Empty.IsFlat());
	TestEqual(TEXT("not authored reads as 1.0"), Empty.EvaluateFixed(0.37), FGSTN_BakedCurve::One);

	FGSTN_BakedCurve Falling;
	Falling.BakeLinear(1.0, 0.6);
	TestFalse(TEXT("a falling curve is not flat"), Falling.IsFlat());

	// Through the model: a flat curve value as WeightScale leaves the knockback as it is
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model = GSTNReactionTest::MakeModel<UGSTN_KnockbackModel_WeightDriven>(GSTNReactionTest::MakeTuning());
	FGSTN_KnockbackInput Input = GSTNReactionTest::MakeInput();
	const float Plain = Model->Compute(Input).Knockback;
	Input.Reaction.WeightScale = Flat.Evaluate(0.8);
	TestEqual(TEXT("knockback unchanged"), Model->Compute(Input).Knockback, Plain);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionBitwiseIdenticalTest, "GSTNCombat.Core.Reaction.BitwiseIdentical", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionBitwiseIdenticalTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	FGSTN_ReactionTuning Tuning = MakeTuning();
	Tuning.PushBackThreshold = 250.f;
	Tuning.LaunchThreshold = 700.f;
	Tuning.HitstopPerKnockback = 0.0078125f;
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> WeightDriven = MakeModel<UGSTN_KnockbackModel_WeightDriven>(Tuning);
	TStrongObjectPtr<UGSTN_KnockbackModel_PercentScaling> Percent = MakeModel<UGSTN_KnockbackModel_PercentScaling>(Tuning);
	TStrongObjectPtr<UGSTN_KnockbackModel_CappedPercent> Capped = MakeModel<UGSTN_KnockbackModel_CappedPercent>(Tuning);
	TStrongObjectPtr<UGSTN_KnockbackModel_Fixed> Fixed = MakeModel<UGSTN_KnockbackModel_Fixed>(Tuning);
	const UGSTN_KnockbackModel* Models[4] = { WeightDriven.Get(), Percent.Get(), Capped.Get(), Fixed.Get() };

	// A fixed pseudo random sweep of inputs (an LCG, not the engine's random, so it is the same everywhere), run twice
	TArray<FGSTN_KnockbackResult> Runs[2];
	for (int32 Run = 0; Run < 2; ++Run)
	{
		uint32 Seed = 12345u;
		auto Next = [&Seed]() { Seed = Seed * 1664525u + 1013904223u; return static_cast<double>(Seed >> 8) / 16777216.0; };
		for (int32 Case = 0; Case < 300; ++Case)
		{
			FGSTN_KnockbackInput Input = MakeInput();
			Input.Hit.Damage = static_cast<float>(Next() * 40.0);
			Input.Hit.BaseKnockback = static_cast<float>(Next() * 600.0);
			Input.Hit.KnockbackGrowth = static_cast<float>(Next() * 60.0);
			Input.Hit.HitstunMultiplier = static_cast<float>(0.5 + Next());
			Input.Weight = static_cast<float>(10.0 + Next() * 300.0);
			Input.DamagePercent = static_cast<float>(Next() * 250.0);
			Input.Reaction.WeightScale = static_cast<float>(0.4 + Next());
			Input.Reaction.KnockbackScale = static_cast<float>(0.5 + Next());
			Input.Reaction.StaggerThreshold.Value = static_cast<float>(Next() * 60.0 - 10.0);
			Input.LaunchDirection = FVector(Next() - 0.5, Next() - 0.5, Next() - 0.2);
			Runs[Run].Add(Models[Case % 4]->Compute(Input));
		}
	}
	bool bSame = Runs[0].Num() == Runs[1].Num();
	for (int32 Index = 0; bSame && Index < Runs[0].Num(); ++Index)
	{
		bSame = Same(Runs[0][Index], Runs[1][Index]);
	}
	TestTrue(TEXT("300 random inputs give the same results on two runs, bit for bit"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionValidatorPercentModelWithNonFlatCurveTest, "GSTNCombat.Core.Reaction.ValidatorPercentModelWithNonFlatCurve", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionValidatorPercentModelWithNonFlatCurveTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TStrongObjectPtr<UGSTN_KnockbackModel_PercentScaling> Percent = MakeModel<UGSTN_KnockbackModel_PercentScaling>(MakeTuning());
	TStrongObjectPtr<UGSTN_KnockbackModel_CappedPercent> Capped = MakeModel<UGSTN_KnockbackModel_CappedPercent>(MakeTuning());
	TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> WeightDriven = MakeModel<UGSTN_KnockbackModel_WeightDriven>(MakeTuning());
	FGSTN_BakedCurve Falling;
	Falling.BakeLinear(1.0, 0.6);
	FGSTN_BakedCurve Flat;
	Flat.BakeConstant(1.0);

	TArray<FGSTN_ReactionMessage> Messages;
	FGSTN_ReactionValidation::ValidateModelWithCurve(Percent.Get(), Falling, Messages);
	TestTrue(TEXT("percent model + falling curve warns"), HasMessage(Messages, false, TEXT("twice")));
	Messages.Reset();
	FGSTN_ReactionValidation::ValidateModelWithCurve(Capped.Get(), Falling, Messages);
	TestTrue(TEXT("capped percent + falling curve warns"), HasMessage(Messages, false, TEXT("twice")));
	Messages.Reset();
	FGSTN_ReactionValidation::ValidateModelWithCurve(Percent.Get(), Flat, Messages);
	TestEqual(TEXT("percent model + flat curve is fine"), Messages.Num(), 0);
	FGSTN_ReactionValidation::ValidateModelWithCurve(Percent.Get(), FGSTN_BakedCurve(), Messages);
	TestEqual(TEXT("percent model + no curve is fine"), Messages.Num(), 0);
	FGSTN_ReactionValidation::ValidateModelWithCurve(WeightDriven.Get(), Falling, Messages);
	TestEqual(TEXT("weight driven + falling curve is the standard"), Messages.Num(), 0);

	FGSTN_BakedCurve Short;
	Short.Samples = { 1, 2, 3 };
	FGSTN_ReactionValidation::ValidateModelWithCurve(WeightDriven.Get(), Short, Messages);
	TestTrue(TEXT("a curve with the wrong sample count is an error"), HasMessage(Messages, true, TEXT("64")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionValidatorThresholdOrderTest, "GSTNCombat.Core.Reaction.ValidatorThresholdOrder", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReactionValidatorThresholdOrderTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReactionTest;
	TArray<FGSTN_ReactionMessage> Messages;
	FGSTN_ReactionValidation::ValidateTuning(FGSTN_ReactionTuning(), Messages);
	TestEqual(TEXT("the defaults are valid"), Messages.Num(), 0);

	FGSTN_ReactionTuning Tuning = MakeTuning();
	Tuning.PushBackThreshold = 500.f;
	Tuning.LaunchThreshold = 400.f;
	FGSTN_ReactionValidation::ValidateTuning(Tuning, Messages);
	TestTrue(TEXT("push back above launch is an error"), HasMessage(Messages, true, TEXT("PushBackThreshold")));

	Messages.Reset();
	Tuning.PushBackThreshold = 300.f;
	Tuning.LaunchThreshold = 600.f;
	FGSTN_ReactionValidation::ValidateTuning(Tuning, Messages);
	TestEqual(TEXT("an ordered set is valid"), Messages.Num(), 0);

	Tuning.LaunchThreshold = 2000.f;
	FGSTN_ReactionValidation::ValidateTuning(Tuning, Messages);
	TestTrue(TEXT("launch above tumble is an error"), HasMessage(Messages, true, TEXT("TumbleThreshold")));

	Messages.Reset();
	Tuning = MakeTuning();
	Tuning.WeightReference = 0.f;
	FGSTN_ReactionValidation::ValidateTuning(Tuning, Messages);
	TestTrue(TEXT("a zero weight reference is an error"), HasMessage(Messages, true, TEXT("WeightReference")));
	Messages.Reset();
	Tuning = MakeTuning();
	Tuning.MaxHitstop = 0;
	FGSTN_ReactionValidation::ValidateTuning(Tuning, Messages);
	TestTrue(TEXT("no hitstop at all is a warning"), HasMessage(Messages, false, TEXT("MaxHitstop")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
