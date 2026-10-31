# Interact — one focus, one prompt: what the button will act on (specification)

Status: **draft for owner review, 2026-10-23**. Docs only. Owner direction that wins over this file: [game-design-direction.md](game-design-direction.md) §2g
(Interact button: tap = grab, hold = pick up objects, "per ora"), §6b (listen server). The owner's answer to question Mod.20, quoted below, is the only *Decision* of
this file; **everything else is *Proposal*** unless a line says otherwise.

Scope: how the game decides which thing the Interact button acts on, shows it, and tells the simulation about it. Out of scope: what an interaction *does* (the
consumer plugins), which input makes a tap or a hold (`GSTNInput` binding data), how pickups are spawned and replicated (`GSTNPickups`), HUD layout.

Sources: [combat-m6-defence-links.md](combat-m6-defence-links.md) §1c, §7.0, §8, §14; [combat.md](combat.md) §5.2–§5.5, §6.1, §6.3, §11.6;
[combat-m2-sim-input.md](combat-m2-sim-input.md) §7a; [combat-m3-moves-kits.md](combat-m3-moves-kits.md) §7.2; [modularity-and-tags.md](modularity-and-tags.md) §3.2,
§3.6–§3.10, §4.1–§4.2b, §5b; [depth-marker.md](depth-marker.md) §4–§6.5 (presentation pattern); [combat-m7-piggyback.md](combat-m7-piggyback.md) §7;
[combat-m10-held-objects.md](combat-m10-held-objects.md) §8; [combat-loadout-mods.md](combat-loadout-mods.md) §4, §12; [game-modes.md](game-modes.md) §2.4.

Notation: *Decision* = decided by the owner or in another document. *Proposal* = this document's suggestion. *Unsure* = I could not verify it against engine source
in this session. "Estimate" = a number to measure, not a fact.

> **Revision 2026-10-23:** first version. Moves the interact probe out of M6 §8 into its own plugin `GSTNInteract` (answer to Mod.20), replaces "nearest to the
> facing direction wins" by a **sticky focus** stored in the simulation, and answers question M6.22.

Owner answer to Mod.20 (2026-10-23, verbatim): "Mod 20. Non basta un'interfaccia con gameplay tag, beh si potrebbe creare un plugin per rendere facile interagire con
cosa si sta puntando invece avere un milioni di armi per terra e non riuscire a capire." *Reading*: the **contract stays small** (an interface plus gameplay tags); a
**dedicated plugin is approved**; its job is the **focus**: what the player points at is the single thing the button acts on, unmistakable among countless weapons.

## 0. Summary

- **Focus** = the one candidate the player points at, from simulated inputs only: the stick's world direction (`StickX/Y` + `CameraYaw`), else the facing, and the
  aim direction while camera aim is on (§2.1). Scored by angle, distance and data priority; **sticky** by a switch margin, a dwell and relaxed leave limits (§2.5).
- **The focus is simulation state**: an opaque `FGSTN_InteractCandidateId` in a sync fragment, with exactly one `Interact.Target.*` tag projected. The ability acts
  on that id and presentation highlights the same id (§3). Candidates are **stamped records**, so a mismatch means one server refusal and one cancel (§3.2).
- **Presentation is local only**: outline or overlay, a prompt (name, icon, tap and hold verbs, hold bar), optional faint markers; Blueprint presenters (§5).
- **Plugin `GSTNInteract`** (Core, Runtime with a game-thread interactor that works with no rollback simulation, Editor) plus the bridge plugin
  **`GSTNInteractCombat`**. It owns the `Interact.*` root and never depends on combat, link or any consumer (§7, §8).

## 1. Purpose and relation to M6 §8

Five consumers share one button: grab (`GSTNLink`), mount (`GSTNPiggyback`), held objects (`GSTNHeld`), kits, support weapons and backpacks (`GSTNPickups`), mission
terminals (`GSTNModeVerbs`, a hold). Only grabs are links, so the probe cannot live in `GSTNLink`; and "nearest to the facing direction" flickers once dozens of
items lie around and does not tell the player what a press will do. This file replaces the probe half of M6 §8.

| Stays in M6 §8 | Moves here |
|---|---|
| The tap/hold table (bindings are slot-set data), grabbability (weight, `GrabLocked`, immunities), grab hitbox, `LinkRequest`, pummel, throw, mash. A tap on a focused object still does nothing in v1: the focus never changes what a binding does | `UGSTN_InteractMath::PickTarget` (now `UpdateFocus`, §2), `IGSTN_InteractCandidateProvider` (§3, §6), `InteractPriority` (per-meaning priorities, §2.3), `InteractReachCm` and the facing arc (rules, §2.2), "project exactly one tag" (the focus's meaning, §3.1). M6.22 is answered by the single focus: enemy and object in reach compete in one scoring, priority as data |

## 2. Focus: what you are pointing at

### 2.1 Pointing direction (simulated inputs only)

| `PointingMode` | Direction | Notes |
|---|---|---|
| `StickThenFacing` (default; *Decision*, owner answer to INT.1, 2026-10-23: the player points with the **character**) | World-plane stick direction (`WorldYaw`, which the classifier already computes from `StickX/Y` and `CameraYaw`, combat.md §6.3) when the stick is past the `Aim` zone, else the facing from the Mover sim state of F-1 | The stick is the intent: walking towards an item points at it; the facing alone lags by the turn rate |
| `Facing` | The facing | Kits that should not steer by stick |
| `CameraAim` (off by default; kept as data for a kit that wants it, since the owner chose the character) | 3D direction from `AimYaw/AimPitch` (combat.md §11.6), origin at the character, never the camera (its position is not simulated) | While `CameraAimWhen` (tag query on the entity's projected tags; the kit's aim ability grants its own tag) holds and the command carries aim values. Pitch reaches shelves and low items; narrower arc |

Planar modes measure angles in the plane perpendicular to the entity's simulated `Up` and bound the height difference along `Up` (`MaxHeightDeltaCm`), never world Z
(gravity can change, game-design-direction §2d). The probe does not read `AimPolicy` (which turns the character during an attack). With a far Mario 3D World camera
the stick is what the player sees, so character-based modes apply; the screen centre would need the camera position in the simulation. The owner confirmed the
character (INT.1, 2026-10-23).

### 2.2 Candidates, reach and arc

A candidate is a point, a radius, a meaning, a priority and a lifetime (§3.2). Reach: `ReachCm`, per candidate overridable up to `HardMaxReachCm`, measured to the
candidate's surface (`max(0, distance - Radius)`). Arc: half-angle `ArcFarDeg` at full reach, widening to `ArcNearDeg` inside `NearRadiusCm`, where the angle is
ignored: an item under the feet is always eligible and a pile around you is reachable whichever way you face. A candidate whose `InteractorRequires` query fails
(against the interactor's tags at end of F-1) is not focusable, unless it sets `bFocusWhenUnactionable` (a locked terminal shows "needs key": focused, no tag
projected; INT.6). `BlockedWhen` (data: hitstun, linked) clears the focus.

### 2.3 Scoring and ties

For each candidate that passes reach and arc, lower is better: `Cost = AngleWeight*(theta/ArcHalf)^AnglePower + DistanceWeight*(d/Reach) + PriorityWeight*(1 -
Priority/PriorityMax)`. `Priority` is the candidate's own value, defaulting to its meaning's entry in the meaning table (default order extends M6's Grab, Pickup,
Mount with Objective and Equipment after Pickup; data; INT.5). `PriorityMode = Lexicographic` makes priority beat cost instead of biasing it. **Determinism:**
positions are quantised to 1 cm, yaw keeps the command's 16 bits, the cost becomes int32 fixed point (1/4096) before any comparison; costs within `TieEpsilon` tie,
then higher `Priority`, then the lower candidate id (total order: `Provider`, `Handle`). The result is a function of the candidate **set**, never of gather order
(§4). *Unsure*: bit-identical floating point across CPU architectures before the quantisation (the hit model has the same assumption, combat.md).

### 2.4 Line of sight

Per-candidate `LosPolicy`: `None` (default for ground pickups and entities: short reach, arc and height band already exclude other floors) or `StaticGeometry`
(terminals, anything behind walls): one segment from the interactor's chest point to the candidate's anchor through `IGSTN_InteractVisibilityQuery` (injected, like
`IGSTN_ExitCollisionQuery`, M6 §7.7), implemented in Runtime as an **object-type query for `WorldStatic` only**, simple collision. Pawns, doors, physics bodies and
anything dynamic never block: they differ between machines, or between a frame and its resimulation. Only the best `MaxLosTraces` (3) by cost are traced, best first,
a blocked one is dropped; the incumbent goes first, so a stable focus costs one trace; no cache to roll back. *Unsure*: that both machines have the same static scene
(streamed cells loaded on the host, not yet on a client); modes load their modules before pawns simulate (game-modes §3). Spike I2.

### 2.5 Sticky focus (hysteresis)

State: `FocusId`, `FocusSinceFrame`, flags. Each probe tick: (1) gather, filter and score (§2.2–§2.4); (2) the incumbent is tested with **relaxed** limits
(`StickyReachScale` 1.15, `StickyArcScale` 1.25), so it stays a candidate a little beyond where it could be newly acquired (a Schmitt trigger on the geometry); (3)
while Interact is pressed or held (`bHoldLocksFocus`) a valid incumbent is kept, so a hold bar is never reset by a neighbour; (4) a challenger B replaces incumbent I
only if `Frame - FocusSinceFrame >= MinDwellFrames` **and** `Cost(B) + SwitchMargin < Cost(I)`, with `SwitchMargin = max(SwitchMarginAbs,
SwitchMarginRatio*Cost(I))`; (5) an incumbent that is gone (removed, outside the relaxed limits, requirement lost) is replaced at once by the best candidate, no
dwell, none left = no focus; (6) acquiring from nothing is immediate. Why: among 50 items the costs of neighbours differ by little and one stick step of 1/127 would
flip the winner every frame; the margin makes a switch cost real movement, the dwell stops A-B-A bounce between two items. An optional "cycle focus" command skips
the incumbent for `CycleSkipFrames` (INT.2); zero cost while unused.

## 3. The focus is part of the simulation result

### 3.1 Fragment, id and projected tag

`FGSTN_InteractFocusFragment` (a `FGSTN_SyncFragment` of the bridge; absent = zero bytes; present when the kit carries interact rules):

| Field | Meaning |
|---|---|
| `FGSTN_InteractCandidateId FocusId` | `{uint8 Provider; uint32 Handle}`, 5 bytes, 0 = none. **Opaque**: no entity reference, no pointer, no tag name |
| `uint8 MeaningIndex` | Compiled index into the meaning table (sorted by tag name, modularity §4.7); 0 = focus with no actionable meaning |
| `int32 FocusSinceFrame`, `uint8 Flags` | Saturated delta from the current frame; flags `Ghost` (§3.2), `Locked`. `CycleSkipId`/`CycleSkipUntilFrame` only when the cycle command is on |

Idle costs 1 bit, focused about 7–9 bytes (estimate, measured in the harness). All four serialization places apply (`NetSerialize`, `ShouldReconcile` on every field,
`Interpolate` = newer value, `ToString`; combat.md §5.5 rule 4). The probe is a tick extension at the tag-projection hook (M3 §7.2 step 10) with an explicit
`Priority`: it reads the end-of-F-1 snapshot, the command of F and the history, writes only this fragment, and requests **exactly one** `Interact.Target.*` tag, the
focus's meaning, as an absolute count nothing else grants (combat.md §5.4). The tag follows the **focus**, not "the nearest of any meaning", so the highlighted
thing, the tag and the acting ability always agree. A press at F+1 acts on the focus written at F.

**Id.** `Provider` comes from the central id registry file (modularity §5b). `Handle` is the provider's own: level-placed actors use a 32-bit hash of a stable
per-actor key (*Unsure* whether name or guid is stable under World Partition; the validator rejects duplicates either way), spawned pickups a per-world monotonic
`uint32` serial allocated by the server, entities a neutral packing the provider chooses (the link provider packs its `FGSTN_EntityRef`); ids are never reused.

### 3.2 Candidate lifetime: stamped records

| Source | Examples | Why the simulation may read it |
|---|---|---|
| Snapshot providers | Enemies to grab, allies to mount, objects that are simulation entities | They read the combat snapshot of F-1 (combat.md §5.3), like hit resolution |
| Level records | Placed pickups, terminals, props | In the level on both machines from frame 0, immovable |
| Spawned records | Drops, supply pods, a thrown weapon that settled | Server-stamped lifetime, replicated; visible to the sim only inside it |

A record is immutable (a moved item is a removal plus a new record) and carries three stamps in server frame numbers, like every stamp in the combat sync state:
`FromFrame` (focusable), `ActionableFrom` (tag projected, >= `FromFrame`), `ToFrame` (exclusive). The registry is **not rolled back**; it is append-only and
stamp-gated, which is what makes reading it from the simulation safe: the candidate set at F depends on the records and F, not on when the machine learned them. The
server stamps outside the simulation (spawn code), so CLAUDE.md rule 1 is untouched: `FromFrame = CurrentFrame + MinAgeFrames` (default 40, 640 ms at the 62.5 Hz
tick), `ToFrame` at removal, tombstones kept `TombstoneFrames` so a late reader sees a removal by stamp, not by absence. A client simulates about one round trip
ahead of the server, so a record is safe if it arrives before the client simulates `FromFrame`: `MinAgeFrames` must cover the supported round trip plus jitter (spike
I1). Objectives arm ahead of time with `ActionableFrom` (game-modes §2.4); items still rolling are not records until settled. *Unsure*: the accessor for the server's
current sim frame from a non-NP actor (assumed exposed by the combat subsystem).

- **Late add** (the record arrives after its `FromFrame` was simulated): the server may focus an item the client does not know. The client keeps the server's
  `FocusId` as a **ghost** (no tag, no highlight) until a known candidate qualifies after `MinDwellFrames`, so an unknown id does not cause a reconcile every frame;
  when the record arrives the ghost becomes a normal incumbent. Worst case, a late pickup the client did not predict.
- **Late remove** (the client focuses what the server already removed, e.g. another player won the race): the server refuses the press, the ability cancels once, the
  focus moves on at the next reconcile. The only unavoidable case, the same race as "simultaneous pickups" (M10 §8, M11 §12).
- **Minimising**: stamps and `MinAgeFrames`, tombstones, the ghost rule, objectives armed early, drop spacing (`MinDropSpacingCm`, `GSTNPickups`) so items stay
  distinguishable, harness `scenario_interact_late_spawn` and `scenario_interact_ghost`.

### 3.3 Acting on the focus

- Bindings are unchanged: the command's requirement is the projected tag. When the combat tick resolves a command whose binding requires an `Interact.Target.*` tag
  it stamps the focus id into the activation context (`FGSTN_ActivationContext::TargetKey`, an opaque `uint64` = `Provider<<32 | Handle`, 0 = none; **seam requested
  from M2 §7a / M3**, combat never names interaction). It is consistent by construction: step 8 of F+1 runs before the probe's step 10 rewrites the fragment, so the
  key is what the tag matched. Fallback if the seam is refused: `UGSTN_InteractUtils::GetFocusSimState` reads the fragment at activation (sanctioned like
  `GetMoverSimState`), safe because the focus is locked on the press and while held, except when the incumbent just became invalid.
- The consumer ability names the target by that id in its server request (M10 `HeldLinkRequest`, the M11 kit pickup, the mode-verb event). Each consumer validates
  its own rules at its confirmation frame; the plugin supplies the pure `IsStillInteractable(Candidate, Interactor, Frame, ServerReachSlackCm)` (stamps, reach with
  lag slack, requirements). A refusal changes nothing: the ability ends once and the hold bar disappears. No `IsServer` or resimulation branch exists.

### 3.4 The same focus without a simulation

`UGSTN_InteractorComponent` (Runtime) runs the same `UGSTN_InteractMath::UpdateFocus` on the game thread with a local `FGSTN_InteractFocusState`: pointing from an
`IGSTN_InteractPointingSource`, tags from `IGameplayTagAssetInterface`, `Interact(Verb)` raising `ServerInteract(Id, Verb)` validated by `IsStillInteractable` and
the target's `CanInteractServer`. Presentation is shared. A pawn has one focus source (validator); in a combat project the bridge's replaces this one.

## 4. Cost with many items

| Concern | Rule |
|---|---|
| Registry | `FGSTN_InteractRegistry` (Core): flat POD records plus a **3D** hash grid, cell = `HardMaxReachCm` (gravity-agnostic, 27 cells per query), O(1) add and remove, no per-frame work. `MaxRecords` 4096 per world; the mode caps drops (`GSTNPickups`: <= 1024 live drops, *Proposal*) |
| Gather, cap, score | Cheap reject by squared distance and height band, then keep the `MaxCandidatesPerProbe` (64) with the smallest distance-only prefilter cost under the total order of §2.3, then at most 64 dot products and `acos` calls (microseconds, estimate) and at most 3 LOS traces. **Never "first K found"**: registration order differs between machines, a cap over the set does not |
| Who probes | Entities whose compiled binding table has a binding requiring an `Interact.Target.*` tag; no interact rules on the kit = no fragment, zero bytes; AI and minions never probe |
| Rollback | The probe reruns on every resimulated frame: budget <= 0.02 ms per interactor per frame (estimate) times the resimulation depth. `ProbeEveryNFrames` (default 1) can halve it by frame parity at one frame of staleness |
| Listen host | N players probe, only the host's own players are presented; measured with the host's frame (modularity §3.5, many-enemies §3.3); `STATGROUP_GSTNInteract`, `gstn.Interact.*`. Replication and dormancy of pickups are `GSTNPickups`'; the record adds about 24 bytes once |

## 5. Presentation (local only, never simulated)

Computed and drawn locally from the **latest predicted** fragment of the locally controlled pawn (what a press now would act on); the presentation subsystem is
created unless `NM_DedicatedServer` (the host renders, *Decision* §6b). Only local players' focus is drawn, one presenter set per local player (split screen); other
players' focus is in replicated state but not drawn (INT.4). The focus resolves to an object through the provider's `ResolveTarget(Id)` (game thread, components may
be read) and `IGSTN_InteractTarget` (`GetDisplayInfo`, `GetHighlightComponents`, `GetAnchor`, BlueprintNativeEvents); an unresolvable id (a ghost) draws nothing.

| Layer | Technique | Notes |
|---|---|---|
| Highlight | Outline (custom depth plus stencil post process), or overlay material on the item's mesh components, or a ground ring decal | *Unsure*: custom depth with Nanite and stencil values shared with other features in 5.8; a stencil range is reserved in settings; overlay is the fallback (spike I3) |
| Prompt | A widget class named by the style, fed a `FGSTN_InteractPresentationView`: item name and icon from the item's data (`GetDisplayInfo`), tap and hold verbs (empty = none: a pickup shows only the hold), hold progress, unactionable reason | Verbs and `HoldFrames` per meaning come from the meaning table; progress from the locally predicted hold frames; a validator compares `HoldFrames` with the binding |
| Nearby markers | Faint pips on up to `MaxNearbyMarkers` (8) other candidates within reach, from a local registry query, nearest first | **Off by default** (INT.3): with many items they add noise, the strong highlight is the point |
| Feedback | 80 ms cross-fade on a change (visual only), `Message.Interact.FocusChanged` for audio and UI, a refusal cue | The simulated focus is never delayed to smooth visuals: highlight and focus must agree |

**Replaceable, as the depth marker (§4–§6.5):** `UGSTN_InteractStyle` (PrimaryDataAsset) lists `Instanced UGSTN_InteractPresenter`s, each with an `ActiveMeanings`
tag query (a grab can look unlike a weapon), curves, tints, the prompt widget class, `MaxNearbyMarkers`. `UGSTN_InteractPresenter` is abstract, Blueprintable,
EditInlineNew, with `BlueprintNativeEvent OnFocusEntered/Updated/Left(view)`; built-ins `_Outline`, `_Overlay`, `_GroundRing`, `_Prompt`, `_NearbyMarkers`. Override
order: settings < component < Experience or mode data, plus a local push and pop override for camera sections.

## 6. Designer extension (modularity §3.8, §3.9)

1. **A new interaction with no C++:** add `UGSTN_InteractableComponent` to the actor Blueprint (`Meaning` tag with `Categories="Interact.Target"`, `DisplayName`,
   `Icon`, verb overrides, `Priority`, `ReachOverrideCm`, `InteractorRequires`, `LosPolicy`, anchor and radius or actor bounds, `bFocusWhenUnactionable`); create the
   leaf `Interact.Target.<Name>` in the project's tag ini; write a Blueprint ability (child of `UGSTN_GameplayAbility` or `UGSTN_GA_BaseCombat`, CLAUDE.md rule 3)
   bound by data to `Press(Interact)` or `Hold(Interact)` requiring that tag. Its server step calls `OnInteractedServer` of the target (BlueprintNativeEvent, free).
2. **Candidates that move in the simulation:** a C++ `IGSTN_InteractCandidateProvider::GatherCandidates(Query, Sink) const`, deterministic, no component reads,
   registered with a provider id and `Priority`, plus its meaning (tag, default priority, verbs, `HoldFrames`) in the meaning table. Snapshot providers live in the
   consumer's NP module, which already depends on combat; the probe passes them its host context as an opaque `{const void* Data; FName Type}` and a provider whose
   type does not match is skipped, so `GSTNInteractCore` knows no combat type (spike I4).
3. **Blueprint forms:** presenters, `IGSTN_InteractTarget` events, `OnInteractedServer`, and the opt-in `UGSTN_InteractCandidateFilter` (pure `BlueprintNativeEvent
   Evaluate(view) -> {bAccept, ScoreBias}` for the best `MaxFilterCalls` (8) survivors only, counted in the budget, with the deny-list validator and debug
   determinism check of §3.8). Gather, scoring and hysteresis stay C++ (Mod.17).
4. **Tuning:** `FGSTN_InteractKitExtension` points a kit at a rules asset (reach, arc, pointing, margins); the Experience selects the default.

## 7. Plugins, modules, dependencies

| Plugin / module | Type | Depends on | Contents |
|---|---|---|---|
| **`GSTNInteract`** / `GSTNInteractCore` | Runtime | `Core`, `CoreUObject`, `GameplayTags` | Pure types, `UGSTN_InteractMath`, registry, meaning table, interfaces, rules structs, the native root tags; Automation-tested with no world |
| `GSTNInteract` / `GSTNInteractRuntime` | Runtime | Core module, `Engine`, `DeveloperSettings`, `GameplayTags`, `UMG` | World subsystem, interactable and interactor components, LOS implementation, settings, presentation subsystem, presenters, style, cvars |
| `GSTNInteract` / `GSTNInteractEditor` | Editor | Runtime, `UnrealEd` | Validators (unique stable keys, verbs and `HoldFrames` against bindings, one focus source per pawn), style preview, reach and arc gizmo |
| **`GSTNInteractCombat`** (bridge, one module) | Runtime | `GSTNInteract` (Core, Runtime), `GSTNCombat` (`GSTNCombatCore`, `GSTNCombatNP`) | Fragment, tick extension, `TargetKey` stamping, `FGSTN_InteractKitExtension`, `UGSTN_InteractCombatFocusSource`; **declares no tag** |
| Consumers `GSTNLink`, `GSTNPiggyback`, `GSTNHeld`, `GSTNPickups`, `GSTNModeVerbs` | existing | `GSTNInteractCore` only, listed in their `.uplugin` | Register providers and meanings; declare their `Interact.Target.*` leaf |

Layer: L3 framework depending on L1 only; `EnabledByDefault: false`. The probe is a bridge plugin, not an NP module of `GSTNInteract`, because an NP module would
force `GSTNCombatNP` on every user and an optional plugin reference may not be enough (*Unsure*, same conclusion as depth-marker §6.1). The bridge follows modularity
§3.2 and §3.7 rule 3: exactly the main plugin plus one other, nothing depends on it. A consumer binding with no focus source is a validator error. **Must never
depend on:** `GASP_Giustino2` and any `/Game/...` content; `GSTNCombat*`, `GSTNInput*` (outside the bridge), `GSTNLink*`, `GSTNPiggyback`, `GSTNHeld`, `GSTNPickups`,
`GSTNModeVerbs`, `GSTNMission`, `GSTNMinion*`, `GSTNParts*`, `GSTNDefence*`; `GameplayAbilities`, `AbilitySystemSimulation`, `NetworkPrediction`, `Mover`; any Editor
module from a runtime module. `check_modules.py` rules 5, 10, 11, 12 apply; defaults are soft references to the plugin's `Content/`; canary build (§3.7 rule 7).

## 8. Gameplay tags

| Tag | Owner | Source | Notes |
|---|---|---|---|
| `Interact`, `Interact.Target` | `GSTNInteract` (`GSTN_InteractTags.h`, Core) | native | Pickers use `Categories="Interact.Target"` |
| `Interact.Target.Grab` / `.Mount` / `.Pickup` / `.Equipment` / `.Objective` | `GSTNLink` / `GSTNPiggyback` / `GSTNHeld` / `GSTNPickups` / `GSTNModeVerbs` | native, **projected** | Open subtree: the plugin that adds a meaning declares its leaf (add to the open list of modularity §4.2) |
| `Interact.Target.<Designer>` | The content's owner | ini (`Config/Tags/GSTNContent.ini` or the content plugin's) | `Chest`, `Talk`, `Lever`; no C++ |
| `Message.Interact.FocusChanged` | `GSTNInteract` | native | Local presentation event, never inside the simulation |

Not declared here: no `Input.Button.*` (the rules name the button as a tag field `InteractButton`, `Categories="Input.Button"`; `Input.Button.Interact` stays the
shared designer tag of `GSTNCommands.ini`), no `Command.*`, no `Combat.*`. Projected tags follow combat.md §5.4 (absolute counts, no ability triggers on them). The
fragment carries the compiled meaning index; the content signature (modularity §3.9) covers the set of registered meanings.

## 9. Types and tuning

| Type | Module | Kind | Role |
|---|---|---|---|
| `FGSTN_InteractCandidateId`, `FGSTN_InteractCandidate` | Core | USTRUCT | `{Provider, Handle}` with total order; one candidate as numbers: id, quantised position, radius, `MeaningIndex`, `Priority`, `ReachOverrideCm`, `LosPolicy`, `QueryIndex`, the three stamps, flags |
| `FGSTN_InteractQuery`, `FGSTN_InteractFocusState`, `FGSTN_InteractFocusResult` | Core | USTRUCT | Inputs of one probe (frame, origin, `Up`, facing, pointing, press and held flags, tags view, rules, opaque host context); persistent focus state; output with a `Reason` (`Acquired`, `Kept`, `Switched`, `Locked`, `Lost`) for debug draw |
| `UGSTN_InteractMath`, `FGSTN_InteractRegistry`, `FGSTN_InteractMeaningTable` | Core | function library; plain structs | Pure `ComputePointing`, `GatherAndScore`, `UpdateFocus`, `IsStillInteractable`; record store with grid and tombstones; meaning tag, compiled index, default priority, verbs, `HoldFrames` |
| `IGSTN_InteractCandidateProvider`, `IGSTN_InteractVisibilityQuery`, `UGSTN_InteractCandidateFilter`, `FGSTN_InteractRules` | Core | native interfaces; abstract Blueprintable; USTRUCT | §6 item 2; §2.4; §6 item 3; tuning below |
| `UGSTN_InteractRulesAsset`, `UGSTN_InteractSettings`, `UGSTN_InteractSubsystem` | Runtime | DataAsset, DeveloperSettings, UWorldSubsystem | Rules per kit or mode; project defaults, caps, stencil range; registry owner, server frame stamping, lookup by id (all net modes) |
| `UGSTN_InteractableComponent`, `UGSTN_InteractorComponent`, `IGSTN_InteractTarget`, `IGSTN_InteractFocusSource`, `UGSTN_InteractStyle`, `UGSTN_InteractPresenter`, `UGSTN_InteractPresentationSubsystem` | Runtime | ActorComponents, interfaces, assets | §6 item 1 (the component replicates its record); §3.4; §5 |
| `FGSTN_InteractFocusFragment`, `FGSTN_InteractTickExtension`, `FGSTN_InteractKitExtension`, `UGSTN_InteractCombatFocusSource` | `GSTNInteractCombat` | | §3.1, §3.3 |

`FGSTN_InteractRules` (*Proposal* defaults), each `EditAnywhere, BlueprintReadOnly`, `Category = "Interact|<Group>"`, with `ClampMin` and `ForceUnits`: **Reach**
`ReachCm` 200, `HardMaxReachCm` 400, `NearRadiusCm` 60, `MaxHeightDeltaCm` 150, `ServerReachSlackCm` 60. **Pointing** `PointingMode`, `CameraAimWhen`, `ArcFarDeg`
50, `ArcNearDeg` 110, `AimArcDeg` 20, `InteractButton`, `BlockedWhen`. **Scoring** `AngleWeight` 1.0, `DistanceWeight` 0.6, `PriorityWeight` 0.25, `AnglePower` 1.5,
`PriorityMode`, `TieEpsilon` 1/256. **Focus** `StickyReachScale` 1.15, `StickyArcScale` 1.25, `MinDwellFrames` 10, `SwitchMarginRatio` 0.2, `SwitchMarginAbs` 0.05,
`bHoldLocksFocus`, `CycleSkipFrames`. **Lifetime** `MinAgeFrames` 40, `TombstoneFrames` 150. **Budget** `MaxCandidatesPerProbe` 64, `MaxLosTraces` 3,
`MaxFilterCalls` 8, `ProbeEveryNFrames` 1, `MaxRecords` 4096.

## 10. Testing

**Automation, pure (`GSTNInteract.Core.*`, no world):** `StickBeatsFacing`, `CameraAimUsesPitch`, `SidewaysUpPlanarAngle`, `OutsideArcRejected`,
`NearRadiusIgnoresAngle`, `HeightBandRejectsOtherFloor`, `PriorityBiasAndLexicographic`, `TieBreakByIdTotal`, `CapIsOrderIndependent` (shuffled registration gives
the same top 64 and focus), `QuantisationStable`, `StickyKeepsIncumbent`, `SwitchNeedsMarginAndDwell`, `HoldLocksFocus`, `GoneIncumbentSwitchesAtOnce`,
`GhostKeptUntilKnownQualifies`, `FromFrameDelaysCandidate`, `TombstoneRemovesAtStamp`, `DuplicateStableKeyRejected`, `NoFlickerAmongFiftyItems` (50 items on a grid
at 40 cm, stick noise of 2 steps, then a walk along the row: zero switches standing, at most one per `MinDwellFrames` walking, each with a real margin, against an
ungated baseline that flips). Several `Up` vectors are used so a hidden world-Z fails. **Runtime (`GSTNInteract.Runtime.*`):** `RecordReplicatesWithStamps`,
`DedicatedServerNoPresentation`, `GameThreadFocusMatchesCore`, `LosIgnoresDynamicActors`, `OnlyLocalPlayerPresented`. **Bridge:** fragment round trip, idle bytes.

**Harness (listen host + 2 clients, then dedicated + 2 clients; 60/15 ms, 2 % loss, forced reconciles):** `scenario_interact_focus_match` (50 pickups in a pile,
scripted stick sweeps, forced rollbacks at random frames: the focus id is equal on host and both clients at every compared frame and equals the presented highlight
id), `scenario_interact_no_flicker`, `scenario_interact_refusal` (the server removes a focused pickup one frame before the press: refused, the ability cancels once,
no reconcile loop), `scenario_interact_late_spawn` (spawn under 150 to 300 ms latency: not focusable before `FromFrame`; mismatches counted against `MinAgeFrames`),
`scenario_interact_ghost` (a client missing a record: reconciles bounded), `scenario_interact_aim`, `scenario_interact_budget` (500 records, 4 players); M6's
`scenario_grab_interact` also asserts the focus id. Spikes: **I1** `MinAgeFrames` against the measured client lead; **I2** the `WorldStatic` segment test equal on
both machines under streaming; **I3** outline and overlay with Nanite and Lumen; **I4** the opaque host context; **I5** fragment bytes and reconcile cost.

## 11. Risks

| Risk | Mitigation |
|---|---|
| Late add or remove of a record causes a mismatch, or an unknown incumbent a reconcile storm | Stamps, `MinAgeFrames`, tombstones, ghost rule; the rest is one refusal and one cancel (§3.2); I1, `scenario_interact_ghost` |
| Scoring differs by one bit across machines; LOS differs when streaming differs | Quantised inputs, fixed-point comparison, total order (*Unsure* across CPU architectures); default `LosPolicy None`, I2 |
| Focus feels too sticky or too jumpy; the priority order is wrong for combat; `TargetKey` needs a change in M2 or M3 (in progress) | Margins, dwell and relaxed limits are per-kit data, cycle command (INT.2), priority per mode (INT.5); ask for the seam now, fallback accessor with the press and hold lock (§3.3) |
| Highlight clashes (stencil values, Nanite, renderer); the opaque host context is type punning | Reserved stencil range, overlay and ring alternatives (I3); type name check, tests, I4 (alternative: the bridge owns the adapter) |
| The fragment replicates to every observer; a Blueprint filter in the probe is costly | 1 bit idle, 7–9 bytes focused, I5 (allies' focus then costs nothing, INT.4); the filter is opt-in, at most 8 calls, in the budget |

## 12. Milestone placement and changes to other documents

**Order.** I0 `GSTNInteractCore` (pure types, math, registry, tests) and I1 `GSTNInteractRuntime` (subsystem, interactable and interactor components, presentation,
test map `L_Interact_Min`) depend on no combat work and can run beside M3 and M4. I2 `GSTNInteractCombat` needs the M6 extension-surface prerequisite (sync
fragments, tick extensions) and the `TargetKey` seam: **M6's grab slice cannot ship before I2**. Consumers: M7, M10, M11e, M12.

| Document | Change |
|---|---|
| M6 §8, §1b, §1c, §14 | Keep the tap/hold table and grab rules; replace the probe paragraph, `PickTarget`, `InteractPriority` and the M6.22 option by a pointer here; `GSTNLinkNP` registers a snapshot provider (meaning `Grab`) and depends on `GSTNInteractCore`; `Interact.Target.Grab` is only a leaf of `GSTNLink`; "a new interact meaning" points to §6 here; `scenario_grab_interact` asserts the focus; open-questions: record the Mod.20 answer, close M6.22, add the INT section |
| modularity §3.6, §4.2 | Row "Interact probe": its own L3 plugin `GSTNInteract` (Core, Runtime, Editor) plus bridge `GSTNInteractCombat`, no longer "with combat"; add the bridge row; `Interact` and `Interact.Target` owned by `GSTNInteract`; `Interact.Target.*` in the open-subtree list; add `Message.Interact.FocusChanged` |
| M7 §7, M10 §8 | Mount is a snapshot provider (meaning `Mount`, priority below grab); `GSTNHeld` registers a provider (records for objects at rest with `PickupPriority` as `Priority` and the object serial as handle, or a snapshot provider if objects are simulation entities); the pickup ability takes its target from `TargetKey`, the hitbox only validates contact; "M6 §8 owns the arbitration" becomes "interaction.md owns it" |
| M11 §4, §12, §16 | `GSTNPickups` depends on `GSTNInteractCore` (not "or `GSTNLinkCore` until then"); the move to `GSTNInteract` becomes *Decision*; `AGSTN_EquipmentPickup` carries a stamped record and implements `IGSTN_InteractTarget` (name and icon from the payload's definition); `MinDropSpacingCm`; "when it is the best target" becomes "when it is the focus" |
| game-modes §2.4, §8.2, plugin table | `GSTNModeVerbs` registers records on mission props; arming uses `ActionableFrom` set ahead, replacing the "armed mask as recorded input" fallback; the `Interact.*` root is `GSTNInteract` |

## Domande per il proprietario

The questions INT.1–INT.6 (in Italian) are in [open-questions-m4-m10.md](open-questions-m4-m10.md), section "Interazione: cosa si sta puntando": point with the character or the screen centre, a button to cycle the focus, faint markers on other candidates, whether allies see your focus, the priority among meanings (M6.22), and unusable items greyed or ignored.
