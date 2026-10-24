#include "Misc/AutomationTest.h"
#include "GameplayTagsManager.h"
#include "GSTN_CombatTags.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CombatGASTagsTest, "GSTNCombat.GAS.Tags.AllRegisteredAndDocumented",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_CombatGASTagsTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTNCombatTags::GetDeclaredTags(Tags);
	TestTrue(TEXT("the module declares tags"), Tags.Num() > 0);

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

#endif // WITH_DEV_AUTOMATION_TESTS
