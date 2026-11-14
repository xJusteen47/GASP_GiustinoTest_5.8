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

#endif
