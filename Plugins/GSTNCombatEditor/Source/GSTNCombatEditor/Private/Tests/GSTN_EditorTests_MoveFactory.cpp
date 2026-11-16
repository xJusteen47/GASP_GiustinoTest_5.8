// The tests of the Combat Move factory (CP5 of the Blueprint seams): the starter timeline, the binding upsert, a real move Blueprint made in a
// temporary package and compiled, and the kit hookup on transient kit and weapon objects. No project content is loaded, no asset is saved.

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Engine/Blueprint.h"
#include "GameplayAbilityBlueprint.h"
#include "GameplayTagsManager.h"
#include "Graph/GSTN_MoveGraphAuthoring.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Misc/AutomationTest.h"
#include "Moves/GSTN_CombatMoveAuthoring.h"
#include "Moves/GSTN_CombatMoveFactory.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNMoveFactoryTest
{
	FGameplayTag AnyTag()
	{
		// Any registered tag will do as a command or a key here: the authoring code never checks the root (the kit's validator does)
		FGameplayTagContainer All;
		UGameplayTagsManager::Get().RequestAllGameplayTags(All, true);
		return All.IsEmpty() ? FGameplayTag() : All.First();
	}

	FString UniqueName(const TCHAR* Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveFactoryTimelineTest, "GSTNCombat.Editor.MoveFactory.StarterTimeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_MoveFactoryTimelineTest::RunTest(const FString& Parameters)
{
	using Lib = UGSTN_CombatMoveAuthoring;
	FGSTN_NewCombatMoveOptions Options;
	TestTrue(TEXT("the default parent is the combat ability"), Options.ParentClass == UGSTN_GA_BaseCombat::StaticClass());
	TestEqual(TEXT("the defaults are in order"), Lib::SanitizeOptions(Options).Num(), 0);

	// Strike: one hitbox track over the active frames, keys from the first to the last active frame
	FGSTN_CombatTimelineData Strike = Lib::MakeStarterTimeline(Options);
	TestEqual(TEXT("total"), Strike.TotalFrames, 30);
	TestEqual(TEXT("actionable"), Strike.FirstActionableFrame, 24);
	if (TestEqual(TEXT("one track"), Strike.Tracks.Num(), 1))
	{
		const FGSTN_TimelineTrack_Hitbox* Track = Strike.Tracks[0].GetPtr<FGSTN_TimelineTrack_Hitbox>();
		if (TestNotNull(TEXT("a hitbox track"), Track))
		{
			TestEqual(TEXT("from the first active frame"), Track->StartFrame, 6);
			TestEqual(TEXT("to the last (end exclusive)"), Track->EndFrame, 10);
			TestTrue(TEXT("one swept box with two keys"), Track->Hitboxes.Num() == 1 && Track->Hitboxes[0].Keys.Num() == 2 && Track->Hitboxes[0].bSweepBetweenFrames);
			TestTrue(TEXT("it hits"), Track->Hitboxes.Num() == 1 && Track->Hitboxes[0].Data.Damage > 0.f && Track->Hitboxes[0].Data.BaseKnockback > 0.f);
			TestEqual(TEXT("the last key on the last active frame"), Track->Hitboxes.Num() == 1 ? Track->Hitboxes[0].Keys[1].Frame : -1, 3);
		}
	}

	// Action: one Event key on the first active frame with the key tag, no hitbox
	Options.Template = EGSTN_CombatMoveTemplate::Action;
	Options.KeyTag = GSTNMoveFactoryTest::AnyTag();
	FGSTN_CombatTimelineData Action = Lib::MakeStarterTimeline(Options);
	const FGSTN_TimelineTrack_Event* Key = Action.Tracks.Num() == 1 ? Action.Tracks[0].GetPtr<FGSTN_TimelineTrack_Event>() : nullptr;
	if (TestNotNull(TEXT("one Event key"), Key))
	{
		TestEqual(TEXT("on the first active frame"), Key->StartFrame, 6);
		TestTrue(TEXT("with the key tag"), Key->KeyTag == Options.KeyTag);
	}
	Options.Template = EGSTN_CombatMoveTemplate::Empty;
	TestEqual(TEXT("empty = no track"), Lib::MakeStarterTimeline(Options).Tracks.Num(), 0);

	// Out-of-order frames are put in order, never refused
	FGSTN_NewCombatMoveOptions Bad;
	Bad.TotalFrames = 0;
	Bad.ActiveStart = 40;
	Bad.ActiveEnd = 2;
	Bad.FirstActionableFrame = 0;
	Bad.ParentClass = nullptr;
	const TArray<FString> Changes = Lib::SanitizeOptions(Bad);
	TestTrue(TEXT("every bad value is reported: ") + FString::Join(Changes, TEXT(" | ")), Changes.Num() >= 4);
	TestTrue(TEXT("and fixed"), Bad.TotalFrames == 1 && Bad.ActiveStart == 0 && Bad.ActiveEnd == 0 && Bad.FirstActionableFrame == 1 && Bad.ParentClass != nullptr);
	TestEqual(TEXT("timeline asset name"), Lib::TimelineAssetNameFor(TEXT("GA_Uppercut")), FString(TEXT("TL_Uppercut")));
	TestEqual(TEXT("timeline asset name without prefix"), Lib::TimelineAssetNameFor(TEXT("Uppercut")), FString(TEXT("TL_Uppercut")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveFactoryBindingTest, "GSTNCombat.Editor.MoveFactory.BindingUpsert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_MoveFactoryBindingTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Command = GSTNMoveFactoryTest::AnyTag();
	if (!TestTrue(TEXT("a registered tag to use as the command"), Command.IsValid()))
	{
		return false;
	}
	FGSTN_AbilityBinding Old;
	Old.Command = Command;
	Old.PriorityOffset = 7;
	FGSTN_AbilityBinding Chained = Old;
	Chained.Requirements.RequireTags.AddTag(Command);
	TArray<FGSTN_AbilityBinding> Bindings = { Old, Chained };

	FGSTN_AbilityBinding New;
	New.Command = Command;
	New.Ability = TSoftClassPtr<UGameplayAbility>(UGSTN_GA_BaseCombat::StaticClass());
	TestTrue(TEXT("same command and requirements: replaced"), UGSTN_CombatMoveAuthoring::UpsertBinding(Bindings, New));
	TestEqual(TEXT("in place"), Bindings.Num(), 2);
	TestTrue(TEXT("the first is the new one"), Bindings[0].PriorityOffset == 0 && !Bindings[0].Ability.IsNull());
	TestEqual(TEXT("the chained one is left alone"), Bindings[1].PriorityOffset, 7);

	New.Requirements.IgnoreTags.AddTag(Command);
	TestFalse(TEXT("other requirements: added"), UGSTN_CombatMoveAuthoring::UpsertBinding(Bindings, New));
	TestEqual(TEXT("at the end"), Bindings.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveFactoryCreateTest, "GSTNCombat.Editor.MoveFactory.CreatesAndBinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_MoveFactoryCreateTest::RunTest(const FString& Parameters)
{
	using Lib = UGSTN_CombatMoveAuthoring;
	const FGameplayTag Command = GSTNMoveFactoryTest::AnyTag();
	TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage(), *GSTNMoveFactoryTest::UniqueName(TEXT("KIT_MoveFactoryTest"))));
	TStrongObjectPtr<UGSTN_WeaponDefinition> Weapon(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage(), *GSTNMoveFactoryTest::UniqueName(TEXT("WPN_MoveFactoryTest"))));

	// A graph move with its timeline as an asset, bound to the kit and the weapon at creation
	const FString Name = GSTNMoveFactoryTest::UniqueName(TEXT("GA_MoveFactoryTest"));
	TStrongObjectPtr<UPackage> Package(CreatePackage(*(FString(TEXT("/Temp/")) + Name)));
	FGSTN_NewCombatMoveOptions Options;
	Options.Template = EGSTN_CombatMoveTemplate::Action;
	Options.bAddMoveEvents = true;
	Options.bTimelineAsAsset = true;
	Options.Hookup.Kit = Kit.Get();
	Options.Hookup.Weapon = Weapon.Get();
	Options.Hookup.Command = Command;
	FString Report;
	FString Error;
	UBlueprint* Move = Lib::CreateCombatMoveIn(Package.Get(), FName(*Name), RF_Transient, Options, Report, Error);
	if (!TestNotNull(TEXT("the move was made: ") + Error, Move) || !TestTrue(TEXT("no error: ") + Error, Error.IsEmpty()))
	{
		return false;
	}
	TestTrue(TEXT("a Gameplay Ability Blueprint"), Move->IsA<UGameplayAbilityBlueprint>());
	TestTrue(TEXT("a combat move"), Lib::IsCombatMoveBlueprint(Move));
	const UGSTN_GA_BaseCombat* Defaults = Move->GeneratedClass ? Cast<UGSTN_GA_BaseCombat>(Move->GeneratedClass->GetDefaultObject()) : nullptr;
	if (TestNotNull(TEXT("its class defaults"), Defaults))
	{
		UGSTN_CombatTimeline* Timeline = Defaults->TimelineAsset.Get();
		TestTrue(TEXT("points to its new timeline asset"), Timeline && Timeline->GetName().StartsWith(TEXT("TL_MoveFactoryTest")) && Timeline->Data.TotalFrames == 30);
		TestEqual(TEXT("the inline timeline stays empty"), Defaults->Timeline.TotalFrames, 0);
		if (Timeline)
		{
			// Not left behind as a standalone object of the editor session
			Timeline->ClearFlags(RF_Standalone | RF_Public);
			Timeline->GetOutermost()->SetDirtyFlag(false);
		}
	}
	const TArray<FString> Graph = UGSTN_MoveGraphAuthoring::DescribeEventGraph(Move);
	for (const TCHAR* Event : { TEXT("K2_OnMoveStarted"), TEXT("K2_OnMoveKey"), TEXT("K2_OnMoveHitConfirmed"), TEXT("K2_OnMoveEnded") })
	{
		TestTrue(FString::Printf(TEXT("the graph has %s: %s"), Event, *FString::Join(Graph, TEXT(" | "))),
			Graph.ContainsByPredicate([Event](const FString& Line)
			{
				// The dump shows the display name ("On Move Started"), not the K2_ function name
				return Line.Replace(TEXT(" "), TEXT("")).Contains(FString(Event).RightChop(3));
			}));
	}
	TArray<FString> Findings;
	TestEqual(TEXT("the empty move events break no simulation rule"), UGSTN_MoveGraphAuthoring::CheckSimulationRules(Move, Findings), 0);
	const TSoftClassPtr<UGameplayAbility> MoveClass(Move->GeneratedClass.Get());
	TestTrue(TEXT("bound on the kit: ") + Report, Kit->KitBindings.Num() == 1 && Kit->KitBindings[0].Command == Command && Kit->KitBindings[0].Ability == MoveClass);
	TestTrue(TEXT("bound on the weapon"), Weapon->Bindings.Num() == 1 && Weapon->Bindings[0].Ability == MoveClass);

	// A second move on the same command replaces the binding (Add to Kit or Weapon on another move)
	const FString Name2 = GSTNMoveFactoryTest::UniqueName(TEXT("GA_MoveFactoryTest2"));
	TStrongObjectPtr<UPackage> Package2(CreatePackage(*(FString(TEXT("/Temp/")) + Name2)));
	FGSTN_NewCombatMoveOptions Strike;
	UBlueprint* Move2 = Lib::CreateCombatMoveIn(Package2.Get(), FName(*Name2), RF_Transient, Strike, Report, Error);
	if (TestNotNull(TEXT("a plain strike: ") + Error, Move2))
	{
		const UGSTN_GA_BaseCombat* Defaults2 = Cast<UGSTN_GA_BaseCombat>(Move2->GeneratedClass->GetDefaultObject());
		TestTrue(TEXT("its timeline is inline"), Defaults2 && Defaults2->TimelineAsset.IsNull() && Defaults2->Timeline.TotalFrames == 30 && Defaults2->Timeline.Tracks.Num() == 1);
		TestTrue(TEXT("rebound: ") + Report, Lib::AddMoveToKitAssets(Move2->GeneratedClass.Get(), Kit.Get(), nullptr, Command, FGSTN_TagRequirements(), Report));
		TestTrue(TEXT("replaced, not added"), Kit->KitBindings.Num() == 1 && Kit->KitBindings[0].Ability == TSoftClassPtr<UGameplayAbility>(Move2->GeneratedClass.Get()));
		TestFalse(TEXT("no command: refused"), Lib::AddMoveToKitAssets(Move2->GeneratedClass.Get(), Kit.Get(), nullptr, FGameplayTag(), FGSTN_TagRequirements(), Report));
		TestTrue(TEXT("and says so"), Report.Contains(TEXT("nothing bound")));
	}
	TestFalse(TEXT("a class that is not a combat ability is refused"), [&]()
	{
		FGSTN_NewCombatMoveOptions Wrong;
		Wrong.ParentClass = nullptr;
		FString WrongError;
		// A null parent is sanitized to the combat ability; an outer of null is the refusal checked here
		return Lib::CreateCombatMoveIn(nullptr, TEXT("GA_Nothing"), RF_Transient, Wrong, Report, WrongError) != nullptr;
	}());

	// The factory carries the same options for a script (no dialog unattended)
	const UGSTN_CombatMoveFactory* Factory = GetDefault<UGSTN_CombatMoveFactory>();
	TestTrue(TEXT("the factory makes ability Blueprints"), Factory->SupportedClass == UGameplayAbilityBlueprint::StaticClass() && Factory->CanCreateNew());
	TestTrue(TEXT("its default parent"), Factory->Options.ParentClass.Get() == UGSTN_GA_BaseCombat::StaticClass());

	Package->SetDirtyFlag(false);
	Package2->SetDirtyFlag(false);
	return true;
}

#endif
