// Automation tests of the M6 follow-up 2 data seams (F2) in the core: the extra hurtboxes and their pick. Pure, no world.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Algo/Reverse.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Rules/GSTN_CombatRulesValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_Hurtbox F2MakeBody(uint8 Priority)
	{
		FGSTN_Hurtbox Box;
		Box.Priority = Priority;
		return Box;
	}

	FGSTN_ExtraHurtbox F2MakeExtra(uint8 Priority, const FGameplayTagContainer& Ignore = FGameplayTagContainer())
	{
		FGSTN_ExtraHurtbox Extra;
		Extra.Priority = Priority;
		Extra.IgnoreHitTypes = Ignore;
		return Extra;
	}

	/** The pick of the M4 follow-up (before F2), written out: the highest priority, ties to the lower index, invalid indices skipped */
	int32 F2ReferencePick(TConstArrayView<int32> Overlapped, TConstArrayView<FGSTN_Hurtbox> Hurtboxes)
	{
		int32 Best = INDEX_NONE;
		for (const int32 Index : Overlapped)
		{
			if (!Hurtboxes.IsValidIndex(Index))
			{
				continue;
			}
			if (Best == INDEX_NONE || Hurtboxes[Index].Priority > Hurtboxes[Best].Priority || (Hurtboxes[Index].Priority == Hurtboxes[Best].Priority && Index < Best))
			{
				Best = Index;
			}
		}
		return Best;
	}

	constexpr int32 F2Extra0 = GSTNHurtboxIndex::ExtraIndexBase;
	constexpr int32 F2Extra1 = GSTNHurtboxIndex::ExtraIndexBase + 1;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HurtboxExtraWinsByPriorityTest, "GSTNCombat.Core.Hurtbox.ExtraWinsByPriority", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HurtboxExtraWinsByPriorityTest::RunTest(const FString& Parameters)
{
	// Body priorities 0, 2, 2, 1; extras at priority 1 and 3
	const TArray<FGSTN_Hurtbox> Body = { F2MakeBody(0), F2MakeBody(2), F2MakeBody(2), F2MakeBody(1) };
	const TArray<FGSTN_ExtraHurtbox> Extras = { F2MakeExtra(1), F2MakeExtra(3) };
	const FGameplayTag Normal = GSTNCombatHitTags::HitType_Normal.GetTag();

	TestEqual(TEXT("a higher body priority beats a lower extra"), UGSTN_HitShapeUtils::PickHurtboxIndex({ 1, F2Extra0 }, Body, Extras, Normal), 1);
	TestEqual(TEXT("a higher extra priority beats the body"), UGSTN_HitShapeUtils::PickHurtboxIndex({ 1, F2Extra1 }, Body, Extras, Normal), F2Extra1);
	TestEqual(TEXT("the bubble wins over a body hurtbox it ties with"), UGSTN_HitShapeUtils::PickHurtboxIndex({ 3, F2Extra0 }, Body, Extras, Normal), F2Extra0);
	TestEqual(TEXT("two extras: the higher priority"), UGSTN_HitShapeUtils::PickHurtboxIndex({ F2Extra0, F2Extra1 }, Body, Extras, Normal), F2Extra1);
	TestEqual(TEXT("the order of the overlap list does not matter"), UGSTN_HitShapeUtils::PickHurtboxIndex({ F2Extra1, 2, F2Extra0, 1 }, Body, Extras, Normal), F2Extra1);

	// A tie between extras goes to the lower position, like the set
	const TArray<FGSTN_ExtraHurtbox> Tied = { F2MakeExtra(2), F2MakeExtra(2) };
	TestEqual(TEXT("extras tied: the lower position"), UGSTN_HitShapeUtils::PickHurtboxIndex({ F2Extra1, F2Extra0 }, Body, Tied, Normal), F2Extra0);
	TestEqual(TEXT("an extra at the priority of the body wins the tie"), UGSTN_HitShapeUtils::PickHurtboxIndex({ 1, F2Extra0 }, Body, Tied, Normal), F2Extra0);

	TestEqual(TEXT("an extra index past the list is skipped"), UGSTN_HitShapeUtils::PickHurtboxIndex({ F2Extra1, 2 }, Body, TArray<FGSTN_ExtraHurtbox>{ F2MakeExtra(9) }, Normal), 2);
	TestEqual(TEXT("nothing overlapped"), UGSTN_HitShapeUtils::PickHurtboxIndex({}, Body, Extras, Normal), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HurtboxExtraIgnoredByHitTypeTest, "GSTNCombat.Core.Hurtbox.ExtraIgnoredByHitType", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HurtboxExtraIgnoredByHitTypeTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_Hurtbox> Body = { F2MakeBody(0), F2MakeBody(1) };
	const FGameplayTag Grab = GSTNCombatHitTags::HitType_Grab.GetTag();
	const FGameplayTag Normal = GSTNCombatHitTags::HitType_Normal.GetTag();
	// A bubble that grabs reach through (priority 9, so it would win otherwise)
	const TArray<FGSTN_ExtraHurtbox> Extras = { F2MakeExtra(9, FGameplayTagContainer(Grab)) };

	TestEqual(TEXT("a normal hit lands on the bubble"), UGSTN_HitShapeUtils::PickHurtboxIndex({ 1, F2Extra0 }, Body, Extras, Normal), F2Extra0);
	TestEqual(TEXT("a grab passes through it to the body"), UGSTN_HitShapeUtils::PickHurtboxIndex({ 1, F2Extra0 }, Body, Extras, Grab), 1);
	TestEqual(TEXT("a grab that overlaps only the bubble hits nothing"), UGSTN_HitShapeUtils::PickHurtboxIndex({ F2Extra0 }, Body, Extras, Grab), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("an unset hit type is never ignored"), UGSTN_HitShapeUtils::PickHurtboxIndex({ 1, F2Extra0 }, Body, Extras, FGameplayTag()), F2Extra0);

	// The ignore list matches hierarchically: a parent tag ignores every hit type under it
	const TArray<FGSTN_ExtraHurtbox> Parent = { F2MakeExtra(9, FGameplayTagContainer(FGameplayTag::RequestGameplayTag(TEXT("Combat.HitType"), false))) };
	TestEqual(TEXT("the parent tag ignores a child type"), UGSTN_HitShapeUtils::PickHurtboxIndex({ 1, F2Extra0 }, Body, Parent, Normal), 1);

	// Only the extra that ignores the type is skipped: the other one still wins
	const TArray<FGSTN_ExtraHurtbox> Two = { F2MakeExtra(9, FGameplayTagContainer(Grab)), F2MakeExtra(5) };
	TestEqual(TEXT("the second extra still takes a grab"), UGSTN_HitShapeUtils::PickHurtboxIndex({ 1, F2Extra0, F2Extra1 }, Body, Two, Grab), F2Extra1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HurtboxNoExtraIdenticalToM4Test, "GSTNCombat.Core.Hurtbox.NoExtraIdenticalToM4", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HurtboxNoExtraIdenticalToM4Test::RunTest(const FString& Parameters)
{
	// Every non-empty subset (in two orders) of seven overlapped candidates, four of them valid body indices, three outside the set, against the M4 rule
	const TArray<FGSTN_Hurtbox> Body = { F2MakeBody(0), F2MakeBody(2), F2MakeBody(2), F2MakeBody(1) };
	const int32 Candidates[] = { 0, 1, 2, 3, 9, -1, 100 };
	const FGameplayTag Normal = GSTNCombatHitTags::HitType_Normal.GetTag();
	int32 Compared = 0;
	for (int32 Mask = 0; Mask < (1 << UE_ARRAY_COUNT(Candidates)); ++Mask)
	{
		TArray<int32> Overlapped;
		for (int32 Bit = 0; Bit < UE_ARRAY_COUNT(Candidates); ++Bit)
		{
			if (Mask & (1 << Bit))
			{
				Overlapped.Add(Candidates[Bit]);
			}
		}
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const int32 Expected = F2ReferencePick(Overlapped, Body);
			TestEqual(TEXT("the body-only overload is the M4 rule"), UGSTN_HitShapeUtils::PickHurtboxIndex(Overlapped, Body), Expected);
			TestEqual(TEXT("the extra overload with no extras is the M4 rule"), UGSTN_HitShapeUtils::PickHurtboxIndex(Overlapped, Body, TConstArrayView<FGSTN_ExtraHurtbox>(), Normal), Expected);
			TestEqual(TEXT("whatever the hit type"), UGSTN_HitShapeUtils::PickHurtboxIndex(Overlapped, Body, TConstArrayView<FGSTN_ExtraHurtbox>(), FGameplayTag()), Expected);
			Algo::Reverse(Overlapped);
			++Compared;
		}
	}
	TestEqual(TEXT("every combination was compared"), Compared, (1 << UE_ARRAY_COUNT(Candidates)) * 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HurtboxIndexRangeReservedTest, "GSTNCombat.Core.Hurtbox.IndexRangeReserved", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HurtboxIndexRangeReservedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("extras start at 0x80"), GSTNHurtboxIndex::ExtraIndexBase, 0x80);
	TestEqual(TEXT("an entity carries two extras"), GSTNHurtboxIndex::MaxExtraHurtboxes, 2);
	TestEqual(TEXT("the first extra is 0x80"), GSTNHurtboxIndex::FromExtraPosition(0), 0x80);
	TestEqual(TEXT("the second extra is 0x81"), GSTNHurtboxIndex::FromExtraPosition(1), 0x81);
	TestEqual(TEXT("and back"), GSTNHurtboxIndex::ToExtraPosition(0x81), 1);
	TestTrue(TEXT("0x80 is an extra"), GSTNHurtboxIndex::IsExtra(0x80));
	TestTrue(TEXT("0x81 is an extra"), GSTNHurtboxIndex::IsExtra(0x81));
	TestFalse(TEXT("0x7F is a set hurtbox"), GSTNHurtboxIndex::IsExtra(0x7F));
	TestFalse(TEXT("0x82 is not assigned"), GSTNHurtboxIndex::IsExtra(0x82));
	TestFalse(TEXT("an unset index (255) is not an extra"), GSTNHurtboxIndex::IsExtra(255));
	TestTrue(TEXT("an extra index fits the byte a hit carries"), GSTNHurtboxIndex::FromExtraPosition(GSTNHurtboxIndex::MaxExtraHurtboxes - 1) <= 255);

	// A set of 128 hurtboxes is the largest the validator accepts; a 129th would take the index of the first extra
	const TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
	for (int32 Index = 0; Index < GSTNHurtboxIndex::ExtraIndexBase; ++Index)
	{
		FGSTN_Hurtbox Box;
		Box.Name = *FString::Printf(TEXT("part%d"), Index);
		Box.Shape.Type = EGSTN_ShapeType::Sphere;
		Box.Shape.Extent = FVector(10.0);
		Box.Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Combat.Hurtbox.Torso"), false));
		Set->Hurtboxes.Add(Box);
	}
	TArray<FGSTN_ReactionMessage> Messages;
	FGSTN_CombatRulesValidation::ValidateHurtboxSet(*Set, Messages);
	int32 Errors = 0;
	for (const FGSTN_ReactionMessage& Message : Messages)
	{
		Errors += Message.bError ? 1 : 0;
	}
	TestEqual(TEXT("128 hurtboxes are valid"), Errors, 0);

	FGSTN_Hurtbox Extra = Set->Hurtboxes.Last();
	Extra.Name = TEXT("one too many");
	Set->Hurtboxes.Add(Extra);
	Messages.Reset();
	FGSTN_CombatRulesValidation::ValidateHurtboxSet(*Set, Messages);
	Errors = 0;
	for (const FGSTN_ReactionMessage& Message : Messages)
	{
		Errors += Message.bError ? 1 : 0;
	}
	TestEqual(TEXT("a 129th hurtbox would take an extra's index: an error"), Errors, 1);

	// An index from the reserved range is never read as a set hurtbox, even by a set too large to be valid
	TArray<FGSTN_Hurtbox> Large;
	Large.Init(F2MakeBody(0), 200);
	TestEqual(TEXT("0x90 with no extras is skipped, not taken as set hurtbox 144"), UGSTN_HitShapeUtils::PickHurtboxIndex({ 0x90 }, Large, TConstArrayView<FGSTN_ExtraHurtbox>(), FGameplayTag()), static_cast<int32>(INDEX_NONE));
	return true;
}

#endif
