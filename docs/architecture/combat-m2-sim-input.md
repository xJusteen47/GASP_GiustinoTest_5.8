# Combat M2 — Input in the simulation (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §5 (simulation architecture), §6 (input), §8 (buffer) there,
and [combat-m1-core-input.md](combat-m1-core-input.md), whose types this milestone wires into the combat NP model.
Section numbers below are local to this file.

> **Revision 2026-10-18:** bindings and AI requests use **command tags**; the resolver result includes the losers to
> consume; the M2 part of the **authoritative event stream** (combat.md §5.6) is added. See combat.md §8.2, §9.

## 1. Scope

**Goal:** a player or an AI enters a pattern (button, direction, motion, chord...) and the slot it matches activates
its bound GAS ability, predicted, with time and hold buffering, consumption and reset rules, rollback-safe. Replaces the
M0 spike's "button *i* → ability *i*" mapping.

In:

- Combat sync state gets the M1 history and classifier state; the command gets the stick, the camera yaw and
  `ForcedCommand`.
- Player and AI input sources.
- `UGSTN_MoverSimStateTracker` moved from the project into `GSTNCombatNP`.
- Command binding set (command tag → ability class) on the combat component; resolver run every frame; requests executed in the
  ASC pre-tick.
- Reset rule events produced by the combat model; watched-tag mask for `OnTagAdded` and contexts.
- Debug output, harness extensions, docs.

Out: moves, weapons, kits (M3); hits (M4). The hitstop test window of M0 stays as a debug feature until M4 replaces it.

## 2. Files and changes

```
Plugins/GSTNCombat/
  GSTNCombat.uplugin                        + GSTNCombatCore (if M1 did not add it)
  Source/GSTNCombatNP/
    GSTNCombatNP.Build.cs                   + GSTNCombatCore, Mover (public)
    Public/Simulation/GSTN_CombatSimTypes.h      extended command and sync state (section 3)
    Public/Simulation/GSTN_CombatSimComponent.h  binding set, input source, resolver, debug API
    Public/Simulation/GSTN_CombatSlotBindings.h  UGSTN_CombatSlotBindingSet (DataAsset)
    Public/Input/GSTN_CombatInputSource.h        IGSTN_CombatInputSource + player and AI sources
    Public/Mover/GSTN_MoverSimStateTracker.h     moved from the project (unchanged behaviour)
    Private/...                                  matching .cpp files
Source/GASP_Giustino2/
  GASP_Giustino2.Build.cs                   + GSTNCombatNP
  GAS/GSTN_GameplayAbility.*, GSTN_GA_Jump.cpp, GSTN_GA_WallSlide.cpp   include the tracker from the plugin
  GAS/GSTN_MoverSimStateTracker.*           deleted
GASP_Giustino2.uproject                     + GSTNCombat plugin entry
Tools/GASNPPTest/gasnpp_test.py             combat scenarios (section 8)
Source/GASP_Giustino2/.../GSTN_GASNPPTestComponent.*   combat state in the snapshot JSON
```

The tracker keeps its class name (`UGSTN_MoverSimStateTracker`) and API; only its module and include path change
(`"Mover/GSTN_MoverSimStateTracker.h"`). Its export macro becomes `GSTNCOMBATNP_API`. Blueprints do not reference it
(it is created at runtime), so no asset redirect is needed; add a `CoreRedirects` class entry anyway in
`Config/DefaultEngine.ini` (`/Script/GASP_Giustino2.GSTN_MoverSimStateTracker` → `/Script/GSTNCombatNP.GSTN_MoverSimStateTracker`)
in case a saved object references it.

## 3. State types

**Command (`FGSTN_CombatInputCmd`)**

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
| `int32 LastActivatedFrame` | Frame of the last request; feeds the `MoveActivated` reset event. |
| `int32 HitstopStartFrame/EndFrame` | M0 debug hitstop, unchanged. |

`ShouldReconcile` compares every field (history by `operator==`). `Interpolate` copies `To`. `ToString` prints the
history in the M1 notation (it ends up in `np.PrintReconciles` logs, which is how mismatches get diagnosed).

## 4. Input sources

- `IGSTN_CombatInputSource` (UInterface, C++): `void ProduceCombatInput(const UGSTN_CombatSimComponent&, int32 DeltaMs,
  FGSTN_CombatInputCmd& Out)`. The component holds one source object (`UPROPERTY(Instanced, EditAnywhere)`), default
  `UGSTN_PlayerCombatInputSource`. Script overrides of M0 (`SetScriptedButtons`, `TapScriptedButtons`) keep priority.
- **`UGSTN_PlayerCombatInputSource`**: uses the component's `UGSTN_CombatInputConfig` (M1) for buttons and a
  `MoveAction` (`UInputAction`, 2D). Binds `Started` of every button action on the owning player's
  `UEnhancedInputComponent` once controlled (same waiting logic as `UGSTN_AbilityInputComponent`), ORing bits into
  pending taps; held bits and the stick come from `UEnhancedPlayerInput::GetActionValue` at production time. This
  reads injected input too, so the harness's `hold()`/`press()` (which inject through Enhanced Input) drive it.
- **`UGSTN_AICombatInputSource`**: `SetHeld(Mask)`, `Tap(Mask)`, `SetStick(FVector2D WorldPlane)`,
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
   (`Combat.BufferEvent.MoveActivated` if `LastActivatedFrame == F-1`, `Combat.BufferEvent.Landed` if the tracker's
   mode went from falling to walking between F-2 and F-1 — keep the previous mode name hash in the sync state for this),
   and `TagsAdded` resolved back to tags — then `ApplyResetRules` with the component's global rules followed by the
   binding set's rules.
5. **Actionable**: the owner has none of `BlockingTags` (component property, default `Combat.State.Busy`,
   `Combat.State.Hitstun`); M3 replaces this with the move/cancel-window logic. Not actionable → skip to 8.
6. **Resolve**: `ForcedCommand` set and valid → that binding (still subject to `CanExecute`); else
   `FGSTN_SlotResolver::Resolve` over the binding set's slots with the owner tags, `CanExecute(i)` = the binding's
   ability has a spec on the ASC and `CanActivateAbility(Spec.Handle, ActorInfo)` returns true (const query on end-of-F-1
   state; the ASC re-checks at execution).
7. **Request**: on success set `RequestedBinding = i + 1`, `RequestFrame = F`, `LastActivatedFrame = F`, consume
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

The union of tags referenced by the binding set's slot contexts and reset rules (`OnTagAdded`), collected and sorted by
tag name when the binding set is assigned (deterministic on every machine), max 64 (`IsDataValid` error beyond).
The mask is only for "added this frame" detection. Slot contexts are evaluated against the owner's full end-of-F-1 tag
container (exact, and already rolled back by the ASC).

## 7. Command binding set

`UGSTN_CombatSlotBindingSet` (DataAsset): `Slots: TArray<FGSTN_CombatSlotBinding>`, each `{ FGSTN_InputSlotEntry Slot
(asset or inline, M1; its command tag is the binding's key); TSubclassOf<UGameplayAbility> Ability; }`, plus
`ResetRules`. At runtime the bindings are sorted by command tag name (deterministic index for requests and
`ForcedCommand`). `IsDataValid` runs the M1
validator over its slots and checks every ability class is set. The component property `SlotBindings` replaces M0's
`ButtonAbilities`; M3 replaces the binding set by the compiled loadout table (same resolver call).

## 7b. Authoritative events (combat.md §5.6)

- `UGSTN_CombatEventSubsystem` (world subsystem, in `GSTNCombatCore`: it only needs tags and actors) with
  `Broadcast` and `Listen(FGameplayTagQuery)`; event struct as in combat.md §5.6. Native event tags
  `Combat.Event.CommandExecuted` (+ the later ones declared now, unused until their milestone).
- The combat tick writes events to a **per-component outbox** (a plain array, not sync state) unconditionally —
  deterministic, no role branch in the simulation. Outside the simulation, after the NP tick (`FinalizeFrame`), the
  component **drains the outbox only on the authority** and broadcasts; on other roles it just clears it. The server
  never re-simulates, so each event is published exactly once.
- `CommandExecuted` tags: the command tag, the owner's context tags at that frame (from the watched/owner tags), the
  binding's ability class tags. Instigator = the pawn.
- Test: a Blueprint-free C++ listener in the harness component counts `CommandExecuted` per command on the server;
  `GetStateSnapshot()` exposes the counts; scenarios check them against the expected number of activations, including
  runs with forced reconciles (the count must not grow on re-simulation).

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
  - Each with lag 60/15 ms, with and without 2 % loss.
- Test content on `BP_GASNPP_TestCharacter` (**tell the user before editing; back up to `Saved/Backups/GAS`**): a test
  input config (attack, special, crouch, dash, buff), a test binding set with: the four standard attack slots, dash on
  a button, buff on a chord, a motion slot, a spin slot (test ability classes so results are distinguishable), and a
  test reset rule. The crouch button must also be mapped for movement if the test character crouches; otherwise only
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
