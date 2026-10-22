# Combat M2 — Input in the simulation (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §5 (simulation architecture), §6 (input), §8 (buffer) there,
and [combat-m1-core-input.md](combat-m1-core-input.md), whose types this milestone wires into the combat NP model.
Section numbers below are local to this file.

> **Revision 2026-10-18:** bindings and AI requests use **command tags**; the resolver result includes the losers to
> consume; the M2 part of the **authoritative event stream** (combat.md §5.6) is added. See combat.md §8.2, §9.

> **Revision 2026-10-18 (context-free commands):** commands carry no context; the binding set holds bindings
> `{Command, Requirements, Ability}` and picks one per frame from the **context tags** (section 6). See combat.md §9.2
> and M1 "Bindings".

> **Revision 2026-10-19 (combat abilities):** a command with **no binding** is legal and is not a candidate (it never
> reserves a press); **lock tags** `Input.Lock.Command[.<root>]` replace `BlockingTags` (section 6b); the request
> carries the **activation context** (trigger button) so abilities can end on release (section 7a). The buffer event
> `MoveActivated` is renamed `Input.BufferEvent.AbilityActivated` (update the comment in `GSTN_BufferResetRules.h`).
> See combat.md §8.2, §11.

> **Revision 2026-10-19 (input plugin):** M2 first does step **S1** of [input.md](input.md) §8 (M1's code moves to the
> new plugin `GSTNInput`, module `GSTNInputCore`), then builds the input simulation in **`GSTNInputNP`** as an
> embeddable piece (`FGSTN_InputCmd`, `FGSTN_InputSimState`, `FGSTN_InputSimulator`, `IGSTN_InputSimHost`) that
> `UGSTN_CombatSimComponent` hosts, plus the standalone `UGSTN_InputSimComponent`. The steps of section 5 are the
> simulator's; the combat component adds only the M0 debug hitstop. Names in this file follow input.md.

## 1. Scope

**Goal:** a player or an AI enters a pattern (button, direction, motion, chord...) and the slot it matches activates
its bound GAS ability, predicted, with time and hold buffering, consumption and reset rules, rollback-safe. Replaces the
M0 spike's "button *i* → ability *i*" mapping.

In:

- Combat sync state gets the M1 history and classifier state; the command gets the stick, the camera yaw and
  `ForcedCommand`.
- Player and AI input sources.
- `UGSTN_MoverSimStateTracker` moved from the project into `GSTNInputNP` (`GSTNInputNP/Mover/`).
- Command binding set (command tag → ability class) on the combat component; resolver run every frame; requests executed in the
  ASC pre-tick.
- Reset rule events produced by the combat model; watched-tag mask for `OnTagAdded`; context tags for bindings.
- Unbound commands, command lock tags, activation context for the activated ability.
- Debug output, harness extensions, docs.

Out: moves, weapons, kits (M3); hits (M4). The hitstop test window of M0 stays as a debug feature until M4 replaces it.

## 2. Files and changes

```
Plugins/GSTNInput/                               new plugin (input.md); deps GameplayAbilities, AbilitySystemSimulation,
  GSTNInput.uplugin                                NetworkPrediction, EnhancedInput, Mover
  Source/GSTNInputCore/                          step S1: M1's generic code moved from GSTNCombatCore (input.md §4)
  Source/GSTNInputNP/
    GSTNInputNP.Build.cs                         GSTNInputCore, GAS, AbilitySystemSimulation, NetworkPrediction, NetCore, Mover
    Public/Simulation/GSTN_InputSimTypes.h       FGSTN_InputCmd, FGSTN_InputSimState (section 3)
    Public/Simulation/GSTN_InputSimulator.h      FGSTN_InputSimulator, IGSTN_InputSimHost (section 5)
    Public/Simulation/GSTN_InputSimComponent.h   standalone component + model (input.md §5.4)
    Public/Simulation/GSTN_CommandSourceInterface.h  IGSTN_CommandSourceInterface, FGSTN_ActivationContext (section 7a)
    Public/Bindings/GSTN_CommandBindingSet.h     UGSTN_CommandBindingSet (DataAsset, section 7)
    Public/Sources/GSTN_InputSource.h            IGSTN_InputSource + player and AI sources (section 4)
    Public/Context/GSTN_InputContextProvider.h   provider base + Mover provider (section 6)
    Public/Mover/GSTN_MoverSimStateTracker.h     moved from GSTNCombatNP (unchanged behaviour)
    Private/...                                  matching .cpp files
Plugins/GSTNCombat/
  GSTNCombat.uplugin                             + plugin dependency GSTNInput
  Source/GSTNCombatCore/                         keeps combat button tags, command tags, UGSTN_StandardSlots (+ tests)
  Source/GSTNCombatNP/
    GSTNCombatNP.Build.cs                        + GSTNInputNP
    Public/Simulation/GSTN_CombatSimTypes.h      sync state = FGSTN_InputSimState Input + M0 hitstop fields; input = FGSTN_InputCmd
    Public/Simulation/GSTN_CombatSimComponent.h  host of the simulator, binding set, source, provider, debug API
Source/GASP_Giustino2/
  GASP_Giustino2.Build.cs                        + GSTNInputNP (and GSTNInputCore if used)
  GAS/GSTN_GameplayAbility.*, GSTN_GA_Jump.cpp, GSTN_GA_WallSlide.cpp   include the tracker from GSTNInputNP
GASP_Giustino2.uproject                          + GSTNInput plugin entry
Plugins/GSTNCombat/Config/DefaultGSTNCombat.ini  combat tiers and exclusive buttons (UGSTN_CombatSettings; first planned in Config/DefaultGame.ini)
Plugins/GSTNInput/Config/DefaultGSTNInput.ini      tracker redirects ([CoreRedirects]; first in Config/DefaultEngine.ini, input.md §8)
Tools/GASNPPTest/gasnpp_test.py                  combat scenarios (section 8)
Source/GASP_Giustino2/.../GSTN_GASNPPTestComponent.*   combat state in the snapshot JSON
```

The tracker keeps its class name (`UGSTN_MoverSimStateTracker`) and API; only its module changes (include path stays
`"Mover/GSTN_MoverSimStateTracker.h"`). Its export macro becomes `GSTNINPUTNP_API`. Blueprints do not reference it
(it is created at runtime), so no asset redirect is needed; add a `CoreRedirects` class entry anyway in
`Plugins/GSTNInput/Config/DefaultGSTNInput.ini` (first `Config/DefaultEngine.ini`; `/Script/GASP_Giustino2.GSTN_MoverSimStateTracker` and `/Script/GSTNCombatNP.GSTN_MoverSimStateTracker` → `/Script/GSTNInputNP.GSTN_MoverSimStateTracker`; done in S2a)
in case a saved object references it.

## 3. State types

**Command (`FGSTN_InputCmd`)**

| Field | Type | Production |
|---|---|---|
| `HeldButtons` | uint32 | Held state of each configured button's `UInputAction` at sample time. |
| `PressedButtons` | uint32 | Bits set by the action's `Started` event since the last sample (taps). |
| `StickX`, `StickY` | int8 | Move action value (2D) **as on the controller** (+Y up), clamped to the unit circle, × 127, rounded. |
| `CameraYaw` | uint8 | Controller control-rotation yaw, 256 steps (≈ 1.4°). |
| `ForcedCommand` | uint16 | 0 = none, else 1 + index of the command in the binding set (sorted by tag name, so the index is the same on every machine). Set by the AI source. |

`NetSerialize`: packed masks; stick and camera yaw as three bytes; `ForcedCommand` behind a 1-bit flag.

**Sync state (`FGSTN_CombatSyncState`)** — fields after M2:

| Field | Purpose |
|---|---|
| `FGSTN_InputHistory History` | M1 history (contains the held mask that replaces M0's `PrevHeldButtons`). |
| `FGSTN_ClassifierState Classifier` | Zone and last low-zone frame, for flicks. |
| `uint64 WatchedTagMask` | Bit *i* = watched tag *i* present on the owner at end of the previous frame (section 5). |
| `uint8 RequestedBinding` / `int32 RequestFrame` | Frame-stamped activation request (1-based index, 0 = none); same semantics as M0. |
| `uint8 RequestButton` | Activation context: 1 + the button of the winner's terminal press, 0 for `ForcedCommand` (section 7a). Serialized with the request. |
| `int32 LastActivatedFrame` | Frame of the last request; feeds the `MoveActivated` reset event. |
| `int32 HitstopStartFrame/EndFrame` | M0 debug hitstop, unchanged. |

`ShouldReconcile` compares every field (history by `operator==`). `Interpolate` copies `To`. `ToString` prints the
history in the M1 notation (it ends up in `np.PrintReconciles` logs, which is how mismatches get diagnosed).

## 4. Input sources

- `IGSTN_InputSource` (UInterface, C++): `void ProduceCombatInput(const UGSTN_CombatSimComponent&, int32 DeltaMs,
  FGSTN_InputCmd& Out)`. The component holds one source object (`UPROPERTY(Instanced, EditAnywhere)`), default
  `UGSTN_PlayerInputSource`. Script overrides of M0 (`SetScriptedButtons`, `TapScriptedButtons`) keep priority.
- **`UGSTN_PlayerInputSource`**: uses the component's `UGSTN_InputConfig` (M1) for buttons and a
  `MoveAction` (`UInputAction`, 2D). Binds `Started` of every button action on the owning player's
  `UEnhancedInputComponent` once controlled (same waiting logic as `UGSTN_AbilityInputComponent`), ORing bits into
  pending taps; held bits and the stick come from `UEnhancedPlayerInput::GetActionValue` at production time. This
  reads injected input too, so the harness's `hold()`/`press()` (which inject through Enhanced Input) drive it.
- **`UGSTN_AIInputSource`**: `SetHeld(Mask)`, `Tap(Mask)`, `SetStick(FVector2D WorldPlane)`,
  `RequestCommand(FGameplayTag Command)` (sets `ForcedCommand` for one input frame). Exposed to Blueprint; BT/StateTree tasks wrapping them
  are a thin follow-up, not required for M2 acceptance.
- Mapping contexts: button actions must be mapped in a context the player has (e.g. `IMC_Sandbox`); they do **not** need
  to be in `AbilitySystemMappingContexts` (the fork's ability input is not involved).

## 5. Combat tick (per frame F, `SimulationTick`)

Inputs read: the command, the input sync state, the owner's **end-of-F-1** state: facing yaw and Mover tags from the
tracker (`GetSyncState()`), owner gameplay tags from the ASC (at priority 47 the ASC is still at end of F-1, also right
after a rollback restore).

1. **Sample**: `Held = Cmd.HeldButtons`; `Pressed = (Held & ~History.Held) | Cmd.PressedButtons`;
   `Released = (History.Held | Pressed) & ~Held` (a tap shorter than a frame is released the frame it is pressed); stick → `FGSTN_DirectionClassifier::Classify(raw stick, Cmd.CameraYaw, facing
   yaw)`, giving the `Stick`, `Facing` and `World` directions (combat.md §6.3).
2. **Append** the sample to `History` (M1 rules).
3. **Watched tags**: compute the current mask from the owner's tags (section 6); `TagsAdded` = bits set now and not in
   `WatchedTagMask`; store the new mask.
4. **Reset rules**: build `FGSTN_BufferFrameEvents` — event tags produced by the combat model this frame
   (`Input.BufferEvent.AbilityActivated` if `LastActivatedFrame == F-1`, `Input.BufferEvent.Landed` if the tracker's
   mode went from falling to walking between F-2 and F-1 — keep the previous mode name hash in the sync state for this),
   and `TagsAdded` resolved back to tags — then `ApplyResetRules` with the component's global rules followed by the
   binding set's rules.
5. **Locks** (section 6b): the owner has `Input.Lock.Command` → nothing can execute, skip to 8 (presses stay
   buffered). M3 adds the timeline and cancel-window logic.
6. **Resolve**: `ForcedCommand` set and valid → that command (still subject to `CanExecute`); else
   `FGSTN_SlotResolver::Resolve` over the slots of the commands that have **at least one binding** (unbound commands are
   not candidates, so they never reserve a press), `CanExecute(i)` = command *i* is not locked (6b), `SelectBinding`
   over its bindings with this frame's context tags (section 6) finds a binding, and that binding's ability has a spec
   on the ASC and `CanActivateAbility(Spec.Handle, ActorInfo)` returns true (const query on end-of-F-1 state; the ASC
   re-checks at execution). A locked command therefore reserves its press like an unavailable one.
7. **Request**: on success set `RequestedBinding = i + 1`, `RequestButton`, `RequestFrame = F`, `LastActivatedFrame = F`, consume
   `Resolution.EventsToConsume` in `History` (the winner's events and the losers of its press group), and append a
   `CommandExecuted` entry to the component's **event outbox** (section 7b).
8. **Debug hitstop** (M0 behaviour) if the debug button is pressed.

ASC pre-tick (`HandleAbilitySimulationPreTick`, unchanged rule): `RequestFrame == TimeStep.ServerFrame` → find the spec
of the binding's ability class (`FindAbilitySpecFromClass`, exact class — document that bindings must name the granted
class) → `TryActivateAbility`. A failed activation is not retried (the press is consumed); log it under
`LogGSTNCombatSim` Verbose.

**Why consume on request, not on confirmed activation:** the confirmation arrives a frame later; consuming
immediately keeps the press from firing twice, and a failed activation is rare because `CanExecute` mirrors the ASC
check. If playtests show lost inputs, add "un-consume on failure next frame" (the pre-tick writes a failure flag the
next combat tick reads) — noted as *Open*.

## 6. Watched tags

The union of tags referenced by the reset rules (`OnTagAdded`), collected and sorted by tag name when the binding set is
assigned (deterministic on every machine), max 64 (`IsDataValid` error beyond). The mask is only for "added this frame"
detection.

**Context tags** (for binding requirements, built once per combat tick): the owner's full end-of-F-1 gameplay tag
container (exact, already rolled back by the ASC) + Mover's simulated tags from the tracker + the projected context
tags `Input.Context.Grounded` / `.Airborne` (from the tracker's movement mode: walking-like modes = grounded, falling
and wall modes = airborne) and `.OnWall` (project wall modes, configured as a list of mode names on the component).
The projected context tags are also applied to the ASC (combat.md §5.4) so abilities and UI see the same context.

## 6b. Lock tags (combat.md §11.4)

- `Input.Lock.Command` (native) locks every command; `Input.Lock.Command.<root>` locks the commands under
  `Command.<root>`. Native lock tags for the native roots (`Attack`, `Strong`, `Special`, `Combo`, `Super`, `Movement`,
  `Weapon`, `Link`); designer roots declare theirs in `Config/Tags/GSTNCommands.ini` next to the command tags.
- Mapping: a lock tag maps to the command tag with the `Input.Lock.Command` prefix replaced by `Command`
  (`RequestGameplayTag(..., false)`, cached per lock tag on first use; an unknown mapping logs once and locks nothing).
  Command *i* is locked if it `MatchesTag` any mapped tag of the owner's lock tags.
- Read from the owner's end-of-F-1 tags (the same container as the context tags). Never written by the combat model:
  abilities and GEs grant them (`ActivationOwnedTags`, granted tags), so ordinary tag counts apply.
- Replaces M0/M2's `BlockingTags` component property (remove it).

## 7. Command binding set

`UGSTN_CommandBindingSet` (DataAsset): `Slots: TArray<FGSTN_InputSlotEntry>` (the commands' input definitions; empty
entries can reference the standard set), `Bindings: TArray<FGSTN_CommandBinding { FGameplayTag Command;
FGSTN_TagRequirements Requirements; TSubclassOf<UGameplayAbility> Ability; }>`, plus `ResetRules`. At runtime commands
are sorted by tag name (deterministic index for `ForcedCommand`) and bindings by (command, declaration order); the
request stores the **binding** index chosen by `SelectBinding`. `IsDataValid`: the M1 slot validator, the M1
`FindBindingConflicts`, every binding's command has a slot, every ability class set. A slot without bindings is
legal (unbound command): no diagnostic. The component property `SlotBindings` replaces M0's
`ButtonAbilities`; M3 replaces the binding set by the compiled loadout table (same resolver call).

## 7a. Activation context (combat.md §11.5)

- `UGSTN_CombatSimComponent::GetActivationContext()` (BlueprintPure): for the request of the **current** frame,
  `FGSTN_ActivationContext { FGameplayTag Command; int32 PressFrame; int8 Button (-1 = none); EGSTN_ActivationSource
  Source (Command / Forced) }`. Valid while the ASC ticks frame F (the request was written at 47 of F); an ability reads
  it in `ActivateAbility` and keeps what it needs in sync vars.
- `IsCombatButtonHeld(int32 Button)` (BlueprintPure): held state of the current frame from `History` (same-frame read
  at 48). Lets an ability end on release (stance, hold-to-aim) without `ActivationInputs`.
- M3 wraps both in `UGSTN_GA_Base` (`bEndOnTriggerRelease`, adoption source).

## 7b. Authoritative events (combat.md §5.6)

- `UGSTN_SimEventSubsystem` (world subsystem, in `GSTNInputCore`: it only needs tags and actors) with
  `Broadcast` and `Listen(FGameplayTagQuery)`; event struct as in combat.md §5.6. Native event tags
  `Input.Event.CommandExecuted` (+ the later ones declared now, unused until their milestone).
- The combat tick writes events to a **per-component outbox** (a plain array, not sync state) unconditionally —
  deterministic, no role branch in the simulation. Outside the simulation, after the NP tick (`FinalizeFrame`), the
  component **drains the outbox only on the authority** and broadcasts; on other roles it just clears it. The server
  never re-simulates, so each event is published exactly once.
- `CommandExecuted` tags: the command tag, the owner's context tags at that frame (from the watched/owner tags), the
  binding's ability class tags. Instigator = the pawn.
- Test: a Blueprint-free C++ listener in the harness component counts `CommandExecuted` per command on the server;
  `GetStateSnapshot()` exposes the counts; scenarios check them against the expected number of activations, including
  runs with forced reconciles (the count must not grow on re-simulation).

> **As implemented (S2c-S2e):** sections 6b, 7a and 7b are built as described in [input.md](input.md) §5.6, with these
> differences: the interface method is `IsCommandButtonHeld` (not `IsCombatButtonHeld`) and `FGSTN_ActivationContext::Button`
> is an `int32`; `RequestButton` is 255 (`CommandWithoutButton`) for a command with no terminal press; lock tags are
> resolved per command when the table is built, not lazily; the lock check uses exact tags; the event outbox lives in
> `GSTNInputCore`/`GSTNInputNP` and is published by `FGSTN_InputHostUtils::PublishOutbox` on `HasAuthority()` (the listen-server
> host's own pawn included). The harness counter of `CommandExecuted` in the test component is not written yet.

## 8. Debug and harness

- `UGSTN_CombatSimComponent::GetDebugString()` (BlueprintCallable): history in M1 notation, last request, watched tags.
  CVar `gstn.Combat.Debug.Input 1` draws it above locally controlled pawns.
- `GSTN_GASNPPTestComponent::GetStateSnapshot()` adds `"combat": {"history": "...", "request": [binding, frame]}` and
  `_diff` in `gasnpp_test.py` compares the history strings (newest 8 events) between server and autonomous client.
- New scenarios in `gasnpp_test.py` (all with `np.PrintReconciles 1`; clean = only forced reconciles, snapshots equal):
  - `scenario_combat_buffer`: press the dash slot while blocked by a test tag, release before the window ends (no
    activation); repeat holding the button (activates when the tag goes, via hold buffer); a reset rule on a test event
    flushes a held press.
  - `scenario_combat_motion`: back → forward flick + button activates the "motion" binding, plain button activates the
    "button" binding; forced reconcile placed inside the motion.
  - `scenario_combat_chord`: A then B within the grace window → chord binding; A alone → A binding after the grace.
  - `scenario_combat_main_inputs`: the four standard attack slots (combat.md §9.1) bound to four distinguishable test
    abilities: neutral, light tilt (aim zone, must give the neutral one), tilt, crouch + neutral, crouch + tilt, and
    crouch pressed 2 frames after the attack; the camera rotated 90° between runs must not change the results.
  - `scenario_combat_spin`: a 360° stick spin started from two different directions, then the button.
  - `scenario_combat_lock`: a test "stance" ability (bound to a button, owns `Input.Lock.Command.Attack`, ends when
    `IsCombatButtonHeld` of its trigger button turns false): while held, walking still works, the dash command still
    activates, an attack press does nothing; releasing the stance within the hold buffer while holding attack makes the
    attack come out; a forced reconcile while the stance is held.
  - Each with lag 60/15 ms, with and without 2 % loss.
  - `scenario_combat_context`: the same `Command.Attack.Neutral` input on the ground and in the air (after a jump)
    activates the grounded and the airborne binding; a command without an airborne binding does nothing in the air
    and does not steal the press from a fall-through command; a command with a slot and no binding at all does not
    reserve the press (the lower command it overlaps activates).
- Test content on `BP_GASNPP_TestCharacter` (**tell the user before editing; back up to `Saved/Backups/GAS`**): a test
  input config (attack, special, crouch, dash, buff), a test binding set with: the four standard attack commands bound
  grounded and airborne, dash on a button, buff on a chord, a motion command, a spin command (test ability classes so
  results are distinguishable), the stance of `scenario_combat_lock`, one slot left unbound, and a test reset rule. The crouch button must also be mapped for movement if the test character crouches; otherwise only
  the combat modifier matters for the test.

## 9. Acceptance criteria

1. Builds; M1 tests still pass; `GSTNCombatCore` still has no GAS/NP/Mover dependency.
2. The scenarios of section 8 are clean as defined there, including forced reconciles across a buffered press, a
   motion in progress and a chord grace window.
3. Existing `scenario_abilities` and `scenario_forced_rollback` stay clean; wall code unchanged in behaviour (the
   tracker move is mechanical).
4. Sync state size: history serialized ≤ 100 bytes worst case (16 button events × 2 bytes + 16 direction events × 4
   bytes + header), ≤ 4 bytes for an idle pawn after 255 frames (measure with a temporary log of `NetSerialize` byte
   counts).
5. combat.md and testing.md updated; CLAUDE.md table lists the tracker's new location.

## 10. Risks and notes

- `CanActivateAbility` from the combat tick runs on end-of-F-1 ASC state while the activation runs at F inside the ASC
  tick: identical on client and server, so a disagreement only costs a failed activation, never a desync.
- If the facing yaw read from the tracker differs after a correction, the history's direction events for re-simulated
  frames are rewritten by the re-simulation itself (they are produced in the tick), so client and server converge.
- Keep the binding set index stable: it is the value in `RequestedBinding`. Changing the asset at runtime is not
  supported (M3's `LoadoutRevision` handles runtime changes).
