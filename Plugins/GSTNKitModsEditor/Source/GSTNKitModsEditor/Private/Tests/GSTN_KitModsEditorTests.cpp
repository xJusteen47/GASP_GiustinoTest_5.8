// Automation tests of the kit mods editor tooling (M11 L8): the extension is registered with the M5 validation, the preview bakes what
// the compile bakes, and the mod, kit-slot and loadout rules say what they should. Transient assets, no world, no Slate.

#include "Misc/AutomationTest.h"
#include "Abilities/GameplayAbility.h"
#include "GSTN_KitModBaking.h"
#include "GSTN_KitModDefinition.h"
#include "GSTN_KitModsEditorRules.h"
#include "GSTN_KitModsEditorUtils.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "Validation/GSTN_ValidationExtensions.h"
#include "Validation/GSTN_ValidationReport.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags KitModsEditorTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A kit with two mod slots and one attack whose timeline has a blade hitbox along X (centre 60 cm, radius 20 cm: tip at 80 cm) */
	struct FEditorFixture
	{
		UGSTN_PrimaryKitDefinition* Kit = nullptr;
		UGSTN_CombatTimeline* Slash = nullptr;
		UGSTN_KitModDefinition* LongBlade = nullptr;
		UGSTN_KitModDefinition* Picky = nullptr;

		FEditorFixture()
		{
			Slash = NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transient);
			Slash->Data.TotalFrames = 20;
			Slash->Data.FirstActionableFrame = 16;
			FGSTN_TimelineTrack_Hitbox Track;
			Track.StartFrame = 4;
			Track.EndFrame = 8;
			FGSTN_HitboxSpec& Blade = Track.Hitboxes.AddDefaulted_GetRef();
			Blade.Shape.Extent = FVector(20.0);
			Blade.Shape.Local.SetTranslation(FVector(60.0, 0.0, 0.0));
			Blade.ReachAxis = EGSTN_ReachAxis::X;
			Slash->Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));

			Kit = NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
			FGSTN_KitModSlots Slots;
			Slots.Slots = { GSTNKitModsTags::Mod_Slot, GSTNKitModsTags::Mod_Slot };
			Slots.Limits.MaxReachScale = 1.25f;
			Kit->Extensions.ExtensionData.Add(FInstancedStruct::Make(Slots));
			FGSTN_AbilityBinding& Attack = Kit->KitBindings.AddDefaulted_GetRef();
			Attack.Command = GSTNCommandTags::Attack;
			Attack.Ability = TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass());
			Attack.TimelineOverride = Slash;

			LongBlade = NewObject<UGSTN_KitModDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
			LongBlade->Slot = GSTNKitModsTags::Mod_Slot;
			FGSTN_MoveModifier& Modifier = LongBlade->MoveModifiers.AddDefaulted_GetRef();
			Modifier.TimeScale = 1.5f;
			Modifier.ReachScale = 1.5f;

			Picky = NewObject<UGSTN_KitModDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
			Picky->Slot = GSTNKitModsTags::Mod_Slot;
			Picky->CompatibleKits = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNKitModsTags::Mod));
			Picky->MoveModifiers.AddDefaulted_GetRef().DamageScale = 2.f;
		}

		FGSTN_KitModSlots& GetSlots() const { return *Kit->Extensions.ExtensionData[0].GetMutablePtr<FGSTN_KitModSlots>(); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsEditorRegisteredTest, "GSTNKitModsEditor.Registered", KitModsEditorTestFlags)
bool FGSTNKitModsEditorRegisteredTest::RunTest(const FString& Parameters)
{
	FEditorFixture Fixture;
	TestTrue(TEXT("The mod rules are an M5 validation extension"), FGSTN_ValidationExtensions::IsRegistered(FGSTN_KitModsEditorRules::ExtensionName));
	TestTrue(TEXT("They apply to a mod"), FGSTN_ValidationExtensions::AppliesToAny(Fixture.LongBlade));
	TestTrue(TEXT("...and to a kit with mod slots"), FGSTN_KitModsEditorRules::AppliesTo(*Fixture.Kit));
	Fixture.Kit->Extensions.ExtensionData.Reset();
	TestFalse(TEXT("...not to a kit without"), FGSTN_KitModsEditorRules::AppliesTo(*Fixture.Kit));

	// What the panel and Validate all run: the mod's own data error (no identity tag) comes out under the mod code
	FGSTN_ValidationReport Report;
	TestTrue(TEXT("RunRules ran on the mod"), FGSTN_ValidationExtensions::RunRules(*Fixture.LongBlade, Report));
	TestTrue(TEXT("The missing identity tag is a Mod.Data error"), Report.HasCode(TEXT("Mod.Data")) && Report.HasErrors());
	TestTrue(TEXT("Findings name the mod"), Report.Findings.Num() > 0 && Report.Findings[0].Asset.Get() == Fixture.LongBlade);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsEditorPreviewTest, "GSTNKitModsEditor.Preview", KitModsEditorTestFlags)
bool FGSTNKitModsEditorPreviewTest::RunTest(const FString& Parameters)
{
	FEditorFixture Fixture;
	FGSTN_KitModPreview Preview;
	FGSTN_KitModsEditorRules::PreviewModsOnKit(*Fixture.Kit, { Fixture.LongBlade, Fixture.Picky }, Preview);
	TestEqual(TEXT("The picky mod is skipped with a warning"), Preview.Warnings.Num(), 1);
	TestEqual(TEXT("One modded move"), Preview.Moves.Num(), 1);
	if (Preview.Moves.Num() == 1)
	{
		const FGSTN_ModMovePreview& Move = Preview.Moves[0];
		TestTrue(TEXT("It is the attack"), Move.Command == GSTNCommandTags::Attack);
		TestEqual(TEXT("Frames scaled as the compile does"), Move.Derived.TotalFrames, 30);
		TestEqual(TEXT("Base untouched"), Move.Base.TotalFrames, 20);
		TestTrue(TEXT("Reach cut back to the kit limit"), Move.bClamped && FMath::IsNearlyEqual(Move.Modifier.ReachScale, 1.25f));
		const FSoftObjectPath ModIds[] = { FSoftObjectPath(Fixture.LongBlade) };
		TestTrue(TEXT("Named like the compiled derived timeline"),
			Move.DerivedName == FGSTN_KitModBaking::MakeDerivedName(FSoftObjectPath(Fixture.Slash), ModIds));
		TestEqual(TEXT("Base tip at 80 cm"), FGSTN_KitModsEditorRules::EstimateReachTip(Move.Base), 80.0, 0.01);
		TestEqual(TEXT("Derived tip at 100 cm (centre 75, radius 25)"), FGSTN_KitModsEditorRules::EstimateReachTip(Move.Derived), 100.0, 0.01);
	}

	const TArray<FGameplayTag> Commands = UGSTN_KitModsEditorUtils::GetModdedCommands(Fixture.Kit, { Fixture.LongBlade });
	TestTrue(TEXT("The menu lists the attack"), Commands.Num() == 1 && Commands[0] == GSTNCommandTags::Attack);

	FString Message;
	UGSTN_CombatTimeline* Timeline = UGSTN_KitModsEditorUtils::MakePreviewTimeline(Fixture.Kit, { Fixture.LongBlade }, GSTNCommandTags::Attack, Message);
	TestTrue(TEXT("The preview is an asset the timeline editor can open"), Timeline != nullptr && Timeline->IsAsset());
	TestTrue(TEXT("...in /Temp, never saved nor validated"), Timeline != nullptr && Timeline->GetPathName().StartsWith(TEXT("/Temp/GSTNModPreview/")));
	TestTrue(TEXT("...holding the derived frames"), Timeline != nullptr && Timeline->Data.TotalFrames == 30);
	TestNull(TEXT("No preview for a command the mods do not change"),
		UGSTN_KitModsEditorUtils::MakePreviewTimeline(Fixture.Kit, { Fixture.LongBlade }, GSTNCommandTags::Strong, Message));
	if (Timeline)
	{
		Timeline->ClearFlags(RF_Public | RF_Standalone);
		Timeline->MarkAsGarbage();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsEditorModRulesTest, "GSTNKitModsEditor.ModRules", KitModsEditorTestFlags)
bool FGSTNKitModsEditorModRulesTest::RunTest(const FString& Parameters)
{
	FEditorFixture Fixture;
	const TArray<const UGSTN_PrimaryKitDefinition*> Kits = { Fixture.Kit };

	FGSTN_ValidationReport Report;
	FGSTN_KitModsEditorRules::CheckMod(*Fixture.LongBlade, Kits, 0.25f, Report);
	TestTrue(TEXT("The kit's limit cutting reach back is an info"), Report.HasCode(TEXT("Mod.Clamped")));
	TestTrue(TEXT("Reach without a blade mesh is an info"), Report.HasCode(TEXT("Mod.ReachNoMesh")));
	TestFalse(TEXT("The mod fits the kit"), Report.HasCode(TEXT("Mod.NoKit")));
	TestFalse(TEXT("A sane scaling breaks no timeline rule the base passes"), Report.HasCode(TEXT("Mod.Derived")));

	FGSTN_ValidationReport PickyReport;
	FGSTN_KitModsEditorRules::CheckMod(*Fixture.Picky, Kits, 0.25f, PickyReport);
	TestTrue(TEXT("A mod no kit accepts is a warning"), PickyReport.HasCode(TEXT("Mod.NoKit")));

	// Fits, but its filter selects nothing
	Fixture.Picky->CompatibleKits = FGameplayTagQuery();
	Fixture.Picky->MoveModifiers[0].MoveFilter = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNKitModsTags::Mod));
	FGSTN_ValidationReport FilterReport;
	FGSTN_KitModsEditorRules::CheckMod(*Fixture.Picky, Kits, 0.25f, FilterReport);
	TestTrue(TEXT("Modifiers that select no move are a warning"), FilterReport.HasCode(TEXT("Mod.NoMove")));
	TestFalse(TEXT("...and the mod fits"), FilterReport.HasCode(TEXT("Mod.NoKit")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsEditorKitRulesTest, "GSTNKitModsEditor.KitRules", KitModsEditorTestFlags)
bool FGSTNKitModsEditorKitRulesTest::RunTest(const FString& Parameters)
{
	FEditorFixture Fixture;
	FGSTN_ValidationReport Clean;
	FGSTN_KitModsEditorRules::CheckKitSlots(*Fixture.Kit, { Fixture.LongBlade }, Clean);
	TestFalse(TEXT("Good slots raise no error"), Clean.HasErrors());
	TestFalse(TEXT("A slot a mod fits is not reported"), Clean.HasCode(TEXT("Mod.Slots.NoMod")));

	Fixture.GetSlots().Slots.Add(GSTNKitModsTags::Mod);
	Fixture.GetSlots().MaxDerivedTimelines = 0;
	FGSTN_ValidationReport Report;
	FGSTN_KitModsEditorRules::CheckKitSlots(*Fixture.Kit, {}, Report);
	TestTrue(TEXT("A slot outside Mod.Slot is an error"), Report.HasCode(TEXT("Mod.Slots.Data")) && Report.HasErrors());
	TestTrue(TEXT("A cap below the moves is a warning"), Report.HasCode(TEXT("Mod.Slots.Cap")));
	TestTrue(TEXT("Slots no mod fits are an info"), Report.HasCode(TEXT("Mod.Slots.NoMod")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsEditorLoadoutTest, "GSTNKitModsEditor.Loadout", KitModsEditorTestFlags)
bool FGSTNKitModsEditorLoadoutTest::RunTest(const FString& Parameters)
{
	FEditorFixture Fixture;
	FGSTN_LoadoutData Loadout;
	Loadout.PrimaryKit = Fixture.Kit->GetPrimaryAssetId();
	FGSTN_LoadoutModsExt Ext;
	FGSTN_KitModConfig& Config = Ext.Configs.AddDefaulted_GetRef();
	Config.Mods.Add({ GSTNKitModsTags::Mod_Slot, FSoftObjectPath(Fixture.LongBlade) });
	Config.Mods.Add({ GSTNKitModsTags::Mod_Slot, FSoftObjectPath(Fixture.Picky) });
	Config.Mods.Add({ GSTNKitModsTags::Mod_Slot, FSoftObjectPath(TEXT("/Game/NoSuchMod.NoSuchMod")) });
	GSTNLoadoutExtension::Set(Loadout.Extensions, Ext);

	FGSTN_ValidationReport Report;
	FGSTN_KitModsEditorRules::CheckLoadout(Loadout, Fixture.Kit, { Fixture.Kit }, Report);
	TestTrue(TEXT("A missing mod asset is a warning"), Report.CountCode(TEXT("Mod.Loadout")) >= 2);
	TestFalse(TEXT("Never an error: the game skips it"), Report.HasErrors());
	return true;
}

#endif
