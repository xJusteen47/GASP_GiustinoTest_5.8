// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "Input/GSTN_BufferResetRules.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Simulation/GSTN_WeaponStatsSwap.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/Object.h"
#include "GSTN_CombatTableSet.generated.h"

class UGSTN_CombatSettings;
class UGSTN_GA_BaseCombat;
class UGSTN_GA_PlatformJump;
class UGSTN_InputSettings;

/**
 * Everything the combat tick reads that depends on the loadout, as one immutable bundle: the command table the input simulation
 * resolves against, the timeline registry and its tiers, the per-binding timeline and jump role, the weapon stats of the loadout.
 * The sync state refers to rows and registry slots by index, so these arrays are the meaning of the indices of one loadout revision.
 *
 * A set is built once (game thread, outside any tick), never changed afterwards, and found by revision: the active table is a pure
 * function of the rolled-back state (FGSTN_CombatSyncCore::LoadoutRevision), so a re-simulated frame meets the table that frame had
 * the first time. Revision 0 is the Blueprint binding set; a loadout's set replaces it wholesale.
 *
 * The command table points into this object (the slots of Compiled and OwnedSlots), so a set is never copied or resized.
 */
UCLASS(Transient)
class GSTNCOMBATNP_API UGSTN_CombatTableSet : public UObject
{
	GENERATED_BODY()

public:
	/** The loadout revision the set was built for (0 = the Blueprint binding set) */
	int32 Revision = 0;

	/** The compiled loadout table the command table was converted from (empty for revision 0). Its Slots are what Table points into */
	FGSTN_CompiledBindingTable Compiled;

	FGSTN_CommandTable Table;

	/** Slots of bindings whose priority offset or buffer differs from their command's; Binding.Slot points at them */
	TArray<TUniquePtr<FGSTN_InputSlotSpec>> OwnedSlots;

	/**
	 * The timeline abilities that something starts (combat-m3 7.1), the index the combat sync state stores. For a loadout set in the
	 * order of the compiled registry, then the adoptable ones; for revision 0 the bound classes in binding order, then the adoptable
	 */
	UPROPERTY(Transient)
	TArray<TSubclassOf<UGSTN_GA_BaseCombat>> TimelineRegistry;

	/** The timeline assets and montages of the registry classes, loaded with the set and kept alive by it */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> TimelineContent;

	/** Per binding: registry index, INDEX_NONE for a plain ability */
	TArray<int16> BindingTimeline;

	/** Per binding: EGSTN_PlatformJumpRole + 1 for a UGSTN_GA_PlatformJump binding, 0 for any other */
	TArray<uint8> BindingJumpRole;

	/** Per registry entry: the priority tier of the first command bound to it (the running tier of the override rule) and its default window */
	TArray<int32> TimelineTier;
	TArray<int32> TimelineDefaultOverride;

	/** The class default of the Jump-role binding of the table (the kit's jump data), null without one */
	const UGSTN_GA_PlatformJump* TableJumpData = nullptr;

	/** The stats effect of each weapon, in loadout order (0 = primary; an entry may leave Effect empty) */
	UPROPERTY(Transient)
	TArray<FGSTN_WeaponStatsEntry> WeaponStats;

	/**
	 * The ricochet fallbacks of the loadout (combat-m4-hits 3b, M4 H12): one per weapon in loadout order, then the kit's. An attack that
	 * authors no response of its own recoils with the active weapon's, else the kit's. RecoilFrames 0 = not authored. Their montages are
	 * in TimelineContent
	 */
	UPROPERTY(Transient)
	TArray<FGSTN_RicochetResponse> WeaponRicochet;

	UPROPERTY(Transient)
	FGSTN_RicochetResponse KitRicochet;

	/** The fallback for an attack that runs with the weapon at WeaponIndex: the weapon's when authored, else the kit's, null when neither is */
	const FGSTN_RicochetResponse* GetRicochetFallback(int32 WeaponIndex) const
	{
		if (WeaponRicochet.IsValidIndex(WeaponIndex) && WeaponRicochet[WeaponIndex].IsAuthored())
		{
			return &WeaponRicochet[WeaponIndex];
		}
		return KitRicochet.IsAuthored() ? &KitRicochet : nullptr;
	}

	/** The loadout's reserved extension block (mods, backpacks), kept so its CRC is part of the checksum */
	TArray<uint8> ReservedExtension;

	/** Deterministic CRC of everything above that two machines must agree on; see ComputeChecksum */
	uint32 Checksum = 0;

	/** Registers the timeline classes of Table.Bindings (binding order, each once) and the per-binding facts. Revision 0 uses it; a converted set fills the same arrays itself */
	void DeriveFromBindings(const UGSTN_InputSettings& InputSettings, const UGSTN_CombatSettings& CombatSettings);

	/** Appends the adoptable timelines after the bound ones, with no tier and no default window (combat-m3 6.3) */
	void AppendAdoptable(TConstArrayView<TSubclassOf<UGSTN_GA_BaseCombat>> Adoptable);

	/** Registers one timeline class and loads its content. Returns its registry index; with bShareExisting a class already registered is not added twice */
	int32 AddTimelineClass(TSubclassOf<UGSTN_GA_BaseCombat> TimelineClass, int32 Tier, int32 DefaultOverrideFrames, bool bShareExisting);

	/**
	 * CRC32 of the table as bytes: the compiled table, the commands by tag name, the bindings (command, source mask, class path,
	 * registry index, jump role), the registry (class path, tier, default window, total frames), the weapon stats and the reserved
	 * block. No pointer and no index that depends on load order, so two machines with the same loadout get the same value.
	 */
	uint32 ComputeChecksum() const;
};

/** What a conversion reads besides the compiled table. The settings are the project's, the loader is injected so a test needs no content */
struct GSTNCOMBATNP_API FGSTN_CombatTableConvertInput
{
	FGSTN_CombatTableConvertInput(TFunction<UClass*(const FSoftObjectPath&)> InLoadClass) : LoadClass(MoveTemp(InLoadClass)) {}

	const UGSTN_InputSettings* InputSettings = nullptr;
	const UGSTN_CombatSettings* CombatSettings = nullptr;

	/** The host's global reset rules: their watched tags join the table's (the Blueprint path passes the same list to BuildTable) */
	TConstArrayView<TInstancedStruct<FGSTN_BufferResetRule>> GlobalResetRules;

	/** Timelines nothing binds but something else starts; they follow the compiled registry */
	TConstArrayView<TSubclassOf<UGSTN_GA_BaseCombat>> Adoptable;

	/** Ability class of a compiled soft path, null if it does not load. Synchronous: the converter runs outside any tick */
	TFunction<UClass*(const FSoftObjectPath&)> LoadClass;

	TArray<FGSTN_WeaponStatsEntry> WeaponStats;
	TArray<FGSTN_RicochetResponse> WeaponRicochet;
	FGSTN_RicochetResponse KitRicochet;
	TArray<uint8> ReservedExtension;
};

/**
 * Compiled loadout table (Core) to combat table set. Pure: no world, no ability system instance; game thread only, never inside a tick.
 *
 * Mapping, all deterministic: commands are the distinct commands of the entries, sorted by tag name; bindings are sorted by (command name,
 * entry order), the entry order being the Core one (kit, weapons 0..N, supers). A binding's source becomes the opaque SourceMask the
 * input resolver filters by; a binding whose effective slot differs from the command's gets its own slot. Entries whose class does not
 * load, whose source has no mask bit, or past the table limits are dropped with an error in the report.
 */
struct GSTNCOMBATNP_API FGSTN_CombatTableConverter
{
	using FExtension = TFunction<void(const FGSTN_CompiledBindingTable&, UGSTN_CombatTableSet&)>;

	/** Never null; the report says what was dropped */
	static UGSTN_CombatTableSet* Convert(UObject* Outer, const FGSTN_CompiledBindingTable& Compiled, const FGSTN_CombatTableConvertInput& Input, FGSTN_CompileReport* OutReport = nullptr);

	/**
	 * A converter extension (the seam for mods and backpacks): runs after the base conversion, in id order, may append bindings or registry
	 * entries, and is part of the checksum through what it changes. Registering an id again replaces it
	 */
	static void RegisterExtension(FName Id, FExtension Extension);
	static void UnregisterExtension(FName Id);
};
