# Level activation: zones of objects one player activates with an action (`GSTNActivation`)

Status: **specification, 2026-10-26.** Owner brainstorm, verbatim in [game-design-direction.md](game-design-direction.md) §6k. The owner
wants a **generic base**: a zone of objects that **one player** activates with an action. The owner builds the actual objects by hand on
top of it (09:58: "Le piattaforme le implemento a mano poi. L'importante è la base per implementarle"). Two reference examples check
that the base is enough:

- **Red and blue panels** (Super Mario 3D Land): a group of panels flips when its owner jumps. Owner's multiplayer ideas: the group
  reacts only to the **first player who stepped on it**, takes that player's colour, and waits a **short delay** before flipping so the
  others can jump in reaction.
- **An object only one player controls** (New Super Mario Bros. Wii, the owner's screenshot at 09:59 and note at 10:00): a platform
  marked with Mario's emblem that **only Mario can control**, shown in **Mario's red**. The point is exclusive control and the owner's
  colour, not the platform's own mechanism.

Owner answers FLP.1–FLP.6 (2026-10-26 10:13) are in [open-questions-m4-m10.md](open-questions-m4-m10.md): FLP.1 (when a zone is
released) is designer data per zone, FLP.2–FLP.6 accept the proposals below. Nothing here is built.

## 1. Goals

1. One base for every "a player's action does something to the level" case: panels, lifts, switches hit by a ground pound, knobs
   turned by a spin, doors opened by a hit, blocks moved by a drill. Screw switches that turn with the spin direction are the spin and
   drill thread's first consumer (its spin-drill spec lives on that thread's branch).
2. **One player at a time**: a zone is used by a single player (its owner) unless the designer opens it, and it shows that player's
   colour.
3. Designers build objects from data and Blueprint. The parts that change the simulation (collision on or off, a position on a
   track) come from generic C++ components configured by data, so Blueprint only does presentation.
4. Network-safe on a listen server: the acting player sees the result at once on their own machine, and every machine shows the same
   state at the same simulation frame whenever the delay allows it.
5. A plugin usable alone; combat, Mover and the crystal colour are optional bridges. The plugin ships no finished objects.

## 2. Concepts

| Concept | What it is |
|---|---|
| **Trigger** | An action of an entity at a frame: `Activation.Trigger.Jump`, `.Land`, `.GroundPound`, `.Spin`, `.Drill` (both with a sense, below), `.Hit`, `.Interact`, `.Enter`, `.Leave`, designer leaves in ini |
| **Zone** (`AGSTN_ActivationZone`) | A set of member objects plus an optional volume. It holds **one state**, **one owner** and the trigger rules. The panels of one area are one zone; one controllable platform is one zone |
| **Owner** (claim) | The one player whose triggers count. How the owner is chosen is the zone's `ClaimRule` |
| **Value** | The zone's state: an integer that each accepted trigger changes by a step, at a scheduled frame. Panels read its parity, a platform on a track reads it as a position, a door as open or closed |
| **Response** (component on a member) | What a member does with the value. Generic C++ responses cover the simulation part; Blueprint hooks cover the look |

## 3. Where triggers come from (simulation side)

Triggers are read from confirmed or predicted simulation state, never from animation or from Mover component queries:

- **Jump**: `FGSTN_JumpState::StartFrame` already exists in the jump state (sync state). A jump at frame F is `StartFrame == F`. The
  bridge `GSTNActivationCombat` reads it from the combat snapshot, so no new seam is needed for jumps.
- **Land, Enter, Leave, standing on**: Mover's floor result in the simulated state, through the bridge `GSTNActivationMover` (the
  `UGSTN_MoverSimStateTracker` view, input.md).
- **Ground pound, spin, drill, hit, interact**: combat outbox events, which needs seam **D-1** (an authoritative event key on the
  timeline, [game-modes.md](game-modes.md) §2) for ability actions. Until D-1 exists only jump, land, enter and hit are available, so
  objects driven by a spin wait for D-1 and the panels do not.
- **Sense** of spin and drill triggers: `int8 Sense` on the trigger event, +1 clockwise and −1 counter-clockwise, seen from above along
  the acting character's up axis (opposite to its gravity). Hit data uses the same convention (`FGSTN_HitData::TurnSense`), and the
  spin's own state (`Combat.State.Spinning.Clockwise/.CounterClockwise`, spin-drill spec) maps onto it one to one.
- **Not here: screws on enemies.** Screws and bolts on mechanical enemies are parts of the enemy, turned by hits, and they hold an
  armour part ([combat-parts.md](combat-parts.md) §3.4 "Screws and bolts"). Zones stay level objects; they are never hosted by enemies.

## 4. Owner: one player at a time (owner idea)

- `EGSTN_ClaimRule`:
  - `FirstToStand` (default): the first player whose floor is a member of the zone becomes the owner (the panels).
  - `FirstToEnter`: the first player inside the zone's volume.
  - `Assigned`: the owner is a fixed player slot, set by the designer or by the level at start (the Mario-only platform). The zone
    shows that player's colour from the start and never changes owner; if the slot is empty, the zone is inactive or falls back to
    `FirstToStand` (a flag).
  - `StandingNow`: the owner is whoever stands on the zone at the trigger's frame; with two players on it, the one who arrived first.
  - `Team`: any player of the owner's team.
  - `None`: anyone's trigger counts (opt-in, for shared switches).
- The server decides ties by player slot (the alphabetical order of [combat-parts.md](combat-parts.md) §6.0).
- Release is **designer data per zone** (owner answer FLP.1): `EGSTN_ClaimRelease ReleaseOn` flags `Idle` (the owner has not been in
  the zone for `ReleaseFrames`), `OwnerKO` (KO or respawn), or none (the claim holds until the level ends). Default `Idle | OwnerKO`
  with `ReleaseFrames = 120` (about 2 s). `Assigned` zones never release.
- Other players can stand and walk on a claimed zone; their triggers do nothing (FLP.4, accepted).
- The zone exposes the **owner's crystal colour** to its members (presentation only, through a colour provider the project
  implements, as the depth marker does). Without the provider it uses a neutral colour.

## 5. Network model (*Proposal*)

Zones are **server-authoritative objects with scheduled steps**.

1. When an accepted trigger happens at frame F, the step is scheduled at `F + DelayFrames`. The server stamps it in the zone's
   replicated state: `{ int16 Value; uint8 OwnerSlot; int32 ScheduledFrame; int8 Step; uint8 ScheduleSerial }`, plus a ring of the
   last 4 schedules.
2. The acting player's machine **predicts** the schedule from its own simulation, so it sees the telegraph (or the move) at once.
3. Every other machine receives the schedule before it is due as long as `DelayFrames` is larger than the replication delay. The step
   then lands on the same frame everywhere.
4. If the schedule arrives late, the machine applies it at once and catches up. A character standing on the object then gets one
   movement correction.
5. The value is a **pure function of the frame**: `Value(F) = InitialValue + sum of Step over schedules with ScheduledFrame <= F`.
   Responses compute their state from `Value(F)` and the frames since the last step, so a resimulated frame can ask for the state at
   that frame. *Unsure*: whether Mover's resimulation sees world geometry at the resimulated frame or at the present one; spike A1
   checks it with the kinematic lift of test map TM1 station 11. If it only sees the present, corrections near a step are accepted
   and measured.
6. The server rejects a predicted trigger it does not confirm (a jump the server did not see); the predicting machine rolls the
   telegraph back.

**Choosing the delay** (designer data per zone, FLP.2 accepted). It depends on who the object affects:
- an object that changes the ground **under other players** (the panels) needs a delay that covers the latency (*Proposal*: 20
  frames, about 0.32 s at 62.5 Hz, which covers a 150 ms one-way trip) so everyone sees the flip on the same frame. The owner's delay
  idea is exactly this, and the telegraph (blink, shake in the owner's colour) makes it readable;
- an object that **carries only its owner** (the one-player platform) can use 0: the owner predicts it and others see it a little late, which only
  matters to a second player riding along.

## 6. Data

```
// On the zone.
FGSTN_ActivationRule {
    FGameplayTagQuery Trigger;              // Activation.Trigger.*
    EGSTN_ActivatorFilter Filter;           // Players (default), Anyone, TaggedEntities
    EGSTN_ClaimRule ClaimRule = FirstToStand;
    EGSTN_ClaimRelease ReleaseOn = Idle | OwnerKO;   // flags, designer choice (FLP.1)
    int32 ReleaseFrames = 120;
    int32 DelayFrames = 20;
    int32 CooldownFrames = 0;
    EGSTN_TriggerSense Sense = Any;         // Any, Clockwise, CounterClockwise (spin and drill triggers)
    int8 Step = 1;                          // negative for the opposite direction (spin sense, lever side)
    bool bRequireInsideZone = true;         // the trigger must start in the zone (FLP.3, accepted)
    int16 MinValue, MaxValue;               // clamp, or wrap when bWrapValue
    bool bWrapValue = false;
}
```

A zone holds an array of rules (for example spin clockwise = +1, spin anticlockwise = −1) and its member list.

## 7. Responses: what the base gives to build objects

Generic C++ components on a member, configured by data, own the parts the simulation reads (modularity-and-tags.md §3.8):

| Response | Reads | Does | Used by |
|---|---|---|---|
| `UGSTN_ActivationCollisionResponse` | `Value(F)` mod N | enables or disables the member's collision per state (a table of states) | panels (N = 2: red solid on even, blue solid on odd) |
| `UGSTN_ActivationTrackResponse` | `Value(F)`, frames since the step | moves the member along a spline to the point of the current value, over `MoveFrames`, with a curve | platforms on a track, sliding doors, moving blocks |
| `UGSTN_ActivationRotationResponse` | same | rotates the member by a fixed angle per step | knobs, screw switches, rotating bridges |

Presentation is Blueprint, through `IGSTN_ActivationListener` (BlueprintNativeEvent hooks, cosmetic only):
`OnClaimed(OwnerSlot, Colour)`, `OnReleased()`, `OnStepScheduled(Step, FramesUntil)` for the telegraph, `OnStepApplied(Value)`.

Building the two examples by hand:
- **Panels**: one zone with `Trigger = Jump`, `ClaimRule = FirstToStand`, `DelayFrames = 20`; each panel has a collision response
  (red or blue table) and a Blueprint that blinks on `OnStepScheduled` and tints itself on `OnClaimed`.
- **A platform only one player controls**: one zone with `ClaimRule = Assigned` (slot 1, red), a trigger the designer picks (for
  example `Spin` clockwise +1 and `Spin` counter-clockwise −1), `DelayFrames = 0`; the platform has a track response on its spline and a
  Blueprint that shows the owner's emblem and colour on `OnClaimed`. The player rides it with Mover's based movement, like any moving
  platform.

## 8. Plugin layout

| Module or plugin | Contains | Depends on |
|---|---|---|
| `GSTNActivationCore` | types, pure rules (schedule, claim, value at frame), native tags `Activation.*` | engine only |
| `GSTNActivationRuntime` | zone actor, responses, listener interface, replication | Core |
| bridge `GSTNActivationCombat` | triggers from the jump state and the combat outbox | `GSTNActivation` + `GSTNCombat` |
| bridge `GSTNActivationMover` | floor, land, enter and leave triggers | `GSTNActivation` + Mover |

No finished objects ship with the plugin. Test objects (one panel zone, one track zone) live in the test map only.

## 9. Tests

- Automation (`GSTNActivation.Core.*`): `ScheduleAtTriggerPlusDelay`, `ValueIsPureFunctionOfFrame`, `StepClampAndWrap`,
  `ClaimFirstToStand`, `ClaimAssigned`, `ClaimStandingNow`, `ClaimTieBySlot`, `OwnerOnlyTriggers`, `ReleaseAfterIdleFrames`, `CooldownBlocksRetrigger`,
  `RejectedPredictionRollsBack`.
- Harness `scenario_activation_zone`: host and two clients; the owner triggers, the others react; checks that all machines apply the
  step on the same frame when the delay covers the latency, and counts corrections on players standing on the objects.
- Test map TM1 station 14, "Activation zones" ([plans/test-maps.md](plans/test-maps.md)).

## 10. Risks

| Risk | Mitigation |
|---|---|
| Mover resimulation sees present geometry, not past | spike A1; long enough delays; corrections measured |
| Late schedules on high latency | `DelayFrames` per zone; catch-up rule; the telegraph hides small delays |
| Too many triggers from crowds (minions) | filters default to players; a per-zone trigger cap per frame |
| Abilities' triggers (spin, ground pound) need D-1 | jump, land, enter and hit work without it |
