#include "Misc/AutomationTest.h"
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

#endif
