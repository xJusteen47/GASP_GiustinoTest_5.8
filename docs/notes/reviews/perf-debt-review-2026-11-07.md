# Perf debt review, 2026-11-07
Source: docs/notes/perf-debt.md as of 2026-11-07 (43 items, 1 already fixed). Categories: **fix before PvP** = per-frame or per-hit cost on host or every client that grows with player/minion count, or bandwidth that scales badly; **fix in toaster pass** = real but bounded cost, profile on the low-end PC first; **accept** = test/editor/debug-only, rare paths, already fixed or trivial.

## Lag compensation
- X4 rewind over all minions (BuildOverlay O(N) per attacker) - **fix before PvP** - per hit, grows with minions; wire GatherRewoundCandidates.
- History ring copies full arrays per tick - **fix before PvP** - host per-tick cost scales with store; write moved entities only.
- G5 EnsureSnapshot builds all entries with no fighter - **fix before PvP** - per frame; skip when no fighter is registered.
- G5 drain 8.9 us per hit - **fix before PvP** - victim entry and view rebuilt per hit; batch per victim.
- G10 SamplePose FindOldest scan - **accept** - only for reads older than the ring, rare.

## Minion batch and LOD
- Corpses stay in schedule, LOD, publish and hash (G9a/G11) - **fix before PvP** - dead slots cost per frame; skip them (watch test expectations).
- Demoted T0 dwell churn (G11) - **fix in toaster pass** - repeated demote/promote and counter growth; add held-by-cap bit.
- Store capacity 160, history ring ~70 % of slot memory (G12a) - **fix in toaster pass** - memory on low-end; ring for T0/T1 only.
- ApplyT0Cap bounded selection (G11) - **accept** - O(over x candidates) with cap 48 is small.
- HoldsAny linear walk on attack tokens (G11) - **accept** - only over cap, few dozen entries.
- T0 count from scheduled slots only (G11) - **accept** - fine while CombatInterval is 1.

## Minion net, replication and bridges
- Full per-minion state every frame, no delta (G6 packs) - **fix before PvP** - bandwidth scales with minions; delta/quantised packs with dormancy.
- NetPublish quantizes every member each pack-frame, linear id search (G6) - **fix before PvP** - host per-frame cost; quantize only rate-allowed members, id index.
- Relevancy pass, connections x members every 96 ms (G6) - **fix before PvP** - reuse batch player positions or hash grid.
- Type index on every item update, 9 bits (G6) - **fix before PvP** - bandwidth per update; needs per-connection baseline.
- Packs destroyed when empty, respawned per wave (G6) - **fix in toaster pass** - allocation churn; pool dormant empty packs.
- SetNetExtTypes global table read per item (G10) - **fix in toaster pass** - one pointer test; per-pack format if it gets hot.
- NotifyPublishObservers copies a pose per flagged slot (G10, bridge) - **accept** - up to 8 platforms, a few floats each.
- NetPublish first loop platform flag per slot (G10) - **accept** - FIXED in G11 (partly).

## Minion presentation (client)
- Pooled SkeletalMesh actors for minion visuals - **fix in toaster pass** - skeletal cost per actor; instanced or vertex-animated beyond a distance tier.
- Client registry walked every frame over all items (G7) - **fix before PvP** - every client, per frame; queue changed ids from OnItemReceived.
- Visible cut sorts every record every frame (G7) - **fix before PvP** - n log n per frame; nth_element or re-rank at 10 Hz.
- FindRecordBySlot linear per predicted hit (G7) - **fix before PvP** - per hit on clients; slot-to-key map.
- Snapshot provider re-walks all records; puppets set transform every frame (G7) - **fix before PvP** - per frame per client; dirty-only updates.
- Puppet mesh, anim class, puppet class, hurtbox set load synchronously (G7) - **fix in toaster pass** - first-use hitch; async preload on resolve.
- Clients TryLoad minion type synchronously on first appearance (G6) - **fix in toaster pass** - hitch; preload with the wave.

## Minion brain / AI
- StateTree execution context built per ticked slot (G8) - **fix before PvP** - per tick x minions on host; reuse one per tree, swap instance data.
- Separation neighbours walk the whole store per T0/T1 tick (G8) - **fix before PvP** - O(N) per minion; query the batch position hash.
- Target pick scans all fighters per slot at 2 Hz (G8) - **fix before PvP** - minions x fighters; push fighter positions once per frame.
- Native brain 10x cheaper than StateTree (G8 spike 6) - **fix before PvP** - largest per-tick cost; native for T1/T2 or grunts.
- StateTree loads synchronously at first spawn of a type (G8) - **fix in toaster pass** - one-off hitch; preload with definition.
- Per-slot StateTree instance arrays sized to 128 (G8) - **fix in toaster pass** - memory; sparse storage for brained slots.

## Waves, spawning and promotion
- Corpse list walk in FreeDueCorpses (G9a) - **fix in toaster pass** - fine to a few dozen; heap if 100+ die at once.
- GetAllActorsWithTag per wave start (G9a) - **fix in toaster pass** - world actor scan per wave; cache per tag.
- Wave preload skips assets already in memory (G9a) - **accept** - trade-off; a streamable handle costs memory.
- Promote loads fighter class synchronously (G9a) - **fix in toaster pass** - lone-promotion hitch; preload with definition.
- PromotedFighters weak list pruned per Promote (G9a) - **accept** - trivial, capped.

## Test harness and tooling
- Bench Attack tick: RequestGameplayTag and world lookup per tick (bench) - **accept** - test tooling only; cache both.
- Scenarios wait fixed wall-clock seconds (harness) - **accept** - test time; poll a condition.
- Minion scenarios wait fixed times, tier clock per record (G7a) - **accept** - test time; no cost of note.
- Bytes scenario reads the log after fixed waits (G11) - **accept** - test time; poll a condition.
- Debug draw per-minion calls when cvar on (G12a) - **accept** - debug only, zero cost off.
- CheckWaveCrossAsset / CheckDefinitionReferences sync loads (G12a) - **accept** - editor on save; never from a tick.

## Top 5 to do first
1. Net: delta/quantised packs, quantize only sendable members, relevancy reuse, drop per-update type index (bandwidth ceiling for PvP).
2. Lag comp: wire GatherRewoundCandidates into BuildOverlay (X4), per-hit O(N) today.
3. Brain: separation neighbours via the batch position hash (O(N) per minion per tick).
4. Client presentation: changed-id queue, slot-to-key map, dirty-only transforms (every client, every frame).
5. Brain: reuse one StateTree context per tree; decide native brain for grunts (spike 6 says 10x).
