// Automation tests of the spin and drill plugin: tags (plan D1).

#include "GameplayTagsManager.h"
#include "GSTN_SpinDrillTags.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SpinDrillTagsTest, "GSTNSpinDrill.Core.Tags.AllRegisteredAndDocumented",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_SpinDrillTagsTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTNSpinDrillTags::GetDeclaredTags(Tags);
	TestEqual(TEXT("Every native tag is listed"), Tags.Num(), 26);

	const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	TSet<FGameplayTag> Seen;
	for (const FGameplayTag& Tag : Tags)
	{
		const FString TagString = Tag.ToString();
		TestTrue(FString::Printf(TEXT("%s is registered"), *TagString), Tag.IsValid());
		bool bAlreadySeen = false;
		Seen.Add(Tag, &bAlreadySeen);
		TestFalse(FString::Printf(TEXT("%s is listed once"), *TagString), bAlreadySeen);
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
		// The comment is the documentation (modularity-and-tags 4.8: at least 10 characters)
		TestTrue(FString::Printf(TEXT("%s has a comment of at least 10 characters"), *TagString), Comment.TrimStartAndEnd().Len() >= 10);
	}

	// The command leaves sit under the combat plugin's family, which keeps its own leaves
	const FGameplayTag Dash = FGameplayTag::RequestGameplayTag(TEXT("Command.Movement.Dash"), false);
	TestTrue(TEXT("Command.Movement.Dash exists (declared by GSTNCombatCore)"), Dash.IsValid());
	TestFalse(TEXT("Command.Movement.Dash is not in this plugin's list"), Tags.Contains(Dash));

	// Reserved for later checkpoints, never declared by the skeleton
	TestFalse(TEXT("Combat.Link.Type.Drill is not declared yet (latch checkpoint)"),
		FGameplayTag::RequestGameplayTag(TEXT("Combat.Link.Type.Drill"), false).IsValid());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
