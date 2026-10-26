# Level activation: objects triggered by actions (`GSTNActivation`)

Status: **specification, 2026-10-26.** Owner brainstorm, verbatim in [game-design-direction.md](game-design-direction.md) §6k: the red and
blue platforms of Super Mario 3D Land, which flip when the player jumps, as the first case of a **reusable way to activate objects with an
action** (jump, and others). Owner's ideas for multiplayer:
- grouped platforms react only to the jumps of the **first player who stepped on them**, and take that player's colour, so several
  players jumping together do not flip them too often;
- a **short delay** before they change side, so the other players have time to jump in reaction.

Questions FLP.1–FLP.6 are in [open-questions-m4-m10.md](open-questions-m4-m10.md). Nothing here is built.

## 1. Goals

1. One mechanism for every "an action does something to the level" case: flip panels, switches hit by a ground pound, knobs turned by a
   spin, doors opened by a hit, blocks moved by a drill. Screw switches that turn with the spin direction are the spin and drill thread's
   first consumer ([spin-drill.md](spin-drill.md)).
2. Designers build activators from data and Blueprint, with no C++ per object.
3. Network-safe on a listen server: every machine shows the same object state at the same simulation frame whenever the delay allows
   it, and the player who acts sees the result at once on their own machine.
4. A plugin usable alone; combat, Mover and the crystal colour are optional bridges.

## 2. Concepts

| Concept | What it is |
|---|---|
| **Trigger** | An action of an entity at a frame: `Activation.Trigger.Jump`, `.Land`, `.GroundPound`, `.Spin` (with its sense), `.Drill`, `.Hit`, `.Interact`, `.Enter`, `.Leave`, designer leaves in ini |
| **Activator** (`UGSTN_ActivatorComponent`, on any actor) | Listens to triggers that match its query, from the entities its **filter** accepts, and schedules an effect |
| **Group** (`AGSTN_ActivatorGroup`) | Several activators that share one state and one owner (the flip panels of one area) |
| **Effect** (`UGSTN_ActivationEffect`, Blueprintable instanced object) | What happens: flip a group, toggle, rotate by steps, move along a spline, open. Targets implement `IGSTN_Activatable` |
| **Claim** | Optional owner of a group: the first player who stood on it. Only the owner's triggers count while the claim holds |

## 3. Where triggers come from (simulation side)

Triggers are read from confirmed or predicted simulation state, never from animation or from Mover component queries:

- **Jump**: `FGSTN_JumpState::StartFrame` already exists in the jump state (sync state). A jump at frame F is `StartFrame == F`. The
  bridge `GSTNActivationCombat` reads it from the combat snapshot, so no new seam is needed for jumps.
- **Land, Enter, Leave, standing on**: Mover's floor result in the simulated state, through the bridge `GSTNActivationMover` (the
  `UGSTN_MoverSimStateTracker` view, input.md).
- **Ground pound, spin, drill, hit, interact**: combat outbox events, which needs seam **D-1** (an authoritative event key on the
  timeline, [game-modes.md](game-modes.md) §2) for ability actions. Until D-1 exists only jump, land, enter and hit are available.

## 4. Network model (*Proposal*)

Activators are **server-authoritative objects with a scheduled effect**. The owner's delay idea is also what makes this work well online.

1. When a matching trigger happens at frame F, the effect is scheduled at `F + DelayFrames`. The server stamps it in the object's
   replicated state: `{ uint8 State; uint8 OwnerSlot; int32 ScheduledFrame; uint8 ScheduleSerial }`.
2. The acting player's machine **predicts** the schedule from its own simulation, so it sees the telegraph at once.
3. Every other machine receives the schedule before it is due as long as `DelayFrames` is larger than the replication delay
   (*Proposal*: default 20 frames, about 0.32 s at 62.5 Hz, which covers a 150 ms one-way trip). The effect then starts on the same
   frame everywhere.
4. If the schedule arrives late, the machine applies it at once and catches up. A character standing on a moving panel then gets one
   movement correction.
5. The object's collision state is a **pure function of the frame**: `Side(F) = InitialSide XOR (number of flips with ScheduledFrame <= F)`,
   from a ring of the last 4 schedules. A resimulated frame can then ask for the state at that frame. *Unsure*: whether Mover's
   resimulation sees world geometry at the resimulated frame or at the present one; spike A1 checks it with the kinematic lift of
   test map TM1 station 11. If it only sees the present, corrections near a flip are accepted and measured.
6. The server rejects a predicted trigger it does not confirm (a jump the server did not see); the predicting machine rolls the
   telegraph back.

## 5. Claim and colour (owner idea)

- `EGSTN_ClaimRule`: `None` (anyone's trigger counts), `FirstToStand` (the owner's idea), `Team`.
- With `FirstToStand`, the first player whose floor is any member of the group becomes the owner. The server decides ties by
  player slot (the alphabetical order of [combat-parts.md](combat-parts.md) §6.0).
- The group takes the **owner's crystal colour** (presentation only, through a colour provider the project implements, as the depth
  marker does). Without the provider it uses a neutral colour.
- Release (*Proposal*, question FLP.1): the claim ends when the owner has not stood on the group for `ReleaseFrames` (default 120, about
  2 s), or on the owner's KO or respawn.
- During `DelayFrames` the panels **telegraph** in the owner's colour (blink, shake), so the others know when to jump.

## 6. Data

```
FGSTN_ActivatorSpec {
    FGameplayTagQuery Trigger;              // Activation.Trigger.*
    EGSTN_ActivatorFilter Filter;           // Anyone, Players, Owner, Team, TaggedEntities
    int32 DelayFrames = 20;
    int32 CooldownFrames = 0;
    bool bRequireStandingOnGroup = false;   // flip panels: only jumps that start from the group, question FLP.3
    TArray<TObjectPtr<UGSTN_ActivationEffect>> Effects;   // instanced, Blueprintable
}
```

Groups hold `EGSTN_ClaimRule ClaimRule`, `int32 ReleaseFrames` and the member list. Effects are Blueprintable with pure
`BlueprintNativeEvent` hooks (`GetStateAtFrame`, `ApplyPresentation`), and their per-frame code that the simulation reads is C++
(modularity-and-tags.md §3.8).

## 7. Plugin layout

| Module or plugin | Contains | Depends on |
|---|---|---|
| `GSTNActivationCore` | types, pure rules (schedule, claim, state at frame), native tags `Activation.*` | engine only |
| `GSTNActivationRuntime` | components, group actor, replication, telegraph presentation | Core |
| bridge `GSTNActivationCombat` | triggers from the jump state and the combat outbox | `GSTNActivation` + `GSTNCombat` |
| bridge `GSTNActivationMover` | floor, land, enter and leave triggers | `GSTNActivation` + Mover |
| content in `GSTNLevelObjects` | `BP_FlipPanelGroup` (red and blue panels), switch, knob samples | `GSTNActivation` |

## 8. Tests

- Automation (`GSTNActivation.Core.*`): `ScheduleAtTriggerPlusDelay`, `StateIsPureFunctionOfFrame`, `ClaimFirstToStand`,
  `ClaimTieBySlot`, `OwnerOnlyFilter`, `ReleaseAfterIdleFrames`, `CooldownBlocksRetrigger`, `RejectedPredictionRollsBack`.
- Harness `scenario_flip_panels`: host and two clients, the owner jumps, the others react; checks that all machines flip on the same
  frame when the delay covers the latency, and counts corrections on players standing on panels.
- Test map TM1 gets station 14, "Flip panels" ([plans/test-maps.md](plans/test-maps.md)).

## 9. Risks

| Risk | Mitigation |
|---|---|
| Mover resimulation sees present geometry, not past | spike A1; long enough delays; corrections measured |
| Late schedules on high latency | `DelayFrames` per level; catch-up rule; the telegraph hides small delays |
| Too many triggers from crowds (minions) | filters default to players; a per-group trigger cap per frame |
| Abilities' triggers need D-1 | jump, land, enter and hit work without it |
