// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Misc/StringBuilder.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_SyncFragment.generated.h"

/**
 * Feature type ids: the one central place where a feature that adds per-entity state to the combat sync state (a sync
 * fragment) or a per-frame hook (a tick extension) gets its fixed small number. They go on the wire, so server and clients
 * must agree on them: an id is assigned here once, never renumbered and never reused for another feature.
 *
 * 0 is invalid. 1 to 127 are for GSTN plugins, 128 to 239 for a game or third-party plugins, 240 to 255 for automation tests
 * only. A fragment and the tick extension of the same feature share the id.
 */
namespace GSTNFeatureIds
{
	constexpr uint8 Invalid = 0;

	constexpr uint8 FirstGame = 128;
	constexpr uint8 FirstTest = 240;
	constexpr uint8 TestA = 240;
	constexpr uint8 TestB = 241;
	constexpr uint8 TestC = 242;
}

/**
 * Base of the per-entity state a feature adds to the combat sync state (combat extension surface, modularity-and-tags 5b).
 * An entity that does not use the feature simply has no fragment of the type, and that costs no bytes on the wire.
 *
 * A fragment is simulated state like any other: integers and frames only (no tags by name, no pointers, no floats that
 * differ between machines), changed only by simulation code, restored by rollback. The four hooks must be deterministic and
 * must give the same result on every machine.
 *
 * Fragments live in a TArray<TInstancedStruct<FGSTN_SyncFragment>> kept sorted by type id; FGSTN_SyncFragmentList is the one
 * place that reads and writes that array.
 */
USTRUCT()
struct GSTNCOMBATCORE_API FGSTN_SyncFragment
{
	GENERATED_BODY()

	virtual ~FGSTN_SyncFragment() = default;

	/** Writes or reads the fragment; the same code for both directions, like the Network Prediction state hooks */
	virtual void NetSerialize(FArchive& Ar) {}

	/**
	 * True when this (the local state) differs from the authority's in a way that needs a rollback. The default compares the
	 * serialized forms, so everything on the wire is compared; override to ignore presentation-only fields
	 */
	virtual bool ShouldReconcile(const FGSTN_SyncFragment& Authority) const;

	/**
	 * Called on a copy of To (so the default, a discrete state that takes the newer frame, needs no code). From can be null.
	 * Override only for state that blends between two frames.
	 */
	virtual void Interpolate(const FGSTN_SyncFragment* From, const FGSTN_SyncFragment* To, float PCT) {}

	/** Appends a one-line description for np.PrintReconciles and logs; no line break */
	virtual void ToString(FAnsiStringBuilderBase& Out) const {}
};

using FGSTN_SyncFragmentArray = TArray<TInstancedStruct<FGSTN_SyncFragment>>;

/**
 * Maps the fixed type ids to the fragment structs. The owner of a fragment registers it when its module starts (before the
 * first simulation tick) and unregisters it when the module shuts down. Only the game thread reads and writes it.
 *
 * Registration is what makes a type id mean something on this machine: a machine without the plugin has no struct for the
 * id and skips the fragment when it arrives (FGSTN_SyncFragmentList::NetSerialize).
 */
class GSTNCOMBATCORE_API FGSTN_SyncFragmentRegistry
{
public:
	/** False (and an ensure) when the id or the struct is already taken by something else; registering the same pair twice is fine */
	static bool Register(uint8 TypeId, const UScriptStruct* Struct);

	template <typename FragmentT>
	static bool Register(uint8 TypeId)
	{
		return Register(TypeId, FragmentT::StaticStruct());
	}

	static void Unregister(uint8 TypeId);

	/** Null when no struct is registered for the id */
	static const UScriptStruct* FindStruct(uint8 TypeId);

	/** GSTNFeatureIds::Invalid when the struct is not registered */
	static uint8 FindId(const UScriptStruct* Struct);
};

/**
 * The operations on a fragment array, written once so that the combat sync state and every other host of fragments agree.
 *
 * Wire format of NetSerialize:
 *
 *   1 bit    any fragments present (0 = nothing else follows: an entity without fragments costs exactly this bit)
 *   packed   number of fragments (1 to MaxFragments)
 *   per fragment, in ascending type id order:
 *     8 bits   type id
 *     packed   payload length in bytes (at most MaxFragmentBytes)
 *     bytes    the fragment's own NetSerialize
 *
 * The payload is length-prefixed on purpose. The alternative, a payload size known from the registry, breaks exactly when it
 * matters: a machine that does not know the id has no size, and two builds that disagree on a fragment's size would
 * desynchronise the rest of the state silently. With the length, an unknown id is skipped whatever it holds, a
 * fragment that fails to read is dropped without moving the reader, and the cost is one byte per present fragment.
 * Fragments are written sorted by id and a duplicate id is written once, so the bytes do not depend on the order of
 * the array or on plugin load order.
 */
struct GSTNCOMBATCORE_API FGSTN_SyncFragmentList
{
	static constexpr int32 MaxFragments = 16;
	static constexpr int32 MaxFragmentBytes = 512;

	/** Writes Fragments, or replaces them with what the archive holds; fragments of unknown type id are skipped. Sets the archive error on a malformed list */
	static void NetSerialize(FArchive& Ar, FGSTN_SyncFragmentArray& Fragments);

	/** True when the two lists differ in which types they hold or in any fragment's ShouldReconcile */
	static bool ShouldReconcile(const FGSTN_SyncFragmentArray& Local, const FGSTN_SyncFragmentArray& Authority);

	/** Out becomes To with every fragment's Interpolate applied against the fragment of the same type in From (From can be null) */
	static void Interpolate(FGSTN_SyncFragmentArray& Out, const FGSTN_SyncFragmentArray* From, const FGSTN_SyncFragmentArray& To, float PCT);

	/** Appends " Fragments: [id: ...] [id: ...]" for a non-empty list and nothing for an empty one, so the text of a state without fragments is unchanged */
	static void ToString(const FGSTN_SyncFragmentArray& Fragments, FAnsiStringBuilderBase& Out);

	/** The fragment of type FragmentT, or null */
	template <typename FragmentT>
	static FragmentT* Find(FGSTN_SyncFragmentArray& Fragments)
	{
		for (TInstancedStruct<FGSTN_SyncFragment>& Fragment : Fragments)
		{
			if (FragmentT* Found = Fragment.template GetMutablePtr<FragmentT>())
			{
				return Found;
			}
		}
		return nullptr;
	}

	template <typename FragmentT>
	static const FragmentT* Find(const FGSTN_SyncFragmentArray& Fragments)
	{
		for (const TInstancedStruct<FGSTN_SyncFragment>& Fragment : Fragments)
		{
			if (const FragmentT* Found = Fragment.template GetPtr<FragmentT>())
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** The fragment of type FragmentT, added in id order when missing. The type must be registered */
	template <typename FragmentT>
	static FragmentT& FindOrAdd(FGSTN_SyncFragmentArray& Fragments)
	{
		if (FragmentT* Found = Find<FragmentT>(Fragments))
		{
			return *Found;
		}
		const int32 At = FindInsertIndex(Fragments, FGSTN_SyncFragmentRegistry::FindId(FragmentT::StaticStruct()));
		Fragments.Insert(TInstancedStruct<FGSTN_SyncFragment>::template Make<FragmentT>(), At);
		return *Fragments[At].template GetMutablePtr<FragmentT>();
	}

	/** Index that keeps Fragments sorted by type id when a fragment with TypeId is inserted */
	static int32 FindInsertIndex(const FGSTN_SyncFragmentArray& Fragments, uint8 TypeId);
};
