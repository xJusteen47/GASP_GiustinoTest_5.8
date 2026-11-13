# Many enemies: replication, presentation, hits, and enemies as platforms (research and proposal)

Research note for [combat-m8-minions.md](combat-m8-minions.md) (minions: store, packs, puppets) and for the new owner rule
"some enemies work as solid platforms" (players stand on them). Topology: listen server, host also renders
([game-design-direction.md](game-design-direction.md) §6b). Written 2026-10-22. **Nothing here edits a specification**: §3.2 lists
what M8 should change and the owner decides. Section numbers are local to this file.

Recommendations are numbered Rec1-Rec12 (§3.1); spikes are R1-R6 and P1-P4 (§3.4). Confidence tags used inline: **[V]** verified
in this repo's code or docs; **[S]** read in the cited source this session (Epic docs, forum, release notes); **[S2]** third-party or
secondary summary (blog, talk summary), not primary; **[K]** engine or industry knowledge I could not re-verify now, treat as
uncertain; **[E]** my estimate or derivation, to be measured. Links in §6.

## 0. Summary

1. The cost of many enemies is `items × rate × bytes` plus puppet rendering, not the replication API. M8's design (server-only store,
   a few pack actors with `FFastArraySerializer`, client puppets) is the right shape; the research changes parameters, not the
   architecture (§3.1).
2. Do not use Mass replication (experimental since 5.1, no further updates, Epic points to Iris [S]). Do not use the Replication
   Graph (made for 100 connections; we have 3-7). Iris is "production-ready" in 5.8 notes [S] but unverified with our NP/Mover fork
   and gives little at our connection count: keep it behind a spike (R3).
3. Bytes planning value: **20 B per item update** (M8 says 18 B before headers [E]). Per client: 5.8 KB/s at 40 enemies, 10.9 at 80,
   20.4 at 150 (T0 capped at 48). Host upload with 3 clients: 17 / 33 / 61 KB/s (§3.3). 150 awake enemies is not a listen-host
   target; 150 as a population with at most about 60 awake is.
4. The biggest wins in order: do not send (dormancy, relevancy, idle = 0 B), lower the rate per tier (per item, not per pack),
   cap visible puppets, only then precision tricks (worth under 10 %).
5. **Platforms**: only fighters and at most 8 flagged "platform minions" (replicated through a small actor) can be bases; swarm
   minions never. The player stands in **base-local space on a rigid, unattached, kinematic collider** (what Mover already does),
   with the base shown interpolated like every other entity. If the spike shows landing/mount errors, add a lag-compensated base pose
   lookup as a recorded sim input (§4.5, approach D). Never use a bone or the animated mesh as the base.

## 1. Findings

### 1.1 Unreal replication options for many actors (5.x, status for 5.8)

**Iris.**
- Status: the 5.8 release notes say "Iris is now production-ready for licensees in UE 5.8" [S: E5]. In 5.7 it moved from
  experimental to beta, is still opt-in (`net.Iris.UseIrisReplication`, off by default), and Epic says it is "not intended to be the
  only option" and that there are no plans to deprecate legacy replication or the Replication Graph [S: E6]. The Iris doc pages
  (overview, introduction, filtering, prioritization) still carry an "Experimental" label [S: E1-E4]: the labels disagree, the 5.8 notes
  are newer.
- Features: owner/connection/group filters run only on change; dynamic filters (`UNetObjectFilter`, built-in `UNetObjectGridFilter`)
  run on every object and "add significant CPU overhead" [S: E3]. Prioritization is a float that accumulates until the object is sent;
  default `SphereNetObjectPrioritizer` (inner 1000 cm, outer 5000 cm), plus `NetObjectCountLimiter` [S: E4]. State is kept quantized
  and updates are push-driven instead of polled [S: E2]. Delta compression is not described on the pages I could read; a third party
  calls it "first-class" [S2: T2].
- Measured: one third-party run, 100 CMC characters, 100 connections, 5.7 preview, single synthetic scenario: server tick 83.9 ms
  legacy vs 60.1 ms Iris (unpatched) [S2: T1]. No bandwidth comparison.
- **For this project**: Epic staff, 2026-09-17: "Iris support was added for the Network Prediction Plugin/Mover with CL 48858597 in
  UE5/Main", and "Mover is still considered experimental"; whether that CL is in a 5.8.x tag is not stated [S: E7]. The 5.8 notes list
  "optional Iris replication support" for Network Prediction [S: E5]. In the repo, the fork's NP `Build.cs` calls `SetupIrisSupport`
  and contains Iris NetSerializers (`NetworkPredictionNetSerializers.cpp`), and no `UseIrisReplication` exists in `Config/` [V]. So
  NP-under-Iris is unproven for our fork and enabling Iris is a project-wide switch for fighters too: spike R3, not v1.

**Replication Graph.** Docs exist in 5.8; built for ~100 connections and ~50,000 replicated actors in Fortnite BR; its gain is
sharing per-frame relevancy work across connections [S: E8, E9]. With 3-7 connections the shared work is small; a third-party lab
measured 14 % lower server frame on its own scenario with +8.8 % bytes [S2: T3, not comparable]. Skip.

**Dormancy, push model, update frequency, priority, relevancy.**
- Epic's own priority order for replication cost: do not replicate (`SetReplicates(false)`, "the biggest win"), lower
  `NetUpdateFrequency`, then dormancy, relevancy [S: E10].
- Dormancy: dormant actors are skipped when the per-connection list is built; "can save several ms" with many rarely-changing actors;
  property changes made while dormant may be lost on wake; and, directly relevant to M8 §8.1, **"modifying a dormant fast array and
  flushing afterward may result in no replication"** [S: E11]. Rule: wake (`SetNetDormancy(DORM_Awake)` or `FlushNetDormancy`) *before*
  mutating the array, never after.
- Push model: only properties marked dirty are compared; works with legacy and Iris [S2: T2]. A third party saw a health path drop
  from 0.42 to 0.11 ms with 20 enemies [S2: T2]. A missed `MARK_PROPERTY_DIRTY` means clients never see the change.
- Priority: `NetPriority` ratios plus distance/view scaling constants (500/2000/8000 cm); only matters once the connection is
  saturated [S: E12]. Adaptive update frequency exists (`UNetDriver::IsAdaptiveNetUpdateFrequencyEnabled`) [K: default unknown].
  Legacy replication runs on net ticks (`NetServerMaxTickRate`, 30 Hz by default [K]; the project config does not set it [V]), so a
  per-actor frequency above the net tick rate is wasted.
- Relevancy: distance, `bAlwaysRelevant`, `IsNetRelevantFor` per actor per connection [K]; with 3-7 connections and a handful of
  pack actors this costs nothing.

**`FFastArraySerializer`.** Per-item add/change/remove with client callbacks, unchanged items cost 0 bytes, removal from the middle does
not resend the tail; third-party figure 3 ms to 0.05 ms server CPU for a changed ~10k-item array versus a plain `TArray`;
"replication still scans the entire data set to find changes" and order is not preserved [S2: E15]. A changed item is serialized whole
(our custom `NetSerialize`), not per field [K]. No per-connection filtering of items inside one array: relevancy is per array/actor.

**Mass Entity and MassReplication.** Docs: "Mass Replication is considered experimental" (5.1 text), one-way server-to-client, needs
C++ for custom values [S: E13]. Epic staff on 5.8.1: still experimental, "never fully developed", will receive no further updates, the
planned replacement (MassIrisReplication) missed the 5.8 branch and exists only on UE6 main, "Epic recommends moving to Iris"; three
regressions are listed in 5.8.1 [S: E14]. 5.8 improves Mass itself (signals in core, off-game-thread entity creation) [S: E5], but our
store is already a struct-of-arrays and M8 needs no ECS. **Reject for replication and simulation.**

**Actor-less proxies vs one actor per enemy.**
- One actor per enemy (legacy): each update carries actor-channel and bunch headers and, with default movement replication,
  `FRepMovement` (quantized location, rotation, velocity, flags; roughly 35-45 B per update with headers [K/E]). Anchor for CPU:
  the third-party run above spent 66.2 ms of `NetBroadcastTickTime` on about 10,000 actor-connection updates, i.e. about
  **6.6 µs per actor-connection update** legacy and 4.6 µs Iris [S2: T1, my division of the author's estimate]. It also costs client
  UObject spawn/destroy churn and per-actor ticks.
- Pack actor with a FastArray (M8 §8): `N/16` actors, 18-22 B per item update [E], cheapest in CPU and bytes, but items inside a pack
  share one relevancy decision and one actor frequency.
- Custom snapshot over an unreliable RPC (or an Iris NetBlob): per-connection interest and delta against a per-connection acked
  baseline (Quake/Gaffer style), best bytes, but we would re-implement acks and baselines. Fallback only (R1 variant C).

### 1.2 State compression

- **Quantization**: bound the world and spend bits where they matter. Gaffer's snapshot article: position at 512 values/m (2 mm) in a
  bounded box = 18+18+14 bits (from 96), orientation by smallest-three = 29 bits (from 128), cube state 321 to 80 bits [S: G1]. M8's
  19-bit/1 cm position and 8-bit yaw for ground entities are already in the same regime. Per-tier precision classes would save only
  0.75-1.5 B of 20 B [E]: not worth the code.
- **Delta against an acknowledged baseline**: in Gaffer's pipeline an unchanged cube costs 1 bit, a changed position averages 26.1
  bits against 50 absolute, and "the big order of magnitude win" is delta + indexing [S: G1]. Reachable here only with Iris
  NetSerializers or a custom `NetDeltaSerialize` (M8 §8.3 already says "not planned"). A rough ceiling for our items: 9-11 B
  instead of 20 B [E, scaled from G1].
- **Snapshot vs event**: Halo Reach separated state (only the latest value matters, intermediate values may be skipped), events
  (unreliable, carry context) and control data, and let priority starve low-value objects: "unreliability enables aggressive
  prioritization"; a grounded idle grenade updated about 3 times per second; 16-player games ran at 250 kbps against a 384 kbps goal
  [S2: G3, summary of Aldridge GDC 2011]. M8 already encodes "events" as state plus counters (`MoveId + MoveStartFrame`,
  `HitCounter`): keep. One-shot cosmetics (death shatter) should carry a seed, not state.
- **Interest management**: distance, screen presence, threat and recent interaction as one priority number per object per client
  (Halo Reach) [S2: G3]; UE legacy scales by distance and view sector [S: E12]; Iris has spatial filters and priorities [S: E3, E4].
  At 3-7 clients a per-pack relevancy plus a per-client cap (Rec5) is enough.
- **Update-rate LOD**: Halo (above) and M8 §7. The missing piece in M8 is to apply the rate **per item**, not per pack (Rec3).
- **AI intent and animation state instead of transforms**: M8 sends `ActionState/MoveId/MoveStartFrame/Speed` already. Extension for
  T1/T2 walkers: send an *order* (a few waypoints, speed, start frame) and let clients walk it, as RTS games send orders
  (AoE style) [K]. This is not lockstep. Prototype R4; saves most of T1/T2 bytes [E].
- **Deterministic client simulation**: real lockstep (AoE "1500 Archers", commands only) needs bit-exact determinism on all
  machines, input delay, and every machine simulating everything [K: A1 not fetched]. It does not fit: rollback players, AI reacting
  to *predicted* players, float behaviour across CPUs, late join needs a snapshot, host advantage. It works for cosmetic or
  independent things seeded from an event: debris, idle sway, death shatter (M8 §4d.4), spawn positions from a wave seed.

### 1.3 Client presentation

- **Interpolation buffer**: NP uses 100 ms (`FixedTickInterpolationBufferedMS=100`, 62.5 Hz, `SimulatedProxyNetworkLOD=Interpolated`) [V].
  Valve's rule is a buffer of about two update intervals [K: V1 not fetched]. At 15 Hz (66 ms) 100 ms holds 1.5 intervals: one lost
  update is bridged by M8's ≤150 ms extrapolation. At **5 Hz (200 ms) or 1 Hz the 100 ms buffer cannot interpolate at all**: T1/T2 need
  dead reckoning with correction blending, or a larger per-tier delay. This is safe because only T0 minions are hit-tested (M8 §7), so
  lag compensation needs the shared render time for T0 only.
- **Extrapolation** is fine for ground walkers up to 150 ms; launched minions extrapolate ballistically (M8 §8.4).
- **Animation without per-frame data**: state + montage start frame + speed (M8). Cost controls, all Epic-provided:
  update-rate optimization (`bEnableUpdateRateOptimizations`, `AnimUpdateRateParams`) and `VisibilityBasedAnimTickOption` [S: E19];
  **Animation Budget Allocator** (default `a.Budget.BudgetMs` 1.0 ms, ranks by significance set from C++ `SetComponentSignificance`,
  throttles tick rate down to `MaxTickRate` 10 and interpolates over budget) [S: E16]; **Animation Sharing** (leader/follower buckets:
  "the difference between 100 and 1000 characters results in a minimum increase", but capped blends and variation mostly by time offset)
  [S: E17]; **Significance Manager** ("does not itself improve performance": use it to rank puppets and feed the allocator) [S: E18].
- **Pooling and materials**: pooled puppets (M8 §10); avoid a dynamic material instance per puppet for flash/damage, use custom primitive
  data [K]; far or dormant crowds as instanced meshes with vertex animation (the cheapest Mass representation, but Mass's ISM animation
  is itself marked experimental [S: E13]; plain ISM does not need Mass).

### 1.4 Hits on many enemies

- **Overwatch** (GDC 2017, via Edgegap summary [S2: G2]): the server keeps about the last half second of each entity as a bounding
  volume and fully rewinds only the entities whose volume intersects the shot; above ~220 ms RTT the rewind is clamped. All gameplay is
  predicted by default; the client clock runs ahead by half the RTT plus one buffered command frame.
- **Valve** lag compensation: rewind to server time minus latency minus interpolation delay [K: V2 not fetched]. NP's lag comp here
  adds `FixedTickInterpolationBufferedMS` and the input buffer to `MaxRewindTimeMS` (200 default) [V: `NetworkPredictionSettings.h`,
  `NetworkPredictionWorldManager.cpp` ~L890-905].
- M8's design (history ring of 24 frames, candidates from the current grid inflated by `MaxMinionSpeed × RewindMs`, `Sample()` per
  candidate, host never rewinds) already matches the Overwatch idea. Two improvements: store a per-frame **swept bound** (position ±
  speed × frame) so candidate rejection is a bound test, and keep **one** pass per frame over `PendingHits` sorted by
  (victim, attacker, hitbox) so cost scales with hits, not with N. Cost estimate for a 12-minion cleave at N=150: tens of microseconds
  [E]; it is not a budget driver.
- **Host-side batching**: all minion hit work already runs in one post-fixed-tick pass (M8 §5). The host's own hits skip rewind (host
  advantage accepted, M8 §4d.1).

### 1.5 Standing on moving enemies

**The problem in this stack.** A predicted player runs ahead of the server by about RTT/2 plus the input buffer (NP `FixedTickDesiredBufferedInputCount`) [V/S2: G2]. An
interpolated entity on that client shows server state that is RTT/2 old on arrival plus the 100 ms buffer [V]. The base is therefore
shown about **RTT + 100 ms behind the player's own simulation frame** (≈160 ms at 60 ms RTT, ≈220 ms at 120 ms) [E]. A platform moving
at 300 cm/s is 48-66 cm away from where the server will have it when the player's input arrives. The host does not have this problem
for its own character. The 5.8 notes list "adaptive time dilation to reduce client predict-ahead" for NP [S: E5]; if the fork has it, the gap shrinks.

**What Mover does today** (engine override in this repo) [V]:
1. A player is based on any walkable *Movable* component found by the floor check (`UWalkingMode::UpdateFloorAndBaseInfo`,
   `IsADynamicBase` = `Mobility == Movable`); `bSupportsKinematicBasedMovement` is true by default.
2. `FMoverDefaultSyncState` stores location, velocity and orientation **relative to the base** plus the captured base pose;
   `NetSerialize` writes the base as an **object reference** (`Ar << MovementBase`), bone name, packed base position and quaternion
   (`MoverDataModelTypes.cpp` L203-233). Consequence: the base must be a net-addressable component. A pack item or a client-only puppet
   is not.
3. `ShouldReconcile` compares **base-space** locations (5 cm) and treats a different base or bone as a mismatch (L258-280). A player
   standing still or walking on a platform compares equal whatever the platform's time offset.
4. Based movement is applied out of band by `FMoverDynamicBasedMovementTickFunction`, ticking after the base's own tick (prerequisite),
   in PrePhysics or PostPhysics; it sweeps (a base can scrape the player along walls) and writes the *pending* sync state
   (`BasedMovementUtils.cpp` L299-405, L504-576; `MoverComponent.cpp` L1051-1101). Walking also applies it at the start of its tick
   (`WalkingMode.cpp` L143-150).
5. On rollback, `SetFrameStateFromContext(bDueToRollback)` **re-samples the base's current transform** instead of restoring the base's
   pose at the restored frame (`MoverComponent.cpp` L949-962). Mover assumes the base shares the player's timeline or is slow. With
   an interpolated base every re-simulated frame of one rollback sees the same base pose [E, inferred from the code: verify in P1].
6. Leaving a base keeps its velocity through `GetMovementBaseVelocityAtPoint` = `GetPhysicsLinearVelocityAtPoint` (`WalkingMode.cpp`
   L396-401); for a kinematic collider moved by transform that may be zero [K: verify, P4].
7. The fork's wall modes also "ride the movement base" (wall-movement.md), so a platform enemy could later be a wall surface for free.

**CMC** [S: E25 for names; K for the wire]: bases are first-class: `bImpartBaseVelocityX/Y/Z`, `bImpartBaseAngularVelocity`,
`bIgnoreBaseRotation`, `bBasedMovementIgnorePhysicsBase`, `bFastAttachedMove`, and **`bCanTrustClientOnLanding`** ("stepping off a
moving platform, the client should be trusted somewhat on landing"). The client sends its position relative to the base
(`ClientMovementBase`, `bBaseRelativePosition`) [K], i.e. Epic's own answer is approach B below plus a tolerance on mount/dismount.
In an input-only deterministic model such as NP the server cannot "trust the client position"; the equivalent is to give the server
what the client saw as an **input** (approach D).

**Epic's Mover content**: `BP_SplineFollowerPlatform` "predictably follows a defined path" (deterministic, approach C) [V: Mover
README]; Chaos Mover has a pathed movement mode for platforms [S: E24; V: `ChaosPathedMovementMode.h`]; the physics-based pawn warns
"Interactions with moving non-physics objects will produce poor results" and "Physics-driven Mover actors are not synchronized with
those using Network Prediction" [S: E22]. Networked physics Resimulation resimulates the whole physics world on a correction [S: E24],
and its Predictive Interpolation mode blends local and server velocity [S: E23]. Mover is still experimental in 5.8 [S: E5, E20].

**Other engines**: Photon Fusion "Forecast Physics" extrapolates the last authoritative state to the client's local time and runs
local physics with reconciliation, with the stated limit that extrapolation "is not aware of obstacles in the world" [S: G7]. Unity
Netcode for Entities runs physics for interpolated ghosts on the server only (clients make them kinematic); predicted ghosts simulate
in the prediction loop [S: G6]. Rocket League (GDC 2018, Jared Cone) is the known case of predicting a shared dynamic object in
the player's time with rewind and resimulation [K: talk not fetched, video linked from G5].

| | Approach | Pros | Cons | Fit |
|---|---|---|---|---|
| A | Base in the player's time: extrapolate or forward-predict the base (Fusion Forecast, Rocket League) | accurate landings | AI intent unknown, so every AI decision mispredicts; fork spike S-FP said not worth it (combat.md §12.7); a collider ahead of the displayed mesh floats the feet | only a boss, later |
| B | Player in **base-local** space, base interpolated (Mover today, CMC) | no new code; observers consistent; standing and walking never reconcile | mount, dismount and world-collision decisions are off by `v × (RTT+100 ms)`; resim sees a frozen base | **v1** |
| C | Deterministic base: pose = f(server time, authored path) | zero error | scripted motion only | authored platform enemies (gait on rails), as an option |
| D | **Lag-compensated base pose as a recorded input**: client and server both read the base's authoritative pose history at the player's view frame | client and server agree by construction; fixes frozen-base resim | new seam in the movement modes, a few input bits, observers must re-compose | **stage 2, if P1/P2 fail** |
| E | Trust the client's landing (CMC style) | cheap | not expressible in NP (server only replays inputs); D is its NP form | no |
| F | Networked physics bodies as bases | physically consistent | whole-world resim; not synchronized with NP actors [E22] | no |

### 1.6 What shipped games publish

- Usable, through secondary summaries of the talks: Halo Reach (priorities, rates, bandwidth) and Overwatch (prediction, rewind, bounding volumes) [S2: G2, G3]; Destiny's
  GDC 2015 talk is about the mission/activity host (10 Hz, mission facts, not per-entity replication) [S2: G4]; Vermintide 2: a developer
  says AI pathfinding breaks down under CPU choking in very large hordes [S2: T4], which supports LOD on AI cost, not only on bytes.
- **Not found**: primary technical sources for Helldivers 2, Deep Rock Galactic, Darktide or Left 4 Dead entity replication, and for
  moving-base handling in Destiny, Overwatch payloads or Halo vehicles. I do not cite them for numbers. Left 4 Dead and Source use PVS,
  snapshot delta against the last acked snapshot and entity interpolation [K: V1 and Gambetta's article were not fetchable].

## 2. Comparison of replication architectures

Per-client bytes for 40 enemies with the M8 tier split 12 T0 (15 Hz) / 20 T1 (5 Hz) / 8 T2 (1 Hz); CPU is host cost for 150 enemies,
3 clients, per 16 ms frame. All estimates [E] except where tagged; to be replaced by spike R1.

| # | Architecture | Bytes/item update | KB/s per client, N=40 | Host replication CPU, N=150 | Risk | Verdict |
|---|---|---|---|---|---|---|
| 1 | One actor per enemy, legacy | 35-45 | 11.5 | ~1.3 ms (6 µs × 150 × 3 × 30 Hz) | client actor churn, per-actor ticks | only for platforms and fighters |
| 2 | **Packs of ≤16 + FastArray (M8)**, push model, dormancy | 20 | **5.8** | ≤ 0.25 ms | relevancy per pack, FastArray scan | **default** |
| 3 | 2 + intent/order for T1/T2 | 20 / 14 | 4.2 | same as 2 | client dead reckoning, ground snap | optional (R4) |
| 4 | Custom snapshot RPC, per-connection delta | 9-11 | ~3.1 | 0.2-0.4 ms | own acks and baselines | fallback if 2 misses budget |
| 5 | Packs under Iris + dynamic filter | ≈20 (unverified) | ≈5.8 | 0.1-0.5 ms (unverified) | NP/Mover under Iris unproven; dynamic filter CPU | after R3 |
| 6 | Mass Entity + MassReplication | n/a | n/a | n/a | experimental, unmaintained [E14] | reject |
| 7 | Replication Graph | n/a | n/a | n/a | built for ≥100 connections | reject |

## 3. Recommendation (*Proposal*)

### 3.1 Decisions

- **Rec1 Keep the M8 shape.** Server-only store, pack actors with FastArray, client puppets. Packs stay at 16 items.
- **Rec2 Push model and dormancy done right.** Enable push model for the pack, platform and fighter-adjacent replicated actors
  (`net.IsPushModelEnabled`; the properties not marked stay polled, so it is safe to enable) [S2: T2]; pack mutation always goes
  *wake or flush first, then mutate and `MarkItemDirty`* (Epic warns that the other order may not replicate, E11).
- **Rec3 Rate per item, not per pack.** An item becomes dirty at most at its own tier's rate (T0 15, T1 5, T2 1 Hz, T0 10 Hz when the host
  budget guard trips); the pack's `NetUpdateFrequency` is only a ceiling. A mixed pack then costs only its changed items; packs
  stay by spawn group. Measure the **relevancy over-inclusion** (items sent to a client that are beyond 8000 cm, divided by items
  sent); if above 30 %, re-pack by (grid cell, tier) with 10 % hysteresis: costs one remove + one add per border crossing (about
  100 B/s per client at 40 walkers [E]) and needs a stable global id (+1 B on add only).
- **Rec4 Send less for T1/T2.** T1 at 2-3 Hz with dead reckoning (or an order: waypoints + speed + start frame), T2 at 0.5-1 Hz, T3 0. T0
  stays snapshot-interpolated because it is hit-tested. Prototype in spike R4; fall back to Rec3 alone if it does not hold.
- **Rec5 Relevancy by budget, not only by distance.** Per connection, the subsystem ranks packs by priority (nearest living fighter, token
  holders, in-view, recently hit) every ~100 ms and grants packs until `MaxRelevantPerClient` (default 64) or
  `gstn.Minion.HostUploadBudgetKBps` is reached; denied packs return `IsNetRelevantFor == false` (Halo-style priority, E: G3).
- **Rec6 Presentation caps.** `MaxVisiblePuppets` (default 60) ranked by significance; the rest instanced/hidden; URO on; animation budget
  1.0 ms client / 1.5-2.0 ms host; Animation Sharing for idle crowds; custom primitive data for flash; `OnlyTickPoseWhenRendered`.
- **Rec7 Per-tier smoothing** (see §1.3): T0 uses the shared render time; T1/T2 use dead reckoning and blend corrections over 200 ms.
- **Rec8 Hits**: swept bounds in the history ring; one sorted pass; host never rewinds; platforms are rewound like fighters (§4.7).
- **Rec9 Platforms**: §4. B in v1, C where authored, D only if measured, bridge plugin, platform actor tier of ≤ 8.
- **Rec10 Iris, Replication Graph, Mass**: not in v1. Revisit Iris only if R3 passes and a measured gain exceeds 25 % of host
  replication CPU or bytes.
- **Rec11 Measure before building anything beyond M8** (§3.4). **R12** If the listen host misses its budget: lower `MaxActiveT0`
  (48 to 32), then T0 rate to 10 Hz, then puppet cap, and record the maximum N honestly (M8 §4d.1 already says so).

### 3.2 What in `combat-m8-minions.md` should change (not edited here)

| M8 section | Change | Reason |
|---|---|---|
| §2 spike 4 | Split into R1 (bake-off: actor-per-enemy vs packs vs custom snapshot), R2 (dormancy + FastArray flush order, wake latency), R3 (Iris viability with NP/Mover); add R4-R6 and platform spikes P1-P4 (§3.4) | Iris line "compare with a `UNetObjectFilter`" is premature; dormancy trap [E11] |
| §2 spike 7 | Add `MaxVisiblePuppets`, ABA/URO/Animation Sharing/significance as the variables, measured on the host | puppets dominate before the server [M8 §15] |
| §4 / §4b | `FGSTN_MinionHistory` becomes a generic `FGSTN_PoseHistory` + sampler; quantizer, frame stamp and interpolator are shared with the platform plugin: decide `GSTNCore` (L2) vs duplicate (§3.5) | second consumer exists |
| §4b rules | One line: swarm minions are never platforms; platform minions use the `GSTNPlatformMinion` bridge | §4 below |
| §5 stage 2.5 | Publish: also write platform minions' end-of-F pose to the platform collider proxy at the post-fixed-tick hook (one defined point per step) | order independence of Mover instances |
| §7 | Platform minions are never T2/T3 while a player is based or within mount range and are exempt from the `MaxActiveT0` demotion; per-item send rate = tier rate | Rec3, §4.6 |
| §8.1 | Dormancy: flush before mutate; push model; over-inclusion metric; (cell, tier) re-pack fallback; budgeted relevancy (Rec5) | Rec2, Rec3, Rec5 |
| §8.2 | Planning size 20 B (not 18 B); `bPlatformActor` flag (those minions get no pack item); stable global id on add if re-packing | headers, §4.4 |
| §8.3 | Replace the table by §3.3 (20 B, 40/80/150, 3 and 7 clients) and add the T1/T2 order option | Rec4 |
| §8.4 | Per-tier interpolation delay and dead reckoning; `MaxVisiblePuppets`; animation cost stack; no per-puppet dynamic material | Rec6, Rec7 |
| §9.2 | Swept bounds per history frame; platform enemies and standing players share the rewind frame | Rec8, §4.7 |
| §11 | Add N=80 and N=150 columns and host rows from §3.3; state 150 as population, not awake | budgets |
| §12 | Scenarios `scenario_minion_bytes` (bytes per tier), `scenario_minion_dormancy_flush`, `scenario_platform_*` | §3.4 |
| §15 | New risks: NP under Iris unverified; FastArray + dormancy order; T1 rate below the interpolation buffer; platform risks (§4.8) | |

### 3.3 Budgets (*Proposal*, listen host with 3 remote clients, 62.5 Hz = 16 ms frame; all [E] until R1/R6)

Bytes: `rate × 20 B` per item; tier split as M8 §8.3 at 40 (12/20/8), 24/32/24 at 80, T0 capped at 48 at 150 (M8 §7). Worst case:
every enemy relevant to every client (co-op players stay together). 1 KB = 1000 B. Excludes the players' own NP traffic (measure in M8
spike 1) and about 1 KB/s of protocol baseline per client [E].

| Enemies awake | T0 / T1 / T2 | KB/s per client | Host upload, 3 clients (kbit/s) | 7 clients | With Rec3+Rec4 (T0 10 Hz, T1 2 Hz, T2 0.5 Hz, 14 B) per client / host 3 |
|---|---|---|---|---|---|
| 40 | 12 / 20 / 8 | 5.8 | 17.3 (138) | 40 | 3.0 / 9.0 |
| 80 | 24 / 32 / 24 | 10.9 | 32.6 (261) | 76 | 5.9 / 17.6 |
| 150 | 48 / 50 / 52 | 20.4 | 61.3 (490) | 143 | 11.4 / 34.1 |

A 1 Mbit/s home upload is 125 KB/s: 150 awake enemies with 3 clients takes half of it before player traffic. The M8 guard
`HostUploadBudgetKBps` (default 40) therefore trips at N=80 with 7 clients and at N=150 with 3; Rec4 is what keeps N=150 under it.

Host CPU per frame (mean; p99 in M8 §11 terms about 2x):

| Item | N=40 | N=80 | N=150 | Basis |
|---|---|---|---|---|
| Minion batch | 1.0 | 2.0 | 2.6 | M8 §11 (1.0, 1.6 at 60); T0 48 × 0.03 + T1 50 × 0.015 + T2 52 × 0.0075 |
| Replication, packs (3 / 5 / 10 packs) | ≤ 0.05 | ≤ 0.1 | ≤ 0.25 | ~20-30 µs per pack-connection update [E] |
| (for comparison) one actor per enemy | 0.35 | 0.7 | 1.3 | 6 µs per update [S2: T1] × 3 clients × 30 Hz |
| Host puppets, game thread (≤ 60 visible, ABA on) | 2.0 | 3.0 | 3.5 | M8 spike 7 target 3 ms at 60 |
| Enemy-related total | ~3.1 | ~5.1 | ~6.4 | |
| Left for host's own game, render submit | ~12.9 | ~10.9 | ~9.6 | M8 §11 host row asks ≤ 11 ms total at N=40 |

Reading: 40 fits; 80 fits only if the host's own game stays under about 10 ms (a spike answer); **150 awake does not belong on a
listen host at 62.5 Hz**. Design the 150 as a population (≤ 60 awake, the rest T3 or dormant) or move to dedicated. Client: download =
per-client column; puppets ≤ 2 / 3 / 4 ms. Memory: store 400 B × N = 16 / 32 / 60 KB; history 16 B × 24 × N = 15 / 31 / 58 KB.

### 3.4 Measurement plan (spikes; record in combat.md §17)

Setup common to all: listen-server host that renders at minimum-spec resolution (`stat unit` split) plus 3 remote clients run with
`-nullrhi` (the harness today is host + 2; run 3 headless clients for upload, see testing.md), 60/15 ms, 2 % loss, 60 s per cell, N in
{20, 40, 80, 150}, Unreal Insights `-trace=cpu,frame,net -NetTrace=1`, `stat net`, `NetProfile`, server rep time (`NetServerRepActorsTime`).

| # | Spike | Question | Exit |
|---|---|---|---|
| R1 | Replication bake-off | Same 150 walkers as (A) lean actor per enemy, (B) packs + FastArray, (C) custom snapshot RPC: host ms, bytes/s per client, client ms | B within ±25 % of §3.3; B ≥ 2x cheaper in bytes than A; else re-decide. C only if B misses the budget by > 25 % |
| R2 | Dormancy and flush order | Wake latency, lost updates, late join, with wake-then-mutate vs mutate-then-flush on a dormant pack | 0 lost items over 10 min of waves; wake latency ≤ 2 pack periods; late join sees all 40 |
| R3 | Iris viability | `-UseIrisReplication=1` with packs: do `scenario_abilities`, `forced_rollback`, `combat_spike`, minion scenarios stay clean? | Any NP or Mover failure stops it (no further work); a gain < 25 % on host replication CPU or bytes stops it |
| R4 | T1/T2 order replication | Waypoint + speed + start frame vs 2-3 Hz snapshots: bytes, visible error vs server, correction pops | ≥ 50 % fewer T1/T2 bytes; error ≤ 40 cm at 95th percentile; no pop > 25 cm |
| R5 | Budgeted relevancy | `MaxRelevantPerClient` and priority ranking: do far packs flicker? | no relevancy toggle faster than 1 s; host upload ≤ guard |
| R6 | Puppet stack | ABA, URO, Animation Sharing, ISM for far, custom primitive data at 60 visible | ≤ 3 ms game thread on host at 60; ≤ 2 ms at 40 |
| P1 | Standing on a fighter (B) | Two clients stand on a fighter moving at 150 / 300 / 500 cm/s, walk, jump on and off | standing/walking: 0 unexplained reconciles per 30 s, max correction ≤ 10 cm; mount success ≥ 95 % at 300 cm/s |
| P2 | Two pings, one platform (D) | Prototype the pose-history lookup with offsets 60 vs 180 ms | both clients: mount success ≥ 95 % at 500 cm/s; observers see feet within 10 cm |
| P3 | Platform lifecycle | Hit, launch, death, shatter, pool release under a standing player | 1 correction at most, no stuck base, no fall through |
| P4 | Dismount and cost | Velocity on leaving the base; cost of 8 colliders moved per step on the host | momentum error ≤ 50 cm/s; ≤ 0.2 ms per frame |

### 3.5 Where it lives (modularity-and-tags.md §3.1-§3.7)

| Piece | Plugin / module | Layer | Rule applied |
|---|---|---|---|
| Pack actor, FastArray, item codec, store, history, interpolator, puppets, LOD, budget guard | `GSTNMinion` (`GSTNMinionCore` pure, `GSTNMinionRuntime`) as M8 §4b | L4 | unchanged |
| Shared net primitives: quantizer, frame-stamp reconstruction, `FGSTN_PoseHistory` + sampler, tier rate scheduler | **`GSTNCore` (L2), `Net/`** (*Proposal*: the "two plugins need the same helper" trigger of §3.6 is met by `GSTNMinion` + `GSTNPlatform`); alternative: stay in `GSTNMinionCore` and duplicate ~200 lines until M10 projectiles need them | L2 | no gameplay concepts, pure, Automation-tested |
| Platform spec data, pose history use, collider proxy, base-pose lookup, platform tags | **new optional plugin `GSTNPlatform`** (`GSTNPlatformCore` pure; `GSTNPlatformNP` for Mover/NP, `EnabledByDefault: false`) depending on `GSTNCombatCore` (lag-comp time) + Mover + the fork, **not on `GSTNMinion`** | L3b | extension surface, no edit of combat (§3.2); fork declared in the `.uplugin` (§3.7 rule 5) |
| Platform minions: `FGSTN_PlatformMinionExtension` on the definition, spawning/despawning the platform proxy actor, feeding poses from the store | **bridge plugin `GSTNPlatformMinion`** (one module), depends on exactly `GSTNPlatform` + `GSTNMinion` | bridge | same pattern as `GSTNPartsMinion` (§3.2, §3.7 rule 3) |
| Platform enemies that are fighters | content only: `UGSTN_EnemyPlatformComponent` added to the fighter Blueprint or by a Game Feature | L4 | no plugin edge to combat |
| Tags | `Platform.*` root owned by `GSTNPlatform` (native; add the row to §4.2 of the modularity doc); minion types stay `Minion.Type.*` | | one owner per root |
| Tests | `GSTNPlatform.Core.*` (history sampling, view-offset mapping, active-state mask), `.NP.*` (collider move order, base velocity), harness `scenario_platform_*` in `Tools/GASNPPTest/` | | §3.5 table; canary build for `GSTNPlatform` alone |

## 4. Enemies as solid platforms (*Proposal*)

### 4.1 Which enemies

| Enemy | Platform? | Why |
|---|---|---|
| Fighters (bosses, elites; NP entities; actor with stable subobjects) | **Yes** | net-addressable base for Mover's `Ar << MovementBase`; interpolated like any proxy; few |
| Platform minions (flagged type, **≤ 8 alive**, `gstn.Platform.MaxActors`) | **Yes, through a platform proxy actor** | the store has no actor and clients have no net-addressable minion; the proxy replicates the collider's pose at 30 Hz (15 Hz when unoccupied), the minion's pack item is suppressed |
| Swarm minions (dozens) | **No** | pack items at 1-15 Hz, T2 skips frames, pooled puppets; no stable base reference; cost of colliders per minion |
| Players | Out of scope | piggyback/grab are links (combat.md §12b); that mechanism parks the rider, the dual of this |
| Flying or launched enemies | Only in the states listed in `ActiveActionStates` (§4.2, §4.6) | a base that is in flight under a player is the worst case for B |

### 4.2 Data (`FGSTN_PlatformSpec`, `GSTNPlatformCore`)

`bIsPlatform`; `Shapes[]` (box, capsule or convex, authored in the enemy's local frame, stable `PlatformId` 0-3 → component names
`Platform0..3`); `ActiveActionStates` mask (e.g. `Idle`, `Walk`, `Move`; never `Launched`, `Dying`, `Spawning`); `MaxSpeedWhileOccupied`
(cm/s, an AI limiter: the platform slows when a player is on or within mount range, a game-design lever and the cheapest accuracy gain);
`BraceTimelineOnMount` (optional timeline played when a player lands, a readable telegraph); `DropGraceFrames` (telegraph before the
platform deactivates; default 20 frames ≈ 320 ms); `bIgnoreBaseRotation`; `bImpartVelocityOnLeave`. Fighters: a data asset on
`UGSTN_EnemyPlatformComponent`. Minions: `FGSTN_PlatformMinionExtension` (bridge) on `UGSTN_MinionDefinition`. Later hook: a
`PlatformModifier` timeline track (shape change per frame, deterministic from `MoveId + frame`), and a platform per part (parts
mask, combat-parts.md) as an extension.

### 4.3 Collision

- A dedicated object channel `GSTN_EnemyPlatform`: **Block** for the Mover capsule, **Ignore** for minion sweeps (`GSTN_MinionBlock`),
  projectiles and hit queries (hits use the combat grid, not physics, M4 §5).
- The collider is a **kinematic, Movable, unattached `UPrimitiveComponent`** (flat-topped box or convex), never a bone, never the
  animated mesh, never the enemy's capsule. Reasons: minions have no mesh or animation on the server at all (M8 §4); animation is not
  deterministic across machines; a capsule top fails the walkable-slope test at its edges; an attached child moves instantly with the
  actor in the enemy's own tick, so its pose would depend on NP instance order.
- It is moved at **one defined point per fixed step** (post-fixed-tick hook of M8 spike 2, from end-of-F state) on the host, and from
  the interpolation sample once per game tick before physics on clients. Collision is enabled only while `Active`.
- Non-platform enemies do **not** block players (owner question 6): predicted player-vs-enemy blocking is a correction machine.

### 4.4 Replication and pose history

- Fighter: nothing new; the base reference is the fighter's `Platform0` component (stable name on a replicated actor).
- Platform minion: `AGSTN_PlatformProxyActor` (replicates, `NetUpdateFrequency` 30/15, dormant when no player within 3000 cm), state
  `{ pos 3×19 bits, yaw 8, velocity XY quantized 2×10, ActiveState 3, FrameStamp 8 }` ≈ 12 B plus headers ≈ 20 B → 600 B/s per platform
  per relevant client at 30 Hz; 8 platforms = 4.8 KB/s per client worst case (not in §3.3). The store keeps simulating the minion.
- `FGSTN_PoseHistory` per platform (ring of 24 frames, pose + velocity), filled from the store on the host and from received samples on
  clients. It is the shared data of lag compensation (§4.7) and of approach D (§4.5).

### 4.5 How the player's prediction handles it

**Stage 0 and 1 (approach B, v1).** Mover already puts a player on any walkable Movable component, stores it in base-local space,
reconciles in base space, applies carry out of band and keeps base velocity on leaving [V]. What we add is only: the correctly built
collider (§4.3), `bIgnoreBaseRotation` per spec, base velocity on dismount (verify, P4; if zero, a subclass of the walking mode
supplies it from `FGSTN_PoseHistory`, per wall-movement.md rule 4 "subclass, never edit the core"), and the AI limiter (§4.2).
Expected behaviour: standing and walking never reconcile; observers are consistent (everything is in server time and shown through
the same interpolation); errors appear at landing, at the edge of the platform and near other world geometry.
Mitigations before D: slow platforms while occupied or when a player is within mount range; a brace animation as the readable window;
platform tops slightly larger than the visible top (+10 cm, a design decision, not a hidden fudge); generous mount grace in the
walking mode (`EntryGraceFrames`-style, as `UGSTN_GA_WallSlide` does for walls).

**Stage 2 (approach D, only if P1 fails).** The base pose a player's simulation uses for frame F is
`History.Sample(BaseRef, F-1 - ViewOffsetFrames)`: the authoritative pose at the frame the player *displayed* (end of F-1, the M4/M8
snapshot rule). `ViewOffsetFrames` (≤ `MaxRewindMs` in frames, delta-coded, 0 when not near a platform) is an **NP input field** chosen by
the client from its interpolation state; the server uses the same number. Both sides read the same immutable data, so there is no
`IsServer` branch (CLAUDE.md rule 1) and resimulation reproduces it exactly, which also removes the frozen-base resim of §1.5. The
seam is one call at the start of the mode's simulation tick: `IGSTN_PlatformPoseSource::PoseForFrame`, which moves the unattached
collider to that pose for this player's tick (NP ticks players one after another, so two players at different pings can share a
platform; cost: one transform set per player per step, measured in P4). Observers must recompose a standing player from base-local
coordinates and **their own** current base pose (re-sample on proxies every frame), otherwise a player simulated at his own view time
appears offset on other screens by the ping difference × speed. Open: whether Mover's input collection takes a custom struct for the
offset (*unverified*, P2); fallback is a field in the combat input `FGSTN_InputCmd`.

**Rejected**: forward-predicting the base (A) except for a boss later; extrapolating the collider (the player would float ahead of the
visible mesh); networked physics bodies (F); trusting a client position (E).

### 4.6 Lifecycle and state changes

| Event | Rule |
|---|---|
| Mount | Landing on an `Active` collider; optional brace timeline; AI limiter engages |
| Hit, hitstun | Stagger in place or push back: stays a platform (position changes slowly); **launched**: deactivates after `DropGraceFrames` unless the launch is the telegraph itself |
| Death, shatter, part break, grab, promotion, pool release | Deactivate **before** the actor goes away: state → `Braced`/`Breaking` for the grace frames, then collision off; never release a platform that has an occupant |
| Server-announced loss | The player's base becomes null on the server first; the client learns it one RTT later as a correction: position nearly continuous, velocity gets the base velocity (`bImpartVelocityOnLeave`); one correction, accepted |
| Hitstop, LOD tier | Hitstop pauses the timeline and freezes the pose (carry stops with it); never T2/T3, never demoted by `MaxActiveT0`, always relevant while occupied or in mount range |
| Gravity change (owner §2d) | Collider authored in the enemy's frame; walkability decided by Mover against the current up |

### 4.7 Hits and lag compensation

A client attacker sees the platform and the player standing on it at the same interpolation time; the server rewinds both to the same
frame (platform from `FGSTN_PoseHistory`, fighters from `RewindActors`, standing players keep world transform history), so the shot
is consistent as long as one `FGSTN_LagCompTime` is used for all (M8 §9.2.3). Under approach D the standing player's rewound world
position uses the base pose *at his view frame*; store the composed world transform in the history, never recompute it from the
current base. Attacks of the platform enemy against the player standing on it are ordinary minion/fighter hits.

### 4.8 Risks

| Risk | Mitigation |
|---|---|
| Frozen base during resimulation (inferred from `MoverComponent.cpp`) | measure in P1; approach D removes it |
| Mover instance order inside one fixed step makes the base pose F or F-1 | collider moved at one point per step (§4.3); D reads end-of-F-1 |
| Mount/dismount and edge errors ∝ speed × (RTT + 100 ms) | AI limiter, brace, grace frames, then D |
| Base lost or gained one RTT late | `DropGraceFrames` telegraph; never silent removal |
| Minion platform proxy count grows | hard cap 8; promotion to fighter is the escape (M8 §10) |
| Dismount velocity zero for a kinematic collider | P4; subclass supplies velocity from pose history |
| Skeletal or capsule bases | forbidden by validator (`IsDataValid`) |
| Host-only exactness (host on the platform has zero error) | accepted, same as M4 §9b |
| Interaction with grabs, throws, piggyback, parts | deactivation rules in §4.6; owned by M6/M7/M10 integration notes |
| 8 colliders moved per step plus per-player transform sets | P4 budget 0.2 ms |

### 4.9 Acceptance (when this is built)

Spikes P1-P4 recorded; a designer flags a fighter or a minion type as a platform without C++; `scenario_platform_stand` and
`scenario_platform_mount` clean (0 unexplained reconciles beyond the exits of §3.4); a removed `GSTNPlatform`/`GSTNPlatformMinion`
leaves combat and minions building and passing their tests (canary build).

## 5. Open points for the owner

Eight questions (Italian) are in the scratchpad file `questions/replication.md`: awake enemy count and clients, minimum host upload,
which enemies are platforms, platform speed and mounting, what happens when a platform breaks, whether other enemies block players,
acceptable visual delay and LOD, and the Iris decision.

## 6. Sources

Access notes: Valve developer wiki pages, Gabriel Gambetta's article and the Age of Empires paper could not be fetched in this session
(permission timeouts); statements attributed to them are tagged [K].

| Id | Source |
|---|---|
| E1-E4 | Epic Iris docs. E1 overview https://dev.epicgames.com/documentation/unreal-engine/iris-replication-system-in-unreal-engine ; E2 introduction https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-iris-in-unreal-engine ; E3 filtering https://dev.epicgames.com/documentation/en-us/unreal-engine/iris-filtering-in-unreal-engine ; E4 prioritization https://dev.epicgames.com/documentation/en-us/unreal-engine/iris-prioritization-in-unreal-engine |
| E5 | UE 5.8 release notes: https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes |
| E6 | Forum "5.7 Iris Strategy" (Epic, 2026-01-13): https://forums.unrealengine.com/t/5-7-iris-strategy/2699685 |
| E7 | Forum "Iris / Mover / Network Prediction compatibility (UE 5.8.2)" (Epic, 2026-09-17): https://forums.unrealengine.com/t/requesting-an-eta-on-iris-mover-network-prediction-plugin-compatibility-ue-5-8-2/2841863 |
| E8, E9 | Replication Graph in UE (5.8 docs) https://dev.epicgames.com/documentation/unreal-engine/replication-graph-in-unreal-engine ; Replication Graph overview (Epic, 2018) https://www.unrealengine.com/tech-blog/replication-graph-overview-and-proper-replication-methods |
| E10 | Performance and bandwidth tips: https://dev.epicgames.com/documentation/unreal-engine/performance-and-bandwidth-tips-for-unreal-engine |
| E11 | Actor net dormancy: https://dev.epicgames.com/documentation/unreal-engine/actor-network-dormancy-in-unreal-engine |
| E12 | Actor priority: https://dev.epicgames.com/documentation/unreal-engine/actor-priority-in-unreal-engine |
| E13 | Overview of Mass Gameplay: https://dev.epicgames.com/documentation/unreal-engine/overview-of-mass-gameplay-in-unreal-engine |
| E14 | Forum "Three Mass Replication regressions in UE 5.8.1" (Epic reply): https://forums.unrealengine.com/t/three-mass-replication-regressions-in-ue-5-8-1/2743854 |
| E15 | Fast TArray replication guide: https://ikrima.dev/ue4guide/networking/network-replication/fast-tarray-replication/ |
| E16 | Animation Budget Allocator: https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-budget-allocator?application_version=4.27 |
| E17 | Animation Sharing plugin (5.8 docs): https://dev.epicgames.com/documentation/unreal-engine/animation-sharing-plugin-in-unreal-engine |
| E18 | Significance Manager: https://dev.epicgames.com/documentation/unreal-engine/significance-manager-in-unreal-engine |
| E19 | `USkinnedMeshComponent` API: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USkinnedMeshComponent |
| E20 | Mover in Unreal Engine: https://dev.epicgames.com/documentation/unreal-engine/mover-in-unreal-engine |
| E22 | Mover Examples: https://dev.epicgames.com/documentation/unreal-engine/mover-examples-in-unreal-engine |
| E23 | Networked Physics overview: https://dev.epicgames.com/documentation/unreal-engine/networked-physics-overview |
| E24 | Forum "Chaos Mover": https://forums.unrealengine.com/t/chaos-mover/2733017 |
| E25 | `UCharacterMovementComponent` API: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UCharacterMovementComponent |
| G1 | Gaffer On Games, Snapshot Compression: https://gafferongames.com/post/snapshot_compression/ |
| G2 | Overwatch netcode (summary of Ford, GDC 2017): https://edgegap.com/blog/game-backend-deep-dive-overwatch-2016-netcode-architecture-rollback ; talk: https://gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and |
| G3 | Halo Reach netcode (summary of Aldridge, GDC 2011): https://edgegap.com/blog/game-backend-deep-dive-halo-reach-netcode-host-migration |
| G4 | Destiny (Truman, GDC 2015): https://gdcvault.com/play/1022246/Shared-World-Shooter-Destiny-s ; summary: https://edgegap.com/blog/destiny-2-s-network-achitecture---multiplayer-game-deep-dive |
| G5 | Rocket League physics talk listing (Cone, GDC 2018): https://gamedeveloper.com/design/video-the-rocket-science-behind-i-rocket-league-s-i-physics |
| G6 | Unity Netcode for Entities, physics: https://docs.unity3d.com/Packages/com.unity.netcode@1.6/manual/physics.html |
| G7 | Photon Fusion 2, Forecast Physics: https://doc.photonengine.com/fusion/v2/manual/physics/physics-forecast |
| T1 | BorMor, Iris with 100 players (5.7 preview): https://bormor.dev/posts/iris-one-hundred-players/ |
| T2 | StraySpark, Iris and push model in 5.7: https://www.strayspark.studio/blog/iris-push-model-ue5-7-replication-frameworks |
| T3, T4 | hon454, Replication Graph lab https://github.com/hon454/ue-dedicated-server-optimization-lab/pull/1 ; Fatshark developer comment on large hordes https://devtrackers.gg/vermintide2/p/98f18743-just-a-question-about-cpu-and-horde-size |
| V1, V2, A1 | Not fetched: Valve Source Multiplayer Networking https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking ; Valve Latency compensating methods (same wiki); Bettner and Terrano, 1500 Archers on a 28.8 https://www.gamedeveloper.com/programming/1500-archers-on-a-28-8-network-programming-in-age-of-empires-and-beyond |
| Repo | `Plugins/EngineOverrides/Mover/Source/Mover/{Private/MoverDataModelTypes.cpp, Private/MoverComponent.cpp, Private/MoveLibrary/BasedMovementUtils.cpp, Private/DefaultMovementSet/Modes/WalkingMode.cpp}`, `Plugins/EngineOverrides/Mover/README.md`, `Config/DefaultNetworkPrediction.ini`, `Plugins/GASNPP/NetworkPrediction/Source/NetworkPrediction/{NetworkPrediction.Build.cs, Private/NetworkPredictionWorldManager.cpp, Public/NetworkPredictionSettings.h}`, `Plugins/GASNPP/PATCHES.md`, docs: combat-m8-minions.md §2-§15, combat-m4-hits.md §5-§9b, combat.md §12.7/§13/§15, modularity-and-tags.md §3.1-§3.7, wall-movement.md |
