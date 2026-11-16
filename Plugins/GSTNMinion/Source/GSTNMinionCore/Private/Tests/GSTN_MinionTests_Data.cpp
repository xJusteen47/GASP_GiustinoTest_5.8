// Automation tests of the minion Core: native tags and the definition asset validation.

#include "Data/GSTN_MinionDefinition.h"
#include "GameplayTagsManager.h"
#include "GSTN_MinionTags.h"
#include "GSTN_MinionTestUtils.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Misc/DataValidation.h"
#include "Store/GSTN_MinionStore.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionTagsDeclaredTest, "GSTNMinion.Core.Tags.AllRegisteredAndUnique", GSTNMinionTest::Flags)
bool FGSTN_MinionTagsDeclaredTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTNMinionTags::GetDeclaredTags(Tags);
	TestEqual(TEXT("Every native tag is listed"), Tags.Num(), 21);

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

#if WITH_EDITOR // tag comments and sources are editor-only data (a Game target has no GetTagEditorData)
		FString Comment;
		FName Source;
		bool bExplicit = false;
		bool bRestricted = false;
		bool bAllowNonRestrictedChildren = false;
		TestTrue(FString::Printf(TEXT("%s is known to the tags manager"), *TagString),
			Manager.GetTagEditorData(Tag.GetTagName(), Comment, Source, bExplicit, bRestricted, bAllowNonRestrictedChildren));
		TestTrue(FString::Printf(TEXT("%s is declared explicitly"), *TagString), bExplicit);
		TestTrue(FString::Printf(TEXT("%s has a comment of at least 10 characters"), *TagString), Comment.TrimStartAndEnd().Len() >= 10);
#endif

		const bool bOwned = TagString == TEXT("Combat.Entity.Minion") || TagString.StartsWith(TEXT("Combat.Event.Minion"))
			|| TagString.StartsWith(TEXT("Minion."));
		TestTrue(FString::Printf(TEXT("%s is in a subtree owned by GSTNMinion"), *TagString), bOwned);
	}

	// Every action state and every tier maps to its own tag
	TSet<FGameplayTag> Mapped;
	for (uint8 State = 0; State < static_cast<uint8>(EGSTN_MinionActionState::Count); ++State)
	{
		const FGameplayTag Tag = GSTNMinionTags::GetActionStateTag(static_cast<EGSTN_MinionActionState>(State));
		TestTrue(FString::Printf(TEXT("Action state %d has a tag"), State), Tag.IsValid());
		bool bAlreadySeen = false;
		Mapped.Add(Tag, &bAlreadySeen);
		TestFalse(FString::Printf(TEXT("Action state %d has its own tag"), State), bAlreadySeen);
	}
	for (uint8 Tier = 0; Tier < static_cast<uint8>(EGSTN_MinionLODTier::Count); ++Tier)
	{
		const FGameplayTag Tag = GSTNMinionTags::GetLODTierTag(static_cast<EGSTN_MinionLODTier>(Tier));
		TestTrue(FString::Printf(TEXT("Tier %d has a tag"), Tier), Tag.IsValid());
	}
	TestFalse(TEXT("Out-of-range state gives the invalid tag"), GSTNMinionTags::GetActionStateTag(EGSTN_MinionActionState::Count).IsValid());

	// The ini leaves the test definitions and the README use (Config/Tags/GSTNMinion.ini)
	TestTrue(TEXT("Minion.Type.Grunt comes from the plugin ini"), FGameplayTag::RequestGameplayTag(TEXT("Minion.Type.Grunt"), false).IsValid());
	TestTrue(TEXT("Command.Minion.Attack.Swing comes from the plugin ini"), FGameplayTag::RequestGameplayTag(TEXT("Command.Minion.Attack.Swing"), false).IsValid());
	return true;
}

#if WITH_EDITOR

namespace
{
	struct FDefinitionFixture
	{
		UGSTN_MinionDefinition* Definition = nullptr;
		UGSTN_CombatTimeline* HitstunTimeline = nullptr;

		static UGSTN_CombatTimeline* MakeTimeline(int32 Frames)
		{
			UGSTN_CombatTimeline* Timeline = NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transient);
			Timeline->Data.TotalFrames = Frames;
			return Timeline;
		}

		FDefinitionFixture()
		{
			Definition = NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
			Definition->TypeTag = FGameplayTag::RequestGameplayTag(TEXT("Minion.Type.Grunt"), false);
			Definition->HurtboxSet = NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
			HitstunTimeline = MakeTimeline(20);
			Definition->Reactions.Hitstun = HitstunTimeline;
			Definition->Reactions.Death = MakeTimeline(40);

			FGSTN_MinionMove Swing;
			Swing.Command = FGameplayTag::RequestGameplayTag(TEXT("Command.Minion.Attack.Swing"), false);
			Swing.Timeline = MakeTimeline(30);
			Definition->Moves.Add(Swing);
		}

		/** Runs IsDataValid and reports the verdict and the finding counts */
		EDataValidationResult Validate(int32& OutErrors, int32& OutWarnings) const
		{
			FDataValidationContext Context;
			const EDataValidationResult Result = Definition->IsDataValid(Context);
			OutErrors = Context.GetNumErrors();
			OutWarnings = Context.GetNumWarnings();
			return Result;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionDefinitionValidTest, "GSTNMinion.Core.Definition.ValidAndInvalidCases", GSTNMinionTest::Flags)
bool FGSTN_MinionDefinitionValidTest::RunTest(const FString& Parameters)
{
	int32 Errors = 0;
	int32 Warnings = 0;

	{
		FDefinitionFixture Fixture;
		TestTrue(TEXT("Valid definition"), Fixture.Validate(Errors, Warnings) == EDataValidationResult::Valid);
		TestEqual(TEXT("with no error"), Errors, 0);
		TestEqual(TEXT("and no warning"), Warnings, 0);
		TestEqual(TEXT("The command maps to move index 0"), Fixture.Definition->FindMoveIndex(Fixture.Definition->Moves[0].Command), 0);
		TestEqual(TEXT("An unknown command maps to nothing"), Fixture.Definition->FindMoveIndex(FGameplayTag()), static_cast<int32>(INDEX_NONE));
	}

	const auto ExpectInvalid = [this](const TCHAR* What, TFunctionRef<void(FDefinitionFixture&)> Break)
	{
		FDefinitionFixture Fixture;
		Break(Fixture);
		int32 FindingErrors = 0;
		int32 FindingWarnings = 0;
		TestTrue(FString::Printf(TEXT("%s is invalid"), What), Fixture.Validate(FindingErrors, FindingWarnings) == EDataValidationResult::Invalid);
		TestTrue(FString::Printf(TEXT("%s reports an error"), What), FindingErrors > 0);
	};

	ExpectInvalid(TEXT("No type tag"), [](FDefinitionFixture& F) { F.Definition->TypeTag = FGameplayTag(); });
	ExpectInvalid(TEXT("The type root instead of a leaf"), [](FDefinitionFixture& F) { F.Definition->TypeTag = GSTNMinionTags::Type; });
	ExpectInvalid(TEXT("A tag outside Minion.Type"), [](FDefinitionFixture& F) { F.Definition->TypeTag = GSTNMinionTags::Entity_Minion; });
	ExpectInvalid(TEXT("Zero health"), [](FDefinitionFixture& F) { F.Definition->Stats.MaxHealth = 0.f; });
	ExpectInvalid(TEXT("Zero weight"), [](FDefinitionFixture& F) { F.Definition->Stats.Weight = 0.f; });
	ExpectInvalid(TEXT("No capsule"), [](FDefinitionFixture& F) { F.Definition->Stats.CapsuleRadius = 0.f; });
	ExpectInvalid(TEXT("No hurtbox set"), [](FDefinitionFixture& F) { F.Definition->HurtboxSet.Reset(); });
	ExpectInvalid(TEXT("No Hitstun timeline"), [](FDefinitionFixture& F) { F.Definition->Reactions.Hitstun.Reset(); });
	ExpectInvalid(TEXT("No Death timeline"), [](FDefinitionFixture& F) { F.Definition->Reactions.Death.Reset(); });
	ExpectInvalid(TEXT("A Hitstun timeline of 1024 frames"), [](FDefinitionFixture& F) { F.Definition->Reactions.Hitstun = FDefinitionFixture::MakeTimeline(GSTNMinion::MaxTimelineFrames + 1); });
	ExpectInvalid(TEXT("A move with no command"), [](FDefinitionFixture& F) { F.Definition->Moves[0].Command = FGameplayTag(); });
	ExpectInvalid(TEXT("A move with no timeline"), [](FDefinitionFixture& F) { F.Definition->Moves[0].Timeline.Reset(); });
	ExpectInvalid(TEXT("A move with an inverted range"), [](FDefinitionFixture& F) { F.Definition->Moves[0].MinRange = 500.f; F.Definition->Moves[0].MaxRange = 100.f; });
	ExpectInvalid(TEXT("A repeated command"), [](FDefinitionFixture& F) { const auto Copy = F.Definition->Moves[0]; F.Definition->Moves.Add(Copy); });
	ExpectInvalid(TEXT("32 moves"), [](FDefinitionFixture& F)
	{
		F.Definition->Moves.SetNum(GSTNMinion::MaxMoves + 1);
		for (FGSTN_MinionMove& Move : F.Definition->Moves)
		{
			Move = F.Definition->Moves[0];
		}
	});
	ExpectInvalid(TEXT("An inconsistent LOD override"), [](FDefinitionFixture& F)
	{
		F.Definition->bOverrideLOD = true;
		F.Definition->LODOverride.NearDistance = F.Definition->LODOverride.CombatDistance;
	});

	// Edges that stay valid
	{
		FDefinitionFixture Fixture;
		Fixture.Definition->Reactions.Hitstun = FDefinitionFixture::MakeTimeline(GSTNMinion::MaxTimelineFrames);
		TestTrue(TEXT("1023 frames is allowed"), Fixture.Validate(Errors, Warnings) == EDataValidationResult::Valid);
	}
	{
		FDefinitionFixture Fixture;
		Fixture.Definition->Reactions.PushBack.Reset();
		TestTrue(TEXT("PushBack is optional"), Fixture.Validate(Errors, Warnings) == EDataValidationResult::Valid);
	}
	{
		FDefinitionFixture Fixture;
		Fixture.Definition->Stats.WalkSpeed = 500.f;
		Fixture.Definition->Stats.RunSpeed = 300.f;
		TestTrue(TEXT("Run below walk is still valid"), Fixture.Validate(Errors, Warnings) == EDataValidationResult::Valid);
		TestEqual(TEXT("but warns once"), Warnings, 1);
	}
	{
		FDefinitionFixture Fixture;
		FGSTN_MinionLODParams Defaults;
		Defaults.CombatDistance = 111.f;
		TestEqual(TEXT("Without an override the project LOD is used"), Fixture.Definition->ResolveLODParams(Defaults).CombatDistance, 111.f);
		Fixture.Definition->bOverrideLOD = true;
		TestEqual(TEXT("With an override the type's own is used"), Fixture.Definition->ResolveLODParams(Defaults).CombatDistance, FGSTN_MinionLODParams().CombatDistance);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionLODSettingsValidTest, "GSTNMinion.Core.Definition.LODSettingsValidation", GSTNMinionTest::Flags)
bool FGSTN_MinionLODSettingsValidTest::RunTest(const FString& Parameters)
{
	UGSTN_MinionLODSettings* Settings = NewObject<UGSTN_MinionLODSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	{
		FDataValidationContext Context;
		TestTrue(TEXT("Defaults are valid"), Settings->IsDataValid(Context) == EDataValidationResult::Valid);
	}
	Settings->Params.FarDistance = Settings->Params.NearDistance - 1.f;
	{
		FDataValidationContext Context;
		TestTrue(TEXT("Decreasing borders are invalid"), Settings->IsDataValid(Context) == EDataValidationResult::Invalid);
		TestTrue(TEXT("with an error"), Context.GetNumErrors() > 0);
	}
	return true;
}

#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
