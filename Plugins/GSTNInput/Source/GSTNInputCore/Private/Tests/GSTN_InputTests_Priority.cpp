#include "Tests/GSTN_TestUtils.h"
#include "Tests/GSTN_TestCriteria.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNTest;

namespace
{
	using FCriteria = TArray<TInstancedStruct<FGSTN_PriorityCriterion>>;

	template<typename T>
	void AddCriterion(FCriteria& Criteria)
	{
		Criteria.Add(TInstancedStruct<FGSTN_PriorityCriterion>::Make<T>());
	}

	FCriteria SpecificityThenTier()
	{
		FCriteria Criteria;
		AddCriterion<FGSTN_PriorityCriterion_Specificity>(Criteria);
		AddCriterion<FGSTN_PriorityCriterion_Tier>(Criteria);
		AddCriterion<FGSTN_PriorityCriterion_ManualOffset>(Criteria);
		return Criteria;
	}

	FGSTN_CommandPriority MakePriority(std::initializer_list<int32> Scores)
	{
		FGSTN_CommandPriority Priority;
		for (const int32 Score : Scores)
		{
			Priority.Scores.Add(Score);
		}
		return Priority;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PriorityVectorTest, "GSTNInput.Core.Input.Priority.VectorComparison", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PriorityVectorTest::RunTest(const FString& Parameters)
{
	// The first score decides, later ones only break ties, whatever their size.
	TestTrue(TEXT("the first score outweighs the rest"), MakePriority({ 1, 99, 99 }) < MakePriority({ 2, 0, 0 }));
	TestFalse(TEXT("and not the other way round"), MakePriority({ 2, -50, -50 }) < MakePriority({ 1, 99, 99 }));
	TestTrue(TEXT("a tie on the first goes to the second"), MakePriority({ 5, 1 }) < MakePriority({ 5, 2 }));
	TestTrue(TEXT("a tie on the first two goes to the third"), MakePriority({ 5, 2, 0 }) < MakePriority({ 5, 2, 1 }));
	TestFalse(TEXT("not less than itself"), MakePriority({ 5, 2, 1 }) < MakePriority({ 5, 2, 1 }));
	TestTrue(TEXT("equal vectors are equal"), MakePriority({ 5, 2, 1 }) == MakePriority({ 5, 2, 1 }));
	TestFalse(TEXT("different vectors are not equal"), MakePriority({ 5, 2, 1 }) == MakePriority({ 5, 2, 2 }));

	// Negative scores are legal (a negative manual offset).
	TestTrue(TEXT("negative scores order too"), MakePriority({ 5, -3 }) < MakePriority({ 5, -1 }));

	// A shorter vector reads as zeros after its end, so the order stays total.
	TestTrue(TEXT("missing scores count as zero"), MakePriority({ 5 }) == MakePriority({ 5, 0, 0 }));
	TestTrue(TEXT("a positive extra score is higher"), MakePriority({ 5 }) < MakePriority({ 5, 1 }));
	TestTrue(TEXT("a negative extra score is lower"), MakePriority({ 5, -1 }) < MakePriority({ 5 }));

	// No criteria: every command ties.
	TestTrue(TEXT("empty vectors tie"), FGSTN_CommandPriority() == FGSTN_CommandPriority());
	TestFalse(TEXT("empty vectors are not ordered"), FGSTN_CommandPriority() < FGSTN_CommandPriority());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PriorityDefaultListTest, "GSTNInput.Core.Input.Priority.DefaultList", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PriorityDefaultListTest::RunTest(const FString& Parameters)
{
	// The class default is Tier, Specificity, ManualOffset.
	const FCriteria Defaults = UGSTN_InputSettings::MakeDefaultPriorityCriteria();
	TestEqual(TEXT("three default criteria"), Defaults.Num(), 3);
	if (Defaults.Num() == 3)
	{
		TestTrue(TEXT("tier first"), Defaults[0].GetPtr<FGSTN_PriorityCriterion_Tier>() != nullptr);
		TestTrue(TEXT("specificity second"), Defaults[1].GetPtr<FGSTN_PriorityCriterion_Specificity>() != nullptr);
		TestTrue(TEXT("manual offset third"), Defaults[2].GetPtr<FGSTN_PriorityCriterion_ManualOffset>() != nullptr);
	}
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	TestEqual(TEXT("a new settings object starts with them"), Settings->PriorityCriteria.Num(), 3);

	// The scores of a slot are the three values the old fixed priority held, in that order.
	FGSTN_InputSlotSpec Directional = Slot(GSTNTestTags::Attack_Directional,
		Pattern({ Press(Attack()), Direction(EGSTN_DirectionSpace::Facing, GSTN_DirectionMasks::AnyTilted, EGSTN_StickZone::Tilt) }));
	Directional.PriorityOffset = 7;
	const FGSTN_CommandPriority Priority = Directional.GetPriority(*Settings);
	TestEqual(TEXT("one score per criterion"), Priority.Scores.Num(), 3);
	if (Priority.Scores.Num() == 3)
	{
		TestEqual(TEXT("tier"), Priority.Scores[0], 100);
		TestEqual(TEXT("specificity"), Priority.Scores[1], Directional.Pattern.GetSpecificity());
		TestEqual(TEXT("offset"), Priority.Scores[2], 7);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PriorityOrderTest, "GSTNInput.Core.Input.Priority.CustomOrder", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PriorityOrderTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();

	// A chord of two attack-tier buttons against a single special: A, B and C pressed together.
	const TArray<FGSTN_InputSlotSpec> Slots = {
		Slot(GSTNTestTags::Special_Neutral, Pattern({ Press(Special()) })),
		Slot(GSTNTestTags::Attack_Directional, Pattern({ Chord(Attack(), Strong(), 2) })),
	};
	const auto Winner = [&]() { return ResolveSlots(Slots, PlayTo(*this, TEXT("5:+A 5:+B 5:+C"), *Settings), *Settings).SlotIndex; };

	TestEqual(TEXT("default order: the higher tier wins over the chord"), Winner(), 0);

	Settings->PriorityCriteria = SpecificityThenTier();
	TestEqual(TEXT("specificity before tier: the chord wins"), Winner(), 1);

	// Same list, but the crouch attack of the tier test against a special: the more demanding pattern wins too.
	const TArray<FGSTN_InputSlotSpec> CrouchSlots = {
		Slot(GSTNTestTags::Attack_CrouchDirectional, Pattern({ Press(Attack()), Hold(Crouch()), Direction(EGSTN_DirectionSpace::Facing, GSTN_DirectionMasks::AnyTilted, EGSTN_StickZone::Tilt) })),
		Slot(GSTNTestTags::Special_Neutral, Pattern({ Press(Special()) })) };
	TestEqual(TEXT("specificity before tier: the crouch attack beats the special"),
		ResolveSlots(CrouchSlots, PlayTo(*this, TEXT("0:+D 0:8t 5:+A 5:+C"), *Settings), *Settings).SlotIndex, 0);

	// Back to the default order: the special.
	Settings->PriorityCriteria = UGSTN_InputSettings::MakeDefaultPriorityCriteria();
	TestEqual(TEXT("default order again: the special"),
		ResolveSlots(CrouchSlots, PlayTo(*this, TEXT("0:+D 0:8t 5:+A 5:+C"), *Settings), *Settings).SlotIndex, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PriorityRemovedCriterionTest, "GSTNInput.Core.Input.Priority.RemovedCriterion", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PriorityRemovedCriterionTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> CrouchSlots = {
		Slot(GSTNTestTags::Attack_CrouchDirectional, Pattern({ Press(Attack()), Hold(Crouch()), Direction(EGSTN_DirectionSpace::Facing, GSTN_DirectionMasks::AnyTilted, EGSTN_StickZone::Tilt) })),
		Slot(GSTNTestTags::Special_Neutral, Pattern({ Press(Special()) })) };
	const auto ResolveCrouch = [&]() { return ResolveSlots(CrouchSlots, PlayTo(*this, TEXT("0:+D 0:8t 5:+A 5:+C"), *Settings), *Settings).SlotIndex; };
	TestEqual(TEXT("with the tier: the special"), ResolveCrouch(), 1);

	// Without the tier criterion the tiers no longer matter at all.
	FCriteria WithoutTier;
	AddCriterion<FGSTN_PriorityCriterion_Specificity>(WithoutTier);
	AddCriterion<FGSTN_PriorityCriterion_ManualOffset>(WithoutTier);
	Settings->PriorityCriteria = WithoutTier;
	TestEqual(TEXT("without the tier: the more demanding pattern"), ResolveCrouch(), 0);
	TestEqual(TEXT("one score fewer"), CrouchSlots[0].GetPriority(*Settings).Scores.Num(), 2);

	// Without the manual offset the offset no longer breaks a tie: the slot order does.
	TArray<FGSTN_InputSlotSpec> Tie = { Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(Attack()) })), Slot(GSTNTestTags::Attack_Directional, Pattern({ Press(Attack()) })) };
	Tie[1].PriorityOffset = 1;
	const auto TieWinner = [&]() { return ResolveSlots(Tie, PlayTo(*this, TEXT("5:+A"), *Settings), *Settings).SlotIndex; };
	TestEqual(TEXT("with the offset the second slot wins"), TieWinner(), 1);
	FCriteria Tiers;
	AddCriterion<FGSTN_PriorityCriterion_Tier>(Tiers);
	AddCriterion<FGSTN_PriorityCriterion_Specificity>(Tiers);
	Settings->PriorityCriteria = Tiers;
	TestEqual(TEXT("without the offset the first slot wins"), TieWinner(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PriorityCustomCriterionTest, "GSTNInput.Core.Input.Priority.CustomCriterion", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PriorityCustomCriterionTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();

	// Two commands on the same press, same tier, same pattern: a tie that the slot order resolves.
	const TArray<FGSTN_InputSlotSpec> Slots = {
		Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(Attack()) })),
		Slot(GSTNTestTags::Attack_Directional, Pattern({ Press(Attack()) })) };
	const auto Winner = [&]() { return ResolveSlots(Slots, PlayTo(*this, TEXT("5:+A"), *Settings), *Settings).SlotIndex; };
	TestEqual(TEXT("default list: the first slot"), Winner(), 0);

	// A criterion defined outside the plugin's three, in first place, picks the second command.
	FGSTN_PriorityCriterion_TestFavorite Favorite;
	Favorite.Favorite = GSTNTestTags::Attack_Directional;
	FCriteria Criteria = UGSTN_InputSettings::MakeDefaultPriorityCriteria();
	Criteria.Insert(TInstancedStruct<FGSTN_PriorityCriterion>::Make<FGSTN_PriorityCriterion_TestFavorite>(Favorite), 0);
	Settings->PriorityCriteria = Criteria;
	TestEqual(TEXT("the favorite command wins the tie"), Winner(), 1);
	TestEqual(TEXT("it adds a score"), Slots[1].GetPriority(*Settings).Scores.Num(), 4);

	// In first place it also outweighs the tier: the favorite attack beats a special pressed together with it.
	const TArray<FGSTN_InputSlotSpec> Mixed = { Slot(GSTNTestTags::Special_Neutral, Pattern({ Press(Special()) })), Slots[1] };
	TestEqual(TEXT("a custom first criterion outweighs the tier"), ResolveSlots(Mixed, PlayTo(*this, TEXT("5:+A 5:+C"), *Settings), *Settings).SlotIndex, 1);

	// In last place it only breaks ties the other criteria left.
	Criteria = UGSTN_InputSettings::MakeDefaultPriorityCriteria();
	Criteria.Add(TInstancedStruct<FGSTN_PriorityCriterion>::Make<FGSTN_PriorityCriterion_TestFavorite>(Favorite));
	Settings->PriorityCriteria = Criteria;
	TestEqual(TEXT("last place: the tier still wins"), ResolveSlots(Mixed, PlayTo(*this, TEXT("5:+A 5:+C"), *Settings), *Settings).SlotIndex, 0);
	TestEqual(TEXT("last place: it still breaks the tie"), Winner(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PriorityEmptyListTest, "GSTNInput.Core.Input.Priority.EmptyListUsesSlotOrder", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PriorityEmptyListTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	Settings->PriorityCriteria.Empty();

	// Attack first, special second: pressed together the special would win by tier, but with no criteria the order decides.
	const TArray<FGSTN_InputSlotSpec> Slots = { Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(Attack()) })), Slot(GSTNTestTags::Special_Neutral, Pattern({ Press(Special()) })) };
	TestEqual(TEXT("no scores"), Slots[0].GetPriority(*Settings).Scores.Num(), 0);
	TestEqual(TEXT("the first slot wins"), ResolveSlots(Slots, PlayTo(*this, TEXT("5:+A 5:+C"), *Settings), *Settings).SlotIndex, 0);

	const TArray<FGSTN_InputSlotSpec> Swapped = { Slots[1], Slots[0] };
	TestEqual(TEXT("swapping the slots swaps the winner"), ResolveSlots(Swapped, PlayTo(*this, TEXT("5:+A 5:+C"), *Settings), *Settings).SlotIndex, 0);

	// An entry left empty in the editor scores 0 and keeps the vector the same length for every slot.
	Settings->PriorityCriteria.Add(TInstancedStruct<FGSTN_PriorityCriterion>());
	TestEqual(TEXT("an empty entry scores zero"), Slots[1].GetPriority(*Settings).Scores.Num(), 1);
	TestEqual(TEXT("the empty entry is 0"), Slots[1].GetPriority(*Settings).Scores[0], 0);

	// More than the cap: the extra entries are ignored.
	Settings->PriorityCriteria.Empty();
	for (int32 Index = 0; Index < GSTNPriority::MaxCriteria + 3; ++Index)
	{
		AddCriterion<FGSTN_PriorityCriterion_ManualOffset>(Settings->PriorityCriteria);
	}
	TestEqual(TEXT("capped at the maximum"), Slots[0].GetPriority(*Settings).Scores.Num(), GSTNPriority::MaxCriteria);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
