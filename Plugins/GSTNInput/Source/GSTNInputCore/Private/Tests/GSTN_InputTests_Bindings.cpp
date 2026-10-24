#include "Tests/GSTN_TestUtils.h"
#include "Input/GSTN_CommandBindings.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNTest;

namespace
{
	FGSTN_TagRequirements Requiring(std::initializer_list<FGameplayTag> Required, std::initializer_list<FGameplayTag> Ignored = {})
	{
		FGSTN_TagRequirements Result;
		for (const FGameplayTag& Tag : Required)
		{
			Result.RequireTags.AddTag(Tag);
		}
		for (const FGameplayTag& Tag : Ignored)
		{
			Result.IgnoreTags.AddTag(Tag);
		}
		return Result;
	}

	FGameplayTagContainer Context(std::initializer_list<FGameplayTag> Tags)
	{
		FGameplayTagContainer Result;
		for (const FGameplayTag& Tag : Tags)
		{
			Result.AddTag(Tag);
		}
		return Result;
	}

	int32 Select(const TArray<FGSTN_TagRequirements>& Bindings, const FGameplayTagContainer& ContextTags)
	{
		TArray<const FGSTN_TagRequirements*> Candidates;
		for (const FGSTN_TagRequirements& Binding : Bindings)
		{
			Candidates.Add(&Binding);
		}
		return UGSTN_CommandBindingUtils::SelectBinding(Candidates, ContextTags);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BindingRequirementsTest, "GSTNInput.Core.Input.Bindings.Requirements", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BindingRequirementsTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Grounded = GSTNInputTags::Context_Grounded.GetTag();
	const FGameplayTag Airborne = GSTNInputTags::Context_Airborne.GetTag();
	const FGameplayTag OnWall = GSTNInputTags::Context_OnWall.GetTag();

	TestTrue(TEXT("empty requirements hold in any context"), FGSTN_TagRequirements().IsSatisfiedBy(Context({})));
	TestTrue(TEXT("empty requirements hold in a context with tags"), FGSTN_TagRequirements().IsSatisfiedBy(Context({ Airborne })));
	TestTrue(TEXT("a required tag present"), Requiring({ Airborne }).IsSatisfiedBy(Context({ Airborne, OnWall })));
	TestFalse(TEXT("a required tag missing"), Requiring({ Airborne }).IsSatisfiedBy(Context({ Grounded })));
	TestFalse(TEXT("one of two required tags missing"), Requiring({ Airborne, OnWall }).IsSatisfiedBy(Context({ Airborne })));
	TestFalse(TEXT("an ignored tag present"), Requiring({}, { Airborne }).IsSatisfiedBy(Context({ Airborne })));
	TestTrue(TEXT("an ignored tag absent"), Requiring({}, { Airborne }).IsSatisfiedBy(Context({ Grounded })));

	TestEqual(TEXT("required count"), Requiring({ Airborne, OnWall }, { Grounded }).GetRequiredCount(), 2);

	TestFalse(TEXT("one requires what the other ignores"), Requiring({ Grounded }).CanHoldTogether(Requiring({}, { Grounded })));
	TestFalse(TEXT("and the other way round"), Requiring({}, { Grounded }).CanHoldTogether(Requiring({ Grounded })));
	TestTrue(TEXT("different required tags can hold together"), Requiring({ Grounded }).CanHoldTogether(Requiring({ OnWall })));
	TestTrue(TEXT("empty requirements hold with anything"), FGSTN_TagRequirements().CanHoldTogether(Requiring({ Airborne })));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BindingSelectionTest, "GSTNInput.Core.Input.Bindings.Selection", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BindingSelectionTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Grounded = GSTNInputTags::Context_Grounded.GetTag();
	const FGameplayTag Airborne = GSTNInputTags::Context_Airborne.GetTag();
	const FGameplayTag OnWall = GSTNInputTags::Context_OnWall.GetTag();

	// A kit binding its jab on the ground, a neutral air in the air, and a wall attack on the wall.
	const TArray<FGSTN_TagRequirements> Bindings = { Requiring({ Grounded }), Requiring({ Airborne }), Requiring({ Airborne, OnWall }) };

	TestEqual(TEXT("grounded picks the grounded binding"), Select(Bindings, Context({ Grounded })), 0);
	TestEqual(TEXT("airborne picks the air binding"), Select(Bindings, Context({ Airborne })), 1);
	TestEqual(TEXT("on the wall the wall binding beats the air one"), Select(Bindings, Context({ Airborne, OnWall })), 2);
	TestEqual(TEXT("nothing holds"), Select(Bindings, Context({})), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("a context of other tags"), Select(Bindings, Context({ OnWall })), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no bindings"), Select({}, Context({ Grounded })), static_cast<int32>(INDEX_NONE));

	// Equal required counts: the first one holding wins.
	const TArray<FGSTN_TagRequirements> Ties = { Requiring({ Airborne }), Requiring({ Airborne }) };
	TestEqual(TEXT("a tie goes to the first"), Select(Ties, Context({ Airborne })), 0);
	const TArray<FGSTN_TagRequirements> FirstFails = { Requiring({ Grounded }), Requiring({ Airborne }) };
	TestEqual(TEXT("the first is skipped when it does not hold"), Select(FirstFails, Context({ Airborne })), 1);

	// A binding with no requirements is the fallback: any more specific one wins over it.
	const TArray<FGSTN_TagRequirements> WithFallback = { FGSTN_TagRequirements(), Requiring({ Airborne }) };
	TestEqual(TEXT("the specific binding beats the fallback"), Select(WithFallback, Context({ Airborne })), 1);
	TestEqual(TEXT("the fallback holds elsewhere"), Select(WithFallback, Context({ Grounded })), 0);

	// A null candidate is skipped.
	const FGSTN_TagRequirements Airbound = Requiring({ Airborne });
	const FGSTN_TagRequirements* WithNull[] = { nullptr, &Airbound };
	TestEqual(TEXT("null candidates are skipped"), UGSTN_CommandBindingUtils::SelectBinding(WithNull, Context({ Airborne })), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BindingConflictsTest, "GSTNInput.Core.Input.Bindings.Conflicts", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BindingConflictsTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Grounded = GSTNInputTags::Context_Grounded.GetTag();
	const FGameplayTag Airborne = GSTNInputTags::Context_Airborne.GetTag();
	const FGameplayTag OnWall = GSTNInputTags::Context_OnWall.GetTag();
	const FGameplayTag Jab = GSTNTestTags::Attack_Neutral.GetTag();
	const FGameplayTag Tilt = GSTNTestTags::Attack_Directional.GetTag();

	const FGSTN_TagRequirements AirA = Requiring({ Airborne });
	const FGSTN_TagRequirements AirB = Requiring({ Airborne });
	const FGSTN_TagRequirements Ground = Requiring({ Grounded });
	const FGSTN_TagRequirements GroundNotAir = Requiring({ Grounded }, { Airborne });
	const FGSTN_TagRequirements AirNotGround = Requiring({ Airborne }, { Grounded });
	const FGSTN_TagRequirements Wall = Requiring({ Airborne, OnWall });
	const FGSTN_TagRequirements WallOther = Requiring({ Airborne, Grounded });

	const auto Conflicts = [](std::initializer_list<FGSTN_BindingKey> Keys)
	{
		return UGSTN_CommandBindingUtils::FindBindingConflicts(MakeArrayView(Keys.begin(), static_cast<int32>(Keys.size())));
	};

	TestEqual(TEXT("two airborne bindings of one command"), Conflicts({ { Jab, &AirA }, { Jab, &AirB } }).Num(), 1);
	TestEqual(TEXT("the same requirements on different commands"), Conflicts({ { Jab, &AirA }, { Tilt, &AirB } }).Num(), 0);
	TestEqual(TEXT("grounded vs airborne: no context holds both"), Conflicts({ { Jab, &GroundNotAir }, { Jab, &AirNotGround } }).Num(), 0);
	TestEqual(TEXT("grounded vs airborne, one side excluding the other is enough"), Conflicts({ { Jab, &Ground }, { Jab, &AirNotGround } }).Num(), 0);
	TestEqual(TEXT("grounded vs airborne without exclusions can hold together"), Conflicts({ { Jab, &Ground }, { Jab, &AirA } }).Num(), 1);
	TestEqual(TEXT("a wall binding is more specific than an airborne one"), Conflicts({ { Jab, &AirA }, { Jab, &Wall } }).Num(), 0);
	TestEqual(TEXT("two wall style bindings with as many tags"), Conflicts({ { Jab, &Wall }, { Jab, &WallOther } }).Num(), 1);
	TestEqual(TEXT("three bindings, one clash"), Conflicts({ { Jab, &AirA }, { Jab, &Wall }, { Jab, &AirB } }).Num(), 1);
	TestEqual(TEXT("no bindings"), Conflicts({}).Num(), 0);
	TestEqual(TEXT("bindings without a command are the owner's problem"), Conflicts({ { FGameplayTag(), &AirA }, { FGameplayTag(), &AirB } }).Num(), 0);

	const TArray<FGSTN_BindingConflict> Found = Conflicts({ { Jab, &AirA }, { Jab, &AirB } });
	if (TestEqual(TEXT("one finding"), Found.Num(), 1))
	{
		TestTrue(TEXT("it points at both bindings"), Found[0].BindingA == 0 && Found[0].BindingB == 1);
		TestTrue(TEXT("and names the command"), Found[0].Message.Contains(TEXT("Command.Test.Attack.Neutral")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BindingExclusiveGroupsTest, "GSTNInput.Core.Input.Bindings.ExclusiveGroups", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BindingExclusiveGroupsTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Grounded = GSTNInputTags::Context_Grounded.GetTag();
	const FGameplayTag Airborne = GSTNInputTags::Context_Airborne.GetTag();
	const FGameplayTag OnWall = GSTNInputTags::Context_OnWall.GetTag();
	const FGameplayTag Jab = GSTNTestTags::Attack_Neutral.GetTag();

	const FGSTN_TagRequirements Ground = Requiring({ Grounded });
	const FGSTN_TagRequirements Air = Requiring({ Airborne });
	const FGSTN_TagRequirements Wall = Requiring({ OnWall });
	const FGSTN_TagRequirements GroundAndAir = Requiring({ Grounded, Airborne });

	FGSTN_ExclusiveTagGroup Pair;
	Pair.Tags.AddTag(Grounded);
	Pair.Tags.AddTag(Airborne);
	const TArray<FGSTN_ExclusiveTagGroup> WithPair = { Pair };

	const auto Conflicts = [](std::initializer_list<FGSTN_BindingKey> Keys, TConstArrayView<FGSTN_ExclusiveTagGroup> Groups)
	{
		return UGSTN_CommandBindingUtils::FindBindingConflicts(MakeArrayView(Keys.begin(), static_cast<int32>(Keys.size())), Groups);
	};

	TestEqual(TEXT("grounded vs airborne with the group: never together"), Conflicts({ { Jab, &Ground }, { Jab, &Air } }, WithPair).Num(), 0);
	TestEqual(TEXT("the same pair without groups: a conflict"), Conflicts({ { Jab, &Ground }, { Jab, &Air } }, {}).Num(), 1);
	TestEqual(TEXT("a requirement outside the group still holds with a member"), Conflicts({ { Jab, &Ground }, { Jab, &Wall } }, WithPair).Num(), 1);
	TestTrue(TEXT("the overload agrees"), !Ground.CanHoldTogether(Air, WithPair) && Ground.CanHoldTogether(Air));
	TestTrue(TEXT("the same tag in both is not a clash"), Ground.CanHoldTogether(Ground, WithPair));

	// Three members: any two different ones exclude each other.
	FGSTN_ExclusiveTagGroup Triple = Pair;
	Triple.Tags.AddTag(OnWall);
	const TArray<FGSTN_ExclusiveTagGroup> WithTriple = { Triple };
	TestEqual(TEXT("three-member group, ground vs wall"), Conflicts({ { Jab, &Ground }, { Jab, &Wall } }, WithTriple).Num(), 0);
	TestEqual(TEXT("three-member group, air vs wall"), Conflicts({ { Jab, &Air }, { Jab, &Wall } }, WithTriple).Num(), 0);
	TestEqual(TEXT("three-member group, ground vs air"), Conflicts({ { Jab, &Ground }, { Jab, &Air } }, WithTriple).Num(), 0);

	// A binding requiring two members of one group can never apply.
	const auto Impossible = [](std::initializer_list<FGSTN_BindingKey> Keys, TConstArrayView<FGSTN_ExclusiveTagGroup> Groups)
	{
		return UGSTN_CommandBindingUtils::FindImpossibleBindings(MakeArrayView(Keys.begin(), static_cast<int32>(Keys.size())), Groups);
	};
	const TArray<FGSTN_BindingConflict> Found = Impossible({ { Jab, &Ground }, { Jab, &GroundAndAir } }, WithPair);
	if (TestEqual(TEXT("one impossible binding"), Found.Num(), 1))
	{
		TestTrue(TEXT("it is the second, reported on itself"), Found[0].BindingA == 1 && Found[0].BindingB == 1);
		TestTrue(TEXT("and names the command"), Found[0].Message.Contains(TEXT("Command.Test.Attack.Neutral")));
	}
	TestEqual(TEXT("without groups nothing is impossible"), Impossible({ { Jab, &GroundAndAir } }, {}).Num(), 0);
	FGSTN_ExclusiveTagGroup Other;
	Other.Tags.AddTag(OnWall);
	const TArray<FGSTN_ExclusiveTagGroup> Unrelated = { Other };
	TestEqual(TEXT("a group the binding does not touch"), Impossible({ { Jab, &GroundAndAir } }, Unrelated).Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
