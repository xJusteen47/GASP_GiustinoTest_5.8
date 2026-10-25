#include "Misc/AutomationTest.h"
#include "GameplayTagsManager.h"
#include "GSTN_PartsTags.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_PARTS_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTagsRegisteredTest, "GSTNParts.Core.Tags.AllRegisteredAndDocumented", GSTN_PARTS_TEST_FLAGS)
bool FGSTN_PartsTagsRegisteredTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTN_PartsTags::GetDeclaredTags(Tags);
	TestEqual(TEXT("the module declares the eleven tags of combat-parts 7"), Tags.Num(), 11);

	const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	TSet<FGameplayTag> Seen;
	for (const FGameplayTag& Tag : Tags)
	{
		const FString TagString = Tag.ToString();
		TestTrue(FString::Printf(TEXT("%s is registered"), *TagString), Tag.IsValid());
		TestFalse(FString::Printf(TEXT("%s is listed once"), *TagString), Seen.Contains(Tag));
		Seen.Add(Tag);
		if (!Tag.IsValid())
		{
			continue;
		}

		FString Comment;
		FName Source;
		bool bExplicit = false;
		bool bRestricted = false;
		bool bAllowNonRestrictedChildren = false;
		TestTrue(FString::Printf(TEXT("%s is known to the tags manager"), *TagString),
			Manager.GetTagEditorData(Tag.GetTagName(), Comment, Source, bExplicit, bRestricted, bAllowNonRestrictedChildren));
		TestTrue(FString::Printf(TEXT("%s is declared explicitly"), *TagString), bExplicit);
		TestFalse(FString::Printf(TEXT("%s has a comment"), *TagString), Comment.TrimStartAndEnd().IsEmpty());
	}
	return true;
}

/** The part tags that are not parts' to declare: stagger and hitstop are the combat plugin's, and parts add no movement, lock or link tags. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTagsNoStateTest, "GSTNParts.Core.Tags.NoStateTagDeclared", GSTN_PARTS_TEST_FLAGS)
bool FGSTN_PartsTagsNoStateTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTN_PartsTags::GetDeclaredTags(Tags);

	static const TCHAR* const ForbiddenRoots[] = { TEXT("Combat.State"), TEXT("Combat.Mover"), TEXT("Combat.Lock"), TEXT("Combat.Link") };
	for (const FGameplayTag& Tag : Tags)
	{
		for (const TCHAR* Root : ForbiddenRoots)
		{
			const FGameplayTag RootTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(Root), false);
			TestFalse(FString::Printf(TEXT("%s is not under %s"), *Tag.ToString(), Root), RootTag.IsValid() && Tag.MatchesTag(RootTag));
			TestFalse(FString::Printf(TEXT("%s does not start with %s."), *Tag.ToString(), Root), Tag.ToString().StartsWith(FString(Root) + TEXT(".")));
		}
	}

	// The state tags the parts project are Combat.Part.*, never the combat plugin's Combat.State.*
	TestTrue(TEXT("the broken tag is under Combat.Part"), GSTN_PartsTags::Part_Broken.GetTag().ToString().StartsWith(TEXT("Combat.Part.")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
