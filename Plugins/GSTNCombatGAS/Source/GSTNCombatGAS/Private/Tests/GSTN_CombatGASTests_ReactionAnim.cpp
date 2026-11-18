#include "Misc/AutomationTest.h"
#include "Cosmetics/GSTN_HitstopFeel.h"
#include "Cosmetics/GSTN_ReactionAnimSet.h"
#include "GSTN_CombatTags.h"
#include "GSTN_CombatCoreTags.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags ReactionTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FGSTN_ReactionAnimRule MakeReactionRule(FName Name, FGameplayTag Required, int32 Priority, FGameplayTag Blocked = FGameplayTag())
	{
		FGSTN_ReactionAnimRule Rule;
		Rule.Name = Name;
		Rule.RequiredTags.AddTag(Required);
		if (Blocked.IsValid())
		{
			Rule.BlockedTags.AddTag(Blocked);
		}
		Rule.Priority = Priority;
		return Rule;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionAnimSelectTest, "GSTNCombat.GAS.ReactionAnim.Select", ReactionTestFlags)
bool FGSTN_ReactionAnimSelectTest::RunTest(const FString& Parameters)
{
	UGSTN_ReactionAnimSet* Set = NewObject<UGSTN_ReactionAnimSet>();
	Set->Rules.Add(MakeReactionRule(TEXT("HitStand"), GSTNCombatTags::State_Hitstun, 0, GSTNCombatTags::State_Tumble));
	Set->Rules.Add(MakeReactionRule(TEXT("Splat"), GSTNCombatTags::State_Splat, 10));
	Set->Rules.Add(MakeReactionRule(TEXT("Tumble"), GSTNCombatTags::State_Tumble, 5));
	Set->Rules.Add(MakeReactionRule(TEXT("TumbleTwin"), GSTNCombatTags::State_Tumble, 5));
	FGSTN_ReactionAnimRule Empty;
	Empty.Name = TEXT("Empty");
	Empty.Priority = 100;
	Set->Rules.Add(Empty);

	FGameplayTagContainer Tags;
	TestEqual(TEXT("No state: no rule"), Set->SelectRule(Tags), INDEX_NONE);

	Tags.AddTag(GSTNCombatTags::State_Hitstun);
	TestEqual(TEXT("Hitstun alone: the hit stand"), Set->SelectRule(Tags), 0);

	Tags.AddTag(GSTNCombatTags::State_Splat);
	TestEqual(TEXT("Splat (projected with hitstun) wins by priority"), Set->SelectRule(Tags), 1);

	FGameplayTagContainer Tumbling;
	Tumbling.AddTag(GSTNCombatTags::State_Hitstun);
	Tumbling.AddTag(GSTNCombatTags::State_Tumble);
	TestEqual(TEXT("Blocked tag drops the hit stand; equal priorities keep the first listed"), Set->SelectRule(Tumbling), 2);
	return true;
}

// CP4: the hitstop tremble. Starts at full amplitude on the first frame, swaps side every drawn frame at 30 Hz / 60 fps, fades to nothing by
// DecaySeconds; a farther camera scales it up, capped
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitstopShakeTest, "GSTNCombat.GAS.ReactionAnim.HitstopShake", ReactionTestFlags)
bool FGSTN_HitstopShakeTest::RunTest(const FString& Parameters)
{
	const float Frame = 1.f / 60.f;
	TestEqual(TEXT("First frame: full amplitude, visibly off"), FGSTN_HitstopShake::Sample(3.f, 30.f, 0.2f, 0.f), 3.f, 1e-4f);
	const float Second = FGSTN_HitstopShake::Sample(3.f, 30.f, 0.2f, Frame);
	TestTrue(TEXT("Second frame: the other side, smaller"), Second < 0.f && FMath::Abs(Second) < 3.f);
	const float Third = FGSTN_HitstopShake::Sample(3.f, 30.f, 0.2f, 2.f * Frame);
	TestTrue(TEXT("Third frame: back, smaller again"), Third > 0.f && Third < 3.f && Third < FMath::Abs(Second) + 0.5f);
	TestEqual(TEXT("Over at DecaySeconds"), FGSTN_HitstopShake::Sample(3.f, 30.f, 0.2f, 0.2f), 0.f);
	TestEqual(TEXT("Before the hitstop: nothing"), FGSTN_HitstopShake::Sample(3.f, 30.f, 0.2f, -0.01f), 0.f);
	TestEqual(TEXT("No amplitude: nothing"), FGSTN_HitstopShake::Sample(0.f, 30.f, 0.2f, 0.05f), 0.f);

	FGSTN_HitstopShake Shake;
	Shake.ReferenceCameraDistanceCm = 400.f;
	Shake.MaxDistanceScale = 3.f;
	TestEqual(TEXT("Close camera: authored size"), Shake.DistanceScale(150.f), 1.f);
	TestEqual(TEXT("Twice as far: twice as big"), Shake.DistanceScale(800.f), 2.f, 1e-4f);
	TestEqual(TEXT("Far away: capped"), Shake.DistanceScale(5000.f), 3.f);
	TestTrue(TEXT("Victim shakes more than attacker by default"), Shake.VictimAmplitudeCm > Shake.AttackerAmplitudeCm);
	return true;
}

// Sakurai #1, #3, #4 (2026-11-18): the attacker shakes too but less; on the ground nothing along up; in the air every screen direction;
// the amplitude converges over the hitstop the hit gives
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitstopShakeSakuraiTest, "GSTNCombat.GAS.ReactionAnim.HitstopShakeSakurai", ReactionTestFlags)
bool FGSTN_HitstopShakeSakuraiTest::RunTest(const FString& Parameters)
{
	const FGSTN_HitstopShake Shake;
	TestTrue(TEXT("#1: the attacker shakes by default"), Shake.AttackerAmplitudeCm > 0.f);
	TestTrue(TEXT("#1: and less than the victim"), Shake.AttackerAmplitudeCm < Shake.VictimAmplitudeCm);
	TestTrue(TEXT("#4: decays over the real hitstop by default"), Shake.DecayMode == EGSTN_HitstopShakeDecay::OverHitstop);
	TestTrue(TEXT("#3: all axes in the air by default"), Shake.bAllAxesInAir);
	TestTrue(TEXT("#3: falling is air"), Shake.AirTags.HasTagExact(GSTNCombatTags::State_Falling));
	TestTrue(TEXT("#3: tumbling is air"), Shake.AirTags.HasTagExact(GSTNCombatTags::State_Tumble));

	// #4: a 5-frame hitstop (62.5 Hz) is over at 80 ms, a long one still shakes there
	const float ShortHitstop = 5.f * 0.016f;
	TestEqual(TEXT("#4: short hitstop, zero at its end"), FGSTN_HitstopShake::Sample(3.f, 30.f, ShortHitstop, ShortHitstop), 0.f);
	TestTrue(TEXT("#4: long hitstop, still shaking at 80 ms"), FMath::Abs(FGSTN_HitstopShake::Sample(3.f, 30.f, 0.4f, 0.08f)) > 1.f);
	TestTrue(TEXT("#4: the same moment is weaker in the short one"),
		FMath::Abs(FGSTN_HitstopShake::Sample(3.f, 30.f, ShortHitstop, 0.05f)) < FMath::Abs(FGSTN_HitstopShake::Sample(3.f, 30.f, 0.4f, 0.05f)));

	// #3 ground: a pitched camera's right with an up part loses it, keeps unit length
	const FVector Ground = FGSTN_HitstopFeelMath::GroundShakeDirection(FVector(0.0, 0.9, 0.3), FVector::UpVector);
	TestEqual(TEXT("#3: ground shake has nothing along up"), Ground.Z, 0.0, 1e-6);
	TestEqual(TEXT("#3: ground shake is a unit direction"), Ground.Size(), 1.0, 1e-5);
	TestTrue(TEXT("#3: a right along up gives no shake, not a vertical one"),
		FGSTN_HitstopFeelMath::GroundShakeDirection(FVector::UpVector, FVector::UpVector).IsZero());

	// #3 air: first cycle horizontal, then new axes that reach the vertical; unit length; deterministic
	const FVector2D First = FGSTN_HitstopFeelMath::AirShakeAxis(30.f, 0.f);
	TestEqual(TEXT("#3: air, first cycle horizontal"), First.X, 1.0, 1e-6);
	bool bVertical = false;
	for (int32 Cycle = 1; Cycle < 6; ++Cycle)
	{
		const FVector2D Axis = FGSTN_HitstopFeelMath::AirShakeAxis(30.f, (Cycle + 0.5f) / 30.f);
		TestEqual(TEXT("#3: air axis is a unit direction"), Axis.Size(), 1.0, 1e-5);
		bVertical |= FMath::Abs(Axis.Y) > 0.5;
	}
	TestTrue(TEXT("#3: air shakes vertically within a few cycles"), bVertical);
	TestTrue(TEXT("#3: air axis is the same for the same time"), FGSTN_HitstopFeelMath::AirShakeAxis(30.f, 0.1f).Equals(FGSTN_HitstopFeelMath::AirShakeAxis(30.f, 0.1f)));
	TestTrue(TEXT("#3: same axis within one cycle"), FGSTN_HitstopFeelMath::AirShakeAxis(30.f, 0.035f).Equals(FGSTN_HitstopFeelMath::AirShakeAxis(30.f, 0.04f)));
	return true;
}

// Sakurai #6 and #7 (2026-11-18): defaults of the pose blend and of the attacker's advance, and the advance math
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitstopPoseAndAdvanceTest, "GSTNCombat.GAS.ReactionAnim.HitstopPoseAndAdvance", ReactionTestFlags)
bool FGSTN_HitstopPoseAndAdvanceTest::RunTest(const FString& Parameters)
{
	const FGSTN_HitstopPoseBlend PoseBlend;
	TestTrue(TEXT("#6: on by default"), PoseBlend.bEnabled);
	TestEqual(TEXT("#6: about 4 frames of blend"), PoseBlend.BlendFrames, 4);
	TestEqual(TEXT("#6: holds on the 4th frame of the clip"), PoseBlend.HoldAtFrame, 4);
	TestTrue(TEXT("#6: the curve puts most of the pose on frame 1 (owner rule: the change reads at once)"),
		FAlphaBlend::AlphaToBlendOption(0.25f, PoseBlend.BlendOption) > 0.5f);

	FGSTN_HitstopAdvance Advance;
	TestTrue(TEXT("#7: per attack by default"), Advance.Mode == EGSTN_HitstopAdvanceMode::TaggedMoves);
	TestTrue(TEXT("#7: default tag"), Advance.MoveTag == GSTNCombatTags::Feel_HitstopAdvance);
	TestTrue(TEXT("#7: less than one frame"), Advance.AdvanceFrames > 0.f && Advance.AdvanceFrames < 1.f);
	FGameplayTagContainer Tagged;
	Tagged.AddTag(GSTNCombatTags::Feel_HitstopAdvance);
	const FGameplayTagContainer Untagged;
	TestTrue(TEXT("#7: a tagged move advances"), Advance.AppliesTo(Tagged));
	TestFalse(TEXT("#7: an untagged move does not"), Advance.AppliesTo(Untagged));
	Advance.Mode = EGSTN_HitstopAdvanceMode::AllMoves;
	TestTrue(TEXT("#7: AllMoves: every move"), Advance.AppliesTo(Untagged));
	Advance.Mode = EGSTN_HitstopAdvanceMode::Off;
	TestFalse(TEXT("#7: Off: none"), Advance.AppliesTo(Tagged));

	const float Max = 0.6f / 60.f;
	TestEqual(TEXT("#7: nothing at the impact"), FGSTN_HitstopFeelMath::AdvanceLead(Max, 0.f), 0.f);
	TestEqual(TEXT("#7: all of it at the end"), FGSTN_HitstopFeelMath::AdvanceLead(Max, 1.f), Max, 1e-7f);
	TestTrue(TEXT("#7: eases out (more than half at half time)"), FGSTN_HitstopFeelMath::AdvanceLead(Max, 0.5f) > 0.5f * Max);
	TestEqual(TEXT("#7: never past the total"), FGSTN_HitstopFeelMath::AdvanceLead(Max, 3.f), Max, 1e-7f);
	TestTrue(TEXT("#7: monotonic"), FGSTN_HitstopFeelMath::AdvanceLead(Max, 0.3f) < FGSTN_HitstopFeelMath::AdvanceLead(Max, 0.6f));
	TestEqual(TEXT("#7: after: waits for the simulation"), FGSTN_HitstopFeelMath::AbsorbLead(1.01f, 1.0f), 0.01f, 1e-6f);
	TestEqual(TEXT("#7: after: nothing once passed"), FGSTN_HitstopFeelMath::AbsorbLead(1.01f, 1.02f), 0.f);
	return true;
}

#endif
