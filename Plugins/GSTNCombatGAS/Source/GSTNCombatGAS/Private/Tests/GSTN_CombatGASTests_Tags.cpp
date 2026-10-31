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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CombatGASHurtboxZonesTest, "GSTNCombat.GAS.Tags.HurtboxZonesRegistered",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_CombatGASHurtboxZonesTest::RunTest(const FString& Parameters)
{
	// The six zones are the designer-facing set of the hurtbox classification (combat-m5-editor 3c): each must be
	// registered, listed by GetDeclaredTags (so the generic comment test above covers it) and sit under one root
	TArray<FGameplayTag> Declared;
	GSTNCombatTags::GetDeclaredTags(Declared);

	const TArray<TPair<const TCHAR*, FGameplayTag>> Zones = {
		{ TEXT("Combat.Hurtbox.Head"), GSTNCombatTags::Hurtbox_Head },
		{ TEXT("Combat.Hurtbox.Torso"), GSTNCombatTags::Hurtbox_Torso },
		{ TEXT("Combat.Hurtbox.Arm"), GSTNCombatTags::Hurtbox_Arm },
		{ TEXT("Combat.Hurtbox.Leg"), GSTNCombatTags::Hurtbox_Leg },
		{ TEXT("Combat.Hurtbox.Hand"), GSTNCombatTags::Hurtbox_Hand },
		{ TEXT("Combat.Hurtbox.Foot"), GSTNCombatTags::Hurtbox_Foot } };

	const FGameplayTag Root = FGameplayTag::RequestGameplayTag(TEXT("Combat.Hurtbox"), false);
	TestTrue(TEXT("the Combat.Hurtbox root exists (implicit parent of the zones)"), Root.IsValid());
	for (const TPair<const TCHAR*, FGameplayTag>& Zone : Zones)
	{
		TestTrue(FString::Printf(TEXT("%s is registered"), Zone.Key), Zone.Value.IsValid());
		TestTrue(FString::Printf(TEXT("%s has the expected name"), Zone.Key), Zone.Value.GetTagName() == FName(Zone.Key));
		TestTrue(FString::Printf(TEXT("%s is listed by GetDeclaredTags"), Zone.Key), Declared.Contains(Zone.Value));
		TestTrue(FString::Printf(TEXT("%s is under Combat.Hurtbox"), Zone.Key), Zone.Value.MatchesTag(Root));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
