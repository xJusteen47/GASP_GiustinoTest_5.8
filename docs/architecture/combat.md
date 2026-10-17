# Combat system (`GSTNCombat` plugin)

Design document for a fighting-game / platform-fighter combat layer on top of GAS, simulated by Network Prediction
(the GAS-NPP fork, see [gas-npp.md](gas-npp.md)). Source: `Plugins/GSTNCombat/`.

Status (2026-10-17): **M0 (risk spike) done and verified**, M1 specified at the end of this document, everything else
is design. Sections marked *Decision* are settled; *Open* items are listed where they apply.

## 1. Goals

- Fighting-game input: designer-defined combinations (button, direction, motion, chord, charge, hold, release, mash),
  a **time buffer** and a **hold buffer** (a press stays valid while the button is held until the move can come out, as
  in Super Smash Bros. Ultimate), standard buffer resets plus designer-defined ones.
- **Weapons** made of modular **moves** bound to **input slots** (e.g. Attack + Back). New slots for non-standard
  combinations are created without code and are checked against the other slots for conflicts.
- **Loadouts**: several weapons, customisations and designer values, saved and replicated.
- Platform-fighter rules in 3D: hitstun, hitstop, knockback, DI on the movement plane, SDI, tech, shield, parry, grab.
  Also PvE, so these get tuned for 3D and for many enemies.
- Hitboxes that do not depend only on the skeleton, authored easily by designers.
- GAS best practices, easy replication, a reusable plugin, modular and extensible.

*Decisions taken with the user:* engine-agnostic **Core** + **NPP adapter** (a stock-GAS adapter may come later);
hitboxes on a **frame-data timeline in the move asset**; the time unit is the **NPP fixed-tick frame**; knockback model
**pluggable, Smash-like by default**; players and AI use **the same pipeline**; **no lock-on**; friendly fire per game
mode, with **per-relationship hit variants**; scale target **40+ enemies** at once.

## 2. Constraints from the fork (verified in code)

| # | Fact | Consequence |
|---|---|---|
| 1 | `FAbilitySimSyncState` is hard-coded; `InternalSimulationTick`/`FillSyncState` are not virtual. | Combat state lives in **its own NP model** on a component. |
| 2 | Sort priorities: ASC = `PreKinematicMovers - 2` (48), Mover = 50. | Combat model at **47**: before the ASC and Mover. |
| 3 | Fork input: 5 bools per `UInputAction`, no axes, no history; `TryActivateAbilitiesFromInput` tries only the first matching ability; `GetSynced*` getters read the wrong frame during resim. | Combat has **its own input command**; moves never use `ActivationInputs`. |
| 4 | GE durations and `UWaitForDurationPredictionTask` use absolute sim time; NPP has no per-entity freeze. | **Every combat timing is a frame count in the combat sync state** (pauses in hitstop). GEs only for attributes, tags, cues. |
| 5 | Nothing runs on simulated proxies (`SimulatedProxyNetworkLOD=Interpolated`): `ApplyGameplayEffectSpecToSelf` and `TryActivateAbility` return early. | Victim reactions are server-authoritative; the attacker predicts only its own side. |
| 6 | Predicted montages exist (`UNetMontageSimulator`, `PlaySyncedMontage`, `PauseCurrentSyncedMontage`); root motion goes to Mover through `FLayeredMove_SyncMontageRootMotion`. | Moves play synced montages; hitstop pauses them. |
| 7 | Lag compensation exists: `UNetworkPredictionLagCompensation` + `UNetworkPredictionWorldManager::RewindActors`. | Used for server-side hit queries from remote clients. |
| 8 | All fixed-tick models are restored and re-ticked together on rollback, in priority order then NP id order. | A model at 47 can drive the ASC at 48 consistently. |
| 9 | Loose gameplay tags are in the ASC sync state and rolled back (`RestoreTags`). | Combat can project its state as loose tags (rules in 5.4). |
| 10 | Interpolated simulated proxies get only `FinalizeFrame` with interpolated states; they never tick. | Hit inboxes are only drained where the instance ticks. |

**Fork patches made for this system** (all committed, see section 17): rollback refreshes the state view of every
instance on the first resim step; `UNpAbilitySystemComponent::OnSimulationPreTick`; the montage pause flag is ignored
by `ShouldReconcile` when no montage plays.

## 3. Modules

`Plugins/GSTNCombat/` (type prefix `GSTN_`, like the project).

| Module | Depends on | Contents |
|---|---|---|
| `GSTNCombatCore` (Runtime) | Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, EnhancedInput (only for `UInputAction` references) | Everything **pure and deterministic**: input samples, history, direction classifier, conditions, matcher, buffer, reset rules, slots and validation; later move definitions, tracks, hit shapes, knockback models. No GAS, no NPP. Covered by Automation tests. |
| `GSTNCombatGAS` (Runtime) | Core, GameplayAbilities, GameplayTasks | Native tags, `UGSTN_CombatAttributeSet`, damage GE and execution, hit effect context, weapons, loadouts, loadout component. Compiles against stock GAS and the fork (same module name). |
| `GSTNCombatNP` (Runtime, **exists**) | Core, GAS, NetworkPrediction, AbilitySystemSimulation, Mover, EnhancedInput | **Fork adapter**: combat NP model, simulation component, input production, `UGSTN_GA_Move`, freeze/knockback layered moves, hit resolution. |
| `GSTNCombatEditor` (Editor) | all | Move asset editor (timeline + viewport), socket baking, slot conflict validator UI, debug draw. |

A future `GSTNCombatStockGAS` would replace only `GSTNCombatNP` (same data, simulation through prediction keys).

*Content versioning:* `.gitignore` ignores `Plugins/**/Content/`. When the plugin ships default assets (standard slot
set, input config), add `!Plugins/GSTNCombat/Content/` and its LFS rules first.

## 4. Glossary

| Term | Meaning |
|---|---|
| Button | Logical combat button. Index 0..31 in the input config; bit *i* of the input masks. |
| Sample | What the combat tick sees for one frame: held/pressed/released masks and the classified stick. |
| History | Compact, rolled-back log of recent button and direction events (sync state). |
| Condition | One test on the history (press, direction, motion...). |
| Pattern | Ordered steps of conditions = one input combination. |
| Slot | Pattern + context + priority + categories. Identifies "which input" (e.g. *Attack + Back, grounded*). |
| Binding | Slot → move or any GAS ability, inside a weapon or a character move set. |
| Move | Data asset with frame data and tracks, executed by a generic GAS ability. |
| Weapon | Set of bindings, passives, tuning, visuals. |
| Loadout | Weapons + customisations + designer values. |

## 5. Simulation architecture

### 5.1 Models and tick order

Per fixed-tick frame F, for every entity: **combat model (47)** → **ASC (48)** → **Mover (50)**. On rollback all
three are restored to the same frame and re-ticked in that order (fact 8).

### 5.2 Communication rule (*Decision*, verified in M0)

- The combat tick reads only: its input command, its own input sync state, and **end-of-F-1 state** of the ASC and
  Mover (through a per-frame state reader, never Mover component queries; see gas-npp.md rule 1).
- It writes **only its own sync state**. Anything it needs from the ASC or Mover is a **frame-stamped request** in that
  state (`RequestFrame == F`).
- `UGSTN_CombatSimComponent` executes the requests from `UNpAbilitySystemComponent::OnSimulationPreTick`, i.e. inside
  the ASC's own tick, before input events and abilities: activations, cancels, montage pause/resume, applying GEs
  (including GEs with `UApplyMoveGameplayEffectComponent`, which is how Mover gets freeze and knockback moves: rule 2 of
  gas-npp.md holds).
- A request runs only on its frame, so a re-simulation runs it again exactly once and a restored older state never
  re-triggers it.
- Results come back one frame later through state the combat tick reads (ability active or not, tags, Mover mode).
  The combat tick may pre-check `CanActivateAbility` (const) to choose among candidates; the ASC has the final word.
- The always-active "driver ability" considered in the review is **not needed**: the pre-tick hook replaced it.

### 5.3 Reading other entities

Other entities' combat state (intangibility, shield, hurtboxes) depends on NP id order inside the frame (F or F-1).
*Decision:* the first combat tick of each (frame, resim pass) builds a **per-step snapshot** of what other entities
expose (transform, hurtbox state, team, relationship data) from their end-of-F-1 state; all hit logic reads the
snapshot, never live components. `RewindActors` (lag compensation) is undone before the combat tick returns.

### 5.4 Tag projection

Each combat tick computes the combat state tags (`Combat.State.Hitstun`, `.Busy`, `.Intangible`, `.CancelWindow.*`...)
from its sync state and requests them as **absolute counts** (`SetLooseGameplayTagCount`) on a `Combat.State.*`
namespace that nothing else grants. Rules: no `AbilityTriggers` or tag-event listeners on these tags (the fork's restore
re-fires tag delegates and `MonitoredTagChanged` is not guarded); cancels are explicit requests, not
`CancelAbilitiesWithTag` reactions to these tags. Other abilities may use them in `ActivationBlockedTags`.

### 5.5 Rules for combat simulation code (extend gas-npp.md)

1. All timers are frame stamps in combat sync state (`EndFrame`/`StartFrame`, plus paused-frame counters for hitstop),
   never GE durations or wait tasks.
2. No `HasAuthority`/resim branches; server-only behaviour comes from where the instance ticks (inbox, section 12.4).
3. Prefer **end-frame stamps over countdowns**: an idle entity's state then does not change frame to frame (cheap
   delta serialization, section 15).
4. Every sync state field is in `NetSerialize`, `ShouldReconcile`, `ToString` and `Interpolate`.

## 6. Input

### 6.1 Input command (`FGSTN_CombatInputCmd`, NP model input)

| Field | Type | Notes |
|---|---|---|
| `HeldButtons` | uint32 | Held at sample time. |
| `PressedButtons` | uint32 | Pressed since the previous sample: a tap shorter than one frame is not lost. |
| `StickX`, `StickY` | int8 | Move stick **already rotated onto the movement plane** (camera yaw applied at production, world XY), quantized to [-127, 127]. *(M2)* |
| `ForcedSlot` | uint16 | 0 = none. Lets AI/scripts request a slot without synthesising inputs. *(M2)* |

Exists today with the first two fields (M0). Serialized packed.

### 6.2 Input sources

`IGSTN_CombatInputSource` produces the command on the controlling machine: `UGSTN_PlayerCombatInputSource` (Enhanced
Input; buttons are the `UInputAction`s of the input config; taps caught through the action's Started event, not by
polling) and `UGSTN_AICombatInputSource` (BT/StateTree tasks: Press, Hold, Release, PerformSlot via `ForcedSlot`).
Test scripting (`SetScriptedButtons`, `TapScriptedButtons`) exists on the component today.

### 6.3 Directions in 3D (*Decision*)

- Default direction of a move is the **character's facing**; the stick on the movement plane overrides it.
- `FGSTN_DirectionClassifier` (pure, Core) turns the stick into a **facing-relative 8-way direction** plus a **zone**:
  `Neutral` (dead zone) → `Aim` (light tilt: aims a "standing" attack without making it directional) → `Tilt`
  (directional). A **flick** flag marks entering `Tilt` from `Neutral`/`Aim` within `FlickFrames` (smash inputs).
- Facing comes from Mover's simulated state at end of F-1. The project's `UGSTN_MoverSimStateTracker` is project code:
  in M2 move an equivalent into `GSTNCombatNP` (the project then uses the plugin's).
- Whether "up/down" are stick directions or modifier buttons is a slot design choice: the classifier only knows
  Forward/Back/Left/Right and diagonals.
- Each move has an `AimPolicy` (`KeepFacing | SnapToStick | SnapIfAimOrTilt`, max turn angle, last frame turning is
  allowed); rotation goes through Mover, not the actor.

### 6.4 History (*Decision*, in sync state)

The review showed the history cannot be rebuilt from inputs alone (the server fills missing inputs, facing comes from
corrected state), so a **compact event log is part of the combat sync state**:

- `ButtonEvents`: ring of the last 16 events `{Frame, Button, bRelease, bConsumed}`.
- `DirectionEvents`: ring of the last 16 direction changes `{Frame, Direction, Zone, bFlick}`.
- Current held mask (the `PrevHeldButtons` of M0).
- Serialized with frames as **saturated deltas** from the current frame (uint8: events older than 255 frames are
  irrelevant and dropped).
- Directions are never consumed; presses are consumed by activations and by reset rules.

Exact layout in the M1 specification.

## 7. Pattern grammar

- A **pattern** is an ordered list of **steps**. A step holds one or more conditions that must hold at a shared
  **anchor frame** (e.g. "Back + Attack" = one step with `Direction(Back)` and `Press(Attack)`), with
  `SimultaneityFrames` of tolerance, and `MaxGapFrames` to the next step.
- Conditions are `TInstancedStruct<FGSTN_InputCondition>` (structs, not UObjects: no GC, deterministic, cheap to
  evaluate). New conditions = new structs deriving from the base.
- Standard conditions: `Press`, `Release` (negative edge), `Hold`, `Direction`, `Motion` (direction sequence with
  leniency, e.g. back→forward), `Chord`, `Charge`, `Mash`, `AnyOf`. Later: `Custom` (const Blueprint class).
- The **matcher** searches backward from the terminal step (bounded backtracking over the rings) and returns the anchor
  of every step plus the button events it used (for consumption).
- Every condition reports a **specificity** (automatic priority) and a **canonical input** (the minimal input that
  satisfies it), used by the conflict validator.

## 8. Buffer

Per frame, in the combat tick: (1) append this frame's events to the history; (2) apply the **reset rules**; (3) if
the character is actionable or a **cancel window** is open, evaluate candidate slots by priority, and the first that
matches **and** can execute is requested; (4) consume the button events it used.

- **Time buffer**: the terminal press is valid if unconsumed and `Now - PressFrame <= BufferFrames`.
- **Hold buffer**: also valid past the window while the button has been held **continuously** since that press, up to
  `MaxHoldBufferFrames` (0 = unlimited). Per slot: on/off.
- **Direction policy** per slot: `AtPress` (directions checked at the anchor) or `AtExecution` (checked at the frame the
  move comes out).
- **Consumption**: one press triggers at most one move.
- **Unavailable higher slot**: if a slot matches but cannot execute (cooldown, blocked), its terminal press is
  **reserved** for this frame so lower slots cannot steal it, unless the slot sets `bFallThroughWhenUnavailable`.
- **Chord grace**: a single-button slot waits up to `ChordGraceFrames` when a higher-priority chord slot containing
  that button could still complete, so A and A+B do not steal each other's input. Adds latency; per-slot opt-out.
- **Reset rules** = list of `TInstancedStruct<FGSTN_BufferResetRule>` at global → weapon → move level: on a frame event
  (hit received, move activated, landed, hit confirmed, state entered), on tag added, after N idle frames, custom.
  Each flushes all or some buttons (marks their presses consumed, which also ends their hold buffer).
- Defaults in `UGSTN_CombatInputSettings` (DeveloperSettings); overrides per slot and per move.

## 9. Slots and conflicts

- `FGSTN_InputSlotSpec`: pattern + **context requirements** (`FGameplayTagRequirements` on the character's tags:
  grounded, airborne, on wall...) + priority (0 = automatic from specificity) + **category tags**
  (`Combat.Slot.Category.Normal/Special/Aerial/Defensive`) used by cancel rules + buffer overrides.
- `UGSTN_InputSlotAsset` (PrimaryDataAsset) wraps one spec for sharing; `UGSTN_InputSlotSet` lists slots (the plugin
  ships a standard set); weapons may also declare **inline slots**. A new slot = a new asset or inline entry: no code,
  no new tags.
- **Conflict validation** (`IsDataValid` on slot sets, later weapons and loadouts): every slot's canonical input is run
  through every other slot's matcher. Same priority and both match → **error** (ambiguous). A higher-priority slot
  always matches another slot's canonical input → **warning** (shadowed slot).
- Project contexts: Mover tags (`Mover_IsOnGround`...) and `State.Movement.WallSliding` → wall attacks.

## 10. Weapons, move sets, loadouts

- **Binding**: slot → **move** or **any GAS ability class** (e.g. `GA_Jump` → buffered jump and jump-cancel for free).
- **`UGSTN_WeaponDefinition`** (PrimaryDataAsset): `MoveSet` (bindings + buffer/priority overrides),
  `AllowedAlternatives` per slot (customisation), `ParentWeapon` (inherits and overrides the move set), passive
  abilities and stat GEs, `FGSTN_WeaponTuning` + `Parameters: TMap<FGameplayTag, float>` (passed as SetByCaller),
  visuals and anim layer (cosmetic).
- A **character move set** independent of weapons (shield, dodge, grab, jump) is merged with the active weapon's
  (the weapon wins on the same slot).
- **Compiled table**: when a loadout is applied (server, not predicted, replicated) the bindings of **all** its weapons
  are compiled in deterministic order (weapon index, priority, asset path) and their abilities granted. In-game weapon
  switching is then just `ActiveWeaponIndex` in the combat sync state: predicted and rollback-safe.
- **No `SourceObject` on specs** (*Decision*): a few `UGSTN_GA_Move` specs, the move index lives in the combat sync
  state. `SourceObject` replication breaks when the asset is not loaded on the client, and per-move specs on every AI
  inflate the ASC state.
- **`LoadoutRevision`** in the combat sync state: no predicted activation while client and server revisions differ
  (the table is replicated outside NPP).
- **`UGSTN_LoadoutDefinition`** (designer preset) and **`FGSTN_LoadoutData`** (runtime, serialisable): weapons by
  PrimaryAssetId, slot → allowed-alternative remaps, designer values `TMap<FGameplayTag, FGSTN_LoadoutValue>`
  (float/int/bool/tag), cosmetics.
- **`UGSTN_LoadoutComponent`** on the PlayerState (survives respawn): `ServerRequestLoadout` validated by
  `UGSTN_LoadoutRules`, replicated data, applies to the pawn (specs, attribute init GE, compiled table, new revision).
- **`UGSTN_LoadoutSubsystem`** (LocalPlayer) + `USaveGame` for saving and loading. Asset Manager primary types:
  `GSTN_Weapon`, `GSTN_Move`, `GSTN_InputSlot`, `GSTN_Loadout`.

## 11. Moves

- **`UGSTN_MoveDefinition`** (PrimaryDataAsset), the frame-data source of truth: `TotalFrames`,
  `FirstActionableFrame`, montage + section (played with `PlaySyncedMontage`; root motion → Mover), `AbilityClass`
  (default `UGSTN_GA_Move`, subclass for special moves), cost and cooldown through standard GAS settings, `AimPolicy`.
- **Tracks** = `TArray<TInstancedStruct<FGSTN_MoveTrack>>`, extensible: `Hitbox`, `HurtboxModifier`
  (intangible/invincible/armor), `CancelWindow` (into categories/slots, condition OnHit/OnBlock/OnWhiff/Always,
  jump-cancel), `Movement` (velocity curves, gravity, facing lock → one queued layered move via GE), `Tags`, `Event`
  (gameplay event / projectile / cue at frame N), `Charge` (loop while held), `LandingLag`/auto-cancel for aerials,
  buffer overrides.
- **`UGSTN_GA_Move`**: the GAS shell (tags, cost, cooldown, block/cancel, cues, `CommitAbility`). No timers of its own:
  reads frame and state from the combat component; ends when the move ends or is interrupted.
- The move frame lives in the combat sync state (`MoveStartFrame` + hitstop-paused frames), so hitstop freezes it.
- Animation: the move frame is the truth; the synced montage follows (same tick, paused in hitstop). GASP keeps motion
  matching for locomotion, moves use the montage slot; anim layer per weapon; GAS cues for VFX/SFX.

## 12. Hits

### 12.1 Hitboxes and hurtboxes

- `FGSTN_HitboxSpec`: shape (sphere/capsule/box), space `Root | BakedSocket | World (at start) | Projectile`, per-frame
  keys for offset/rotation/size (interpolated), `HitGroup` (one hit per group per target), `FGSTN_HitData` (damage,
  base and growth knockback, fixed knockback, angle with special 3D-adapted angles, yaw source: attacker facing |
  hitbox centre → victim | hitbox velocity, hitstun and hitstop multipliers, shield damage, priority/clank, type
  `Normal/Grab/Projectile/Unblockable`, extra GEs, cue tag, instanced custom data), `RelationshipVariants` (12.3).
- **`BakedSocket`**: the editor samples the montage at the NPP tick rate and stores each frame's socket transform
  relative to the root. No animation is evaluated at runtime: deterministic, also on a dedicated server.
- Hurtboxes: `UGSTN_HurtboxSetDefinition` (capsules on bones in ref pose, can be generated from the Physics Asset),
  relative to the root; per-move overrides in the `HurtboxModifier` track (bakeable). `UGSTN_CombatWorldSubsystem`
  registers hittable entities and owns the spatial broadphase.

### 12.2 Queries

Own shape math (`UGSTN_HitShapeUtils`: sphere/capsule/OBB) against the per-step snapshot (5.3). All Movers are at end of
F-1 during the combat tick, so results do not depend on tick order. On the server, for attackers controlled by remote
clients, lag compensation (`RewindActors`) is optional in `UGSTN_CombatRules`.

### 12.3 Teams and friendly fire

`UGSTN_TeamRelationshipResolver` (replaceable, chosen per game mode in `UGSTN_CombatRules`) gives the attacker→target
relationship as `Combat.Relationship.Self/Ally/Enemy/Neutral`; the mode says which relationships can be hit.
Each hitbox has `RelationshipVariants: TMap<relationship tag, FGSTN_HitVariant>`: `Ignore`, shape scale (smaller on
allies), `FGSTN_HitData` overrides/multipliers (less damage, no hitstun, different knockback or GEs). The relationship
is computed **before** the query, so the test already uses the variant's shape.

### 12.4 Delivery and resolution

- **Inbox** on the victim's combat component, frame-stamped, **drained only where the instance ticks** (server; the
  owning client only for local-only interactions such as split-screen). The victim consumes entries stamped before the
  current frame: reaction always at F+1, simultaneous trades work, independent of NP id order.
- **Resolution pipeline** = ordered list of `UGSTN_HitResolutionStep` (instanced, in `UGSTN_CombatRules`): team filter
  → intangible → parry → shield/block (directional on the plane) → armor/poise → clank/priority → apply (damage GE
  with `UGSTN_DamageExecution`, knockback, hitstun, `Combat.Event.Hit.*` gameplay event for reaction abilities) →
  hitstop. New mechanics = new steps.

### 12.5 Knockback, hitstun, DI, tech

- `UGSTN_KnockbackModel` (replaceable; default Smash-like: percent, weight, base, growth; hitstun = knockback × factor).
- **DI** with the victim's stick **on the movement plane** at the frame hitstop ends; **SDI** as small displacements
  during hitstop; launch with `FGSTN_LayeredMove_Knockback` (queued through a GE); tumble; **tech** = press within N
  frames before contact, with an anti-mash lockout, read from the history.

### 12.6 Hitstop

`HitstopStartFrame/EndFrame` for attacker and victim: freezes the move frame and buffer expiry, pauses the synced
montage (only while one plays: the pause flag does not exist without one, see section 17), freezes Mover with
`FGSTN_LayeredMove_Freeze` (OverrideAll, zero velocity, keeps the velocity to restore after), queued once with a
duration through a GE. **Project wall modes do not freeze** with that alone: `UGSTN_ControlledWallMovementMode` reads
input and rewrites velocity after layered moves (wall jump/detach possible, timers advance). Needed: a "movement locked"
extension point in `UGSTN_WallMovementModeBase` (Mover tag or modifier) that skips input-driven exits and stops the
timers — added by subclass/extension point, per wall-movement.md rules.

### 12.7 Network model

- Attacker predicts its own hitstop and hit-confirm; if the server disagrees, the combat state differs and the attacker
  reconciles (correct).
- Victim state is server-authoritative; on the victim's own client it arrives as a correction: **every hit on a local
  player costs a rollback of about RTT/16 ms frames** (budget it).
- `UGSTN_HitPresentationComponent` plays flash/sparks/visual freeze on the interpolated victim immediately on the
  attacker's client (essential in PvE), cancelled if not confirmed.
- **Grabs**: a `Grab` hitbox links grabber and grabbed in both states (authoritative); the grabbed follows the grabber's
  socket in a "Grabbed" Mover mode; throws are slots active only while grabbing. Risk: a grabbed local player follows a
  grabber interpolated ~100 ms in the past → constant corrections. **Needs its own spike before building.**

## 13. Many enemies (40+)

Two paths with the same Core logic:

- **Fighters** (players, bosses, elites): the full NP model above.
- **Minions**: `UGSTN_MinionCombatComponent`, **server only**, no NP model per entity. `UGSTN_MinionCombatSubsystem`
  ticks them in batch at the fixed tick (moves, hitboxes, hurtboxes, knockback from Core), with LOD (far/inactive skip
  queries and debug) and the world subsystem's broadphase. Compact replicated state (FastArray, reduced rate), clients
  interpolate + hit presentation. Player hits on minions resolve on the server with lag compensation; minion hits on
  players go to the player's inbox (server), so a correction on the player's client.
- Promoting a minion to fighter = swapping the component.

## 14. Editor and debug

- Move asset editor: preview viewport, frame timeline (ruler, frame stepping), one row per track, hitbox gizmos,
  `FGSTN_HitData` details, "Bake sockets" button.
- `IsDataValid`: frame ranges, overlapping windows, slot conflicts (section 9).
- CVars `gstn.Combat.Debug.Hitboxes/Hurtboxes/Buffer`, input display widget (notation of section 19.4, frames),
  Gameplay Debugger category, NP Insights traces of the combat state.

## 15. Bandwidth

- `NetSerialize` delta-aware against `FNetSerializeParams::BaseDeltaStatePtr` (sim proxies already get delta frames).
- End-frame stamps instead of countdowns (5.5) make an idle entity's state constant: about one bit per frame.
- History serialized with saturated uint8 deltas, rings capped at 16 events.
- Minions are outside NPP (section 13).

## 16. Project integration

- `SandboxCharacter_Mover`: `UGSTN_CombatSimComponent`, hurtbox component, player input source;
  `UGSTN_LoadoutComponent` on the PlayerState. **Shared Blueprints: tell the user before editing**; prefer
  `Tools/GAS/setup_gas_assets.py` (extended) or the editor MCP.
- `GA_Jump` bound to a slot for buffered jump and jump-cancel; existing abilities otherwise unchanged.
- Wall modes: the "movement locked" extension point (12.6).
- Docs: this file; a future skill `add-gstn-move`.

## 17. M0 results (risk spike, 2026-10-17)

What exists: plugin `GSTNCombat`, module `GSTNCombatNP`, `UGSTN_CombatSimComponent` (model at 47, buttons from
Enhanced Input or scripted, press of button *i* requests `ButtonAbilities[i]`, `HitstopTestButton` opens a hitstop
window), test character `BP_GASNPP_TestCharacter` with button 0 → `GSTN_GA_TestDash`, button 1 → 8-frame hitstop,
harness `scenario_combat_spike` and `combat_tap` (see [testing.md](testing.md)).

Fork patches found necessary:

1. **State view on rollback** (`NetworkPredictionService_Rollback.inl`): the view (pending frame, sim time, pending
   states) was refreshed only by `DoTick`, so after a rollback any call into an instance before its own resim tick used
   the newest pre-rollback frame (Mover scheduled queued moves late, GE/task/cue start times were wrong). This already
   affected `ApplyMoveGameplayEffectComponent`. Now the first resim step sets each view to the restored frame.
2. **`UNpAbilitySystemComponent::OnSimulationPreTick`**: lets the combat model drive the ASC from inside the ASC's tick.
3. **Montage pause flag** (`FMontageSimSyncState::ShouldReconcile`): the flag is serialized only with a montage but was
   always compared; pausing with no montage caused a guaranteed reconcile. Found by the spike.

Verified with the PIE harness (dedicated server + 2 clients, 60 ms lag, 15 ms jitter, with and without 2 % loss): a
combat-requested ability activates exactly once across forced reconciles placed over the request frame and over the
hitstop window; server and client states converge; **no reconciles besides the forced ones** (checked with
`np.PrintReconciles 1`; the harness rollback counter counts each rollback twice). The existing `scenario_abilities` and
`scenario_forced_rollback` stay clean with the patches.

Not verified yet: hitstop with a montage actually playing; Mover freeze; anything cross-entity; `scenario_jump_wall`
(it is calibrated for another spawn point and does not reach the wall on `DefaultLevel`'s current spawn).

## 18. Roadmap

| # | Milestone | Content |
|---|---|---|
| M0 | Risk spike | **Done** (section 17). |
| M1 | Core input layer | `GSTNCombatCore`: samples, classifier, history, conditions, matcher, buffer, reset rules, slots, validation, Automation tests. **Specified below.** |
| M2 | Input in the simulation | History in `FGSTN_CombatSyncState`, stick + `ForcedSlot` in the command, input sources (player/AI), Mover state reader in the plugin, slot set → activation of any GAS ability (buffered jump), harness scenarios. |
| M3 | Moves, weapons, loadouts | `GSTNCombatGAS`, `UGSTN_GA_Move`, synced montage, cancel windows, movement track, compiled table, predicted weapon switch, `LoadoutRevision`, save/load. |
| M4 | Hits | Hurtboxes, hitboxes, snapshot, inbox, damage, hitstop (Mover freeze, wall lock), hitstun/knockback/DI, lag compensation, presentation, teams and variants. |
| M5 | Editor | Move editor, socket baking, validators UI, debug draw, input display. |
| M6 | Defence | Shield, parry, tech, SDI; grab spike then grabs/throws. |
| M7 | Minions | Server subsystem, LOD, compact replication. |
| M8 | Stock GAS adapter | Optional. |

## 19. M1 specification — Core input layer

### 19.1 Scope

In: module `GSTNCombatCore`, every pure type and algorithm of sections 6.3, 6.4, 7, 8, 9; developer settings; slot
assets and validation; Automation tests. Out: NP integration and input command changes (M2), Enhanced Input sources
(M2), moves/weapons (M3), editor UI (M5). Nothing in `GSTNCombatNP` or the project changes in M1, except adding the
module to `GSTNCombat.uplugin`.

**Hard constraint:** `GSTNCombatCore` must not depend on GameplayAbilities, AbilitySystemSimulation,
NetworkPrediction or Mover. Every function is deterministic: integer frames, no wall-clock time, no randomness, no
iteration over unordered containers.

### 19.2 Files

```
Plugins/GSTNCombat/Source/GSTNCombatCore/
  GSTNCombatCore.Build.cs
  Public/
    GSTN_CombatCoreTypes.h            enums: EGSTN_StickDirection, EGSTN_StickZone, EGSTN_DirectionPolicy; direction masks
    Input/GSTN_CombatInputSettings.h  UGSTN_CombatInputSettings (UDeveloperSettings, config=Game)
    Input/GSTN_CombatInputConfig.h    UGSTN_CombatInputConfig (DataAsset): button list
    Input/GSTN_InputSample.h          FGSTN_InputSample, FGSTN_StickSample
    Input/GSTN_DirectionClassifier.h  FGSTN_DirectionClassifier + FGSTN_ClassifierState
    Input/GSTN_InputHistory.h         FGSTN_ButtonEvent, FGSTN_DirectionEvent, FGSTN_InputHistory
    Input/GSTN_InputConditions.h      FGSTN_InputCondition base + standard conditions
    Input/GSTN_InputPattern.h         FGSTN_InputStep, FGSTN_InputPattern, FGSTN_PatternMatch
    Input/GSTN_InputMatcher.h         FGSTN_InputMatcher (pattern matching + buffer rules)
    Input/GSTN_BufferResetRules.h     FGSTN_BufferResetRule base + standard rules, FGSTN_BufferFrameEvents
    Input/GSTN_InputSlot.h            FGSTN_InputSlotSpec, UGSTN_InputSlotAsset, UGSTN_InputSlotSet
    Input/GSTN_SlotResolver.h         FGSTN_SlotResolver, FGSTN_SlotResolution
    Input/GSTN_InputSlotValidation.h  UGSTN_InputSlotValidation (BlueprintFunctionLibrary, stateless)
    Input/GSTN_InputScript.h          FGSTN_InputScript: builds histories from the notation of 19.4 (tests, validator, debug)
  Private/
    GSTNCombatCoreModule.cpp
    Input/*.cpp
    Tests/GSTN_InputTests_*.cpp       WITH_DEV_AUTOMATION_TESTS
```

Conventions from CLAUDE.md apply (prefix, export macro `GSTNCOMBATCORE_API`, header doc comments explaining intent and
determinism constraints, tuning as `UPROPERTY(EditAnywhere/EditDefaultsOnly)` with `Category = "Area|Section"`,
`ClampMin`).

### 19.3 Types

**Enums (`GSTN_CombatCoreTypes.h`)**

- `EGSTN_StickDirection : uint8` — `Neutral, Forward, ForwardRight, Right, BackRight, Back, BackLeft, Left,
  ForwardLeft` (facing-relative). `FGSTN_DirectionMask` = uint16 bitmask over these (with helpers for "any forward",
  "any back"...).
- `EGSTN_StickZone : uint8` — `Neutral, Aim, Tilt` (ordered: comparisons "at least Tilt" are valid).
- `EGSTN_DirectionPolicy : uint8` — `AtPress, AtExecution`.
- `EGSTN_OverrideBool : uint8` — `Default, On, Off` (per-slot override of a settings default).

**Settings (`UGSTN_CombatInputSettings`, Project Settings > Plugins > GSTN Combat Input)**

| Property | Default | Meaning |
|---|---|---|
| `DefaultBufferFrames` | 8 | Time buffer. |
| `bHoldBufferByDefault` | true | Hold buffer for slots that do not override it. |
| `MaxHoldBufferFrames` | 0 | 0 = unlimited while held. |
| `ChordGraceFrames` | 3 | Single-button wait when a chord could complete. |
| `DefaultSimultaneityFrames` | 3 | Tolerance inside a step (e.g. direction vs press). |
| `StickDeadZone` / `AimZone` | 0.2 / 0.5 | Normalised magnitude thresholds: below dead = Neutral, below aim = Aim, else Tilt. |
| `FlickFrames` | 3 | Max frames from Neutral/Aim to Tilt for a flick. |
| `DiagonalHalfAngle` | 22.5° | Sector half-width for diagonals (8 equal sectors by default). |
| `DefaultResetRules` | (empty) | Global reset rules. |

Values are designer defaults, to be tuned in M2 playtests.

**Input config (`UGSTN_CombatInputConfig`)** — `Buttons: TArray<FGSTN_CombatButtonDef>`, max 32; each
`{ FGameplayTag Tag; TObjectPtr<UInputAction> Action; }`. The array index is the bit. `IsDataValid`: ≤ 32 entries,
unique tags, unique actions. `FindButtonIndex(Tag)`.

**Sample (`FGSTN_InputSample`)** — what one frame provides to the history:
`int32 Frame; uint32 Held; uint32 Pressed; uint32 Released; EGSTN_StickDirection Direction; EGSTN_StickZone Zone;
bool bFlick;`. Built in M2 from the command + classifier; in M1 by `FGSTN_InputScript` and tests.
`Pressed` includes taps (`Cmd.PressedButtons`) and held edges; a button both pressed and released inside one frame
produces a press then a release event on the same frame.

**Classifier (`FGSTN_DirectionClassifier`)** — pure static `Classify(FVector2D PlaneStick, float FacingYawDeg,
const FGSTN_ClassifierState& Prev, int32 Frame, const UGSTN_CombatInputSettings& S, FGSTN_ClassifierState& Out)`
→ direction, zone, flick. `FGSTN_ClassifierState { EGSTN_StickZone Zone; int32 LastLowZoneFrame; }` lives in the sync
state in M2 (it is needed to detect flicks after a rollback). Stick is rotated by `-FacingYaw` so +X = Forward,
+Y = Right. Quantized input (int8) is converted to [-1, 1] before classification so client and server classify the
same values.

**History (`FGSTN_InputHistory`)**

```
FGSTN_ButtonEvent    { int32 Frame; uint8 Button : 5; uint8 bRelease : 1; uint8 bConsumed : 1; }
FGSTN_DirectionEvent { int32 Frame; EGSTN_StickDirection Direction; EGSTN_StickZone Zone; bool bFlick; }
FGSTN_InputHistory {
  TStaticArray<FGSTN_ButtonEvent, 16> ButtonEvents;   + uint8 ButtonHead, ButtonCount
  TStaticArray<FGSTN_DirectionEvent, 16> DirectionEvents; + uint8 DirectionHead, DirectionCount
  uint32 HeldButtons;
  FGSTN_DirectionEvent Current;   // direction/zone at the newest frame (also when unchanged)
}
```

Operations: `Append(const FGSTN_InputSample&)` (press events in button index order, then releases; a direction event
only when direction or zone changes or a flick happens); iteration newest → oldest; `GetDirectionAt(Frame)` (last
direction event with `Frame <= F`); `IsHeldContinuouslySince(Button, Frame, Now)` (no release event of that button
after the press at `Frame` and the bit is in `HeldButtons`); `Consume(EventIndex)`; `FlushPresses(Mask, UpToFrame)`.
`NetSerialize(FArchive&, int32 CurrentFrame)`: counts + per event `uint8 delta = min(Current - Frame, 255)` + payload;
events with delta 255 are dropped on read. `operator==` for `ShouldReconcile`. Ring overflow drops the oldest.

**Conditions (`FGSTN_InputCondition`, USTRUCT base, `TInstancedStruct` elements)**

Virtual interface (all const):

- `bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const` — does the
  condition hold with its reference event at or before `AnchorFrame` and no earlier than `Ctx.EarliestFrame`? `Out`
  gets the matched frame and the button event indices used.
- `bool IsAnchorCondition() const` — true for conditions that produce a frame (Press, Release, Chord, Motion end,
  Mash): one per step anchors it; others (Direction, Hold, Charge) are checked relative to the anchor within
  `SimultaneityFrames`.
- `int32 GetSpecificity() const`.
- `void AppendCanonicalInput(FGSTN_InputScript& Script, int32& Frame) const`.

| Condition | Fields | Holds when | Specificity |
|---|---|---|---|
| `Press` | `ButtonMask` (any of) | An **unconsumed, unreserved** press of a button in the mask. | 10 |
| `Release` | `Button`, `MinHeldFrames` | A release of `Button` whose press was held ≥ `MinHeldFrames` (negative edge). | 10 |
| `Hold` | `Button`, `MinFrames` | `Button` held continuously for ≥ `MinFrames` ending at the anchor. | 5 |
| `Direction` | `Directions` mask, `MinZone`, `bRequireFlick` | Direction at the anchor (± simultaneity) is in the mask with zone ≥ `MinZone` (and flick if required). | 5, +3 if `MinZone = Tilt`, +2 if flick |
| `Motion` | `Sequence: TArray<FGSTN_DirectionMask>`, `MaxFramesBetween`, `MaxTotalFrames`, `MinZone` | Direction events matching the sequence in order, gaps ≤ `MaxFramesBetween`, whole motion ≤ `MaxTotalFrames`; other events in between are allowed (leniency). Anchor = frame of the last element. | 5 per element |
| `Chord` | `ButtonMask` (all of), `WithinFrames` | Unconsumed presses of every button in the mask, all within `WithinFrames` of each other. Anchor = newest. | 10 per button |
| `Charge` | `Directions` mask, `MinFrames`, `MaxReleaseFrames` | Direction in the mask held for ≥ `MinFrames`, left no more than `MaxReleaseFrames` before the anchor. | 15 |
| `Mash` | `Button`, `Count`, `WithinFrames` | `Count` unconsumed presses of `Button` within `WithinFrames`. | 5 × Count |
| `AnyOf` | `Conditions` (instanced) | Any child holds (first in order wins). | max of children |

**Pattern (`FGSTN_InputPattern`)** — `Steps: TArray<FGSTN_InputStep>`; `FGSTN_InputStep { TArray<TInstancedStruct<
FGSTN_InputCondition>> AllOf; int32 SimultaneityFrames = -1 (= settings); int32 MaxGapFrames = 15 (to the next step);
}`. Validation: each step has exactly one anchor condition; at least one step. `GetSpecificity()` = sum of conditions.
`FGSTN_PatternMatch { TArray<int32, TInlineAllocator<4>> StepAnchors; TArray<uint8, TInlineAllocator<8>>
UsedButtonEvents; int32 TerminalPressEvent = INDEX_NONE; bool bViaHoldBuffer; }`.

**Matcher (`FGSTN_InputMatcher`)** — `static bool Match(const FGSTN_InputPattern&, const FGSTN_InputHistory&,
const FGSTN_MatchParams&, FGSTN_PatternMatch& Out)`. `FGSTN_MatchParams { int32 Now; int32 BufferFrames; bool
bHoldBuffer; int32 MaxHoldBufferFrames; EGSTN_DirectionPolicy DirectionPolicy; int32 DefaultSimultaneityFrames;
uint32 ReservedEvents (bitmask over the 16 button events); }`. Algorithm:

1. Terminal step: candidate anchors newest first, restricted to `[Now - BufferFrames, Now]`; if `bHoldBuffer`, also
   older presses that are still held continuously (and within `MaxHoldBufferFrames` when non-zero) — then
   `bViaHoldBuffer = true`.
2. With `DirectionPolicy = AtExecution`, non-anchor direction conditions of the terminal step are evaluated at `Now`
   instead of the anchor.
3. Each earlier step searches `[Anchor(next) - MaxGapFrames, Anchor(next)]`, newest first; backtrack on failure.
   An event used by a later step cannot be reused by an earlier one.
4. Bound: at most 16 candidates per step, depth = step count; abort and fail past 4096 evaluations (assert in dev).

**Reset rules (`FGSTN_BufferResetRule`)** — base fields `uint32 ButtonMask` (0 = all). Interface
`bool ShouldFlush(const FGSTN_BufferFrameEvents& Events, const FGSTN_InputHistory& H, int32 Now) const`.
`FGSTN_BufferFrameEvents { FGameplayTagContainer EventTags; FGameplayTagContainer TagsAdded; }` is filled by the
caller (M2: from combat events and ASC tags; M1: tests). Standard rules: `OnEvent(EventTag)`, `OnTagAdded(Tag)`,
`AfterIdleFrames(Frames)` (no button or direction event for `Frames`). Flush = `FlushPresses(Mask, Now)`, which ends
time and hold buffering of those presses. `ApplyResetRules(History, Rules, Events, Now)` is a static pure function;
rule lists concatenate global → weapon → move.

**Slots (`GSTN_InputSlot.h`)**

- `FGSTN_InputSlotSpec { FName DebugName; FGSTN_InputPattern Pattern; FGameplayTagRequirements Context; int32 Priority
  = 0 (0 = automatic); FGameplayTagContainer Categories; int32 BufferFramesOverride = -1;
  EGSTN_OverrideBool HoldBuffer = Default; EGSTN_DirectionPolicy DirectionPolicy = AtPress;
  bool bFallThroughWhenUnavailable = false; bool bWaitForChords = true; }` + `GetEffectivePriority()`.
- `UGSTN_InputSlotAsset : UPrimaryDataAsset { FGSTN_InputSlotSpec Spec; }` (primary type `GSTN_InputSlot`).
- `UGSTN_InputSlotSet : UDataAsset { TArray<FGSTN_InputSlotEntry> Slots; }` where an entry is either an asset reference
  or an inline spec. `IsDataValid` runs the validator.

**Resolver (`FGSTN_SlotResolver`)** — `static FGSTN_SlotResolution Resolve(TConstArrayView<const FGSTN_InputSlotSpec*>
Slots, const FGSTN_InputHistory&, const FGameplayTagContainer& OwnerTags, int32 Now, const UGSTN_CombatInputSettings&,
TFunctionRef<bool(int32 SlotIndex)> CanExecute)`. Result `{ int32 SlotIndex = INDEX_NONE; FGSTN_PatternMatch Match;
bool bDeferredForChord; }`. Steps: sort indices by effective priority descending (stable on input order); skip slots
whose `Context` fails; match with the slot's params and the current reserved mask; if matched and `CanExecute` → done;
if matched and not executable → reserve its terminal press unless `bFallThroughWhenUnavailable`; chord grace: before
accepting a single-press slot, if a higher-priority slot with a `Chord` containing that button has a partial match
(the other buttons not pressed yet) and `Now - press < ChordGraceFrames`, return `bDeferredForChord` with no slot. The
caller consumes `Match.UsedButtonEvents` on activation (M2 does it in the sync state).

**Validation (`UGSTN_InputSlotValidation`)** — `FindConflicts(Slots) → TArray<FGSTN_SlotConflict {A, B, Kind
(Ambiguous | Shadowed), Message}>`: build each slot's canonical script (`FGSTN_InputScript` from
`AppendCanonicalInput`), run the resolver over every other slot on that history (context ignored, unless both contexts
are provably disjoint: one requires a tag the other forbids). Equal priority + match → Ambiguous (error); higher
priority always matching → Shadowed (warning).

### 19.4 Input notation (`FGSTN_InputScript`)

Used by tests, the validator and later the input display. Space-separated tokens `<frame>:<item>`:

- Buttons by index or config tag name: `+A` press, `-A` release, `*A` tap (press and release on the same frame).
- Directions, top-down numpad **relative to facing**: `8` forward, `2` back, `4` left, `6` right, `7 9 1 3` diagonals,
  `5` neutral; zone suffix `a` (aim) or `t` (tilt, default); `!` flick. Example: `0:2t 6:8t! 7:+A` = back, then flick
  forward and press A.
- `Build(const FString&, FGSTN_InputHistory&, int32& OutLastFrame)` fails loudly on syntax errors.
- *Open:* confirm this notation with the designers (it differs from the 2D fighting-game convention where 6 = forward).

### 19.5 Tests (Automation, names `GSTNCombat.Core.Input.<Area>.<Case>`)

| Area | Cases (expected result) |
|---|---|
| History | Ring overflow keeps the newest 16; serialize → deserialize equality; events older than 255 frames dropped; same-frame press+release recorded in order; `GetDirectionAt` returns the last change at or before the frame. |
| Classifier | Dead/aim/tilt thresholds exactly at and around the limits; facing 90° maps world stick correctly to Forward/Right; flick within `FlickFrames`, none when slower; quantized int8 input classified identically to its float. |
| Conditions | One positive and one negative case per condition and field (window edges inclusive, consumed press ignored, reserved press ignored, negative edge needs the minimum hold, motion with noise between elements passes, motion over `MaxTotalFrames` fails, chord outside `WithinFrames` fails, charge released too early fails, mash count). |
| Buffer | Press at `Now - BufferFrames` matches, at `-BufferFrames-1` fails; held press matches past the window (`bViaHoldBuffer`), fails after release, fails past `MaxHoldBufferFrames`; `AtPress` vs `AtExecution` give different results when the direction changes after the press. |
| Resolver | Motion+A beats Dir+A beats A on one history; a consumed press does not fire again next frame; unavailable higher slot reserves the press (no lower slot fires) and with `bFallThroughWhenUnavailable` the lower one fires; context filtering; chord grace defers then resolves A+B, and resolves A alone after `ChordGraceFrames`. |
| Reset | `OnEvent` flushes only masked buttons; `AfterIdleFrames`; a flush ends hold buffering. |
| Validation | Two identical patterns with equal priority → Ambiguous; A vs Dir+A → no conflict (different priority) and A not reported shadowed; a slot fully covered by a higher one → Shadowed; disjoint contexts → no conflict. |
| Determinism | Same script resolved twice → identical results; copy the history mid-script, resolve the rest on both copies → identical (rollback stand-in). |

Run headless:
`UnrealEditor-Cmd.exe "<project>/GASP_Giustino2.uproject" -ExecCmds="Automation RunTests GSTNCombat.Core;Quit" -unattended -nullrhi -nopause -log`
(or the Session Frontend). Add the command to testing.md.

### 19.6 Acceptance criteria

1. The project builds (Development Editor) with `GSTNCombatCore` listed in `GSTNCombat.uplugin`; Core's Build.cs has
   none of the forbidden dependencies (19.1).
2. All tests of 19.5 pass headless.
3. Every public type has a header doc comment on intent and determinism, at the density of the existing headers.
4. A standard slot set can be authored in the editor with the condition structs (instanced struct pickers work) and
   `IsDataValid` reports conflicts. (Plugin content needs the `.gitignore` exception of section 3; until then create it
   under `/Game/GAS/` and tell the user.)
5. This document is updated where implementation differs (types, defaults), and testing.md gets the test command.

### 19.7 Notes for the implementer

- Keep the history and every result type POD-like and small: in M2 they are copied every frame into the sync state.
- Never iterate `TMap`/`TSet` in matching; never use floats in frame logic (the classifier converts once).
- The resolver will be called every frame for every fighter: avoid allocations in the hot path (inline allocators,
  static arrays).
- No changes to `GSTNCombatNP` or project code in M1; the spike keeps working as is until M2 replaces its
  button→ability mapping with the resolver.
