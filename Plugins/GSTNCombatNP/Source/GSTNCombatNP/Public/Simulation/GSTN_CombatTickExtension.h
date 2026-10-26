// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Timeline/GSTN_TimelinePlayer.h"

class AActor;
class UAbilitySystemComponent;
struct FGSTN_SimEventOutbox;

/**
 * The fixed points of the combat tick where an extension can run (combat-m3 7.2). They are part of the contract: a point is
 * never moved relative to the others, and a new one is added, not inserted in the middle of the existing meaning.
 *
 * The combat tick of today is the embedded input simulation plus the debug hitstop, so the points sit where the steps of 7.2
 * can be reached now: the input simulator runs candidates, resolution and the request as one call. When the timeline clock,
 * adoption and tag projection join the model, the call sites move to their steps and the meaning below holds.
 */
enum class EGSTN_CombatHookPoint : uint8
{
	/** The clocks (timeline, hitstop) of the frame are up to date: 7.2 steps 1 to 4. Runs on the restored state, before candidates */
	AfterClocks,

	/** Right before the candidate commands are collected and resolved: 7.2 step 5 */
	BeforeCandidates,

	/** The winner of the frame, if any, is written as a request: 7.2 step 8 */
	AfterExecute,

	/** Right before the combat tags are projected for the ability system: 7.2 step 10. Last point of the tick */
	BeforeTagProjection,

	Count
};

/** What a tick extension reads and writes at a hook point */
struct FGSTN_CombatTickContext
{
	/** Network Prediction frame being simulated */
	int32 Frame = 0;

	/** The state the frame started from; never written */
	const FGSTN_CombatSyncState& In;

	/** The state the frame produces; an extension changes only its own fragment in here */
	FGSTN_CombatSyncState& Out;

	/**
	 * Out-parameter of the BeforeCandidates point: an extension sets the bits of the hit groups of the entity's own hitboxes that
	 * must not hit this frame (combat-m4-hits 3b, item 6). Empty at the start of every frame, which masks nothing; nothing reads
	 * it before the attacker side exists (M4 H8)
	 */
	FGSTN_HitGroupMask HitGroupMask;

	/**
	 * What the clock step evaluated this frame, for the windows the track hooks stamped (FGSTN_TimelineOutput::StampedWindows): an extension writes them into its own
	 * fragment at AfterClocks. Valid for this tick only; null in a test
	 */
	const FGSTN_TimelineOutput* Timeline = nullptr;

	/**
	 * The entity's actor and ability system, for an extension that must reach content or the tags of the entity (it projects tags from its
	 * fragment at BeforeTagProjection). Null in a test. Read-only use of content: an extension never reads simulation state from them
	 */
	AActor* Owner = nullptr;
	UAbilitySystemComponent* AbilitySystem = nullptr;

	/**
	 * The outbox of the frame, for an event the extension makes without a hit (a restored part). The component publishes it on the authority only,
	 * once per frame, so an extension appends unconditionally and a re-simulated frame never publishes twice. Null in a test
	 */
	FGSTN_SimEventOutbox* Outbox = nullptr;

	FGSTN_CombatTickContext(int32 InFrame, const FGSTN_CombatSyncState& InState, FGSTN_CombatSyncState& OutState)
		: Frame(InFrame), In(InState), Out(OutState)
	{
	}
};

/**
 * A hook an optional plugin registers to run inside the combat tick (combat extension surface, modularity-and-tags 5b). C++
 * only: it runs for every entity on every simulated and re-simulated frame, so there is no Blueprint variant.
 *
 * It is simulation code and follows the rules of gas-npp.md: deterministic, rollback-safe, its state in its own sync
 * fragment (FGSTN_SyncFragment) and nowhere else, no reads of components, the world or the ability system, no branches on
 * the net mode. The hooks of all registered extensions run in the order of (priority, type id), identical on every machine
 * whatever the plugin load order.
 */
class GSTNCOMBATNP_API IGSTN_CombatTickExtension
{
public:
	virtual ~IGSTN_CombatTickExtension() = default;

	/** The extension's feature id from GSTNFeatureIds, the same as its sync fragment's. Fixed for the lifetime of the registration */
	virtual uint8 GetTypeId() const = 0;

	/** Lower runs first; equal priorities run in type id order. Fixed for the lifetime of the registration */
	virtual int32 GetPriority() const { return 0; }

	virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) = 0;

	/**
	 * Does the extension's state forbid the entity to start any command this frame (a held shield, a grab)? OR-ed over every extension into the
	 * can-act rule, asked once per frame after the BeforeCandidates point and before the candidates are collected, so an extension that
	 * changed its fragment there is seen. It closes the gate the way a hit reaction does: nothing starts, the input is recorded and aged as usual. A pure
	 * read of Context.Out (and In), no side effect. Default: never
	 */
	virtual bool BlocksActions(const FGSTN_CombatTickContext& Context) const { return false; }
};

/**
 * The registered tick extensions, kept in their run order. The combat model calls Run at every hook point whether or not
 * anything is registered. Only the game thread uses it; extensions register when their module starts, before any
 * simulation tick, and unregister when it shuts down.
 */
class GSTNCOMBATNP_API FGSTN_CombatTickExtensionRegistry
{
public:
	/** The registry the combat model runs */
	static FGSTN_CombatTickExtensionRegistry& Get();

	/** False when an extension with the same type id is already registered (two hooks of one feature have no defined order) */
	bool Register(IGSTN_CombatTickExtension* Extension);

	void Unregister(IGSTN_CombatTickExtension* Extension);

	/** Calls every extension's hook for Point, in order */
	void Run(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) const;

	/** True when any extension's BlocksActions says so (every extension is asked, in order, so none depends on another's answer) */
	bool BlocksActions(const FGSTN_CombatTickContext& Context) const;

private:
	struct FEntry
	{
		int32 Priority = 0;
		uint8 TypeId = 0;
		IGSTN_CombatTickExtension* Extension = nullptr;
	};

	// Priority and type id are read once at registration, so the order cannot change under a running simulation
	TArray<FEntry> Entries;
};
