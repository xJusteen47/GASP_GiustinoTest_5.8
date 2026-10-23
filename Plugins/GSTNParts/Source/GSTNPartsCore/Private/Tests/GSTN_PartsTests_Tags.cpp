// Automation tests of the parts plugin: tags, settings and the module (plan P1).

#include "GameplayTagsManager.h"
#include "GSTN_PartsSettings.h"
#include "GSTN_PartsTags.h"
#include "GSTN_PartsTestUtils.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSmokeTest, "GSTNParts.Core.Smoke", GSTNPartsTest::Flags)
bool FGSTN_PartsSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("GSTNPartsCore module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("GSTNPartsCore")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTagsDocumentedTest, "GSTNParts.Core.Tags.AllRegisteredAndDocumented", GSTNPartsTest::Flags)
bool FGSTN_PartsTagsDocumentedTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTN_PartsTags::GetDeclaredTags(Tags);
	TestEqual(TEXT("Every native tag is listed"), Tags.Num(), 11);

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
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTagsNoStateTest, "GSTNParts.Core.Tags.NoStateTagDeclared", GSTNPartsTest::Flags)
bool FGSTN_PartsTagsNoStateTest::RunTest(const FString& Parameters)
{
	// Parts project Combat.Part.* and emit Combat.Event.Part.*; stagger and hitstop stay the combat core's tags (combat-parts.md §7)
	TArray<FGameplayTag> Tags;
	GSTN_PartsTags::GetDeclaredTags(Tags);
	const TCHAR* Forbidden[] = { TEXT("Combat.State"), TEXT("Combat.Mover"), TEXT("Combat.Lock"), TEXT("Combat.Link") };
	for (const FGameplayTag& Tag : Tags)
	{
		for (const TCHAR* Root : Forbidden)
		{
			const FGameplayTag RootTag = FGameplayTag::RequestGameplayTag(FName(Root), false);
			TestFalse(FString::Printf(TEXT("%s is not under %s"), *Tag.ToString(), Root), RootTag.IsValid() && Tag.MatchesTag(RootTag));
			TestFalse(FString::Printf(TEXT("%s does not start with %s."), *Tag.ToString(), Root), Tag.ToString().StartsWith(FString(Root) + TEXT(".")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTagsOwnedRootsTest, "GSTNParts.Core.Tags.NativeTagsUnderOwnedSubtrees", GSTNPartsTest::Flags)
bool FGSTN_PartsTagsOwnedRootsTest::RunTest(const FString& Parameters)
{
	// Each native tag sits in a subtree the plugin owns (modularity-and-tags 4.2): Combat.Part, Combat.Event.Part,
	// Combat.Entity.Destructible and the Message.Combat.PartBroken channel
	TArray<FGameplayTag> Tags;
	GSTN_PartsTags::GetDeclaredTags(Tags);
	for (const FGameplayTag& Tag : Tags)
	{
		const FString Name = Tag.ToString();
		const bool bOwned = Name.StartsWith(TEXT("Combat.Part.")) || Name.StartsWith(TEXT("Combat.Event.Part."))
			|| Name == TEXT("Combat.Entity.Destructible") || Name == TEXT("Message.Combat.PartBroken");
		TestTrue(FString::Printf(TEXT("%s is in a subtree owned by GSTNParts"), *Name), bOwned);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTagsIniVocabularyTest, "GSTNParts.Core.Tags.IniVocabularyRegistered", GSTNPartsTest::Flags)
bool FGSTN_PartsTagsIniVocabularyTest::RunTest(const FString& Parameters)
{
	// The engine reads Config/Tags only for the project and Game Feature plugins: the module registers the plugin's folder
	for (const TCHAR* Name : { TEXT("Part.Type.Head"), TEXT("Part.Type.Torso"), TEXT("Part.Type.Arm"), TEXT("Part.Type.Leg"),
		TEXT("Part.Type.Weapon"), TEXT("Part.Type.Armour"), TEXT("Part.Type.Crystal"),
		TEXT("GameplayCue.Combat.Part.Hit"), TEXT("GameplayCue.Combat.Part.Break"),
		TEXT("GameplayCue.Combat.Part.Ricochet"), TEXT("GameplayCue.Combat.Part.Shatter") })
	{
		TestTrue(FString::Printf(TEXT("%s comes from the plugin's tag ini"), Name),
			FGameplayTag::RequestGameplayTag(FName(Name), false).IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSettingsDefaultsTest, "GSTNParts.Core.Settings.Defaults", GSTNPartsTest::Flags)
bool FGSTN_PartsSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const UGSTN_PartsSettings& Settings = UGSTN_PartsSettings::Get();
	TestEqual(TEXT("Weight scale at the cap"), Settings.DefaultWeightScaleAtCap, 0.6f);
	TestEqual(TEXT("Minimum weight scale"), Settings.MinWeightScale, 0.35f);
	TestEqual(TEXT("Chunk pool"), Settings.PresentationBudget.MaxChunks, 48);
	TestEqual(TEXT("Chunk lifetime"), Settings.PresentationBudget.ChunkLifetime, 4.f);
	TestEqual(TEXT("Shard particles"), Settings.PresentationBudget.MaxShardParticles, 600);
	TestEqual(TEXT("Geometry Collections"), Settings.PresentationBudget.MaxGeometryCollections, 2);
	TestEqual(TEXT("Sleep after"), Settings.PresentationBudget.SleepAfter, 1.5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
