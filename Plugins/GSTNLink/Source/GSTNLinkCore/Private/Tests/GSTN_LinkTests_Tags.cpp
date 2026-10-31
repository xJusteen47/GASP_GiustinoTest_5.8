// Automation tests of the link plugin: tags (plan F5a).

#include "GameplayTagsManager.h"
#include "GSTN_LinkTags.h"
#include "GSTN_LinkTestUtils.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkTagsDocumentedTest, "GSTNLink.Core.Tags.AllRegisteredAndDocumented", GSTNLinkTest::Flags)
bool FGSTN_LinkTagsDocumentedTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTNLinkTags::GetDeclaredTags(Tags);
	TestEqual(TEXT("Every native tag is listed"), Tags.Num(), 39);

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

	// Combat.HitType.Grab belongs to the combat core: this plugin reads it and never declares it
	const FGameplayTag GrabHitType = FGameplayTag::RequestGameplayTag(TEXT("Combat.HitType.Grab"), false);
	TestTrue(TEXT("Combat.HitType.Grab exists (declared by GSTNCombatCore)"), GrabHitType.IsValid());
	TestFalse(TEXT("Combat.HitType.Grab is not in this plugin's list"), Tags.Contains(GrabHitType));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkTagsNoForeignRootTest, "GSTNLink.Core.Tags.NoForeignRootDeclared", GSTNLinkTest::Flags)
bool FGSTN_LinkTagsNoForeignRootTest::RunTest(const FString& Parameters)
{
	// Link declares nothing under the roots of the other plugins (m6 spec 1c): Combat.State.* and the shield and parry immunities
	// belong to GSTNDefence, Combat.Part.* to GSTNParts, Command.Defence to the defence command set. It reads those by name.
	// The subtrees reserved for M7, M8, M10 and the drill belong to their own plugins.
	TArray<FGameplayTag> Tags;
	GSTNLinkTags::GetDeclaredTags(Tags);
	const TCHAR* Forbidden[] = {
		TEXT("Combat.State"), TEXT("Combat.Part"), TEXT("Combat.SetByCaller"), TEXT("Combat.Mover.Tumble"), TEXT("Command.Defence"),
		TEXT("Input.Lock.Command.Defence"), TEXT("Combat.Immunity.Parry"), TEXT("Combat.Immunity.Shield"), TEXT("Combat.HitType.Grab"),
		TEXT("Combat.Link.Type.Mount"), TEXT("Combat.Link.Type.Ride"), TEXT("Combat.Link.Type.Held"), TEXT("Combat.Link.Type.Minion"),
		TEXT("Combat.Link.Type.Drill"), TEXT("Combat.Link.Role.Carrier"), TEXT("Combat.Link.Role.Rider"), TEXT("Combat.Link.Role.Holder"),
		TEXT("Combat.Link.Role.HeldObject"), TEXT("Combat.Link.Role.Driller"), TEXT("Combat.Link.Role.Drilled") };
	for (const FGameplayTag& Tag : Tags)
	{
		const FString Name = Tag.ToString();
		for (const TCHAR* Root : Forbidden)
		{
			TestFalse(FString::Printf(TEXT("%s is not %s or under it"), *Name, Root), Name == Root || Name.StartsWith(FString(Root) + TEXT(".")));
		}

		// Each native tag sits in a subtree the plugin owns (m6 spec 1c)
		const bool bOwned = Name.StartsWith(TEXT("Combat.Link."))
			|| Name == TEXT("Combat.Immunity.Grab") || Name == TEXT("Combat.Immunity.Latch") || Name == TEXT("Combat.Lock.Link")
			|| Name == TEXT("Combat.HitType.Throw") || Name == TEXT("Combat.HitType.Pummel")
			|| Name == TEXT("Combat.Mover.Linked") || Name == TEXT("Interact.Target.Grab")
			|| Name.StartsWith(TEXT("Command.Link.")) || Name == TEXT("Input.Lock.Command.Link")
			|| Name == TEXT("Combat.Event.LinkFormed") || Name == TEXT("Combat.Event.LinkBroken")
			|| Name == TEXT("Combat.Event.Throw") || Name == TEXT("Combat.Event.GrabEscaped");
		TestTrue(FString::Printf(TEXT("%s is in a subtree owned by GSTNLink"), *Name), bOwned);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
