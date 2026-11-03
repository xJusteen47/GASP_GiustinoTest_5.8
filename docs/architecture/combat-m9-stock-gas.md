# Combat M9 — Stock GAS adapter (implementation specification, optional milestone)

Part of the combat design ([combat.md](combat.md)): read §2, §3, §5, §6.4, §11, §12 there, [gas-npp.md](gas-npp.md), the M1–M4
specifications and the outline in [combat-m5-m9.md](combat-m5-m9.md) (M9). Section numbers below are local to this file.
Written 2026-10-21 from the outline; M5–M8 are not implemented, so every statement about them is an *Assumption*.

> **Status:** optional. Section 2 holds the go/no-go criteria; nothing in sections 3+ is built until they say go. Section
> 4 (the dependency rule and its automatic check) is **not** optional: it is a cheap guard that starts now and protects
> the option.

> **Revision 2026-10-22 (owner direction, [game-design-direction.md](game-design-direction.md) §6b):** the reference topology
> is a **listen server**; the stock GAS adapter must also run on a listen host (§9b). Nothing else in this file changes; the
> other owner-direction items (parts, grabs) live above the adapter and reach it only through the neutral seams of §4.

## 1. Scope

**Goal:** a project with **stock** Gameplay Ability System (no Network Prediction, no Mover, no GAS-NPP fork) uses the
combat layer: input commands, buffering, kits, weapons, loadouts, timeline abilities, hitboxes, the hit pipeline,
knockback, hitstun. The same data assets (slot sets, kits, timelines, hurtbox sets, rules) work in both adapters.

In: the adapter `GSTNCombatStockGAS` (+ `GSTNInputStockGAS`), the extraction of the combat tick into pure Core (section
5), prediction through GAS prediction keys, input history sent with the activation, server-authoritative hits with own
lag compensation, GAS-style correction, a movement bridge for `UCharacterMovementComponent`, a canary project and tests.

Out: **rollback and re-simulation** (the point of the adapter is that there is none), forward prediction of simulated
proxies, Mover-based movement (a stock project uses CMC or its own movement; Mover itself depends on NP in 5.8), porting
`UGSTN_GA_PlatformJump`/wall modes (project code stay NP-only), M5 editor changes (the editor module is adapter-neutral).

*Assumptions about earlier milestones* (verify in S0): M2's combat tick and M4's attacker/victim logic are written against
host interfaces (read end-of-F-1 state, write own state, frame-stamped requests) and contain no NP calls; the M3 data
types (`GSTNCombatGAS`) already compile against stock GAS (combat.md §3). Where S0 finds otherwise, extraction is the first
task. M6 grabs/links, M7 mounts, M8 minions are specified **against the adapter-neutral interfaces** of section 5.

## 2. Go / no-go

Build M9 only if **at least one** of the *triggers* holds and **no** *blocker* does. Decide after M4, re-check after M7.

| Triggers | Threshold |
|---|---|
| A second consumer needs the plugin without the fork | A named project or a release plan (store, other team, console build) that cannot ship the fork. |
| Fork maintenance cost | Re-syncing the fork to a new engine version costs more than ~5 working days (count the items of `Plugins/GASNPP/PATCHES.md` that needed manual work) **and** the owner would rather pay M9 once. |
| Rollback cost in play | M4 spike 2.1 shows corrections on the local victim that playtesters call unplayable and forward prediction (combat.md §12.7) is not planned. Stock GAS then is not better; this trigger is only valid if the owner wants **server-reconciled without rollback** feel. |

| Blockers | Why |
|---|---|
| Section 4 check red | The Core/GAS modules acquired NP dependencies: fix first (cost of M9 grows every week). |
| Game needs frame-exact defence | Parry/tech windows below 5 frames across 100 ms+ latency (table in section 11): the adapter degrades them. |
| Another milestone is cheaper | M9 is ~4–6 weeks solo (estimate: extraction 1, adapter 2, bridge + lag comp 1, tests and canary 1–2). |

Default recommendation (*Proposal*): **no-go until a trigger is named by the owner**; do section 4 and the S0 audit now.

## 3. Spikes first (record results in combat.md §17)

0. **S0 audit** (half a day, can run now): list every `#include` and call into NP/Mover/`UNp*` from the M2 tick and the M4
   hit code; estimate the extraction (section 5). Output: a table in combat.md §17.
1. **Payload race**: `ServerSetReplicatedTargetData` with a custom `FGameplayAbilityTargetData` (the activation payload)
   sent with a prediction key: confirm the server ability receives payload and activation in either order, under 60/15 ms and 5 %
   loss, and that a rejected activation drops the payload.
2. **Movement bridge**: CMC freeze (hitstop), launch (knockback) and montage root motion with client prediction; measure
   corrections per hit on the victim's client and whether a custom movement mode `Frozen` can be entered predicted.
3. **Lag compensation history**: server ring of hurtbox transforms (80 frames) versus the client's interpolated view;
   hit disagreement rate (client "hit" vs server "hit") at 60/15 ms for a walking and a dashing target.
4. **Time base**: the owner's frame counter from `AGameStateBase::GetServerWorldTimeSeconds()` versus a free-running
   local counter; drift of the timeline phase between client and server over 5 minutes.

## 4. The dependency rule and its automatic check (starts now)

**Rule (*Decision*):** `GSTNInputCore`, `GSTNCombatCore`, `GSTNCombatGAS` (and any later adapter-neutral module) must not
reference Network Prediction, AbilitySystemSimulation, Mover, ChaosMover, `UNp*`/`FNp*`/`FSync*` types, the fork's
`UNpGameplayAbility`, or any `*NP`/`*StockGAS` module. They may reference stock `GameplayAbilities` (same module name in
the fork) and `GameplayTags`, `GameplayTasks`, `EnhancedInput` (input only).

Four automatic layers (all CI-style, run by every milestone's acceptance):

1. **`Tools/Build/check_module_boundaries.py`** (static, no engine; seconds). Reads `Plugins/*/*.uplugin` and
   `Source/*/*.Build.cs`; table `Tools/Build/module_rules.json` = per module: `allowed_deps` (Build.cs names),
   `forbidden_includes` (regex), `allowed_plugin_deps` (uplugin). Fails on (a) a forbidden name in any `*DependencyModuleNames`
   array, (b) a forbidden `#include` in `Public/` or `Private/` (patterns: `NetworkPrediction`, `Mover`, `NpAbility`,
   `NpGameplay`, `AbilitySystemSimulation`, `Sync.*Types`, `GSTN_*NP/`, `GSTN_*Stock/`), (c) a forbidden identifier
   (`UNpGameplayAbility`, `FSyncInteger`, `UNpAbilitySystemComponent`, `FNetworkPrediction`), (d) a plugin
   dependency outside the allow list, (e) a module present on disk but missing from the rules file (forces a decision).
   Exit code 1 prints module, file, line. Run as a git pre-commit hook (optional) and by the CI job.
2. **`.Build.cs` self-check**: the Core/GAS `.Build.cs` files call `GSTNBoundary.Assert(this, Forbidden)` — a small
   helper in `GSTNBoundary.Build.cs` (UBT compiles every `*.Build.cs`), which throws `BuildException` listing the
   forbidden name found in the rules' dependency lists. It runs on every UBT invocation and project generation, so a
   local edit in Rider fails at once. (If UBT does not allow the helper file in the plugin, keep only layer 1.)
3. **Reflection test** `GSTNCombat.Boundary.NoAdapterTypes` (Automation, editor): iterates `UClass`/`UScriptStruct`
   whose package is `/Script/GSTNCombatCore|GSTNCombatGAS|GSTNInputCore`, walks parents, property types and function
   signatures, and fails on any type from a forbidden package (`/Script/NetworkPrediction`, `/Script/Mover`,
   `/Script/AbilitySystemSimulation`, `/Script/GSTN*NP`, `/Script/GSTN*Stock*`). Catches what text matching misses.
4. **Canary build** (section 12): a minimal project with only the neutral plugins and **no** `Plugins/GASNPP`; if a
   dependency slips in, it does not compile or does not load. Weekly in CI, and on demand before releases.

Today `GSTNCombat.uplugin` lists `NetworkPrediction`, `AbilitySystemSimulation` and `Mover` as plugin dependencies: with
that file the canary cannot load, so **the plugin split of section 5b is part of the rule, not only of M9**.

## 5. Extraction: the combat tick as pure Core (prework M9.0)

- **`FGSTN_CombatState`**: the data of the combat sync state without NP traits (`NetSerialize`, `==`, `ToString`
  members only; the NP adapter's `FGSTN_CombatSyncState` wraps it and adds `Interpolate`/`ShouldReconcile`).
- **`FGSTN_CombatStepper::Step(const FGSTN_CombatState& In, const FGSTN_StepInput&, IGSTN_CombatEnvironment&, FGSTN_CombatState&
  Out)`**, static, doubles, deterministic: M2's tick steps 1–7, M3's timeline execution (frame advance, windows, cancels,
  adoption), M4's attacker queries and victim inbox resolution. No GAS, no NP.
- **`IGSTN_CombatEnvironment`** (what the host provides): `GetMovementView()` (end-of-F-1 transform, velocity, facing, grounded,
  tags: the adapter's reader, `IGSTN_MovementBridge` in section 7), `GetOwnerTags()`, `IsAbilityActive(binding)` and
  `CanExecuteBinding(binding)` (const pre-check), `GetWorldSnapshot()` (hurtboxes: M4 §5), `RequestActivation/Cancel/
  Montage/Effect(...)` (frame-stamped, written into `Out`), `PublishEvent(...)`, `GetFrame()`.
- **`IGSTN_AdapterCapabilities`** (Core): `FGameplayTagContainer GetCapabilities()` (section 12b tags).
- The NP adapter implements the interface in `GSTNCombatNP` with **no behaviour change**: M9.0 passes only if every M2–M4
  harness scenario and Automation test is unchanged (extraction proof). The pre-tick request mechanism of combat.md §5.2
  stays inside the NP adapter; the stock adapter executes requests directly (no ASC pre-tick hook needed).

### 5b. Plugin and module boundaries

| Plugin | Modules | Allowed dependencies (.uplugin and Build.cs) | Forbidden dependencies |
|---|---|---|---|
| `GSTNInput` | `GSTNInputCore` | Engine core, GameplayTags, EnhancedInput (UInputAction refs) | NP, Mover, AbilitySystemSimulation, GSTNCombat, every adapter |
| `GSTNInputNP` (new plugin; module moves out of `GSTNInput`) | `GSTNInputNP` | GSTNInput, GameplayAbilities (fork), AbilitySystemSimulation, NetworkPrediction, Mover | GSTNCombat, any `*StockGAS` |
| `GSTNInputStockGAS` (new) | `GSTNInputStockGAS` | GSTNInput, GameplayAbilities (stock), EnhancedInput | NP, Mover, AbilitySystemSimulation, any `*NP`, GSTNCombat |
| `GSTNCombat` | `GSTNCombatCore`, `GSTNCombatGAS` | GSTNInput, GameplayAbilities (same name in both worlds), GameplayTasks | NP, Mover, AbilitySystemSimulation, every adapter and every later feature plugin (`GSTNHeld`...) |
| `GSTNCombatNP` (new plugin; the module exists today inside `GSTNCombat`) | `GSTNCombatNP` | GSTNCombat, GSTNInputNP, AbilitySystemSimulation, NetworkPrediction, Mover | any `*StockGAS` |
| `GSTNCombatStockGAS` (new) | `GSTNCombatStockGAS` | GSTNCombat, GSTNInputStockGAS, GameplayAbilities, NetCore | NP, Mover, AbilitySystemSimulation, any `*NP` |

What may depend on an adapter: only the game project and feature-plugin adapter modules (`GSTNHeldNP`, ...), never a neutral module. Editor and debug tooling are separate plugins (`GSTNCombatEditor`, `GSTNCombatDebug`, M5 §3b), not modules of `GSTNCombat`; optional feature plugins (`GSTNDefence`, `GSTNLink`, `GSTNMinion`, ...) keep their `*NP` modules inside their plugin (M6 §1b, M8 §4b); whether they also get stock adapters is not decided here (modularity-and-tags.md §3.6, open question M9).

- *Enable/disable independently:* a project enables `GSTNCombat` + exactly one adapter pair (NP or Stock). Both adapters
  enabled in one project is **unsupported** and fails at startup with a clear log: both define `UGSTN_GA_Base`
  (section 6) and register as `IGSTN_CombatAdapter` (a single registered instance, asserted in `FModuleManager` startup).
- *Isolation:* `GSTNInput` + `GSTNCombat` load and pass `GSTNInput.Core.*`/`GSTNCombat.Core.*`/`GSTNCombat.Boundary.*` in the
  canary project without any adapter; each adapter pair loads on its own engine configuration (fork project / canary).
- Moving `GSTNCombatNP` and `GSTNInputNP` into their own plugins is a refactor of `.uplugin` files and redirects only (C++
  module names do not change); done as M9.0 step 0, before the extraction, and valid even if M9 is never built.
- Later plugins (`GSTNHeld`, see M10) depend on `GSTNCombat` and `GSTNLink` (M6 owns the link system) only and ship adapter modules like this table; a stock adapter for `GSTNLink` is not designed here (open question M9).

## 6. GAS side (`GSTNCombatStockGAS`)

Both adapters expose the **same C++ names and Blueprint-callable functions**, so one set of ability Blueprints is portable
by reparenting (a Python helper `Tools/Build/reparent_abilities.py` plus `CoreRedirects`, section 12).

- **`UGSTN_GA_Base : UGameplayAbility`** (stock): activation context (`GetActivationContext()`, same struct), combat clock
  (`GetCombatFrame()`, `WaitCombatFrames` as `UGSTN_AT_WaitCombatFrames : UAbilityTask`, ticked by the component),
  `IsTriggerButtonHeld()`, `bEndOnTriggerRelease`, `AimPolicy`/camera aim, `InterruptPolicy`. Defaults: `InstancedPerActor`,
  `NetExecutionPolicy = LocalPredicted`, `NetSecurityPolicy = ClientOrServer`. Variables survive nothing: no sync vars; state
  lives in the component's `FGSTN_CombatState` (rule 4 of combat.md §5.5 holds, the storage differs).
- **`UGSTN_GA_BaseCombat`**: as M3; the timeline frame comes from the component.
- **`UGSTN_StockCombatComponent : UActorComponent`** (replicated): owns `FGSTN_CombatState`, runs the stepper in a fixed step
  (62.5 Hz, accumulator, at most 4 catch-up steps per engine frame, `TG_PrePhysics`, prerequisite of the movement
  component), on **the owning client (autonomous) and the server**; simulated proxies run nothing (they replay presentation
  from replicated state). Implements `IGSTN_CombatEnvironment`: activations go through `ASC->TryActivateAbility` (client:
  new prediction key; server: from the validated payload, section 8).
- **Frames are local** (*Decision*): each machine counts its own steps. **No absolute frame ever crosses the wire**, only
  deltas relative to the activation or to the packet's send frame. Timeline frame 0 = activation frame on that machine.
- Tag projection (combat.md §5.4) as loose tags on the ASC on every machine that steps, plus on proxies from replicated
  state (presentation only). Replicated state: `FGSTN_ProxyCombatState` (about 8 bytes: action state, timeline frame
  low byte, binding index, hitstop flag; `COND_SimulatedOnly`, `NetUpdateFrequency` 30) and `FGSTN_OwnerCombatState`
  (`COND_OwnerOnly`: hitstun/hitstop **remaining** frames, state serial, meters mirror, last confirmed hit id).
- `UGSTN_StockHitProvider`: the world subsystem `UGSTN_CombatWorldSubsystem` (stock variant) with the M4 registry;
  the **server** keeps `UGSTN_HurtboxHistory` (ring of 80 frames × entity: transform, velocity, hurt state; ~250 KB for 48
  entities). Victim inbox as M4 §7 (drained at the victim's next step), same ordering, no rollback trimming needed.
- **`GSTNCombatGAS` additions:** none beyond tags (section 12b); the adapter-specific classes stay in this module.

## 7. Movement bridge

`IGSTN_MovementBridge` (Core): `GetView()` (transform, velocity, facing, grounded, movement tags), `Launch(Velocity,
bOverrideXY, bOverrideZ)`, `SetFrozen(bool)`, `RequestRotation(Yaw, MaxDeltaDeg)`, `AddRootMotionMove(...)`, `Teleport(...)`
(server), `GetNetCorrectionCount()` (debug). `UGSTN_CMCMovementBridge` (StockGAS) works on `UCharacterMovementComponent`:
knockback and freeze are **server-applied** and corrected on the owner by CMC's own net correction (a snap, stated in
section 11); freeze uses `UGSTN_CharacterMovementComponent` with a custom mode `Frozen` entered through a compressed flag
so owner and server enter it on the same move (spike 2). Other bridges (Mover without NP, custom movement) are
user-provided. Project wall modes and Mover layered moves have no stock equivalent: out of scope.

## 8. Activation with prediction keys and the input history

**Client** (owning): the stepper resolves a command (Core matcher on the local history) → `RequestedBinding` →
`TryActivateAbility` inside an `FScopedPredictionWindow`: GAS creates the key, applies predicted costs, cooldown, tags
and the predicted montage under it. In the same window the component sends **`FGSTN_ActivationPayload`** with
`ServerSetReplicatedTargetData(Handle, ActivationKey, DataHandle, ApplicationTag = Combat.Net.Activation, ScopedKey)`
(reliable, ordered with `ServerTryActivateAbility`). The server ability waits for it in
`UGSTN_AT_WaitActivationPayload` (uses `CallReplicatedTargetDataDelegatesIfSet`: works if the data arrives first).

| Field | Type | Bytes | Notes |
|---|---|---|---|
| `LoadoutRevision` | uint16 | 2 | Mismatch → reject + `ClientResyncLoadout`. |
| `BindingIndex` | uint16 | 2 | Compiled table (M3 §), same on both sides by revision. |
| `Flags` | uint8 | 1 | source (2 bits), forced, hasAim, hasCancel, hasHistory. |
| `TriggerButton`, `TerminalPressAge` | uint8 ×2 | 2 | Age in frames since the terminal press (buffer age). |
| `ViewAgeMs` | uint16 | 2 | Age of the world the client rendered when acting (interpolation delay + latency): lag compensation input; clamped by the server to `MaxRewindMs` and to its own ping estimate + `ViewAgeSlackMs` (50). |
| Stick X/Y, CameraYaw | 3 × uint8 | 3 | Same quantization as `FGSTN_InputCmd`. |
| AimYaw, AimPitch | 2 × uint16 | 0 / 4 | Only for camera-aimed abilities. |
| `CancelFromTimelineFrame`, `CancelledSerial` | uint16 + uint8 | 0 / 3 | Only for a cancel: the client's timeline frame of the cancelled ability; the server validates against **this**, not its arrival time (section 9). |
| History | M1 wire format | 3 – 54 | Window = last 64 frames, newest first, at most 8 button events (2 B) and 8 direction events (4 B) + held mask and counts. |

Typical ≈ 18 bytes, worst case ≈ 70. Server: checks revision, binding legality (`CanExecuteBinding` on **its** state), cost/
cooldown (GAS), lock tags; **revalidation of the input pattern** (`Matcher.Match` on the received history, a pure Core
function, so bit-identical) is `bValidateInputPattern` in `UGSTN_CombatRules` (default on in development builds, off for
co-op PvE). Rejection → stock `ClientActivateAbilityFailed` (prediction key rejected) and event `Combat.Event.ActivationRejected`.

**Input stream during an ability** (stick for DI/aim, hold/release for charge, buffered presses for cancels):
`ServerSendInputFrames` (unreliable RPC), the last 16 frames delta-coded `{held mask (varint), pressed mask (varint), stick,
camera yaw}` every 2nd frame (31 Hz) **only while** a timeline ability is active, a button is held or the stick changed
(heartbeat 4 Hz otherwise): ~0.6 kB/s per active player, 0 idle. Redundancy (16 frames over 2-frame sends) survives 7
consecutive lost packets; the **release of a button bound as trigger/hold** also goes as a reliable `ServerSetReplicatedEvent`
(rare). The server component merges the stream into its own `FGSTN_InputHistory` by frame delta from the packet's send frame.

## 9. Server-authoritative hits, lag compensation, prediction of consequences

- Server steps the attacker's timeline from the activation. Hit queries use the world snapshot; for attackers controlled
  remotely and `bUseLagCompensation`, the target transforms come from `UGSTN_HurtboxHistory` at `now − ViewAgeMs +
  timelineFrame × dt` (interpolated between samples), overlay per attacker like M4 §9; AI attackers never rewind.
  Replaces `RewindActors`.
- **Cancel windows and OnHit conditions on the server** are judged on the client-declared `CancelFromTimelineFrame`
  (window bounds ± `CancelSlackFrames`, default 2), never on packet arrival time: jitter must not reject what the client
  legally did. The server also advances an attacker's timeline phase from the activation instant, so it runs `ViewAge`
  behind the client; the replicated `TimelineFrame` lets the client measure the phase gap (section 10).
- Victim reaction: server resolves the pipeline (inbox, M4 §7), applies GEs (stock replication to the owner), launches through the
  bridge, sets `FGSTN_OwnerCombatState`. **No victim prediction** (stock GAS cannot predict another actor's effect).
- Attacker's predicted consequences (hitstop, `OnHit` cancels, cue, meter gain): computed locally from its interpolated
  view; each gets a **hit id** `{ActivationKey short, hitbox index, target net id}`. The server answers every hit with
  `ClientHitResult(HitId, bConfirmed, FrameAdjust)` (reliable, ~6 bytes). Not confirmed → `UGSTN_HitPresentationComponent`
  stops flash/pause, the component ends the predicted hitstop and removes `OnHit` flags. Costs and cooldown applied by the
  predicted ability are unaffected (the activation was real).

## 9b. Listen server (owner direction §6b; *Proposal*)

Stock GAS on a **listen-server host**: the host's own pawn has authority and is its own owner, so it activates abilities
**without a prediction key round trip** (a locally controlled authority activation: no `ServerTryActivateAbility`, no payload
RPC, no `ViewAge`, no rejection or rollback), while remote clients keep the whole predicted path of sections 8–10. Consequences:

- **No payload race and no correction on the host's own pawn**: its hits are resolved in the same tick with current server state
  (zero lag compensation), the host-advantage of M4 §13, accepted by default.
- **Remote attackers against the host's pawn** are rewound by `UGSTN_HurtboxHistory` exactly as on a dedicated server; the host's
  pawn is a normal entry of the history.
- **Cost on the host**: hurtbox history (~250 KB for 48 entities, section 6), payload merge and lag-compensated queries share the host's
  frame with its own render; budgeted with M8's host rows (`combat-m8-minions.md` §4d.1), and host upload multiplies per-client RPC and
  correction bytes by connected clients (section 8's 18 B typical activation per action).
- **Stock GAS on a listen server** needs the same adapter code path (nothing forks on topology, CLAUDE.md rule 1): the
  "owner is also server" case is the authority branch of `UAbilitySystemComponent` itself, not a branch in M9 code.
  The canary harness (section 13) gains a listen-server configuration (host + 2 clients) next to dedicated + 2 clients.

## 10. Misprediction correction (GAS-style)

| Case | Correction |
|---|---|
| Activation rejected | Stock path: key rejected → predicted GEs/tags/montage removed. Plus `FGSTN_CombatState` restored from the snapshot saved at activation (`FGSTN_ActivationSnapshot`, one per in-flight key, max 4), history consumption undone (consumed flags), buffered press restored so the command may retry once. |
| Activation accepted, state differs | Owner state (`FGSTN_OwnerCombatState`) wins: **snap**, no replay (*Level 0*). Timeline phase gap ≤ 2 frames is absorbed over 8 steps (skip/duplicate), larger gaps snap. |
| Hit predicted, not confirmed | `ClientHitResult` (section 9). |
| Hit not predicted, confirmed | Hitstop/launch start on arrival; montage and timeline frame adjusted by `FrameAdjust`. |
| Victim hit | Server-applied hitstun/knockback arrive as owner state + CMC correction: visible snap, hidden by presentation (flash, camera shake) and by `ReactionBlendFrames` on the mesh. |
| Movement divergence | CMC's own correction. |

*Proposal (not in M9):* Level 1 — replay of the **combat state only** from the server snapshot with the stored local inputs
(cheap because the stepper is pure; the ASC and movement are not replayed). Spike it only if Level 0 corrections are judged
too visible; it adds an input ring and a snapshot ring (64 frames) to the component.

## 11. What degrades without rollback

| Feature | NP adapter | Stock adapter | Mitigation / owner decision |
|---|---|---|---|
| Own activation, hitbox timing, hit-confirm | Exact, rolled back | Predicted, may be un-confirmed (rare visible undo) | Hit ids, short predicted hitstop |
| Victim reaction on own client | Rollback ≈ RTT/16 ms frames, seamless | Late by ~RTT/2 + snap | Presentation, `ReactionBlendFrames` |
| Hitstun/knockback exactness | Bit-exact on client | Approximate (CMC correction) | Tolerance in tests: 5 cm |
| Parry/tech windows < 5 frames | Frame-exact | Judged on arrival; lag comp of the **press** capped at `ParryLagCompFrames` (6) or disabled | Widen windows for stock, PvE only |
| DI/SDI | Own stick at its frame | Latest received stick (~RTT/2 old) | Accept |
| Cancel into next move on jitter | Deterministic | Client-declared frame, ±2 slack | Section 9 |
| Hitstop synchrony attacker/victim | Same frame | Phases differ by ≤ latency | Phase absorption (section 10) |
| Links: grabbed player | Parked transform, predicted | Server-driven attach, owner loses position control | Per design; PvE |
| Links: mount (M7) | Predicted both sides | Rider's mount request predicted by key, carrier sees server state | Keep mount predicted by key |
| Pair moves (M7) partner start | Corrected once | Same plus snap | Same design rule |
| Minions (M8), objects (M10) | Server-only | Same (no change) | none |
| Frame-identical snapshots in harness | Yes | No: tolerance compare | New harness (section 13) |
| Forward prediction of proxies, Rewind Debugger | Possible | Not available | none |
| Wall modes, platform jump (project) | Yes | Not provided | Project code stays NP |

## 12. Content, tools, canary project

- **Canary project** `Tools/StockGASCanary/` (own `.uproject`, engine 5.8 stock plugins, `Plugins/` junctions to `GSTNInput*`,
  `GSTNCombat*` and **not** `GASNPP`): test pawn on `ACharacter` + `UGSTN_CMCMovementBridge`, `UGSTN_StockCombatComponent`, the
  default slot set, one three-hit timeline, a dummy target. CI builds it (UBT target `StockGASCanaryEditor`) and runs
  Automation there.
- `Tools/Build/reparent_abilities.py`: reparents ability Blueprints between adapters (Python in the editor); plus
  `CoreRedirects` shipped as `Config/GSTNCombatStockGAS.ini` (`GSTN_GA_Base`/`GSTN_GA_BaseCombat` from `/Script/GSTNCombatNP`).
  Data assets (kits, timelines, slot sets) need nothing.
- Debug: `gstn.Combat.Debug.Net` (draws server hurtbox history, shows payload size, hit ids), same input display widget.

### 12b. Gameplay tags

All native (CLAUDE.md), declared with `UE_DECLARE_GAMEPLAY_TAG_EXTERN` in the owning module; no ini tags.

| Tag | Owner (declares) | Meaning | Added/removed by |
|---|---|---|---|
| `Combat.Capability` (root) | `GSTNCombatCore` | Root of adapter capability tags (loose tags on the ASC, permanent) | — |
| `Combat.Capability.Rollback` | `GSTNCombatCore` (granted by NP adapter only) | Re-simulation exists | Adapter component at BeginPlay |
| `Combat.Capability.PredictedVictimReaction` | `GSTNCombatCore` (NP only) | Victim reaction predicted on its client | same |
| `Combat.Capability.FrameExactWindows` | `GSTNCombatCore` (NP only) | Parry/tech windows judged on the client's frame | same |
| `Combat.Capability.LagCompensation` | `GSTNCombatCore` (both) | Server rewinds targets for remote attackers | same (when `bUseLagCompensation`) |
| `Combat.Capability.PredictedHitConfirm` | `GSTNCombatCore` (both) | Attacker predicts hit-confirm | same |
| `Combat.Event.ActivationRejected` | `GSTNCombatCore` (SimEvent type) | Server refused a predicted activation (instigator = pawn, magnitude = reason code) | Stock adapter server; NP adapter may also emit |
| `Combat.Event.HitUnconfirmed` | `GSTNCombatCore` | Predicted hit not confirmed (diagnostics, local only) | Stock adapter client |
| `Combat.Net.Activation` | `GSTNCombatStockGAS` | `ApplicationTag` of the activation payload target data | Adapter only |
| `Combat.Net.Input` | `GSTNCombatStockGAS` | Same for the reliable edge event | Adapter only |

Existing tags (`Combat.State.*`, `Input.*`, `Command.*`) are unchanged and produced identically by both adapters.
Rules: kits may put capability tags in binding `Requirements` (e.g. a frame-exact parry binding requires
`Combat.Capability.FrameExactWindows`, with a wider-window fallback binding for stock).

## 13. Tests and harness

- **Automation (pure):** payload round trip and size bounds (min, typical, worst), history window trimming, server pattern
  revalidation equals client, stepper determinism (same inputs, 1e5 frames, identical state hash — this is also the NP
  extraction proof), input-stream merge with loss/reorder/duplicates, hurtbox history interpolation, lag-comp overlay
  against hand-computed positions, snapshot restore after rejection, phase absorption arithmetic.
- **Boundary tests** (section 4): script, reflection test, canary build.
- **Canary PIE harness** `Tools/StockGASCanary/stock_test.py` (modelled on `gasnpp_test.py`: dedicated server + 2 clients **and, from 2026-10-22, a listen-server host + 2 clients** (§9b), net
  emulation 60/15 ms, 0/2/5 % loss): `s9_activation_basic`, `s9_activation_rejected` (server forces a cooldown → no
  residue: tags, GEs, montage, buffered press), `s9_payload_loss`, `s9_hit_basic`, `s9_hit_lagcomp` (target dashing,
  client hit ⇒ server hit ≥ 95 %), `s9_hit_unconfirmed` (target intangible on the server), `s9_cancel_jitter` (± 20 ms jitter,
  no spurious rejection), `s9_victim_reaction` (snap size, hitstun frames within ± 2 of the server), `s9_two_attackers`.
  "Clean" = server and client `GetStateSnapshot()` equal on combat state fields, timeline frame within 2, positions within 5 cm.
- The NP harness stays the reference for the fork; M2–M4 scenarios must pass unchanged after extraction.

## 14. Acceptance criteria

1. Boundary check (all four layers) green, in the fork project and in the canary; failing it is a release blocker.
2. M9.0: extraction and plugin split done, every M2–M4 scenario and test unchanged on the NP adapter.
3. Canary project: a three-hit string with cancel windows, hitstop, knockback and hitstun between two clients at 60/15 ms
   and 2 % loss; scenarios of section 13 clean; payload ≤ 70 bytes, stream ≤ 1 kB/s per active player (measured).
4. Rejected activations leave no residue; unconfirmed hits are undone within 1 RTT.
5. The degradation table (section 11) is checked against measurements and updated; combat.md §3/§17/§18 updated.

## 15. Risks

- **Extraction larger than estimated** (S0): the tick may use NP view types deeply. Mitigation: S0 first; if > 2 weeks, no-go.
- **Two code paths drift:** a feature added to the NP adapter only. Mitigation: all logic in the stepper; adapter reviews
  check the `IGSTN_CombatEnvironment` surface; the canary scenarios run in CI.
- **Timeline phase gap** (server runs ViewAge behind): cancels and hit-confirm feel off. Mitigation: client-declared frames, slack.
- **Stock GAS prediction limits** (no predicted effects on other actors; `FScopedPredictionWindow` rules): victim reaction
  stays late. Accepted and tabulated.
- **Same class names in two adapters** break Blueprints when switching: redirects and the reparent tool; both adapters in one
  project is blocked.
- **Maintenance:** the adapter adds a second engine-version surface (CMC, stock GAS). Counted in the go/no-go.

## 16. How to extend

- *New per-frame logic:* write it in the Core stepper against `IGSTN_CombatEnvironment`; both adapters get it. If it needs a
  new host service, add a method to the interface **and** implement it in both adapters in the same change.
- *New replicated combat data:* add to `FGSTN_ProxyCombatState` only if proxies need it for presentation; otherwise owner state.
- *New payload field:* append with a flag bit, bump `FGSTN_ActivationPayload::Version`, extend the size test.
- *Another movement system:* implement `IGSTN_MovementBridge`, register it on the component.
- *Another adapter* (e.g. Iris-specific, GAS with a custom prediction layer): a new plugin depending on `GSTNCombat` only, with an
  entry in `module_rules.json`, capability tags it truly provides, and the canary scenarios.
