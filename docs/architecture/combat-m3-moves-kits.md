# Combat M3 — Combat abilities, kits, weapons, meters, supers, loadouts, jump (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §5, §10, §11 there, and the M1/M2 specifications. Section
numbers below are local to this file. (The file name still says "moves": kept so links do not break.)

> **Revision 2026-10-18:** cancel windows, customisations and binding keys use **command tags**; timelines get a
> **startup override window** with deferred commit (combat.md §8.3); new authoritative events. Slot categories are
> gone (the command tag hierarchy replaces them).

> **Revision 2026-10-18 (context-free commands):** bindings carry `Requirements` (combat.md §9.2); jump, air jump,
> fast fall and dash are one command each with context-dependent bindings.

> **Revision 2026-10-19 (combat abilities):** no "move" concept any more. `UGSTN_GA_Move` + `UGSTN_MoveDefinition`
> become `UGSTN_GA_Base` (any combat ability) and `UGSTN_GA_BaseCombat` (with a **timeline**, inline or a
> `UGSTN_CombatTimeline` asset); one spec per ability class; timeline abilities started without a command are
> **adopted** one frame later; lock tags; combat clock; camera aim input; kits start from the default command set and
> add or remove commands. Old names → new: `UGSTN_MoveDefinition` → `UGSTN_CombatTimeline` / `FGSTN_CombatTimelineData`,
> `FGSTN_MoveTrack` → `FGSTN_TimelineTrack`, `FGSTN_MoveBinding` → `FGSTN_AbilityBinding`, `MoveSet` → `Bindings`,
> `FGSTN_CompiledMoveTable` → `FGSTN_CompiledBindingTable`, `MoveStartFrame` → `TimelineStartFrame`. See combat.md §11.

## 1. Scope

**Goal:** a character spawns with a loadout (primary kit + support weapon + supers + skin). Its stats come from the kit
and change with the active weapon; commands of the kit, the active weapon and the super slots activate **combat
abilities**; timeline abilities run frame data, synced montages, movement and cancel windows; abilities started by GAS
triggers, effects, timers or AI are adopted by the combat model; EX abilities and supers spend meter; switching weapon is
a predicted timeline ability. The character jumps with a **variable-height jump** (independent rise/fall gravity), air
jumps, fast falls and dashes as kit abilities (combat.md §11b). All rollback-safe. No hits yet (M4): hitbox tracks exist
only as data; dodges come with intangibility in M4.

Out: hit detection and everything in combat.md §12 (M4), including hitscan; the timeline editor (M5, M3 authors
timelines in the details panel); defensive abilities (M6).

## 2. Start with three spikes (half a day each, before the rest)

1. **Runtime grants**: granting abilities (about 40 classes, one spec each) and applying infinite GEs on the server
   *outside* the simulation (loadout application) — confirm how the fork brings them to the owning client (sync state
   correction vs replication), that no reconcile storm follows, and that `FindAbilitySpecFromClass` works on the client
   afterwards.
2. **Stats modifier**: a permanent Mover movement modifier rewriting `UCommonLegacyMovementSettings::MaxSpeed` from an
   attribute in `OnPreMovement`, and Mover's gravity override (`SetGravityOverride`) from a gravity scale; an infinite GE
   changes the attributes; forced reconciles across the change and across a jump's apex; server and client positions
   equal. If the shared settings object turns out to be shared between Mover instances, fall back to
   per-mode `MaxSpeedOverride` (exists on `SimpleWalkingMode`) or project-side mode subclasses.
3. **GAS triggers in the fork**: an ability with `AbilityTriggers` (one `GameplayEvent`, one `OwnedTagAdded`) on the fork
   ASC; trigger it from another ability's `SendGameplayEvent` inside the simulation and from a GE that grants the tag;
   check it activates once on client and server, exactly once again in a forced re-simulation, without reconciles. If
   the fork does not support it, stop and decide with the user (fork patch, or a trigger table evaluated by
   `UGSTN_GA_Base`).

Record the results in combat.md §17 before continuing.

## 3. Modules

**`GSTNCombatGAS`** (new). Depends on Core, GameplayAbilities, GameplayTags, GameplayTasks (no NP, no Mover): data
assets and GAS types usable by any GAS project.

```
Public/
  GSTN_CombatTags.h                     native tags (section 4)
  Attributes/GSTN_MovementAttributeSet.h
  Attributes/GSTN_MeterAttributeSet.h
  Effects/GSTN_GE_MeterCost.h           instant cost GE (Cost data)
  Effects/GSTN_GE_WeaponStats.h         base class for weapon stat GEs (infinite)
  Abilities/GSTN_CombatTimeline.h       FGSTN_CombatTimelineData, UGSTN_CombatTimeline, track structs (section 6.3)
  Abilities/GSTN_ActivationContext.h    FGSTN_ActivationContext, EGSTN_ActivationSource, EGSTN_InterruptPolicy
  Kits/GSTN_PrimaryKitDefinition.h
  Kits/GSTN_WeaponDefinition.h
  Kits/GSTN_SuperDefinition.h
  Kits/GSTN_SuperInputScheme.h
  Kits/GSTN_SkinDefinition.h
  Loadout/GSTN_LoadoutTypes.h           FGSTN_LoadoutData, FGSTN_LoadoutValue, FGSTN_SlotCustomisation
  Loadout/GSTN_LoadoutDefinition.h      designer presets
  Loadout/GSTN_LoadoutRules.h           validation (UObject, replaceable per game)
  Loadout/GSTN_CompiledBindingTable.h   FGSTN_CompiledBindingTable + timeline registry + compiler (pure)
  Loadout/GSTN_LoadoutComponent.h       PlayerState component
  Loadout/GSTN_LoadoutSubsystem.h       LocalPlayer subsystem + UGSTN_LoadoutSaveGame
  Cosmetics/GSTN_SkinComponent.h        applies skins (outside the simulation)
```

**`GSTNCombatNP`** gains: `Abilities/GSTN_GA_Base.h`, `Abilities/GSTN_GA_BaseCombat.h`, a `WaitCombatFrames`
prediction task (modelled on the fork's `UWaitForDurationPredictionTask`), `FGSTN_StatsMovementModifier`,
`FGSTN_LayeredMove_TimelineMovement`, `UGSTN_TimelineMovementMoveCreator`, `FGSTN_SetFacingEffect` (if Mover has no
rotation-only instant effect), the camera aim fields of the input command, and the combat tick logic of section 7.

## 3b. Settings additions (`UGSTN_InputSettings`, Core)

- `DefaultOverrideFramesByTier (TMap<FGameplayTag, int32>)` — startup override window per command tier root, e.g.
  `Command.Attack` 2 (jab-like timelines set 1), `Command.Strong` 2, `Command.Special` 2, `Command.Movement.Jump` = the
  jump squat length (set by the jump timeline). Looked up like the tiers (nearest configured ancestor).
- `DefaultCommandSet (TSoftObjectPtr<UGSTN_InputSlotSet>)` — the plugin's default set (combat.md §9.1), the base of every
  kit's command set unless the kit names another.

## 4. Native tags (`GSTN_CombatTags.h`, `UE_DECLARE_GAMEPLAY_TAG_EXTERN`)

- States (projected, informative): `Combat.State.Busy` (a timeline before its first actionable frame),
  `Combat.State.InTimeline`, `Combat.State.SuperInput`, `Combat.State.CancelWindow`, `Combat.State.OverrideWindow`,
  `Combat.State.Falling` (from the jump phase).
- Locks (M2 added the command locks): `Combat.Lock.Movement` (read by the stats modifier, section 5).
- Abilities: `Combat.Ability.Timeline` (ability tag of every `UGSTN_GA_BaseCombat`), `Combat.Ability.AwaitingAdoption`
  (loose tag while waiting, section 7.4).
- Commands beyond M1's: `Command.Movement.Jump`, `.FastFall`, `.Dash`, `Command.Weapon.Switch.Next/.Previous/.Index<N>`,
  `Command.Super.Slot<N>`, and their lock tags under `Input.Lock.Command.*`.
- `Combat.Weapon.Role.Primary/Support`; asset tag roots `Weapon.`, `Kit.`, `Super.`.
- Events `Combat.Event.TimelineStarted/TimelineFinished/WeaponSwitched/SuperUsed`; `Combat.SetByCaller.MeterCost`,
  `Combat.SetByCaller.Stat.*` (one per movement attribute); `Input.BufferEvent.AbilityActivated/Landed/WeaponSwitched`.

One-line comment each on who grants/reads it.

## 5. Attributes and stats

**`UGSTN_MovementAttributeSet`**: `WalkSpeed`, `Acceleration`, `Braking`, `GroundFriction`, `AirControl`, `Weight`,
`MaxCarryWeight` (used from M7), and the jump set of combat.md §11b: `JumpVelocity`, `JumpRiseGravityScale`,
`JumpCutGravityScale`, `FallGravityScale`, `MaxFallSpeed`, `FastFallSpeed`, `FastFallGravityScale`,
`MinJumpHoldFrames`, `MaxJumpHoldFrames`, `AirJumpCount`, `AirJumpVelocity`, `JumpSquatFrames` (frame counts stored as
floats, rounded down when read). Clamped in `PreAttributeChange` (≥ 0; gravity scales ≥ 0.05). Modified only by
GEs.

**`UGSTN_MeterAttributeSet`**: `ExMeter`, `ExMeterMax`, `SuperMeter`, `SuperMeterMax`; current values clamped to
[0, Max]. Bars = value / `PointsPerBar` (kit data); UI only.

**Kit base stats**: `FGSTN_KitStats` on the kit = one value per movement attribute + meter maxima; applied by
`UGSTN_LoadoutComponent` through the kit's init GE (`InitStatsEffect`, instant, SetByCaller
`Combat.SetByCaller.Stat.*` filled from `FGSTN_KitStats` so designers edit numbers, not GEs).

**Weapon stats**: `UGSTN_WeaponDefinition::StatsEffect` (subclass of `UGSTN_GE_WeaponStats`, infinite, modifiers on the
movement set, magnitudes from SetByCaller `FGSTN_WeaponStatModifiers` on the weapon). Applied when the weapon becomes
active, removed when it stops (section 7.7). The primary weapon's is applied at spawn.

**`FGSTN_StatsMovementModifier`** (`GSTNCombatNP`, `FMovementModifierBase`): queued once by the combat component at
BeginPlay on the authority and the autonomous client (it lives in Mover's sync state, so a correction restores it).
`OnPreMovement`: read the owner ASC's current movement attributes, write `UCommonLegacyMovementSettings` (`MaxSpeed`,
`Acceleration`, `Deceleration`, `GroundFriction`, `AirControl`, the terminal fall speed) and **the gravity override**:
world gravity × the scale of the current jump phase, read from the combat sync state of the same frame (7.9) —
`JumpRiseGravityScale` while rising with the jump held (within `MaxJumpHoldFrames`), `JumpCutGravityScale` while rising
after release, `FallGravityScale` while descending, `FastFallGravityScale` and `FastFallSpeed` while fast falling.
While the owner has **`Combat.Lock.Movement`** (ASC tags of the same frame: the ASC ticks first) it writes `MaxSpeed`
and `Acceleration` 0 (the character brakes with its normal deceleration; gravity and layered moves are unaffected).
Never ends. Also used outside jumps (falling off a ledge = fall phase).

**Jump**: combat characters use the plugin's `UGSTN_GA_PlatformJump` (7.9). The project's
`UGSTN_JumpImpulseMoveCreator` also reads `JumpVelocity` when the attribute set is present (fallback to its current
tuning), so the project jump and the plugin jump agree on height.

## 6. Combat abilities (combat.md §11)

### 6.1 `UGSTN_GA_Base` (`GSTNCombatNP`, `UNpGameplayAbility`)

| Member | Notes |
|---|---|
| `GetCombatComponent()` | From the avatar; cached in `OnGiveAbility`. |
| `ActivationContext` (sync vars) | Copied in `ActivateAbility` from the combat component: M2's `GetActivationContext()` for a command, the adoption record (7.4) for an adopted timeline, else source `Other`. |
| `bEndOnTriggerRelease` | `OnSimulationTick`: the context has a trigger button and `IsCombatButtonHeld` is false → `EndAbility`. |
| `GetCombatClock()`, `WaitCombatFrames(N)` | Combat clock (7.1 `PausedFramesTotal`); the task stores its start clock in a sync var. |
| `GetAimRotation()` | Aim of the current frame (7.10). |
| `InterruptPolicy` | `SkipIfBusy` (default) / `InterruptCurrent`, used only when the activation is not a command (combat.md §11.7). `CanActivateAbility` returns false for `SkipIfBusy` while the combat state is `InTimeline`; `InterruptCurrent` cancels the running timeline ability (`CancelAbilityHandle` from inside the ASC tick) in `ActivateAbility`. |

Lock tags are plain `ActivationOwnedTags`. Every other GAS feature stays as usual (costs, cooldowns, block/cancel tags,
cues, triggers).

### 6.2 `UGSTN_GA_BaseCombat` (child)

| Member | Notes |
|---|---|
| `Timeline` (`FGSTN_CombatTimelineData`, inline) / `TimelineAsset` (`UGSTN_CombatTimeline`) | The asset wins when set. A binding may override both (8). `GetTimeline()` returns the one the combat state says is running. |
| Ability tags | `Combat.Ability.Timeline` added in the constructor. |
| Instancing | `InstancedPerActor`, `bRetriggerInstancedAbility = true` (cancel windows into the same ability re-activate it). |

`ActivateAbility`:

1. If the combat state of this frame shows this class's registry entry with `TimelineStartFrame == F` (command path,
   written at 47) → **start**: store `TimelineStartFrame` and the registry index in sync vars, `PlaySyncedMontage`,
   apply the movement GE (7.6) if the timeline starts with one. Costs commit at `DeferredCommitFrame` (7.2 step 8).
2. Else (trigger, effect, AI, other ability) → apply `InterruptPolicy`, add the loose tag
   `Combat.Ability.AwaitingAdoption`, set the sync var `bAwaitingAdoption`, do nothing else.

`OnSimulationTick`: awaiting and the combat state shows this entry with `TimelineStartFrame == F` → remove the loose tag,
start (as 1). Awaiting for more than 2 frames (another ability was adopted) → `EndAbility` (cancelled). Started and the
combat state's entry or start frame no longer match → `EndAbility`. `EndAbility`: stop the synced montage if it is still
this ability's, remove the loose tag if present.

### 6.3 Timeline data (`FGSTN_CombatTimelineData`; asset `UGSTN_CombatTimeline`, primary type `GSTN_CombatTimeline`)

| Field | Notes |
|---|---|
| `TotalFrames`, `FirstActionableFrame` | Frames at the NPP tick rate. Validation: FAF ≤ Total. |
| `Montage`, `MontageSection`, `MontagePlayRate` | Played with `PlaySyncedMontage` at start. `IsDataValid` warns if the montage length (at the play rate) ≠ `TotalFrames` ± 1 frame. |
| `AimPolicy` | `KeepFacing / SnapToStick / SnapIfAimOrTilt / Camera`, `MaxTurnDegrees`, `TurnUntilFrame`. |
| `Tracks` | `TArray<TInstancedStruct<FGSTN_TimelineTrack>>`. |
| `OverrideFrames` | Startup override window (combat.md §8.3); -1 = the default of its command tier in the settings (`DefaultOverrideFramesByTier`), 0 = none. For an adopted timeline the tier is the one of its ability tags' command, else none (0). |
| `OverrideInto` | `FGameplayTagQuery` on command tags; empty = "any command of a higher tier". Ground attacks add `Command.Movement.Jump` (attack cancel); the jump squat adds the up-shaped attack/special commands. |
| `DeferredCommitFrame` | Derived = `OverrideFrames`. Validation: no hitbox, event, movement track, projectile or cost before it. |
| `BufferResetRules` | Appended while the timeline runs. |

Costs (meter, cooldown) are the ability's ordinary GAS cost and cooldown; `UGSTN_GE_MeterCost` per
meter attribute, the amount in its `Cost` (FScalableFloat).

Track structs in M3 (base `FGSTN_TimelineTrack { int32 StartFrame; int32 EndFrame; }`):

- `CancelWindow { FGameplayTagQuery IntoCommands; ECancelCondition Condition (Always/OnHit/OnBlock/OnWhiff); }` —
  e.g. `IntoCommands = Command.Special | Command.Movement.Jump`; OnHit/OnBlock evaluate false until M4 (hit confirm).
- `Movement { UCurveVector* VelocityCurve (local space, cm/s over timeline frames); float GravityScale; bool
  bLockFacing; EGSTN_MoveMixMode MixMode (Additive/Override); EGSTN_MomentumMode MomentumMode; bool bCancelOnLand;
  int32 LandingLagFrames; }`.
- `Tags { FGameplayTagContainer Tags; }` — granted while inside the window (combat.md §5.4; lock tags allowed here).
- `Event { FGameplayTag CueTag; FGameplayTag GameplayEventTag; }` at `StartFrame` (cue through the ASC request path;
  the gameplay event may trigger other abilities, which are adopted if they are timelines).
- `WeaponSwitch { int32 SwapFrame; EGSTN_SwitchTarget Target (Next/Previous/Index); int32 Index; }` — only on weapon
  switch abilities.
- `Hitbox`, `HurtboxModifier` — **declared** with their M4 fields so assets can be authored, ignored at runtime in M3.

Track lookup at runtime must be O(tracks) per frame without allocation (prebuilt per-timeline frame index optional).

## 7. Combat model changes (`GSTNCombatNP`)

### 7.1 Sync state additions

| Field | Purpose |
|---|---|
| `EGSTN_ActionState ActionState` | `Actionable`, `InTimeline` (M4 adds hitstun etc.). |
| `int16 ActiveTimeline` | Index in the timeline registry (section 9), `INDEX_NONE` when none. |
| `EGSTN_ActivationSource ActiveSource` | `Command`, `Forced` or `Adopted`: events and the override tier. |
| `int32 TimelineStartFrame`, `int32 TimelinePausedFrames` | Timeline frame = `F - TimelineStartFrame - TimelinePausedFrames` (paused frames grow in hitstop, M4). |
| `int32 PausedFramesTotal` | All hitstop frames of the owner so far: combat clock = `F - PausedFramesTotal` (11.5). Constant outside hitstop. |
| `uint8 ActiveWeaponIndex` | 0 = primary, 1.. = additional weapons in loadout order. |
| `uint16 LoadoutRevision` | Written only by the server, out-of-band (section 7.8). |
| `FGSTN_CombatRequests Requests` | Fixed array (4) of `{ EGSTN_RequestType Type; int32 Frame; int16 Arg; }`: ActivateEntry, CancelTimeline, CancelAbility (Arg = registry index), CommitTimeline, SwapWeaponStats, SetFacing, Cue. Replaces M2's single request. |

All in `NetSerialize`/`ShouldReconcile`/`ToString`/`Interpolate`; requests serialized only when `Frame` equals the
state's frame (older ones are dead).

### 7.2 Per-frame order (extends M2 §5)

1. M2 steps 1–4 (sample, history, watched tags, reset rules — plus the running timeline's reset rules).
2. **Timeline clock**: if `InTimeline` and the timeline frame reached `TotalFrames` → `ActionState = Actionable`,
   `ActiveTimeline = INDEX_NONE`, event `TimelineFinished`. If the ability of the running entry is no longer active on
   the ASC (cancelled by something else, e.g. a GAS cancel tag or another ability's `InterruptCurrent`) → same, without
   the event.
3. **Adoption** (7.4).
4. **Weapon switch track**: at `SwapFrame` → new `ActiveWeaponIndex`, request `SwapWeaponStats` (Arg = old index),
   buffer event `WeaponSwitched`.
5. **Candidates**: compiled table entries whose source is the kit, the active weapon, or a super slot; filtered by the
   `LoadoutRevision` gate (7.8). Removed and unbound commands have no entries (M2: not candidates).
6. **Can act?** Owner has `Input.Lock.Command` → nothing (M2 6b). `Actionable` → every candidate. `InTimeline` inside
   the **startup override window** (timeline frame < `OverrideFrames`) → candidates matching `OverrideInto` (default:
   higher tier than the running command); the winner **replaces** the running ability: its deferred effects never
   happened, the facing it set is kept. `InTimeline` before `FirstActionableFrame` → candidates matching an open cancel
   window's `IntoCommands`. `InTimeline` at or after `FirstActionableFrame` → every candidate (interruptible recovery).
   Candidates are passed to the M1 resolver, so press groups, tiers, locks (M2 6b) and loser consumption apply
   unchanged.
7. **Resolve** (M1 resolver, `CanExecute` = M2's rule: not locked, a binding holds, ASC `CanActivateAbility` incl.
   cost).
8. **Execute**: a running timeline being replaced or cancelled into another ability → request `CancelTimeline` first.
   Timeline entry → `ActionState = InTimeline`, `ActiveTimeline` = the entry's registry index, `ActiveSource`,
   `TimelineStartFrame = F`, `TimelinePausedFrames = 0`, requests `ActivateEntry` and `SetFacing` (from `AimPolicy`,
   the sample's stick or the camera aim). Plain-ability entry → request `ActivateEntry` only (no timeline state).
9. **Deferred commit**: on the frame the timeline reaches `DeferredCommitFrame`, request `CommitTimeline` (cost commit in
   the ASC pre-tick) and start the tracks. Only committed or completed things become outbox events
   (`TimelineStarted`, `TimelineFinished`, `WeaponSwitched`, `SuperUsed`); a replaced timeline publishes nothing.
10. **Tag projection**: `Combat.State.InTimeline`, `Busy` (frame < FAF), `Tags` track windows, `CancelWindow`,
    `OverrideWindow`, `Falling`.

### 7.3 ASC pre-tick execution

- `ActivateEntry`: spec of the entry's ability class (`FindAbilitySpecFromClass`) → `TryActivateAbility`.
- `CancelTimeline`: cancel the running timeline ability (spec found from the registry entry's class).
- `CancelAbility`: cancel the ability of a registry entry that lost an adoption (7.4).
- `CommitTimeline`: `CommitAbility` on the running timeline ability.
- `SwapWeaponStats`: remove the old weapon's stats GE (handle found by the GE class + source tag, never stored outside
  the ASC state), apply the new one.
- `SetFacing`: queue the facing effect on Mover (instant, rotation only).
- `Cue`: `ExecuteGameplayCue` with the timeline's cue tag.

### 7.4 Adoption

Timeline abilities activated at F-1 without a command (combat.md §11.2) are found and started at F:

1. Skip unless the owner's end-of-F-1 tags contain `Combat.Ability.AwaitingAdoption` (cheap test every frame).
2. Scan the timeline registry in index order for entries whose ability spec is active and awaiting (the ability's
   `bAwaitingAdoption` sync var, read through the spec's instance). The first one wins; the others get a
   `CancelAbility` request.
3. A running timeline still active at this point (its ability did not cancel it: `SkipIfBusy` normally prevents the
   activation, so this is a GAS-tag edge case) → request `CancelTimeline` (the newcomer was allowed to activate by GAS,
   so it wins).
4. Write the timeline state as for a command (7.2 step 8) with `ActiveSource = Adopted`, no `ActivateEntry` request (the
   ability is already active and starts itself in its `OnSimulationTick` at F, 6.2), `SetFacing` per `AimPolicy`.
   Adopted timelines skip the startup override window's replacement by lower tiers (their tier is that of their ability
   tags' command, else none).
5. Buffer event `AbilityActivated`, outbox event `TimelineStarted` (source `Adopted`) at the deferred commit.

### 7.5 `UGSTN_GA_PlatformJump`, weapon switch and other plugin abilities

All plugin abilities are `UGSTN_GA_BaseCombat` children with default timelines that designers override in Blueprint
children.

### 7.6 Movement track

`UGSTN_TimelineMovementMoveCreator` (on a GE with `UApplyMoveGameplayEffectComponent`, duration = track length in ms)
makes `FGSTN_LayeredMove_TimelineMovement { UCurveVector* Curve; int32 ElapsedFrames; float GravityScale; bool
bLockFacing; }` which samples the curve at `ElapsedFrames` (its own counter in Mover's sync state, incremented per
simulated frame, **not** derived from time, so M4's freeze can pause it by not incrementing while the Mover tag
`Combat.Mover.Frozen` is present). Velocity in the character's facing at activation.

### 7.7 Weapon switch

A switch ability is an ordinary timeline ability with a `WeaponSwitch` track, bound in the **kit** bindings to the
switch commands (`Command.Weapon.Switch.*`). Next/previous by default; `Index` for one-command-per-weapon schemes.
Switch-in/out montages via the timeline's montage; per-weapon anim layer linked in `FinalizeFrame` from
`ActiveWeaponIndex` (presentation only).

### 7.8 Loadout revision gate

- The server's `UGSTN_LoadoutComponent` increments `Revision` when it applies a loadout and writes it into the combat
  sync state out-of-band (`NetworkPredictionProxy.WriteSyncState`, as the NP mock does for OOB edits): the client gets
  it with the next correction.
- The compiled table and the registry are rebuilt locally on every machine from the replicated
  `FGSTN_AppliedLoadout { Data; Revision; }` (`OnRep` on clients).
- The combat tick **makes requests and adopts only if `State.LoadoutRevision == LocalTable.Revision`**; otherwise it
  still runs history and buffer (the presses stay buffered until the table arrives). An ability awaiting adoption in
  that time ends itself after 2 frames (6.2).

### 7.9 Jump and movement abilities

- **Sync state**: `EGSTN_JumpPhase JumpPhase` (None / Squat / Rising / Falling), `int32 JumpStartFrame`,
  `int16 JumpPressEvent` (the history button event that started it), `bool bJumpCut`, `uint8 AirJumpsUsed`,
  `bool bFastFalling`.
- **Per frame** (combat tick, after the timeline clock): read Mover's mode and vertical velocity at end of F-1 from the
  tracker. Grounded → `JumpPhase = None`, `AirJumpsUsed = 0`, `bFastFalling = false`. Rising and (the jump press is no
  longer held, or `F - JumpStartFrame ≥ MaxJumpHoldFrames`) and `F - JumpStartFrame ≥ MinJumpHoldFrames` →
  `bJumpCut = true`. Vertical velocity ≤ 0 → `Falling`. Fast-fall command executed while falling → `bFastFalling =
  true`. Airborne without a jump (walked off a ledge) → `Falling`.
- **`UGSTN_GA_PlatformJump`** (plugin, `GSTNCombatNP`, a timeline ability): `Command.Movement.Jump` (`Press(Jump)`) has
  two kit bindings, requirements `Grounded` → ground jump and `Airborne` → air jump (its `CanActivateAbility` also
  checks `AirJumpsUsed < AirJumpCount`). Timeline: `JumpSquatFrames` of startup (phase `Squat`; its startup override
  window covers the whole squat with `OverrideInto` = the up-shaped attack and special commands, as Smash's jump-cancel
  up smash / up special), then at take-off the combat model sets `Rising`, `JumpStartFrame`, `JumpPressEvent`, and
  requests the impulse GE (`FJumpImpulseEffect` through a move creator, upward speed = `JumpVelocity` or
  `AirJumpVelocity`). Air jumps increment `AirJumpsUsed` and reset `bJumpCut` and `bFastFalling`.
- The gravity phase itself is applied by the stats modifier (section 5), which reads `JumpPhase`, `bJumpCut` and
  `bFastFalling` from the combat component's sync state for this frame.
- **Fast fall**: `Command.Movement.FastFall` (default proposal: `Press(Crouch)`; combat.md §11b keeps the input open)
  with one binding, requirements `Airborne` + `Combat.State.Falling`, to a zero-frame timeline ability that sets
  `bFastFalling`.
- **Dash**: `Command.Movement.Dash` on a flick input (`Direction(Stick or Facing, any, MinZone = Tilt, bRequireFlick)`),
  bound with requirements `Grounded` to a timeline ability with a movement track (`MomentumMode`) and a cancel window
  into run/jump; run is a looping timeline while the stick stays tilted (Charge-style track) or plain walking with the
  kit's run speed — decide in implementation by feel.

### 7.10 Camera aim (input side; combat.md §11.6)

- `FGSTN_InputCmd` gains `AimYaw`, `AimPitch` (uint16, 65536 steps). `UGSTN_PlayerInputSource` fills them
  from the control rotation when the input config's `bSendCameraAim` is true; the AI source from its aim target.
  `NetSerialize`: a 1-bit "changed" flag, the two values only when different from the previous command (the NP
  input buffer keeps previous commands; if delta against the previous command is not available there, always send
  them while `bSendCameraAim` is on).
- The component keeps a pointer to the command of the frame being simulated (set at the top of `SimulationTick`,
  cleared after the ASC tick; transient, rewritten every simulated frame, so it is correct in re-simulation too).
  `GetAimRotation()` on the component and on `UGSTN_GA_Base` reads it.
- `AimPolicy = Camera`: `SetFacing` to the aim yaw at the timeline start (and every frame up to `TurnUntilFrame`).
- Aim origin: kit `AimOriginSocket` on the character. Hits (projectile, hitscan `Ray`) are M4.

## 8. Kits, weapons, supers, loadout (data)

**`UGSTN_PrimaryKitDefinition`**: `DisplayName`, `Icon`, `KitTags`; `PrimaryWeapon`; `Stats (FGSTN_KitStats)`;
`InitStatsEffect`; **`CommandSet`** `{ UGSTN_InputSlotSet* BaseSet (empty = settings' `DefaultCommandSet`);
TArray<FGSTN_InputSlotEntry> AddedSlots; FGameplayTagContainer RemovedCommands; }`; `KitBindings
(TArray<FGSTN_AbilityBinding>)`; `TriggeredAbilities (TArray<TSubclassOf<UGameplayAbility>>)` (granted, started by GAS
triggers or other abilities; timeline ones enter the registry); `Passives (TArray<TSubclassOf<UGameplayAbility>>)`;
`Meters (TArray<FGSTN_MeterSetup { FGameplayAttribute Value; FGameplayAttribute Max; float PointsPerBar; int32 Bars; }>)`;
`SuperSlotCount (0..8)`; `SuperInputScheme`; `AllowedAdditionalWeapons (FGameplayTagQuery)`; `MaxAdditionalWeapons
(default 1)`; `AllowedSupers (FGameplayTagQuery)`; `CompatibleSkins (FGameplayTagQuery)`; `AimOriginSocket`;
`HurtboxSet` (M4); `LocomotionAnimLayer`.

**`FGSTN_AbilityBinding`**: `{ FGameplayTag Command (the key); FGSTN_TagRequirements Requirements (combat.md §9.2: e.g.
`Grounded` for the jab, `Airborne` for the neutral air, both on `Command.Attack.Neutral`); FGSTN_InputSlotEntry
SlotOverride (optional: empty = the kit command set's slot for `Command`); TSubclassOf<UGameplayAbility> Ability;
TObjectPtr<UGSTN_CombatTimeline> TimelineOverride (optional, only for `UGSTN_GA_BaseCombat` classes); int32
PriorityOffset; int32 BufferFramesOverride; }` — a command outside the command set needs `SlotOverride`; a binding to a
removed command is a validation error.

**`UGSTN_WeaponDefinition`**: `DisplayName`, `WeaponTags` (incl. role), `ParentWeapon`, `Bindings`,
`AllowedAlternatives (TMap<FGameplayTag Command, TArray<TSubclassOf<UGameplayAbility>>>)`, `StatsEffect` +
`FGSTN_WeaponStatModifiers`, `PassivesWhileActive`, `Tuning`, `Parameters (TMap<FGameplayTag, float>)`,
`AnimLayer`, `VisualActor`. Binding inheritance: parent first, child entries with the same command tag (and the same
requirements) replace.

**`UGSTN_SuperDefinition`**: `Ability` (+ optional `TimelineOverride`), `SuperTags`, `MeterCost`, `InputCode
(FGSTN_InputPattern)`, `Requirements (FGameplayTagRequirements on the owner)`.

**`UGSTN_SuperInputScheme`**: `Mode (FixedSlots/PerSuperCode)`; `FixedSlots (TArray<FGSTN_InputSlotSpec>)` (one per
super slot); `CodeEntry (FGSTN_InputSlotSpec template: the hold/release frame around codes)`; `GrantedWhileEntering
(tag)`. In `PerSuperCode` the compiler builds each super slot as `CodeEntry` steps + the super's `InputCode` steps.

**`FGSTN_LoadoutData`**: `PrimaryKit (FPrimaryAssetId)`, `AdditionalWeapons (TArray<FPrimaryAssetId>)`,
`Supers (TArray<FPrimaryAssetId>, size = SuperSlotCount, empty allowed)`, `SlotCustomisations (TArray<{WeaponIndex,
Command, AlternativeAbility}>)`, `DesignerValues (TMap<FGameplayTag, FGSTN_LoadoutValue>)`, `Skin (FPrimaryAssetId)`.
`NetSerialize` and `Serialize` (save game) both supported.

**`UGSTN_LoadoutRules`** (default implementation, replaceable class in project settings): kit exists; additional
weapons match the kit query and count; supers match the kit query; customisations reference allowed alternatives;
skin compatible; **compiled table has no Ambiguous conflicts** (M1 validator over the kit command set + each weapon +
supers; Shadowed is a warning); every timeline ability the kit can trigger is in the registry. Returns `TArray<FText>`
errors for UI.

## 9. Compiled table and timeline registry (`FGSTN_CompiledBindingTable`, pure, in `GSTNCombatGAS`)

`Compile(const FGSTN_LoadoutData&, const UGSTN_PrimaryKitDefinition&, weapons, supers) → table`:

- **Commands**: the kit's command set (base set − `RemovedCommands` + `AddedSlots`) plus binding `SlotOverride`s and
  super slots.
- **Entries** `{ FGSTN_InputSlotSpec Slot (copied); EGSTN_EntrySource Source (Kit/Weapon/Super); uint8 SourceIndex;
  TSubclassOf<UGameplayAbility> Ability; int16 TimelineIndex (registry, -1 = plain ability); FGSTN_CommandPriority
  Priority; }`. Order: kit entries, then weapon 0..N, then super slots 0..M; inside each by command tag name.
  Customisations replace the ability bound to the command. Commands without any binding produce no entry.
- **Timeline registry** `TArray<{ TSubclassOf<UGSTN_GA_BaseCombat> Ability; const UGSTN_CombatTimeline* Override; }>`:
  every distinct pair from the entries, then the kit's triggered abilities and passives that are timeline abilities
  (no override); sorted by (class path, override path). The combat sync state stores indices into it.
- `Revision`.

The same table feeds the resolver (entries filtered by source per frame), adoption (registry) and the grant list
(unique ability classes of entries, triggered abilities and passives).

## 10. Loadout component, application, save

- `UGSTN_LoadoutComponent` (PlayerState): `ServerRequestLoadout(FGSTN_LoadoutData)` (reliable) → `AcceptLoadout` (resolve, compile,
  rules; a refusal goes back to the client as `OnLoadoutRejected`) → store as `Applied` (replicated with the `Revision`, `OnRep`) →
  `ApplyAppliedLoadout()`: called directly on the server (a listen host never gets its own OnRep) and by the OnRep on a client. It
  resolves the ids once, compiles the table on every machine and, on the server only, grants. A pawn that is not there yet leaves the
  grant pending until `OnPawnSet` (applied one tick later) or a manual `ApplyAppliedLoadout()`.
- **Apply** (server, outside the simulation, at accept and at possession/respawn): remove what the previous apply granted (handles
  kept by the component, server only), kit init GE, primary weapon stats GE, grant the table's ability classes (which include the
  triggered abilities and passives) plus the primary weapon's passives. `Revision` goes up when a loadout is accepted (not again on a
  respawn: a new pawn starts from its own state), is replicated with `Applied`, and is a plain value in this component; the OOB write
  of the revision into the combat sync state (7.8) and setting the combat component's table are the NP adapter's (C8).
- **`UGSTN_LoadoutSubsystem`** (LocalPlayer): `SaveLoadout(PresetName, Data)`, `LoadLoadout(PresetName, Out)`, `DeleteLoadout`,
  `HasLoadout`, `ListLoadouts()`, `RequestLoadout(PresetName, Component)`; `UGSTN_LoadoutSaveGame { TMap<FString, FGSTN_LoadoutData>
  Loadouts; int32 SaveVersion; }`. On load, unknown asset ids are dropped with a warning (content changes between versions), on a copy:
  the file keeps what was saved.
- **Skins**: `UGSTN_SkinComponent` on the pawn applies the loadout's resolved skin (mesh, materials, anim class) on spawn and whenever
  `OnLoadoutApplied` fires (it follows the PlayerState's loadout component); no simulation code reads it, nothing runs on a dedicated server.

## 11. Test content and harness

Test assets under `/Game/GASNPPTest/Combat/` (tell the user before touching `BP_GASNPP_TestCharacter`): kit
`KIT_Test` (stats differing from the default; command set = default set minus one command plus one added command),
primary weapon `WPN_Test_Primary` (jab with a cancel window into a second jab, a dash-like ability with a movement
track; one jab with an inline timeline, one with a shared timeline asset), support weapon `WPN_Test_Support` (+40 % walk
speed, one ability), an EX version of a special (chord, meter cost), two supers with codes, a skin, a test meter-gain GE
(debug only), and for the new activation paths:

- `GA_Test_Fuse` (`UGSTN_GA_Base`, triggered by gameplay event `Test.Event.Arm`): `WaitCombatFrames(250)`, then sends
  `Test.Event.Strike`.
- `GA_Test_AutoStrike` (`UGSTN_GA_BaseCombat`, triggered by `Test.Event.Strike`, `InterruptCurrent`) and
  `GA_Test_AutoStrikeSoft` (same, `SkipIfBusy`), as kit triggered abilities.
- `GA_Test_Stance` (`UGSTN_GA_Base`, bound to a button, `bEndOnTriggerRelease`, owns `Input.Lock.Command.Attack`).
- `GA_Test_AimedShot` (`UGSTN_GA_Base`, `bSendCameraAim` on the test input config): logs and draws its aim (no hit
  until M4).

Scenarios (`gasnpp_test.py`, clean = only forced reconciles, snapshots equal incl. combat state, attributes, active
weapon):

- `scenario_weapon_switch`: walk, switch, walk (speed differs as configured), switch back, forced reconcile across the
  swap frame.
- `scenario_timeline_cancel`: jab, jab within the window (second jab), jab outside the window (no cancel before FAF).
- `scenario_ex_meter`: EX input without meter → normal special; give meter (debug GE on server); EX input → EX
  special, meter spent.
- `scenario_super_code`: enter each super's code; wrong code does nothing; forced reconcile mid-code.
- `scenario_loadout_change`: server applies another loadout between lives; revision gate (no predicted activation or
  adoption until `OnRep`), no reconcile storm.
- `scenario_variable_jump`: jumps holding the button 0, 4, 8, 16 frames and the maximum: peak heights strictly
  increase and match between server and client (within 1 cm); forced reconcile at the apex and right after the
  release; a jump buffered during landing lag comes out on the first actionable frame.
- `scenario_air_jump_fastfall`: ground jump, air jumps up to `AirJumpCount` (the next one refused), fast fall after the
  apex (fall speed reaches `FastFallSpeed`), landing resets the count.
- `scenario_dash`: dash flick, dash cancelled into jump, momentum per `MomentumMode`.
- `scenario_startup_override`: attack then special 1 frame later → the special replaces the attack (no attack cost
  paid, no attack event); attack then jump within the window → short-hop aerial (attack cancel); directional attack
  turning the character, then jump + opposite stick within the window → back aerial with the new facing (instant back
  air); attack then special 3 frames later (outside the window) → no replacement, the special is buffered; attack and
  special within the window with a combo command bound → the combo; jump squat → up-shaped special replaces it; a
  forced reconcile inside each window.
- `scenario_triggered_timeline`: send `Test.Event.Arm` (server debug command and predicted from a test ability); 250
  combat frames later `GA_Test_AutoStrike` is adopted (timeline frame 0 on the next frame), exactly one
  `TimelineStarted` event with source `Adopted`; during a jab it interrupts it, `GA_Test_AutoStrikeSoft` does not
  activate; forced reconciles across the fuse expiry and across the adoption frame; a hitstop window (M0 debug) during
  the fuse delays the strike by its length.
- `scenario_stance_lock`: hold the stance: walking works, attack presses are locked and buffered, dash still works;
  release within the hold buffer while holding attack → the attack comes out; movement lock tag test (a debug GE
  granting `Combat.Lock.Movement`: the character brakes and stays, gravity still applies); forced reconcile while held.
- `scenario_camera_aim`: aimed shot with the camera turned and pitched; aim and facing equal on server and client;
  forced reconcile right after the shot.
- `scenario_command_set`: the removed command's input does nothing and does not reserve the press; the added command
  activates its ability.

Automation tests (Core/GAS, pure): table compile order, command set composition (base − removed + added), registry
order and dedupe, customisation; loadout rules (each rule positive/negative); super code slot building; save game round
trip.

## 12. Acceptance criteria

1. Spikes of section 2 recorded; all scenarios of section 11 clean with 60/15 ms and 2 % loss.
2. M1/M2 tests and scenarios still clean.
3. Speeds with the support weapon match the configured stats on server and client (harness measurement); jump
   heights are continuous in the hold time and identical on server and client.
4. A designer can author a kit (with a changed command set), a weapon, a timeline ability with montage, movement and
   cancel window (inline and with a timeline asset), a triggered timeline ability, a stance with lock tags, an EX
   ability and a super without code (verify by building the test content from the details panels and Blueprint
   class defaults only).
5. combat.md updated (stats, adoption and revision sections reflect what was built).

## 13. Risks

- Shared settings writes (section 2.2) — the main technical risk of the stats design.
- Triggers in the fork (section 2.3): if unsupported, the automatic-activation path needs a fork patch first.
- Root motion from the synced montage and the movement track on the same frames: decide per timeline with `MixMode`;
  validation warns when both are present without an explicit mode.
- Weapon stat GEs found "by class + source tag" must be unique per weapon; validation enforces one stats GE class per
  weapon.
- The fork's runtime grant path (section 2.1) may need its own fix in the fork; budget it. Spec count (one per ability
  class, ~40 per fighter) also weighs on the ASC sync state size: measure it in spike 1.
- Adoption reads ability instances' sync vars from the combat tick (end-of-F-1 state, like `CanActivateAbility` in M2):
  keep the read const and on instanced-per-actor abilities only.
