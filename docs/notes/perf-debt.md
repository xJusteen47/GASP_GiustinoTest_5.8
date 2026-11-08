# Perf debt

Choices suspected of not being the most performant. Target hardware is very low end; revisit at the end of M8. One line each:
what, where, cheaper alternative.

- X4 lag-comp rewind iterates all minions: `GSTNLagComp::BuildOverlay` samples every provider entry per rewinding attacker (O(N)); the cheap
  `GatherRewoundCandidates` query exists but is not wired into GSTNCombatNP. Alternative: provider hook in BuildOverlay using the hash + swept bound.
- Pooled SkeletalMesh actors for minion visuals (`AGSTN_MinionActor`): alternative, instanced/vertex-animated meshes beyond a distance tier.
- Full per-minion net state each frame, no delta (`GSTNMinion` replication, G6 packs): alternative, delta/quantised packs with dormancy.
- History ring copies full arrays per tick (`GSTN_MinionHistory`, world subsystem ring): alternative, write only moved entities / ring of deltas.
- G5 `EnsureSnapshot(F)` from the post-tick hook builds S(F) with all minion entries even when no fighter ticked: skip when no fighter is registered.
- G5 drain cost 8.9 us per hit (spike 5): victim entry and view rebuilt per hit; batch per victim and reuse the view.
- Bench Attack tick (`GSTN_MinionBench.cpp`): `RequestGameplayTag` and a combat-world lookup every world tick; cache both (test tooling only).
- Harness: scenarios wait fixed wall-clock seconds; poll a condition instead (test time only).
- G6 `NetPublish` (`GSTN_MinionCombatSubsystem_Net.cpp`) quantizes every member each pack-frame and finds items with a linear search: keep an
  id-to-index table on the pack and quantize only members whose rate limiter allows a send.
- G6 relevancy pass: connections x members distance checks every 96 ms: reuse the batch's player positions or the hash grid.
- G6 packs are destroyed when empty and respawned in waves: pool empty packs (dormant, 0 items).
- G6 clients `TryLoad` a minion type synchronously when it first appears: preload with the wave (G9).
- G6 type index sent on every item update (9 bits): send it on add only with a per-connection baseline (needs custom delta).
- G7 presentation (`GSTN_MinionPresentationSubsystem`): the client registry is walked every frame over all items; queue changed ids from
  OnItemReceived instead.
- G7 visible cut sorts every record every frame: partial select (nth_element) or re-rank at 10 Hz.
- G7 `FindRecordBySlot` is linear per predicted hit on clients: keep a slot-to-key map.
- G7 puppet mesh, anim class, puppet class and hurtbox set load synchronously on first use: async preload when the registry resolves a type.
- G7 snapshot provider re-walks all records per build; puppets call SetActorLocationAndRotation every frame even when unchanged.
- G7a: the minion harness scenarios still wait fixed times and judge from log dumps; tier inference now reads the clock per record per frame (cheap, no new cost of note).
- G8 brain (`GSTN_MinionBrainSubsystem.cpp`): an FStateTreeExecutionContext is built per ticked slot: reuse one per tree and swap the instance data.
- G8 separation neighbours walk the whole store per T0/T1 brain tick (O(N) per minion): query the batch's position hash.
- G8 target pick scans every fighter per slot at 2 Hz and resolves owner actors per pass: push fighter positions from the combat world once per frame.
- G8 the type's StateTree loads synchronously at the first spawn of a type: async preload with the definition (with the G9 wave preload).
- G8 per-slot StateTree instance data arrays sized to store capacity (128): sparse storage for brained slots only.
- G8 spike 6: native brain is 10x cheaper than the StateTree (0.49 vs 4.96 us per tick): for the low-end target, consider native for T1/T2 or for grunts.
- G9a corpses (`FreeDueCorpses`): a list walk each batch (only the waiting corpses, order-preserving RemoveAt): fine up to a few dozen; a heap by end frame if waves of 100+ die at once.
- NOT fixed in G11 (the hash feeds the platform observer and lag-comp counters; changing it risks test expectations): G9a a corpse stays in the schedule, the LOD update, the publish pass and the hash rebuild like a live minion: skip Dead slots in the schedule and in the candidate hash (it is not hittable).
- G9a `AGSTN_MinionSpawnerActor`: spawn point actors are gathered with GetAllActorsWithTag once per wave start (a world actor scan); cache per tag on a world subsystem if waves start often.
- G9a wave preload skips assets already in memory (nothing holds them): a streamable handle for the whole wave would keep them resident, at the price of memory on the low-end target.
- G9a Promote loads the fighter class synchronously when nothing preloaded it (a lone promotion, not from a wave): preload PromotionFighterClass with the definition.
- G9a `PromotedFighters` weak list is pruned on every Promote call (linear in at most the cap): trivial, noted for completeness.
- G10 `NotifyPublishObservers` copies one pose per flagged slot into a reused array each step (a few floats each, up to 8 platforms): hand the observer the store columns by slot list instead if a bridge ever needs more than a handful.
- G10 `SetNetExtTypes` is a process-wide table read by every item `NetSerialize` (one pointer test while no type has an ext byte): a per-pack type format cached on the pack would avoid the global.
- FIXED G11 (partly: the flag byte is read only while a platform is flagged; the activation walk itself stays, it finds the unassigned slots): G10 `NetPublish`'s first loop now tests the platform flag for every activated slot each frame (one byte test per slot): fold it into the slot list the dirty pass already walks once G11 restructures the pass.
- G10 `SamplePose` falls back to `FindOldest`, a forward scan of up to the ring depth (32) only for a frame older than the ring: fine for a rare lag-comp read; keep the ring's first-frame index per slot if it becomes hot.
- G11 `ApplyT0Cap` defers the tier change of every Combat candidate to the end of the LOD step and finds the overshoot with a bounded selection (Count passes over the candidates, O(over x candidates)); fine for the cap of 48. An `nth_element` or a bucket by distance if the cap ever sits far below the T0 population.
- G11 `HoldsAny` on the attack tokens is a linear walk of the entries, called only for candidates and only while over the cap: index token holders by slot if the entries grow past a few dozen.
- G11 the T0 count is taken from the scheduled slots only: a slot woken by the inbox is counted from its next step; fine while CombatInterval is 1, keep a running Combat counter in the store if a definition ever sets a larger interval.
- G11 the harness bytes scenario reads the log line of the report after fixed waits (like the other minion scenarios): poll a condition instead.
- G11 a demoted T0 slot dwells in Near, is pulled back to Combat by the stepper after MinDwellFrames and silently demoted again (no event, but the dwell restarts and the cumulative demoted counter grows, 696 in 80-minion bytes): keep a per-slot "held by cap" bit and skip the stepper's promotion while still over the cap.
- G12a debug draw (`GSTN_MinionDebugDraw.cpp`): with a cvar on, one pass iterates all alive slots and issues per-minion DrawDebug* calls (and an FString per minion for `.LOD`, up to N points per minion for `.History`); debug only, zero cost off (tick gated by one flag). Alternative if it is ever left on in a profile: draw only the Near-and-closer tiers, cache the tier strings, and use a line batch for the history points.
- G12a store capacity 160: history ring (32 x 52 B) is about 70 % of the per-slot memory, allocated for every slot though Dormant and Far slots never write it. Alternative: a ring only for T0/T1 slots (compacted index) or a depth cut by the lag comp window actually used.
- G12a `GSTNMinionValidation::CheckWaveCrossAsset` and `CheckDefinitionReferences` load soft references synchronously (editor, on save only); fine for validation, do not call from a tick.
- FP G1 the predicted partner presentation adds a second tick function to every `UGSTN_LinkPresentationComponent` (registered off a dedicated server): on a client pawn that is not locally controlled it ends after the cvar read and one cast; on the locally controlled pawn it also looks the link fragment up in the combat sync state each frame (a linear walk of the small fragment list). Gate it by enabling the tick function only while a link is shown if it ever shows up in a profile.
- FP G1 `BeginPredictedWindow` runs `FindOrAdd` on the partner and requests the partner montage asynchronously on every partner move; the streamable request is cheap when the montage is resident, but a cold montage means the first move of a match shows no montage (the reaction is still published): preload the partner timeline montages with the kit's anim sets.
- E17 combat trace: with `gstn.Combat.Debug.Trace 1` every entity-frame builds a `FGSTN_CombatTraceFrame` and calls a virtual sink even when the Insights channel is off (the emit is then a no-op after the channel check); fine for a debug cvar. If it is left on in a profile, check `UE_TRACE_CHANNELEXPR_IS_ENABLED(GSTNCombatChannel)` in the sink before building the record.
- P18 hurtbox openings (`GSTN_CombatTick_Attacker.cpp`): one bool per touching hurtbox without an opening; with one, a size test then a mouth overlap (up to MaxSweepSubsteps + 1 shape tests when the hitbox sweeps). The per-hurtbox hardness override costs one emptiness test per part hit, a TMap find only on parts that have overrides. Alternative if openings get common: cache the hitbox's pass radius once per hitbox-victim pair instead of per hurtbox.
