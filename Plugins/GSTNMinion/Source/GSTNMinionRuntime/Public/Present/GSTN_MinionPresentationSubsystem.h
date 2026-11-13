// Minion runtime (client presentation, M8 G7).

#pragma once

#include "CoreMinimal.h"
#include "LOD/GSTN_MinionLOD.h"
#include "Presentation/GSTN_HitPresentation.h"
#include "Present/GSTN_MinionInterpolator.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"
#include "GSTN_MinionPresentationSubsystem.generated.h"

class AGSTN_MinionPuppet;
class AGSTN_MinionShatter;
class UGSTN_HurtboxSetDefinition;
class UGSTN_MinionClientRegistry;
class UGSTN_MinionCombatSubsystem;
class UGSTN_MinionDefinition;
class UGSTN_MinionPromotedComponent;

/** Where the presentation reads minions from this frame */
enum class EGSTN_MinionPresentSource : uint8
{
	None,
	/** Listen host or standalone: the authority store, no network copy, no interpolator, no render delay (4d.1) */
	Store,
	/** Remote client: the items of UGSTN_MinionClientRegistry, smoothed per tier (8.4) */
	Registry
};

/** Where the client's T0 render time came from */
enum class EGSTN_MinionRenderTimeSource : uint8
{
	None,
	/** The NP fixed-tick interpolation time (ToFrame - 1 + PCT): the clock interpolated players use */
	NetworkPrediction,
	/** No NP interpolation running yet: newest received frame advanced on local time, minus the NP buffer (100 ms) */
	FixedDelay
};

/** One minion this machine presents: its smoothing, its hit flash and, while it is in the visible set, its puppet */
struct FGSTN_MinionPresentRecord
{
	/** Store slot on the host; PackId << 4 | ItemId on a client */
	int32 Key = INDEX_NONE;
	/** Authority store slot (the pipeline id is MinionEntityBase | Slot); INDEX_NONE on a client until the pack's slot table arrived */
	int32 Slot = INDEX_NONE;
	int32 TypeIndex = INDEX_NONE;
	/** Host: the store generation of the slot (a new minion in a reused slot restarts the record) */
	uint16 Generation = 0;
	TWeakObjectPtr<const UGSTN_MinionDefinition> Definition;

	FGSTN_MinionInterpolator Interpolator;
	FGSTN_MinionHitFlash Flash;
	FGSTN_MinionDisplayPose Pose;
	EGSTN_MinionLODTier Tier = EGSTN_MinionLODTier::Combat;
	float HealthFraction = 1.f;

	/** Client: the registry's update count already taken in (a new count = a new sample) */
	int32 SeenUpdates = -1;
	/** Tick serial of the last time the source still had this minion */
	uint32 SeenSerial = 0;

	TWeakObjectPtr<AGSTN_MinionPuppet> Puppet;
	bool bVisible = false;
	/** The death placeholder already played for this minion */
	bool bShattered = false;
	/** The latest sample carries the Promoting flag (G9b): its Dead state is a hand-over to a fighter, not a death */
	bool bPromoting = false;
	/** Its puppet was swapped for the promoted fighter: the record stays until the item goes, without a puppet */
	bool bPromotionSwapped = false;
};

/** Counters of the presentation, for the dump and the harness. Not state */
struct FGSTN_MinionPresentStats
{
	int32 Active = 0;
	int32 Visible = 0;
	int32 Culled = 0;
	int32 Pooled = 0;
	int32 PuppetsSpawned = 0;
	int32 Acquires = 0;
	int32 Releases = 0;
	int32 ShattersActive = 0;
	int32 ShattersPooled = 0;
	int32 ShattersPlayed = 0;
	int32 HittableEntries = 0;
	int32 PredictedHits = 0;
	int32 PredictionCancels = 0;
	/** Promotion swaps (G9b): done, fighters with no puppet to swap, puppets released with no fighter, and the last swap's numbers */
	int32 PromotionSwaps = 0;
	int32 PromotionsWithoutPuppet = 0;
	int32 PromotionPuppetsExpired = 0;
	float LastPromotionPopCm = 0.f;
	float LastPromotionWaitMs = 0.f;
	double RenderFrame = 0.0;
	EGSTN_MinionRenderTimeSource RenderSource = EGSTN_MinionRenderTimeSource::None;
	EGSTN_MinionPresentSource Source = EGSTN_MinionPresentSource::None;
};

/**
 * Shows the minions on every machine that renders (combat-m8-minions.md 8.4 and 4d.1). Not created on a dedicated server (nothing renders)
 * and inert on a PIE dedicated server world.
 *
 * Every frame (FTickableGameObject, after the world's tick groups, so after NP's interpolation update of the frame):
 *  1. Reads the minions: on a listen host or standalone the authority store directly (current transform, ActionState, MoveId, HitCounter;
 *     no pack item, no interpolator, no extra render delay); on a remote client the latest items of UGSTN_MinionClientRegistry,
 *     dequantized with NP's latest received server frame as the reference and fed to one FGSTN_MinionInterpolator per minion.
 *  2. T0 is interpolated on the NP interpolation time (FFixedTickState::Interpolation: ToFrame - 1 + PCT, the time NP shows interpolated
 *     players at); T1/T2 dead-reckon with 200 ms correction blends; Teleport snaps. The tier is inferred from the sample gaps (the wire
 *     does not carry it).
 *  3. Hit presentation: follows the local player's own combat state with FGSTN_HitPresentationTracker (the tracker of
 *     UGSTN_HitPresentationComponent, which cannot present minions itself: it looks victims up as registered actors and flashes through a
 *     material parameter). A new HitTargets entry on a minion id flashes and pauses its puppet at once; the minion's HitCounter (store on
 *     the host, item on a client) confirms it within 500 ms, an entry that vanishes cancels it, a counter change with no prediction plays
 *     the same effect (other players' hits).
 *  4. Keeps the MaxVisiblePuppets nearest minions (gstn.Minion.Present.MaxVisible, default 60) with a pooled AGSTN_MinionPuppet each; the
 *     others are culled: no puppet, but still presented (smoothing, flash state) and still hittable.
 *  5. A minion that reaches Dead plays the pooled placeholder shatter once and loses its puppet.
 *  6. Promotion swap (G9b, combat-m8-minions.md 10): a minion whose item carries Promoting is not a death (no shatter). Its fighter (with
 *     UGSTN_MinionPromotedComponent) hands itself over at BeginPlay; the fighter is hidden while the puppet is still shown, and after
 *     gstn.Minion.Present.PromoteSwapSeconds of fighter life (its first interpolated transforms) the puppet goes back to the pool and the
 *     fighter shows: a swap at the same place, no material fade (the puppet has no opacity parameter). A puppet whose item leaves first is
 *     held where it stood; held puppets and waiting fighters give up after gstn.Minion.Present.PromoteHoldSeconds. One "MinionPromote" log
 *     line per swap with the distance between the puppet and the fighter's feet (the pop) and the wait.
 *
 * Client hittables (9.1): on a remote client this subsystem is the IGSTN_HittableProvider of the Minion kind (the authority's subsystem is
 * inactive there) and appends one entry per presented minion whose store slot is known, with the server's pipeline id MinionEntityBase |
 * slot (the pack replicates its slot table), the displayed transform, the type's hurtboxes and team. So the local attacker's prediction
 * hits puppets with no new code in the attacker path and writes the same HitTargets id the server's tick writes. Generation is 0 (the
 * client never posts to a minion inbox) and hurt states are left Normal (no client-side timeline evaluation yet).
 *
 * Presentation only: nothing here is read by the simulation (the provider entries are the client's own prediction input, as the spec
 * wants), no RPC, no replicated state.
 */
UCLASS()
class GSTNMINIONRUNTIME_API UGSTN_MinionPresentationSubsystem : public UTickableWorldSubsystem, public IGSTN_HittableProvider
{
	GENERATED_BODY()

public:
	/** The subsystem of World when it is presenting, null otherwise (dedicated server, editor world, before begin play) */
	static UGSTN_MinionPresentationSubsystem* GetActive(const UWorld* World);

	// UWorldSubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// IGSTN_HittableProvider (registered on remote clients only)
	virtual int32 GetProviderId() const override;
	virtual void AppendSnapshotEntities(int32 Frame, TArray<FGSTN_SnapshotEntry>& OutEntries) override;
	virtual uint8 GetEntityKind() const override { return GSTNEntityKinds::Minion; }

	/** One presentation frame at local time NowSeconds. Tick calls it with the world's time; tests call it directly */
	void TickPresentation(double NowSeconds);

	/** Smoothing tunables (8.4) */
	FGSTN_MinionSmoothingParams Smoothing;
	FGSTN_MinionFlashParams FlashParams;

	/** A puppet already shown keeps its place in the visible set until another minion is this much nearer (no flicker at the cut) */
	float VisibleStickyFraction = 0.1f;

	/** Placeholder shatter lifetime */
	float ShatterSeconds = 0.5f;

	/** MaxVisiblePuppets in force: the test override when set, else gstn.Minion.Present.MaxVisible */
	int32 GetMaxVisible() const;

	const TMap<int32, FGSTN_MinionPresentRecord>& GetRecords() const { return Records; }
	const FGSTN_MinionPresentRecord* FindRecord(int32 Key) const { return Records.Find(Key); }
	const FGSTN_MinionPresentStats& GetStats() const { return Stats; }
	bool IsPresenting() const { return bActive; }

	/** Pool: a puppet for Definition (reused when one is free, spawned otherwise). Public for the pool tests */
	AGSTN_MinionPuppet* AcquirePuppet(const UGSTN_MinionDefinition* Definition, int32 Key, const FTransform& Transform);
	void ReleasePuppet(AGSTN_MinionPuppet* Puppet);
	int32 GetNumPooledPuppets() const;

	/** Log lines for the harness (gstn.Minion.Present.Dump): one per presented minion and a summary; formats in the .cpp */
	void DumpToLog() const;

	/** A promoted fighter began play on this machine (its component calls it): swap it with the puppet of its minion (item 6 above) */
	void NotePromotedFighter(UGSTN_MinionPromotedComponent* Fighter);

	// Tests and the harness
	/** Activates without begin play (a test world has none). bAsClient registers the hittable provider like a remote client */
	void ActivateForTests(bool bAsClient = false);
	/** Forces the viewer location of the visible cut (unset: the first local player's view point) */
	TOptional<FVector> ViewerOverrideForTests;
	/** Overrides gstn.Minion.Present.MaxVisible when >= 0 */
	int32 MaxVisibleOverrideForTests = INDEX_NONE;
	/** Forces the client T0 render frame (unset: from NP) */
	TOptional<double> RenderFrameOverrideForTests;
	/** Client path: a predicted hit on a minion pipeline id, as the local attacker's tracker reports it */
	void NotePredictedHitForTests(int32 EntityId, double NowSeconds) { OnPredictedHit(EntityId, NowSeconds); }
	void NotePredictionCancelledForTests(int32 EntityId, double NowSeconds) { OnPredictionCancelled(EntityId, NowSeconds); }

private:
	void Activate(bool bAsClient);
	void SyncFromStore(const UGSTN_MinionCombatSubsystem& Minions, double NowSeconds);
	void SyncFromRegistry(const UGSTN_MinionClientRegistry& Registry, double NowSeconds);
	/** Client: NP's reference frame (before the sync) */
	void UpdateReferenceFrame();
	/** Client: the T0 render frame (after the sync, which feeds the fixed-delay fallback) */
	void UpdateRenderTime(double NowSeconds);
	void UpdatePredictedHits(double NowSeconds);
	void OnPredictedHit(int32 EntityId, double NowSeconds);
	void OnPredictionCancelled(int32 EntityId, double NowSeconds);
	FGSTN_MinionPresentRecord* FindRecordBySlot(int32 Slot);
	void UpdateVisibleSet(double NowSeconds);
	void ApplyToPuppets(double NowSeconds);
	void PlayShatter(const FGSTN_MinionPresentRecord& Record, double NowSeconds);
	void TickShatters(double NowSeconds);
	void RemoveRecord(int32 Key);
	void UpdatePromotionSwaps(double NowSeconds);
	FVector GetViewerLocation() const;
	const UGSTN_HurtboxSetDefinition* GetHurtboxes(const UGSTN_MinionDefinition* Definition);

	bool bActive = false;
	bool bClientProvider = false;
	uint32 TickSerial = 0;

	TMap<int32, FGSTN_MinionPresentRecord> Records;

	/** Every puppet and shatter this subsystem spawned (keeps them referenced; none is destroyed before the world goes) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGSTN_MinionPuppet>> AllPuppets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGSTN_MinionShatter>> AllShatters;

	/** Free puppets by definition (null key = a type with no definition resolved). Keys are compared, never dereferenced */
	TMap<const UGSTN_MinionDefinition*, TArray<TWeakObjectPtr<AGSTN_MinionPuppet>>> FreePuppets;
	TArray<TWeakObjectPtr<AGSTN_MinionShatter>> FreeShatters;

	/** Hurtbox sets by definition, loaded once per type (client provider entries) */
	TMap<const UGSTN_MinionDefinition*, TWeakObjectPtr<const UGSTN_HurtboxSetDefinition>> HurtboxCache;
	TMap<const UGSTN_MinionDefinition*, float> BoundRadiusCache;

	/** Client clock: the reference server frame for stamp reconstruction and the fixed-delay fallback */
	int32 ReferenceFrame = INDEX_NONE;
	int32 FallbackNewestFrame = INDEX_NONE;
	double FallbackNewestSeconds = 0.0;

	/** The local attacker's HitTargets ring, followed for predicted hits on minions */
	FGSTN_HitPresentationTracker HitTracker;

	/**
	 * Minion hits this machine already predicted, by key (victim, move start, hit group). An NP correction can resimulate the move and drop a hit
	 * group from the ring for a frame or two before the resim adds it back with the same key (G7b: 6 victims of one group cancelled and
	 * re-predicted 40 ms later). A vanished key is only a cancel once it stayed away for HitCancelGraceSeconds; a key that comes back is the
	 * same hit (no cancel, no second prediction). Presentation only, never read by the simulation. The rule is shared with the pawn component.
	 */
	FGSTN_HitPredictionGrace PredictedMinionHits;
	static constexpr double HitCancelGraceSeconds = FGSTN_HitPredictionGrace::DefaultCancelGraceSeconds;
	/** A key not seen for this long is forgotten (its move is long over) */
	static constexpr double PredictedHitMemorySeconds = FGSTN_HitPredictionGrace::DefaultMemorySeconds;

	FGSTN_MinionPresentStats Stats;

	/** A Promoting minion's puppet whose item already left: it stays where it stood for its fighter (item 6) */
	struct FHeldPuppet
	{
		int32 Key = INDEX_NONE;
		TWeakObjectPtr<AGSTN_MinionPuppet> Puppet;
		double ReleaseAtSeconds = 0.0;
	};
	TArray<FHeldPuppet> HeldPuppets;

	/** A promoted fighter waiting for its swap */
	struct FPendingSwap
	{
		TWeakObjectPtr<UGSTN_MinionPromotedComponent> Fighter;
		int32 Key = INDEX_NONE;
		double NotedSeconds = -1.0;
		bool bHidden = false;
	};
	TArray<FPendingSwap> PendingSwaps;

	// Scratch of the visible cut
	TArray<int32> ScratchKeys;
	TArray<float> ScratchDistances;
	TArray<bool> ScratchHasPuppet;
	TArray<int32> ScratchVisible;
};
