// Automation tests of the defence plugin: tags (plan F3).

#include "GameplayTagsManager.h"
#include "GSTN_DefenceTags.h"
#include "GSTN_DefenceTestUtils.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTagsDocumentedTest, "GSTNDefence.Core.Tags.AllRegisteredAndDocumented", GSTNDefenceTest::Flags)
bool FGSTN_DefenceTagsDocumentedTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTNDefenceTags::GetDeclaredTags(Tags);
	TestEqual(TEXT("Every native tag is listed"), Tags.Num(), 20);

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
		TestTrue(FString::Printf(TEXT("%s is declared explicitly"), *TagString), bExplicit);
		// The comment is the documentation (modularity-and-tags 4.8: at least 10 characters)
		TestTrue(FString::Printf(TEXT("%s has a comment of at least 10 characters"), *TagString), Comment.TrimStartAndEnd().Len() >= 10);
	}

	// The fixed-bubble decision: no poke event or tag exists
	TestFalse(TEXT("There is no ShieldPoked event"), FGameplayTag::RequestGameplayTag(TEXT("Combat.Event.ShieldPoked"), false).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTagsNoForeignRootTest, "GSTNDefence.Core.Tags.NoForeignRootDeclared", GSTNDefenceTest::Flags)
bool FGSTN_DefenceTagsNoForeignRootTest::RunTest(const FString& Parameters)
{
	// Defence declares nothing under the roots of the sibling and of the other plugins (m6 spec 1c): Combat.Link.* belongs to
	// GSTNLink, Combat.Part.* to GSTNParts, Interact.* to GSTNInteract. It reads those by name, never by extern.
	TArray<FGameplayTag> Tags;
	GSTNDefenceTags::GetDeclaredTags(Tags);
	const TCHAR* Forbidden[] = { TEXT("Combat.Link"), TEXT("Combat.Part"), TEXT("Combat.Lock.Link"), TEXT("Interact"), TEXT("Combat.HitType") };
	for (const FGameplayTag& Tag : Tags)
	{
		const FString Name = Tag.ToString();
		for (const TCHAR* Root : Forbidden)
		{
			const FGameplayTag RootTag = FGameplayTag::RequestGameplayTag(FName(Root), false);
			TestFalse(FString::Printf(TEXT("%s is not under %s"), *Name, Root), RootTag.IsValid() && Tag.MatchesTag(RootTag));
			TestFalse(FString::Printf(TEXT("%s does not start with %s."), *Name, Root), Name.StartsWith(FString(Root) + TEXT(".")));
		}

		// Each native tag sits in a subtree the plugin owns (m6 spec 1c)
		const bool bOwned = Name.StartsWith(TEXT("Combat.State.")) || Name == TEXT("Combat.Mover.Tumble")
			|| Name == TEXT("Combat.Immunity.Parry") || Name == TEXT("Combat.Immunity.Shield")
			|| Name == TEXT("Command.Defence.Shield") || Name == TEXT("Input.Lock.Command.Defence")
			|| Name.StartsWith(TEXT("Combat.Event.")) || Name == TEXT("Combat.SetByCaller.ShieldDamage");
		TestTrue(FString::Printf(TEXT("%s is in a subtree owned by GSTNDefence"), *Name), bOwned);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
