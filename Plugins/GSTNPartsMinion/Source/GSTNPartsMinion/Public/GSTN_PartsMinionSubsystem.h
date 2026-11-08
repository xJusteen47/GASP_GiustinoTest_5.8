// Bridge between parts and minions (plan P16).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartsLimits.h"
#include "Data/GSTN_PartsMinionExtension.h"
#include "GameplayTagContainer.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Store/GSTN_MinionStore.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "GSTN_PartsMinionSubsystem.generated.h"

class AActor;
class UGSTN_MinionClientRegistry;
class UGSTN_MinionCombatSubsystem;
class UGSTN_MinionDefinition;
class UGSTN_PartSetDefinition;
struct FGSTN_HitResolutionContext;
struct FGSTN_HitVictimView;

/**
 * The part state of one part-bearing minion (combat-parts.md §5.5): the store row the bridge keeps per slot, 9 bytes. The same bytes
 * FGSTN_PartsSnapshotExt::FromMinionStore reads and ToMinionStore writes. No break serial: a minion's events are keyed by its handle.
 */
struct FGSTN_PartsMinionRow
{
	uint8 BrokenMask = 0;
	uint8 PartDamage[GSTN_PartsLimits::MaxMinionParts] = {};
};
static_assert(sizeof(FGSTN_PartsMinionRow) == 1 + GSTN_PartsLimits::MaxMinionParts, "the minion part row is 9 bytes");

/** Counters of the bridge since the world started (tests and the bench) */
struct FGSTN_PartsMinionStats
{
	/** Hits whose outcome carried a part outcome and were written to a row */
	int32 PartHits = 0;
	/** Minions the bridge killed because a lethal part broke */
	int32 LethalKills = 0;
	/** Activations of part-bearing types (rows reset) */
	int32 PartActivations = 0;
	/** SetExtByte calls: one per hit that changed a broken mask, never for a dent (sub-gate B) */
	int32 ExtByteWrites = 0;
	/** Promotions of a part-bearing minion whose row went to the fighter as a seed */
	int32 PromotionSeeds = 0;
	/** Definitions this machine flagged bHasNetExt (registration on the server, registry resolve on a client) */
	int32 NetExtFlagged = 0;
	/** Bench timing of the bridge's own hit work, in cycles: the victim view build and the outcome write (the step times itself) */
	uint64 ViewCycles = 0;
	int32 Views = 0;
	uint64 OutcomeCycles = 0;
	int32 Outcomes = 0;
};

/**
 * Parts on minions, server side (plan P16 sub-gate A; combat-parts.md §5.5, combat-m8-minions.md §4d.2).
 *
 * Columns: one FGSTN_PartsMinionRow per store slot (9 B), a type byte per slot (0 = no parts: the only thing a minion of a type without an
 * extension ever costs after its activation), the slot's generation (a row of a freed or reused slot is never read for the new
 * occupant), and one view scratch FGSTN_PartsSnapshotExt per slot (what the victim view points at, valid until that slot's next view).
 * The columns are sized once to the store capacity at the first activation, so no hit allocates here.
 *
 * Binding (Initialize, through the generic seams of UGSTN_MinionCombatSubsystem, GSTNMinion knowing nothing of parts):
 *  - OnMinionActivated: one type lookup per type (cached by store TypeIndex), then the slot's type byte, generation and a zero row;
 *  - VictimViewExtension: points the minion victim view at an FGSTN_PartsSnapshotExt built from the row (FromMinionStore);
 *  - the extra victim step slot: UGSTN_HitStep_PartResolveMinion, the unchanged part math right before ComputeKnockback;
 *  - ApplyOutcomeExtensions: writes the part outcome back into the row (ToMinionStore) and asks for death when a part of the type's
 *    LethalParts broke. No gameplay effect, no fragment: the store's Health took MainDelta through Outcome.Damage like any minion hit.
 *
 * Wire (sub-gate B, combat-m8-minions.md §8.2 G10 ExtByte): a part-bearing type gets UGSTN_MinionDefinition::bHasNetExt, set by this bridge
 * from its own config at the one moment the type's wire format is decided: OnDefinitionRegistering on the server (its first spawn in the
 * world) and OnDefinitionResolving of the client registry on a client, the same rule on both from the same config, so both build the same
 * format. The byte is the row's BrokenMask, written with SetExtByte only when a hit changed it (a dent changes nothing on the wire, the damage
 * bytes never leave the server). A type without parts keeps 0 extension bits. A definition registered here after its first spawn keeps its
 * format (warning): flipping the flag later would desync the item codec.
 *
 * Promotion (sub-gate B, M8 §10): OnMinionPromoted copies the row to the fighter as a seed of FGSTN_PartsAttach (by part name), merged into the
 * fighter's parts fragment by the parts tick extension at its next frame, so the fighter keeps the parts the minion lost. The fighter needs
 * its own part set attached (kit or UGSTN_PartsAttachComponent); without one the seed waits and dies with the actor.
 *
 * Determinism: the hooks run inside stage 2 of the batch, in the drain's fixed order (victim slot, attacker, hitbox), on the authority
 * only (the minion subsystem is active nowhere else). Every write is a pure function of the row, the static part set and the hit.
 * The Kill flag of a MainDamage break effect is not read: for a minion, lethality is LethalParts and nothing else (FGSTN_PartsMinionExtension).
 */
UCLASS()
class GSTNPARTSMINION_API UGSTN_PartsMinionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Attaches an extension to one exact definition (wins over the type tag of UGSTN_PartsMinionSettings). Takes effect at the next activation
	 * of that type; minions already alive keep what they had
	 */
	void RegisterDefinition(const UGSTN_MinionDefinition* Definition, const FGSTN_PartsMinionExtension& Extension);

	/** The extension that applies to Definition on this machine (exact definition first, then its type tag), null when none */
	const FGSTN_PartsMinionExtension* FindExtension(const UGSTN_MinionDefinition* Definition) const;

	/**
	 * The part set of an extension when it can serve a minion (loaded, not empty, bForMinions, at most 8 parts), null otherwise. The one
	 * rule behind the activation, the server's net flag and the client's net flag, so the three always agree. May load synchronously
	 */
	static UGSTN_PartSetDefinition* ResolveUsableSet(const FGSTN_PartsMinionExtension& Extension);

	/** The part set this machine shows for a definition (presentation, a client included): cached per definition, null without parts */
	const UGSTN_PartSetDefinition* GetPresentationSet(const UGSTN_MinionDefinition* Definition);

	/** Attaches an extension to a type tag (what the settings map does). Same timing as RegisterDefinition */
	void RegisterTypeTag(const FGameplayTag& TypeTag, const FGSTN_PartsMinionExtension& Extension);

	/** True for a live minion of a part-bearing type */
	bool HasParts(const FGSTN_MinionHandle& Handle) const;

	/** The part row of a live part-bearing minion, null otherwise (stale handle, freed slot, type without parts) */
	const FGSTN_PartsMinionRow* GetRow(const FGSTN_MinionHandle& Handle) const;

	/** The lethal mask of the minion's type (bit i = part i kills when it breaks), 0 without parts */
	uint8 GetLethalMask(const FGSTN_MinionHandle& Handle) const;

	/** The part set of the minion's type, null without parts */
	const UGSTN_PartSetDefinition* GetPartSet(const FGSTN_MinionHandle& Handle) const;

	const FGSTN_PartsMinionStats& GetStats() const { return Stats; }

	/** Bench: the frame a part of the slot's current occupant last broke, INDEX_NONE before any break (the report's "attacked after the break") */
	int32 GetLastBreakFrame(const FGSTN_MinionHandle& Handle) const;

	/**
	 * Bench (gstn.PartsMinion.Bench.Parts): builds a transient minion part set (a Head part on HeadHurtboxes, bone "head", cap HeadCap, not
	 * lethal, and a Legs part on "feet" that does not break) and attaches it to TypeTag. Returns the set. No asset is created or saved
	 */
	UGSTN_PartSetDefinition* RegisterBenchParts(const FGameplayTag& TypeTag, const TArray<FName>& HeadHurtboxes, float HeadCap);

	/** Bench (gstn.PartsMinion.Bench.Report): the server line (rows, masks by global id, timings) and, on a client, its received ext bytes */
	void ReportBench() const;

	/** Slots the columns are sized to: 0 until the first part-bearing minion activates (a world without one pays no column) */
	int32 GetColumnCapacity() const { return Rows.Num(); }

	/** True once the bridge is bound to the world's minion subsystem */
	bool IsBound() const { return BoundMinions.IsValid(); }

private:
	struct FResolvedType
	{
		const UGSTN_PartSetDefinition* PartSet = nullptr;
		uint8 LethalMask = 0;
	};

	/** TypeToResolved values that are not an index */
	static constexpr int16 TypeUnresolved = -2;
	static constexpr int16 TypeWithoutParts = -1;

	void Bind(UGSTN_MinionCombatSubsystem* Minions);
	void Unbind();

	void HandleMinionActivated(const FGSTN_MinionHandle& Handle);
	/** Both net seams: sets bHasNetExt on a definition whose extension resolves to a usable set (never clears a designer's flag) */
	void HandleDefinitionNet(UGSTN_MinionDefinition* Definition);
	void HandleMinionPromoted(const FGSTN_MinionHandle& Handle, AActor* Fighter);
	void ExtendVictimView(int32 Slot, FGSTN_HitVictimView& View);
	bool ApplyOutcome(const FGSTN_MinionHandle& Handle, const FGSTN_HitResolutionContext& Context, int32 Frame);

	/** ResolvedTypes index of a store TypeIndex, TypeWithoutParts when the type has no valid extension. Cached */
	int16 ResolveType(int32 TypeIndex);
	void EnsureColumns(int32 Capacity);

	/** The ResolvedTypes index of a slot whose current occupant is Generation, INDEX_NONE otherwise */
	int32 GetSlotType(int32 Slot, uint16 Generation) const;

	TWeakObjectPtr<UGSTN_MinionCombatSubsystem> BoundMinions;
	FDelegateHandle ActivatedHandle;
	FDelegateHandle RegisteringHandle;
	FDelegateHandle PromotedHandle;
	TWeakObjectPtr<UGSTN_MinionClientRegistry> BoundRegistry;
	FDelegateHandle ResolvingHandle;

	/** GetPresentationSet cache (keys compared, never dereferenced; a null value = no parts). The sets are kept alive by ResolvedSets */
	TMap<TObjectKey<UGSTN_MinionDefinition>, const UGSTN_PartSetDefinition*> PresentationSets;

	/** Sets built by RegisterBenchParts, kept alive for the world */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGSTN_PartSetDefinition>> BenchSets;

	TMap<TObjectKey<UGSTN_MinionDefinition>, FGSTN_PartsMinionExtension> DefinitionExtensions;
	TMap<FGameplayTag, FGSTN_PartsMinionExtension> TagExtensions;

	/** Append-only: a slot's type byte indexes it, so a later registration never moves an entry a live minion uses */
	TArray<FResolvedType> ResolvedTypes;
	/** Keeps the resolved part sets alive while a row or a view points at them */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGSTN_PartSetDefinition>> ResolvedSets;
	/** By store TypeIndex (at most 256 types) */
	TArray<int16> TypeToResolved;

	// Columns, by store slot (sized to the store capacity)
	TArray<FGSTN_PartsMinionRow> Rows;
	/** 0 = no parts, else ResolvedTypes index + 1 */
	TArray<uint8> SlotType;
	TArray<uint16> SlotGeneration;
	TArray<FGSTN_PartsSnapshotExt> ViewExts;
	/** Bench only: the batch frame of the last break, by slot (reset on activation) */
	TArray<int32> LastBreakFrame;

	FGSTN_PartsMinionStats Stats;
};
