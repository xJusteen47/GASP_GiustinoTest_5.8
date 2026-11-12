// Automation tests of the pure kit mod layer: combine and limits, time baking, hit baking, derived names, config selection, wire id.

#include "Misc/AutomationTest.h"
#include "GSTN_KitModBaking.h"
#include "GSTN_KitModTypes.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags KitModsTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** 30 frames, actionable at 24, one hitbox track 10-12 with two keys, a 1-frame cancel window at 20, a baked socket of 30 frames */
	FGSTN_CombatTimelineData MakeBase()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 24;
		Data.MontagePlayRate = 1.f;
		FGSTN_TimelineTrack_Hitbox Hitbox;
		Hitbox.StartFrame = 10;
		Hitbox.EndFrame = 12;
		FGSTN_HitboxSpec& Spec = Hitbox.Hitboxes.AddDefaulted_GetRef();
		Spec.Data.Damage = 10.f;
		Spec.Data.BaseKnockback = 4.f;
		Spec.Shape.Extent = FVector(10.0);
		Spec.Keys.AddDefaulted(2);
		Spec.Keys[0].Frame = 0;
		Spec.Keys[1].Frame = 1;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Hitbox));
		FGSTN_TimelineTrack_CancelWindow Cancel;
		Cancel.StartFrame = 20;
		Cancel.EndFrame = 21;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Cancel));
		FGSTN_BakedSocketFrames& Socket = Data.BakedSockets.Add(TEXT("Tip"));
		for (int32 Frame = 0; Frame < 30; ++Frame)
		{
			Socket.Frames.Add(FTransform(FVector(Frame, 0.0, 0.0)));
		}
		return Data;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsCombineTest, "GSTNKitMods.Core.Combine", KitModsTestFlags)
bool FGSTNKitModsCombineTest::RunTest(const FString& Parameters)
{
	FGSTN_MoveModifier Slow;
	Slow.TimeScale = 1.5f;
	Slow.ReachScale = 1.5f;
	Slow.DamageScale = 2.f;
	FGSTN_MoveModifier Slower = Slow;
	FGSTN_KitModLimits Limits;
	Limits.MaxTimeScale = 2.f;
	Limits.MaxReachScale = 2.f;
	const FGSTN_CombinedMoveModifier Combined = FGSTN_KitModBaking::Combine({ &Slow, &Slower }, Limits);
	TestEqual(TEXT("Time 1.5 x 1.5 clamped to the kit's max"), Combined.TimeScale, 2.f);
	TestEqual(TEXT("Reach 2.25 clamped to the kit's max"), Combined.ReachScale, 2.f);
	TestEqual(TEXT("Damage multiplies, unclamped"), Combined.DamageScale, 4.f);
	TestTrue(TEXT("No modifier is the identity"), FGSTN_KitModBaking::Combine({}, Limits).IsIdentity());
	TestTrue(TEXT("Empty filter selects every move"), FGSTN_KitModBaking::MatchesFilter(FGameplayTagQuery(), FGameplayTagContainer()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsBakeTimeTest, "GSTNKitMods.Core.BakeTime", KitModsTestFlags)
bool FGSTNKitModsBakeTimeTest::RunTest(const FString& Parameters)
{
	const FGSTN_CombatTimelineData Base = MakeBase();
	FGSTN_CombinedMoveModifier Modifier;
	Modifier.TimeScale = 1.5f;
	FGSTN_CombatTimelineData Out;
	FGSTN_KitModBaking::BakeTimeline(Base, Modifier, Out);
	TestEqual(TEXT("Total 30 -> 45"), Out.TotalFrames, 45);
	TestEqual(TEXT("Actionable 24 -> 36"), Out.FirstActionableFrame, 36);
	TestEqual(TEXT("Montage plays slower"), Out.MontagePlayRate, 1.f / 1.5f, 1e-4f);
	const FGSTN_TimelineTrack_Hitbox* Hitbox = Out.Tracks[0].GetPtr<FGSTN_TimelineTrack_Hitbox>();
	TestEqual(TEXT("Hitbox start 10 -> 15"), Hitbox->StartFrame, 15);
	TestEqual(TEXT("Hitbox end 12 -> 18"), Hitbox->EndFrame, 18);
	TestEqual(TEXT("Key 1 -> 2 (1.5 rounds half up)"), Hitbox->Hitboxes[0].Keys[1].Frame, 2);
	TestEqual(TEXT("Baked socket resampled to 45 frames"), Out.BakedSockets[TEXT("Tip")].Frames.Num(), 45);
	TestEqual(TEXT("Last baked frame is the source's last"), Out.BakedSockets[TEXT("Tip")].Frames.Last().GetTranslation().X, 29.0, 1e-6);

	// Faster: a 1-frame window and two adjacent keys never collapse
	Modifier.TimeScale = 0.25f;
	FGSTN_KitModBaking::BakeTimeline(Base, Modifier, Out);
	const FGSTN_TimelineTrack* Cancel = Out.Tracks[1].GetPtr<FGSTN_TimelineTrack>();
	TestTrue(TEXT("Cancel window keeps one frame"), Cancel->EndFrame - Cancel->StartFrame >= 1);
	const FGSTN_HitboxSpec& Spec = Out.Tracks[0].GetPtr<FGSTN_TimelineTrack_Hitbox>()->Hitboxes[0];
	TestTrue(TEXT("Keys stay strictly ordered"), Spec.Keys[1].Frame > Spec.Keys[0].Frame);

	// Same inputs, same bits
	FGSTN_CombatTimelineData Again;
	FGSTN_KitModBaking::BakeTimeline(Base, Modifier, Again);
	TestEqual(TEXT("Deterministic total"), Again.TotalFrames, Out.TotalFrames);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsBakeHitsTest, "GSTNKitMods.Core.BakeHits", KitModsTestFlags)
bool FGSTNKitModsBakeHitsTest::RunTest(const FString& Parameters)
{
	const FGSTN_CombatTimelineData Base = MakeBase();
	FGSTN_CombinedMoveModifier Modifier;
	Modifier.DamageScale = 1.5f;
	Modifier.KnockbackScale = 2.f;
	Modifier.HitboxScale = FVector(2.0, 1.0, 1.0);
	Modifier.AddedTags.AddTag(GSTNKitModsTags::Mod);
	FGSTN_CombatTimelineData Out;
	FGSTN_KitModBaking::BakeTimeline(Base, Modifier, Out);
	const FGSTN_HitboxSpec& Spec = Out.Tracks[0].GetPtr<FGSTN_TimelineTrack_Hitbox>()->Hitboxes[0];
	TestEqual(TEXT("Damage scaled"), Spec.Data.Damage, 15.f);
	TestEqual(TEXT("Knockback scaled"), Spec.Data.BaseKnockback, 8.f);
	TestEqual(TEXT("Extent X scaled"), Spec.Shape.Extent.X, 20.0);
	TestEqual(TEXT("Frames untouched"), Out.TotalFrames, 30);
	const FGSTN_TimelineTrack_Tags* Tags = Out.Tracks.Last().GetPtr<FGSTN_TimelineTrack_Tags>();
	TestTrue(TEXT("Added tags span the move"), Tags && Tags->StartFrame == 0 && Tags->EndFrame == 30 && Tags->Tags.HasTagExact(GSTNKitModsTags::Mod));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsNamesAndConfigTest, "GSTNKitMods.Core.NamesAndConfig", KitModsTestFlags)
bool FGSTNKitModsNamesAndConfigTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Base(TEXT("/Game/GAS/TL_Slash.TL_Slash"));
	const FSoftObjectPath ModA(TEXT("/Game/GAS/Mods/MOD_A.MOD_A"));
	const FSoftObjectPath ModB(TEXT("/Game/GAS/Mods/MOD_B.MOD_B"));
	const FName AB = FGSTN_KitModBaking::MakeDerivedName(Base, { ModA, ModB });
	TestEqual(TEXT("Same inputs, same name"), AB, FGSTN_KitModBaking::MakeDerivedName(Base, { ModA, ModB }));
	TestNotEqual(TEXT("Slot order matters"), AB, FGSTN_KitModBaking::MakeDerivedName(Base, { ModB, ModA }));
	TestTrue(TEXT("Prefixed"), AB.ToString().StartsWith(TEXT("GSTNModTL_")));

	FGSTN_LoadoutModsExt Ext;
	Ext.Configs.AddDefaulted(2);
	Ext.Configs[0].Mods.AddDefaulted(1);
	Ext.Configs[1].Kit = GSTNKitModsTags::Mod;
	FGameplayTagContainer KitTags(GSTNKitModsTags::Mod);
	TestTrue(TEXT("Kit entry wins over the wildcard"), FGSTN_KitModBaking::SelectConfig(Ext, KitTags) == &Ext.Configs[1]);
	TestTrue(TEXT("Other kits get the wildcard"), FGSTN_KitModBaking::SelectConfig(Ext, FGameplayTagContainer()) == &Ext.Configs[0]);
	TestTrue(TEXT("Wire id registered"), FGSTN_LoadoutExtensionTypes::FindType(FGSTN_LoadoutModsExt::WireId) == FGSTN_LoadoutModsExt::StaticStruct());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsKitSwapRuleTest, "GSTNKitMods.Core.KitSwapRule", KitModsTestFlags)
bool FGSTNKitModsKitSwapRuleTest::RunTest(const FString& Parameters)
{
	// Owner answer M11.4: on a kit swap from the ground the ground kit's mods apply, so the player's entry travels with the old kit
	const FGSTN_LoadoutModsExt Mods;
	FGSTN_KitSwapCarryOver Rules;
	TestFalse(TEXT("default: the mods travel with the kit"), Mods.StaysWithPlayerOnKitSwapUnder(Rules));
	TestFalse(TEXT("and the rule-free answer agrees"), Mods.StaysWithPlayerOnKitSwap());
	Rules.bModsStayWithPlayer = true;
	TestTrue(TEXT("PlayerSetup: the player's setups stay with them"), Mods.StaysWithPlayerOnKitSwapUnder(Rules));
	return true;
}

#endif
