# Combat M3 — Moves, kits, weapons, meters, supers, loadouts, jump (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §5, §10, §11 there, and the M1/M2 specifications. Section
numbers below are local to this file.

## 1. Scope

**Goal:** a character spawns with a loadout (primary kit + support weapon + supers + skin). Its stats come from the kit
and change with the active weapon; slots of the kit, the active weapon and the super slots execute **moves** with frame
data, synced montages, movement and cancel windows; EX moves and supers spend meter; switching weapon is a predicted
move. The character jumps with a **variable-height jump** (independent rise/fall gravity), air jumps, fast falls and
dashes as kit moves (combat.md §11b). All rollback-safe. No hits yet (M4): hitbox tracks exist only as data; dodges
come with intangibility in M4.

Out: hit detection and everything in combat.md §12 (M4); the move editor (M5, M3 authors moves in the details panel);
defensive moves (M6).

## 2. Start with two spikes (half a day each, before the rest)

1. **Runtime grants**: granting abilities and applying infinite GEs on the server *outside* the simulation (loadout
   application) — confirm how the fork brings them to the owning client (sync state correction vs replication), that
   no reconcile storm follows, and that `FindAbilitySpecFromClass` works on the client afterwards.
2. **Stats modifier**: a permanent Mover movement modifier rewriting `UCommonLegacyMovementSettings::MaxSpeed` from an
   attribute in `OnPreMovement`, and Mover's gravity override (`SetGravityOverride`) from a gravity scale; an infinite GE
   changes the attributes; forced reconciles across the change and across a jump's apex; server and client positions
   equal. If the shared settings object turns out to be shared between Mover instances, fall back to
   per-mode `MaxSpeedOverride` (exists on `SimpleWalkingMode`) or project-side mode subclasses.

Record the results in combat.md §17 before continuing.

## 3. Module `GSTNCombatGAS` (new)

Depends on Core, GameplayAbilities, GameplayTags, GameplayTasks (no NP, no Mover): data assets and GAS types usable by
any GAS project.

```
Public/
  GSTN_CombatTags.h                 native tags (section 4)
  Attributes/GSTN_MovementAttributeSet.h
  Attributes/GSTN_MeterAttributeSet.h
  Effects/GSTN_GE_MeterCost.h       instant cost GE (SetByCaller)
  Effects/GSTN_GE_WeaponStats.h     base class for weapon stat GEs (infinite)
  Moves/GSTN_MoveDefinition.h       UGSTN_MoveDefinition + track structs (section 6)
  Kits/GSTN_PrimaryKitDefinition.h
  Kits/GSTN_WeaponDefinition.h
  Kits/GSTN_SuperDefinition.h
  Kits/GSTN_SuperInputScheme.h
  Kits/GSTN_SkinDefinition.h
  Loadout/GSTN_LoadoutTypes.h       FGSTN_LoadoutData, FGSTN_LoadoutValue, FGSTN_SlotCustomisation
  Loadout/GSTN_LoadoutDefinition.h  designer presets
  Loadout/GSTN_LoadoutRules.h       validation (UObject, replaceable per game)
  Loadout/GSTN_CompiledMoveTable.h  FGSTN_CompiledMoveTable + compiler (pure)
  Loadout/GSTN_LoadoutComponent.h   PlayerState component
  Loadout/GSTN_LoadoutSubsystem.h   LocalPlayer subsystem + UGSTN_LoadoutSaveGame
  Cosmetics/GSTN_SkinComponent.h    applies skins (outside the simulation)
```

`GSTNCombatNP` gains: `UGSTN_GA_Move`, `FGSTN_StatsMovementModifier`, `FGSTN_LayeredMove_MoveTrack`,
`UGSTN_MoveTrackMoveCreator`, `FGSTN_SetFacingEffect` (if Mover has no rotation-only instant effect), and the combat
tick logic of section 7.

## 4. Native tags (`GSTN_CombatTags.h`, `UE_DECLARE_GAMEPLAY_TAG_EXTERN`)

`Combat.State.Busy` (in a move before its first actionable frame), `Combat.State.InMove`, `Combat.State.SuperInput`,
`Combat.State.CancelWindow` (+ `.Category.<X>` children generated from categories are **not** native: cancel windows
are evaluated in the combat tick, the tag is only informative); `Combat.Slot.Category.Normal/Special/Aerial/Defensive/
Super/WeaponSwitch/Jump/Movement`; `Combat.Weapon.Role.Primary/Support`; `Combat.SetByCaller.MeterCost`, `Combat.SetByCaller.Stat.*`
(one per movement attribute); `Combat.BufferEvent.MoveActivated/Landed/WeaponSwitched`. One-line comment each on who
grants/reads it.

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
active, removed when it stops (section 7.6). The primary weapon's is applied at spawn.

**`FGSTN_StatsMovementModifier`** (`GSTNCombatNP`, `FMovementModifierBase`): queued once by the combat component at
BeginPlay on the authority and the autonomous client (it lives in Mover's sync state, so a correction restores it).
`OnPreMovement`: read the owner ASC's current movement attributes, write `UCommonLegacyMovementSettings` (`MaxSpeed`,
`Acceleration`, `Deceleration`, `GroundFriction`, `AirControl`, the terminal fall speed) and **the gravity override**:
world gravity × the scale of the current jump phase, read from the combat sync state of the same frame (7.8) —
`JumpRiseGravityScale` while rising with the jump held (within `MaxJumpHoldFrames`), `JumpCutGravityScale` while rising
after release, `FallGravityScale` while descending, `FastFallGravityScale` and `FastFallSpeed` while fast falling.
Never ends. Also used outside jumps (falling off a ledge = fall phase).

**Jump**: combat characters use the plugin's `UGSTN_GA_PlatformJump` (7.8). The project's
`UGSTN_JumpImpulseMoveCreator` also reads `JumpVelocity` when the attribute set is present (fallback to its current
tuning), so the project jump and the plugin jump agree on height.

## 6. Move definition (`UGSTN_MoveDefinition`, `GSTN_Move`)

| Field | Notes |
|---|---|
| `TotalFrames`, `FirstActionableFrame` | Frames at the NPP tick rate. Validation: FAF ≤ Total. |
| `Montage`, `MontageSection`, `MontagePlayRate` | Played with `PlaySyncedMontage` on activation. `IsDataValid` warns if the montage length (at the play rate) ≠ `TotalFrames` ± 1 frame. |
| `AbilityClass` | Default `UGSTN_GA_Move`. |
| `CostEffect` + `MeterCosts` | `UGSTN_GE_MeterCost` with SetByCaller per meter attribute; any other GAS cost/cooldown as usual on the ability class. |
| `AimPolicy` | `KeepFacing / SnapToStick / SnapIfAimOrTilt`, `MaxTurnDegrees`, `TurnUntilFrame`. |
| `Tracks` | `TArray<TInstancedStruct<FGSTN_MoveTrack>>`. |
| `Categories` | Tags the move counts as for cancel rules (`Combat.Slot.Category.*`); default from its slot. |
| `BufferResetRules` | Appended while the move runs. |

Track structs in M3 (base `FGSTN_MoveTrack { int32 StartFrame; int32 EndFrame; }`):

- `CancelWindow { FGameplayTagContainer IntoCategories; TArray<FName> IntoSlots; ECancelCondition Condition
  (Always/OnHit/OnBlock/OnWhiff); bool bJumpCancel; }` — OnHit/OnBlock evaluate false until M4 (hit confirm).
- `Movement { UCurveVector* VelocityCurve (local space, cm/s over move frames); float GravityScale; bool bLockFacing;
  EGSTN_MoveMixMode MixMode (Additive/Override) }`.
- `Tags { FGameplayTagContainer Tags; }` — projected by the combat model while inside the window (combat.md §5.4).
- `Event { FGameplayTag CueTag; FGameplayTag GameplayEventTag; }` at `StartFrame` (cue through the ASC request path).
- `WeaponSwitch { int32 SwapFrame; EGSTN_SwitchTarget Target (Next/Previous/Index); int32 Index; }` — only on weapon
  switch moves.
- `Hitbox`, `HurtboxModifier` — **declared** with their M4 fields so assets can be authored, ignored at runtime in M3.

Track lookup at runtime must be O(tracks) per frame without allocation (prebuilt per-move frame index optional).

## 7. Combat model changes (`GSTNCombatNP`)

### 7.1 Sync state additions

| Field | Purpose |
|---|---|
| `EGSTN_ActionState ActionState` | `Actionable`, `InMove` (M4 adds hitstun etc.). |
| `int16 ActiveTableIndex` | Entry of the compiled table being executed, `INDEX_NONE` when none. |
| `int32 MoveStartFrame`, `int32 MovePausedFrames` | Move frame = `F - MoveStartFrame - MovePausedFrames` (paused frames grow in hitstop, M4). |
| `uint8 ActiveWeaponIndex` | 0 = primary, 1.. = additional weapons in loadout order. |
| `uint16 LoadoutRevision` | Written only by the server, out-of-band (section 7.7). |
| `FGSTN_CombatRequests Requests` | Fixed array (4) of `{ EGSTN_RequestType Type; int32 Frame; int16 Arg; }`: ActivateEntry, EndMove, SwapWeaponStats, SetFacing, Cue. Replaces M2's single request. |

All in `NetSerialize`/`ShouldReconcile`/`ToString`/`Interpolate`; requests serialized only when `Frame` equals the
state's frame (older ones are dead).

### 7.2 Per-frame order (extends M2 §5)

1. M2 steps 1–4 (sample, history, watched tags, reset rules — plus the active move's reset rules).
2. **Move clock**: if `InMove` and the move frame reached `TotalFrames` → `ActionState = Actionable`,
   `ActiveTableIndex = INDEX_NONE`, request `EndMove`. If the GA_Move of this entry is no longer active on the ASC
   (cancelled by something else, e.g. a GAS cancel tag) → same, without request.
3. **Weapon switch track**: at `SwapFrame` → new `ActiveWeaponIndex`, request `SwapWeaponStats` (Arg = old index),
   buffer event `WeaponSwitched`.
4. **Candidates**: compiled table entries whose source is the kit, the active weapon, or a super slot; filtered by
   `LoadoutRevision` gate (section 7.7).
5. **Can act?** `Actionable` → every candidate. `InMove` before `FirstActionableFrame` → only candidates allowed by a
   cancel window open at the current move frame (`IntoCategories` ∩ entry categories, or `IntoSlots` contains the
   slot name; `bJumpCancel` allows entries in `Combat.Slot.Category.Jump`). `InMove` at or after
   `FirstActionableFrame` → every candidate (the remaining frames are interruptible recovery; the new move replaces
   the old one).
6. **Resolve** (M1 resolver, `CanExecute` = ASC `CanActivateAbility` of the entry's ability incl. cost).
7. **Execute**: for a move entry → `ActionState = InMove`, `ActiveTableIndex`, `MoveStartFrame = F`,
   `MovePausedFrames = 0`, requests `ActivateEntry` and `SetFacing` (from `AimPolicy` and the sample's stick); for a
   plain-ability entry → request `ActivateEntry` only (no move state).
8. **Tag projection**: `Combat.State.InMove`, `Busy` (frame < FAF), move `Tags` windows, `CancelWindow`.

### 7.3 ASC pre-tick execution

- `ActivateEntry`: spec of the entry's ability class → `TryActivateAbility`. `UGSTN_GA_Move` has
  `bRetriggerInstancedAbility = true` so move → move cancels re-activate the same instance.
- `EndMove`: nothing to do if GA_Move ends itself (7.4); kept for plain abilities flagged "end with move" later.
- `SwapWeaponStats`: remove the old weapon's stats GE (handle found by the GE class + source tag, never stored outside
  the ASC state), apply the new one.
- `SetFacing`: queue the facing effect on Mover (instant, rotation only).
- `Cue`: `ExecuteGameplayCue` with the move's cue tag.

### 7.4 `UGSTN_GA_Move`

- `ActivateAbility`: read the active entry from the combat component (state written this frame at 47), commit (cost),
  `PlaySyncedMontage` with the move's montage/section/rate, apply the movement track GE (7.5) if any.
- `OnSimulationTick`: end itself (`EndAbility`) when the combat state's `ActiveTableIndex`/`MoveStartFrame` no longer
  match what it activated with (both stored as sync vars `FSyncInteger`).
- `EndAbility`: stop the synced montage if it is still the move's.
- Ability tags: `Combat.Move`; designers add block/cancel tags on Blueprint children as usual.

### 7.5 Movement track

`UGSTN_MoveTrackMoveCreator` (on a GE with `UApplyMoveGameplayEffectComponent`, duration = track length in ms) makes
`FGSTN_LayeredMove_MoveTrack { UCurveVector* Curve; int32 ElapsedFrames; float GravityScale; bool bLockFacing; }`
which samples the curve at `ElapsedFrames` (its own counter in Mover's sync state, incremented per simulated frame,
**not** derived from time, so M4's freeze can pause it by not incrementing while the Mover tag
`Combat.Mover.Frozen` is present). Velocity in the character's facing at activation.

### 7.6 Weapon switch

A switch move is an ordinary move with a `WeaponSwitch` track, bound in the **kit** move set to switch slots
(`Combat.Slot.Category.WeaponSwitch`). Next/previous by default; `Index` for one-slot-per-weapon schemes. Switch-in/out
montages via the move's montage; per-weapon anim layer linked in `FinalizeFrame` from `ActiveWeaponIndex`
(presentation only).

### 7.7 Loadout revision gate

- The server's `UGSTN_LoadoutComponent` increments `Revision` when it applies a loadout and writes it into the combat
  sync state out-of-band (`NetworkPredictionProxy.WriteSyncState`, as the NP mock does for OOB edits): the client gets
  it with the next correction.
- The compiled table is rebuilt locally on every machine from the replicated `FGSTN_AppliedLoadout { Data; Revision; }`
  (`OnRep` on clients).
- The combat tick **makes requests only if `State.LoadoutRevision == LocalTable.Revision`**; otherwise it still runs
  history and buffer (the presses stay buffered until the table arrives).

### 7.8 Jump and movement moves

- **Sync state**: `EGSTN_JumpPhase JumpPhase` (None / Squat / Rising / Falling), `int32 JumpStartFrame`,
  `int16 JumpPressEvent` (the history button event that started it), `bool bJumpCut`, `uint8 AirJumpsUsed`,
  `bool bFastFalling`.
- **Per frame** (combat tick, after the move clock): read Mover's mode and vertical velocity at end of F-1 from the
  tracker. Grounded → `JumpPhase = None`, `AirJumpsUsed = 0`, `bFastFalling = false`. Rising and (the jump press is no
  longer held, or `F - JumpStartFrame ≥ MaxJumpHoldFrames`) and `F - JumpStartFrame ≥ MinJumpHoldFrames` →
  `bJumpCut = true`. Vertical velocity ≤ 0 → `Falling`. Fast-fall slot matched while falling → `bFastFalling = true`.
  Airborne without a jump (walked off a ledge) → `Falling`.
- **`UGSTN_GA_PlatformJump`** (plugin, `GSTNCombatNP`): bound in the kit move set to the jump slot (`Press(Jump)`,
  grounded context) and to the air jump slot (`Press(Jump)`, airborne context, requires `AirJumpsUsed <
  AirJumpCount`). It is a move: `JumpSquatFrames` of startup (phase `Squat`, cancellable into specified categories),
  then at take-off the combat model sets `Rising`, `JumpStartFrame`, `JumpPressEvent`, and requests the impulse GE
  (`FJumpImpulseEffect` through a move creator, upward speed = `JumpVelocity` or `AirJumpVelocity`). Air jumps
  increment `AirJumpsUsed` and reset `bJumpCut` and `bFastFalling`.
- The gravity phase itself is applied by the stats modifier (section 5), which reads `JumpPhase`, `bJumpCut` and
  `bFastFalling` from the combat component's sync state for this frame.
- **Fast fall**: a kit slot (default proposal: `Press(Crouch)` with an airborne context and `JumpPhase = Falling`;
  combat.md §11b keeps the input open) bound to a zero-frame move that sets `bFastFalling`.
- **Dash**: a kit move on a flick slot (`Direction(Stick or Facing, any, MinZone = Tilt, bRequireFlick)`, grounded)
  with a movement track (`MomentumMode`, see 6) and a cancel window into run/jump; run is a looping move while the
  stick stays tilted (Charge-style track) or plain walking with the kit's run speed — decide in implementation by feel.
- Movement track options added for movement moves: `MomentumMode` (Keep / Reset / Blend over N frames),
  `bCancelOnLand`, `LandingLagFrames`.

## 8. Kits, weapons, supers, loadout (data)

**`UGSTN_PrimaryKitDefinition`**: `DisplayName`, `Icon`, `KitTags`; `PrimaryWeapon`; `Stats (FGSTN_KitStats)`;
`InitStatsEffect`; `KitMoveSet (TArray<FGSTN_MoveBinding>)`; `Passives (TArray<TSubclassOf<UGameplayAbility>>)`;
`Meters (TArray<FGSTN_MeterSetup { FGameplayAttribute Value; FGameplayAttribute Max; float PointsPerBar; int32 Bars; }>)`;
`SuperSlotCount (0..8)`; `SuperInputScheme`; `AllowedAdditionalWeapons (FGameplayTagQuery)`; `MaxAdditionalWeapons
(default 1)`; `AllowedSupers (FGameplayTagQuery)`; `CompatibleSkins (FGameplayTagQuery)`; `HurtboxSet` (M4);
`LocomotionAnimLayer`.

**`FGSTN_MoveBinding`**: `{ FGSTN_InputSlotEntry Slot; TObjectPtr<UGSTN_MoveDefinition> Move; TSubclassOf<
UGameplayAbility> Ability (if no move); int32 PriorityOverride; int32 BufferFramesOverride; }` — exactly one of
`Move`/`Ability`.

**`UGSTN_WeaponDefinition`**: `DisplayName`, `WeaponTags` (incl. role), `ParentWeapon`, `MoveSet`,
`AllowedAlternatives (TMap<FName SlotName, TArray<UGSTN_MoveDefinition*>>)`, `StatsEffect` +
`FGSTN_WeaponStatModifiers`, `PassivesWhileActive`, `Tuning`, `Parameters (TMap<FGameplayTag, float>)`,
`AnimLayer`, `VisualActor`. Move set inheritance: parent first, child entries with the same slot name replace.

**`UGSTN_SuperDefinition`**: `Move`, `SuperTags`, `MeterCost`, `InputCode (FGSTN_InputPattern)`, `Requirements
(FGameplayTagRequirements on the owner)`.

**`UGSTN_SuperInputScheme`**: `Mode (FixedSlots/PerSuperCode)`; `FixedSlots (TArray<FGSTN_InputSlotSpec>)` (one per
super slot); `CodeEntry (FGSTN_InputSlotSpec template: the hold/release frame around codes)`; `GrantedWhileEntering
(tag)`. In `PerSuperCode` the compiler builds each super slot as `CodeEntry` steps + the super's `InputCode` steps.

**`FGSTN_LoadoutData`**: `PrimaryKit (FPrimaryAssetId)`, `AdditionalWeapons (TArray<FPrimaryAssetId>)`,
`Supers (TArray<FPrimaryAssetId>, size = SuperSlotCount, empty allowed)`, `SlotCustomisations (TArray<{WeaponIndex,
SlotName, AlternativeMove}>)`, `DesignerValues (TMap<FGameplayTag, FGSTN_LoadoutValue>)`, `Skin (FPrimaryAssetId)`.
`NetSerialize` and `Serialize` (save game) both supported.

**`UGSTN_LoadoutRules`** (default implementation, replaceable class in project settings): kit exists; additional
weapons match the kit query and count; supers match the kit query; customisations reference allowed alternatives;
skin compatible; **compiled table has no Ambiguous conflicts** (M1 validator over kit + each weapon + supers; Shadowed
is a warning). Returns `TArray<FText>` errors for UI.

## 9. Compiled table (`FGSTN_CompiledMoveTable`, pure, in `GSTNCombatGAS`)

`Compile(const FGSTN_LoadoutData&, const UGSTN_PrimaryKitDefinition&, weapons, supers) → table` with entries
`{ FGSTN_InputSlotSpec Slot (copied); EGSTN_EntrySource Source (Kit/Weapon/Super); uint8 SourceIndex; Move or Ability;
int32 EffectivePriority; FGameplayTagContainer Categories; }` and `Revision`. Order: kit entries, then weapon 0..N,
then super slots 0..M; inside each by slot name. Customisations replace the move of the named slot. The same table
feeds the resolver (entries filtered by source per frame) and the grant list (unique ability classes).

## 10. Loadout component, application, save

- `UGSTN_LoadoutComponent` (PlayerState): `ServerRequestLoadout(FGSTN_LoadoutData)` (reliable) → rules → store as
  `Applied` (replicated, `OnRep`) → if the pawn is alive and the game allows it, apply now, else on next spawn.
- **Apply** (server, outside the simulation, at possession/respawn): remove previous grants (handles kept by the
  component, server only), grant the table's ability classes, passives, kit init GE, primary weapon stats GE,
  `Revision++`, OOB write of the revision (7.7), set the combat component's table.
- **`UGSTN_LoadoutSubsystem`** (LocalPlayer): `SaveLoadout(SlotName, Data)`, `LoadLoadout(SlotName)`,
  `ListLoadouts()`; `UGSTN_LoadoutSaveGame { TMap<FString, FGSTN_LoadoutData> Loadouts; int32 Version; }`. On load,
  unknown asset ids are dropped with a warning (content changes between versions).
- **Skins**: `UGSTN_SkinComponent` on the pawn applies `Applied.Skin` (mesh, materials, anim class) on spawn and
  `OnRep`; no simulation code reads it.

## 11. Test content and harness

Test assets under `/Game/GASNPPTest/Combat/` (tell the user before touching `BP_GASNPP_TestCharacter`): kit
`KIT_Test` (stats differing from the default), primary weapon `WPN_Test_Primary` (jab with a cancel window into a
second jab, a dash-like move with a movement track), support weapon `WPN_Test_Support` (+40 % walk speed, one move),
an EX version of a special (chord, meter cost), two supers with codes, a skin, a test meter-gain GE (debug only).

Scenarios (`gasnpp_test.py`, clean = only forced reconciles, snapshots equal incl. combat state, attributes, active
weapon):

- `scenario_weapon_switch`: walk, switch, walk (speed differs as configured), switch back, forced reconcile across the
  swap frame.
- `scenario_move_cancel`: jab, jab within the window (second jab), jab outside the window (no cancel before FAF).
- `scenario_ex_meter`: EX input without meter → normal special; give meter (debug GE on server); EX input → EX
  special, meter spent.
- `scenario_super_code`: enter each super's code; wrong code does nothing; forced reconcile mid-code.
- `scenario_loadout_change`: server applies another loadout between lives; revision gate (no predicted activation
  until `OnRep`), no reconcile storm.
- `scenario_variable_jump`: jumps holding the button 0, 4, 8, 16 frames and the maximum: peak heights strictly
  increase and match between server and client (within 1 cm); forced reconcile at the apex and right after the
  release; a jump buffered during landing lag comes out on the first actionable frame.
- `scenario_air_jump_fastfall`: ground jump, air jumps up to `AirJumpCount` (the next one refused), fast fall after the
  apex (fall speed reaches `FastFallSpeed`), landing resets the count.
- `scenario_dash`: dash flick, dash cancelled into jump, momentum per `MomentumMode`.

Automation tests (Core/GAS, pure): table compile order and customisation; loadout rules (each rule positive/negative);
super code slot building; save game round trip.

## 12. Acceptance criteria

1. Spikes of section 2 recorded; all scenarios of section 11 clean with 60/15 ms and 2 % loss.
2. M1/M2 tests and scenarios still clean.
3. Speeds with the support weapon match the configured stats on server and client (harness measurement); jump
   heights are continuous in the hold time and identical on server and client.
4. A designer can author a kit, a weapon, a move with montage, movement and cancel window, an EX move and a super
   without code (verify by building the test content from the details panels only).
5. combat.md updated (stats and revision sections reflect what was built).

## 13. Risks

- Shared settings writes (section 2.2) — the main technical risk of the stats design.
- Root motion from the synced montage and the movement track on the same frames: decide per move with `MixMode`;
  validation warns when both are present without an explicit mode.
- Weapon stat GEs found "by class + source tag" must be unique per weapon; validation enforces one stats GE class per
  weapon.
- The fork's runtime grant path (section 2.1) may need its own fix in the fork; budget it.
