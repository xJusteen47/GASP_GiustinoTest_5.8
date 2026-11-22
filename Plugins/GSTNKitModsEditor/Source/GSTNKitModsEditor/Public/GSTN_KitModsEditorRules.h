// Editor tooling of kit mods (M11 L8). Editor only.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_KitModLoadout.h"

class UGSTN_KitModDefinition;
class UGSTN_PrimaryKitDefinition;
struct FGSTN_EquipmentVisual;
struct FGSTN_LoadoutData;
struct FGSTN_ValidationReport;

/** The codes of the mod rules. Stable: the validation panel filters and a project's settings key on them */
namespace GSTNKitModsCodes
{
	/** The mod asset's own checks (identity tag, slot tag, empty modifiers): IsDataValid, classified */
	GSTNKITMODSEDITOR_API FName Data();
	/** No kit in the project has a slot of the mod's type that its CompatibleKits accepts */
	GSTNKITMODSEDITOR_API FName NoKit();
	/** The mod has move modifiers, but on a kit it fits none selects a move */
	GSTNKITMODSEDITOR_API FName NoMove();
	/** The kit's limits cut the mod back on a move (info) */
	GSTNKITMODSEDITOR_API FName Clamped();
	/** The derived timeline breaks a timeline rule the base timeline passes */
	GSTNKITMODSEDITOR_API FName Derived();
	/** The scaled hitboxes and the mod's blade mesh disagree on reach */
	GSTNKITMODSEDITOR_API FName ReachMesh();
	/** The mod changes reach and has no mesh to show it (info) */
	GSTNKITMODSEDITOR_API FName ReachNoMesh();
	/** A kit's mod slots: a tag outside Mod.Slot, limits that exclude 1, no slot */
	GSTNKITMODSEDITOR_API FName SlotsData();
	/** A kit's MaxDerivedTimelines is below the moves its mods could change */
	GSTNKITMODSEDITOR_API FName SlotsCap();
	/** No mod in the project fits a slot of the kit (info) */
	GSTNKITMODSEDITOR_API FName SlotsNoMod();
	/** A loadout's mod choice that the game would skip (missing asset, wrong kit, no free slot) */
	GSTNKITMODSEDITOR_API FName Loadout();
}

/**
 * The mod rules of the combat validation (spec combat-loadout-mods 15, M5 hooks): registered with FGSTN_ValidationExtensions, so they
 * show in the M5 validation panel, Validate all, and Data Validation (save, Validate Assets). They apply to three kinds of asset:
 *
 * - A mod definition: its own checks; the kits it fits; on each, the moves it changes, as the loadout compile bakes them
 *   (FGSTN_KitModLoadout::BuildPreview): limits that cut it back, derived timelines that break a timeline rule the base passes (the M5
 *   rules run on both), and the reach of the scaled hitboxes against the longest mesh of the mod's visual (a "longer sword" whose blade
 *   and hitbox disagree, tolerance in the editor preferences).
 * - A kit with the Kit mod slots extension: the slots and limits, the cap against the moves, slots no mod fits.
 * - Any asset holding an FGSTN_LoadoutData with mods: each choice the game would skip, with the reason ResolveMods gives.
 *
 * Every finding is a warning or an info except broken data (Data and SlotsData errors, Derived errors): a kit swap never fails because
 * of a mod (L5 rule), so a mod is only "invalid" when its own data is. The functions taking explicit kit and mod lists are what the
 * tests call; Run gathers them from the Asset Registry (and loads them).
 */
struct GSTNKITMODSEDITOR_API FGSTN_KitModsEditorRules
{
	static const FName ExtensionName;

	static void Register();
	static void Unregister();

	static bool AppliesTo(const UObject& Asset);
	static void Run(UObject& Asset, FGSTN_ValidationReport& Report);

	static void CheckMod(const UGSTN_KitModDefinition& Mod, TConstArrayView<const UGSTN_PrimaryKitDefinition*> Kits, float ReachTolerance, FGSTN_ValidationReport& Report);
	static void CheckKitSlots(const UGSTN_PrimaryKitDefinition& Kit, TConstArrayView<const UGSTN_KitModDefinition*> Mods, FGSTN_ValidationReport& Report);
	static void CheckLoadout(const FGSTN_LoadoutData& Loadout, UObject* Asset, TConstArrayView<const UGSTN_PrimaryKitDefinition*> Kits, FGSTN_ValidationReport& Report);

	/** The mod has a slot of its type on the kit, and CompatibleKits accepts the kit's tags plus its primary weapon's */
	static bool ModFitsKit(const UGSTN_KitModDefinition& Mod, const UGSTN_PrimaryKitDefinition& Kit);

	/** What the mods do to the kit with its primary weapon, as the compile would (kit and weapon bindings, verbs, baking). Loads */
	static void PreviewModsOnKit(const UGSTN_PrimaryKitDefinition& Kit, TConstArrayView<const UGSTN_KitModDefinition*> Mods, FGSTN_KitModPreview& Out);

	/**
	 * How far the farthest reach-scalable hitbox of the timeline ends from its space's origin, along its ReachAxis, in cm: the centre's
	 * coordinate plus the largest half extent. An upper bound for a rotated shape; 0 when no hitbox has a ReachAxis
	 */
	static double EstimateReachTip(const FGSTN_CombatTimelineData& Data);

	/** How far the longest mesh of the visual reaches from its pivot (bounds, part scale included), in cm; 0 with no mesh. Loads */
	static double EstimateVisualReach(const FGSTN_EquipmentVisual& Visual);

	/** The kits and mods of the project (Asset Registry, loaded) */
	static void GatherKits(TArray<const UGSTN_PrimaryKitDefinition*>& OutKits);
	static void GatherMods(TArray<const UGSTN_KitModDefinition*>& OutMods);
};
