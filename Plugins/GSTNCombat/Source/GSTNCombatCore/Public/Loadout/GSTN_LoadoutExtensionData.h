// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_LoadoutExtensionData.generated.h"

/**
 * What a whole-kit swap from the ground keeps with the player (owner answers to M11.2 and M11.4, 2026-11-12). Pure data, filled by the
 * caller from the mode's rules (UGSTN_LoadoutModeRules::MakeKitSwapCarryOver in GSTNCombatGAS), so the swap itself stays a pure function.
 * The defaults are the owner's answers; the meters (M11.3) are not loadout data and are handled where the attributes live.
 */
struct FGSTN_KitSwapCarryOver
{
	/** The player's support weapon (the last additional weapon) goes with them to the new kit, if that kit can carry it (M11.2) */
	bool bSupportWeaponStaysWithPlayer = true;
	/** The player's mod setups stay with them, so their saved setup for the new kit applies; false = the ground kit's mods apply (M11.4) */
	bool bModsStayWithPlayer = false;
};

/**
 * Base of the data an optional plugin keeps in a player's loadout (combat-loadout-mods 11.1): the mods put on a kit, a backpack,
 * anything a plugin adds to what the player brings. It lives in FGSTN_LoadoutData::Extensions as an instanced struct, so the
 * loadout never names a plugin type and a disabled plugin is simply absent.
 *
 * An entry travels by a stable wire id (FGSTN_LoadoutExtensionTypes) and its bytes. The default bytes are the struct's reflected
 * properties in binary (UScriptStruct::SerializeBin): enough for ids, tags, numbers and soft paths, which is what a loadout holds.
 * A type whose layout must survive a change of its own fields between builds overrides SerializePayload with a versioned format.
 * An entry never holds a hard object pointer: a loadout is sent to the server and kept in a save game, so it carries ids only.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	virtual ~FGSTN_LoadoutExtensionData() = default;

	/**
	 * Writes or reads this entry's payload. Return false to use the default (reflected binary). A reader that returns true and
	 * flags the archive drops the entry; it never fails the whole loadout.
	 */
	virtual bool SerializePayload(FArchive& Ar) { return false; }

	/**
	 * Whether the entry stays with the player when they take a whole kit from the ground (a backpack: spec §10 "a kit change keeps
	 * the backpack"), instead of travelling with the kit they leave behind. False by default: an entry belongs to the kit (the ground kit's
	 * own setup comes with it), so only a type that answers otherwise for itself says true. Read by GSTNLoadoutExtension::CarryOverOnKitSwap
	 */
	virtual bool StaysWithPlayerOnKitSwap() const { return false; }

	/**
	 * The same question under the mode's kit swap rules: a type whose answer is a mode choice (the mods, M11.4) overrides this one.
	 * Defaults to the rule-free answer above
	 */
	virtual bool StaysWithPlayerOnKitSwapUnder(const FGSTN_KitSwapCarryOver& Rules) const { return StaysWithPlayerOnKitSwap(); }
};

/**
 * The wire ids of the loadout extension types. A plugin registers its types at module startup (Register) so the id on the wire
 * is the one it chose, which survives a rename of the struct; an unregistered type travels under its struct path. Reading an id
 * that resolves to no type derived from FGSTN_LoadoutExtensionData (its plugin is disabled in this build) drops the entry with a
 * warning. Game thread only, like every loadout serialization.
 */
struct GSTNCOMBATCORE_API FGSTN_LoadoutExtensionTypes
{
	/** Registers Struct under WireId. Returns false (and changes nothing) if Struct is not an extension type or the id is taken by another type */
	static bool Register(FName WireId, const UScriptStruct* Struct);

	/** Removes the id; entries of that type then travel under their struct path */
	static void Unregister(FName WireId);

	/** The id an entry of Struct is written under: its registered id, else its struct path */
	static FName GetWireId(const UScriptStruct* Struct);

	/** The type of a wire id: a registered one, else a loaded struct of that path. Null when unknown or not an extension type */
	static const UScriptStruct* FindType(FName WireId);
};

/** Helpers to read and write typed loadout extensions without touching FInstancedStruct by hand */
namespace GSTNLoadoutExtension
{
	/** The first entry of type T in Extensions (exact type or a child), null if none */
	template <typename T>
	const T* Find(const TArray<FInstancedStruct>& Extensions)
	{
		for (const FInstancedStruct& Entry : Extensions)
		{
			if (const T* Typed = Entry.GetPtr<T>())
			{
				return Typed;
			}
		}
		return nullptr;
	}

	/** Replaces the entry of T's exact type, or appends one. One entry per type is the convention every plugin follows */
	template <typename T>
	T& Set(TArray<FInstancedStruct>& Extensions, const T& Value)
	{
		for (FInstancedStruct& Entry : Extensions)
		{
			if (Entry.GetScriptStruct() == T::StaticStruct())
			{
				Entry.InitializeAs<T>(Value);
				return Entry.GetMutable<T>();
			}
		}
		FInstancedStruct& Added = Extensions.AddDefaulted_GetRef();
		Added.InitializeAs<T>(Value);
		return Added.GetMutable<T>();
	}

	/**
	 * A whole-kit swap: Taken is the loadout the player picks up (becomes theirs), Left the loadout they put down. Every entry of Left
	 * that StaysWithPlayerOnKitSwap moves into Taken; an entry of the same type already in Taken goes to Left in its place (a swap, so
	 * nothing is lost). Order of the other entries is kept. Pure; the same answer on every machine
	 */
	GSTNCOMBATCORE_API void CarryOverOnKitSwap(TArray<FInstancedStruct>& Taken, TArray<FInstancedStruct>& Left);

	/** The same under the mode's kit swap rules: an entry stays when StaysWithPlayerOnKitSwapUnder(Rules) says so */
	GSTNCOMBATCORE_API void CarryOverOnKitSwap(TArray<FInstancedStruct>& Taken, TArray<FInstancedStruct>& Left, const FGSTN_KitSwapCarryOver& Rules);

	/** Writes the entries as the loadout's extension payload (count, then wire id, length and bytes per entry). Entries of no known type are skipped */
	GSTNCOMBATCORE_API void WritePayload(const TArray<FInstancedStruct>& Extensions, TArray<uint8>& OutBytes, int32 MaxBytes);

	/** Reads a payload WritePayload made. Unknown types and malformed entries are dropped with a warning; returns false only for a broken stream */
	GSTNCOMBATCORE_API bool ReadPayload(const TArray<uint8>& Bytes, TArray<FInstancedStruct>& OutExtensions);
}
