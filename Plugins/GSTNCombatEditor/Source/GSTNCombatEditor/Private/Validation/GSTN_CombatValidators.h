// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Bake/GSTN_SocketBaker.h"
#include "Input/GSTN_InputSlot.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "Timeline/GSTN_TimelineValidation.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GSTN_ValidationReport.h"

class UGSTN_CombatTimeline;
class UGSTN_GA_BaseCombat;
class UGSTN_HurtboxSetDefinition;
class UGSTN_InputSettings;
class UGSTN_LoadoutRules;
class UGSTN_PrimaryKitDefinition;
class UGSTN_WeaponDefinition;
class USkeletalMesh;
struct FGSTN_LoadoutData;
struct FGSTN_ResolvedLoadout;

/** The codes of the rules (combat-m5-editor 9.2). Stable names: a filter or a setting may key on them */
namespace GSTNValidationCodes
{
	inline FName FrameRange() { return TEXT("Timeline.FrameRange"); }
	inline FName MontageLength() { return TEXT("Timeline.MontageLength"); }
	inline FName OverlapSameKind() { return TEXT("Timeline.OverlapSameKind"); }
	inline FName RootMotionMix() { return TEXT("Timeline.RootMotionMix"); }
	inline FName DeferredCommit() { return TEXT("Timeline.DeferredCommit"); }
	inline FName CancelWindowTarget() { return TEXT("Timeline.CancelWindowTarget"); }
	inline FName TagsTrack() { return TEXT("Timeline.TagsTrack"); }
	inline FName AimTurn() { return TEXT("Timeline.AimTurn"); }
	inline FName UnknownTrack() { return TEXT("Timeline.UnknownTrack"); }

	// The rules of the hit data (M4, E11)
	inline FName BakedStale() { return TEXT("Timeline.BakedStale"); }
	inline FName BakedSocketMissing() { return TEXT("Timeline.BakedSocketMissing"); }
	inline FName BakedLength() { return TEXT("Timeline.BakedLength"); }
	inline FName HitboxKeys() { return TEXT("Timeline.HitboxKeys"); }
	inline FName HitData() { return TEXT("Timeline.HitData"); }
	inline FName HurtState() { return TEXT("Timeline.HurtState"); }
	inline FName Variants() { return TEXT("Timeline.Variants"); }

	// A hurtbox set asset
	inline FName HurtboxEmpty() { return TEXT("Hurtbox.Empty"); }
	inline FName HurtboxDuplicateName() { return TEXT("Hurtbox.DuplicateName"); }
	inline FName HurtboxExtent() { return TEXT("Hurtbox.Extent"); }
	inline FName HurtboxTag() { return TEXT("Hurtbox.Tag"); }
	inline FName HurtboxData() { return TEXT("Hurtbox.Data"); }
	inline FName HurtboxBone() { return TEXT("Hurtbox.BoneMissing"); }
	inline FName HurtboxOutside() { return TEXT("Hurtbox.Outside"); }
	inline FName HurtboxGenerate() { return TEXT("Hurtbox.Generate"); }
	inline FName HurtboxMirror() { return TEXT("Hurtbox.Mirror"); }

	// Findings of the checks of Core that the spec's table has no row for
	inline FName PartSelector() { return TEXT("Timeline.PartSelector"); }
	inline FName EmptyTrack() { return TEXT("Timeline.EmptyTrack"); }
	inline FName Ricochet() { return TEXT("Timeline.Ricochet"); }
	inline FName CoreOther() { return TEXT("Timeline.Core"); }

	// The inline timeline of an ability Blueprint (E13)
	inline FName AbilityInlineIgnored() { return TEXT("Ability.InlineIgnored"); }
	inline FName AbilityTimelineAssetUnresolved() { return TEXT("Ability.TimelineAssetUnresolved"); }

	// Slots, shared by kit, weapon and preset
	inline FName SlotAmbiguous() { return TEXT("Slot.Ambiguous"); }
	inline FName SlotShadowed() { return TEXT("Slot.Shadowed"); }
	inline FName SlotDuplicateCommand() { return TEXT("Slot.DuplicateCommand"); }
	inline FName SlotInvalid() { return TEXT("Slot.InvalidSlot"); }

	inline FName KitData() { return TEXT("Kit.Data"); }
	inline FName WeaponData() { return TEXT("Weapon.Data"); }
	inline FName LoadoutTable() { return TEXT("Loadout.Table"); }
	inline FName LoadoutRule() { return TEXT("Loadout.Rule"); }
}

/**
 * What the timeline rules may know besides the data (everything is optional: what is missing is not checked). The rules are pure
 * functions of the data and this context, so a test fills it by hand; MakeTimelineContext fills it from an asset for the validators and
 * the Validate button.
 */
struct FGSTN_TimelineRuleContext
{
	/** The context of the checks that Core owns: tick rate, montage lengths, the default override frames */
	FGSTN_TimelineValidationContext Core;

	/** The montage plays root motion (RootMotionMix). False when the montage is unknown */
	bool bMontageHasRootMotion = false;

	/**
	 * The commands of every kit that references the ability, for CancelWindowTarget. Null = not scanned, the rule does not check it.
	 * The scan loads kits, so only a caller that has time for it fills this.
	 */
	const TArray<FGameplayTag>* KnownCommands = nullptr;

	/**
	 * The names of the hurtboxes of the set the ability's kit uses, for HurtState (a modifier that names a hurtbox the set does not have).
	 * Null = no set known, the rule does not check names. Like KnownCommands it needs a scan of the kits, so only a caller that has the set fills it.
	 */
	const TSet<FName>* KnownHurtboxNames = nullptr;

	/**
	 * Where the baked sockets stand against their inputs (CheckBakeStatus). Unset = not checked: MakeTimelineContext fills it when it may load (it
	 * reads the mesh the last bake recorded), a test fills it by hand, and the open editor overwrites it with the status of the mesh it previews
	 */
	TOptional<FGSTN_BakeStatusResult> BakeStatus;

	/** The asset the findings belong to */
	UObject* Asset = nullptr;
};

/**
 * The rules of combat-m5-editor 9.2 that exist with M3, as pure functions that fill a report, and the checks of kit, weapon and
 * loadout. Two surfaces use them (the Data Validation validators and the Validate button), and the tests call them directly. Every rule
 * that Core or the input plugin already implements is wrapped, never written again: this class classifies what they say, names what is
 * missing from it (the slots, the pattern) and adds only the rules that were nowhere.
 */
struct FGSTN_CombatValidators
{
	// ---- Timeline

	/**
	 * The context for an asset: the tick rate the editor uses, and the montage length and root motion read from the montage.
	 * bLoadMontage loads the soft reference (a validator may, a rule never does).
	 */
	static FGSTN_TimelineRuleContext MakeTimelineContext(const FGSTN_CombatTimelineData& Data, UObject* Asset, float TickRate, bool bLoadMontage);

	/** Every timeline rule below, then the Validate of the track editor of each track */
	static void ValidateTimeline(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** The checks of FGSTN_TimelineValidation (Core), classified into the codes of the table. Adds the frame of the track to a finding about a track */
	static void CheckCoreRules(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** TotalFrames at least 1, no negative frame: what Core does not check */
	static void CheckFrameRange(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** Two Movement tracks, two HurtboxModifier tracks of different kinds on the same part, a global lock together with a narrower one */
	static void CheckOverlapSameKind(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** A root motion montage together with a Movement track that overrides the velocity */
	static void CheckRootMotionMix(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** A cancel window into nothing, into no command of the kits, or on hit/block with no hitbox on the timeline */
	static void CheckCancelWindowTarget(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** A tag of a Tags window, an event or the ricochet cue that is not registered */
	static void CheckTagsTrack(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** TurnUntilFrame past the end, MaxTurnDegrees not positive for a policy that turns */
	static void CheckAimTurn(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** A track struct with no track editor (it is shown as a generic row) */
	static void CheckUnknownTracks(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/**
	 * Hitbox keys (HitboxKeys): a key outside the track's window, keys out of order, a shape with no volume (the last two are Core's wording,
	 * classified), and two hitboxes of one HitGroup that overlap in time and could tie (same Priority, different hit data, different place).
	 * A hitbox with no key is not reported: M4 defines it as a shape that stays where its space puts it.
	 */
	static void CheckHitboxKeys(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** The hitbox half of OverlapSameKind: two hitboxes of one HitGroup that overlap in time and have the same shape (the second adds nothing) */
	static void CheckHitboxOverlap(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** Negative damage, knockback, multipliers or stagger; a fixed knockback next to growth; an unregistered hit type or cue; of the base hit and of each variant that overrides fields */
	static void CheckHitData(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** A relationship variant under another root than Combat.Relationship, a scale of 0 or less that does not ignore, a negative multiplier */
	static void CheckVariants(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** Armor with no positive threshold; a modifier naming a hurtbox that the known set lacks */
	static void CheckHurtState(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/**
	 * Baked sockets: a hitbox whose socket has no frames (BakedSocketMissing), frames that are not TotalFrames long (BakedLength), and frames of a socket
	 * no hitbox uses (BakedStale, info). The hash half of BakedStale (the inputs of the bake changed) arrives with the baker.
	 */
	static void CheckBakedSockets(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/**
	 * The hash half of BakedStale: the frames were baked by the editor and an input changed since (montage data, section, play rate, length, tick rate,
	 * mesh, sockets). An error, since a stale bake plays the wrong hitboxes. Frames baked by another tool (no record) are a warning: the editor cannot tell.
	 * Reads Context.BakeStatus and does nothing while it is unset. A length mismatch is BakedLength's, not reported twice
	 */
	static void CheckBakeStatus(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/** The Validate of the registered track editor of each track */
	static void CheckTrackEditors(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report);

	/**
	 * The timeline an ability Blueprint carries inline, read from the default object of its class: the same rules as a timeline asset
	 * (ValidateTimeline) on the Timeline property. When the ability names a TimelineAsset, the asset is what plays and has its own
	 * validator, so the inline data is not checked; if it still holds a timeline, Ability.InlineIgnored (warning) says it is dead data.
	 * A TimelineAsset that does not resolve (the asset was deleted or renamed, or is not a combat timeline) is Ability.TimelineAssetUnresolved
	 * (error): it still wins over the inline data, so the ability would play no timeline. bLoadMontage also allows loading the asset to
	 * prove it resolves; without it only the asset registry is asked.
	 */
	static void ValidateAbilityTimeline(const UGSTN_GA_BaseCombat& Ability, UObject* Asset, float TickRate, bool bLoadMontage, FGSTN_ValidationReport& Report);

	/** Does a TimelineAsset reference point at a combat timeline that exists? The pure part of the Ability.TimelineAssetUnresolved check */
	static bool IsTimelineAssetResolvable(const TSoftObjectPtr<UGSTN_CombatTimeline>& Reference, bool bLoad);

	/** Does the inline timeline hold anything: a length, a track or a montage? A default timeline is the empty state of a Blueprint that uses TimelineAsset */
	static bool HasInlineContent(const FGSTN_CombatTimelineData& Data);

	// ---- Hurtbox set

	/**
	 * The set's own checks (FGSTN_CombatRulesValidation::ValidateHurtboxSet) classified into the Hurtbox.* codes, then what they lack: a tag that is not
	 * under Combat.Hurtbox, a shape further from the root than the settings' sane radius (Hurtbox.Outside, warning) and a hurtbox on a bone the mesh does not
	 * have (Hurtbox.BoneMissing, warning). The mesh is MeshOverride (the open editor's preview), else the set's PreviewMesh; with neither the bone check is skipped.
	 * A finding about one hurtbox carries its position in Where.TrackIndex.
	 */
	static void CheckHurtboxSet(const UGSTN_HurtboxSetDefinition& Set, UObject* Asset, FGSTN_ValidationReport& Report, const USkeletalMesh* MeshOverride = nullptr);

	// ---- Kit, weapon, loadout

	/** The checks the kit asset has for itself (UGSTN_PrimaryKitDefinition::Validate), as findings */
	static void CheckKitData(const UGSTN_PrimaryKitDefinition& Kit, FGSTN_ValidationReport& Report);

	/** The checks the weapon asset has for itself (UGSTN_WeaponDefinition::Validate, loads the parents), as findings */
	static void CheckWeaponData(const UGSTN_WeaponDefinition& Weapon, FGSTN_ValidationReport& Report);

	/** The kit's own checks, then its command set and bindings (the kit alone, no weapon) through CheckCommandTable */
	static void CheckKit(const UGSTN_PrimaryKitDefinition& Kit, const UGSTN_InputSettings& Settings, FGSTN_ValidationReport& Report);

	/** Whether the kit can carry the weapon: it is its primary weapon, or its additional weapon query matches the weapon's tags */
	static bool KitCanCarryWeapon(const UGSTN_PrimaryKitDefinition& Kit, const UGSTN_WeaponDefinition& Weapon);

	/**
	 * What the weapon adds to the kit: the table of the kit with the weapon, minus the findings of the kit alone. Messages start with the
	 * kit's name, since a weapon may sit on several kits.
	 */
	static void CheckWeaponOnKit(const UGSTN_WeaponDefinition& Weapon, const UGSTN_PrimaryKitDefinition& Kit, const UGSTN_InputSettings& Settings, FGSTN_ValidationReport& Report);

	/** What CheckCommandTable may be given besides the loadout */
	struct FCommandTableOptions
	{
		/** The asset the findings belong to */
		UObject* Asset = nullptr;

		/** Put in front of every message, e.g. "On kit Foo: " */
		FText Prefix;

		/**
		 * A loadout whose findings are not reported again: the weapon check passes the kit alone, so it reports what the weapon adds and
		 * not the kit's own problems.
		 */
		const FGSTN_ResolvedLoadout* Baseline = nullptr;

		/** Receives the texts of Core's table report that this check reported itself, so the loadout rules can leave them out */
		TSet<FString>* OutHandledTexts = nullptr;
	};

	/**
	 * Compiles the loadout with the compiler the game uses and checks the table with the input plugin's validators (UGSTN_InputSlotValidation
	 * for the slots, the binding conflict checks for the bindings), once per weapon context. A slot finding names both slots and lists
	 * their input patterns in the notation of M1 section 4 in Detail; when a slot comes from a weapon or a super it says which. Table
	 * problems that are not about two slots are passed on as they are (Loadout.Table).
	 */
	static void CheckCommandTable(const FGSTN_ResolvedLoadout& Resolved, const UGSTN_InputSettings& Settings, const FCommandTableOptions& Options, FGSTN_ValidationReport& Report);

	/** CheckCommandTable, then the loadout rules (allowed weapons, supers and skins, customisations, the registry), without repeating a table finding */
	static void CheckLoadout(const FGSTN_ResolvedLoadout& Resolved, const UGSTN_InputSettings& Settings, const UGSTN_LoadoutRules& Rules, UObject* Asset, FGSTN_ValidationReport& Report);

	/** The assets the ids of a loadout name, through the Asset Manager (loads them). Hold keeps them alive. A missing asset stays null */
	static void ResolveLoadoutData(const FGSTN_LoadoutData& Data, FGSTN_ResolvedLoadout& OutResolved, TArray<TStrongObjectPtr<UObject>>& OutHold);

	// ---- Notation

	/**
	 * The canonical input of the slot (the shortest input that triggers it) in the notation of M1 section 4, frames counted from its
	 * first event. ButtonTags are the buttons the letters stand for, in order (A is the first). Empty when the pattern is malformed.
	 */
	static FString DescribeSlotInput(const FGSTN_InputSlotSpec& Spec, TConstArrayView<FGameplayTag> ButtonTags, const UGSTN_InputSettings& Settings);

	/** The buttons the specs refer to, sorted by name: the order DescribeSlotInput's letters follow, and the order the slot validator uses */
	static void GatherButtonTags(TConstArrayView<const FGSTN_InputSlotSpec*> Specs, TArray<FGameplayTag>& OutButtonTags);
};
