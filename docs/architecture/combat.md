# Combat system (`GSTNCombat` plugin)

Design document for a fighting-game / platform-fighter combat layer on top of GAS, simulated by Network Prediction
(the GAS-NPP fork, see [gas-npp.md](gas-npp.md)). Source: `Plugins/GSTNCombat/`.

Status (2026-10-17): **M0 (risk spike) done and verified**; everything else is design. This file is the architecture;
each milestone has its own implementation specification (section 18). Sections marked *Decision* are settled; *Open*
items are listed where they apply.

## 1. Goals

- Fighting-game input: designer-defined combinations (button, direction, motion, chord, charge, hold, release, mash),
  a **time buffer** and a **hold buffer** (a press stays valid while the button is held until the move can come out, as
  in Super Smash Bros. Ultimate), standard buffer resets plus designer-defined ones.
- **Weapons** made of modular **moves** bound to **input slots** (e.g. Attack + Back). New slots for non-standard
  combinations are created without code and are checked against the other slots for conflicts.
- **Primary kit**: the character's "class" (as in a class-based MMO): a primary weapon plus base stats (speed, jump
  height...) and kit-wide moves. Independent of the look (**skins** are cosmetic and combine freely).
- **Additional weapons** (for now one support weapon) the character switches to; each changes some of the stats.
- **Meters** (EX bars) spent by EX moves, supers and other abilities.
- **Supers** independent of the current weapon, chosen separately and placed in dedicated slots (Helldivers stratagem
  style; the input scheme is still open, so the design supports several).
- **Loadout** = primary kit + additional weapons + chosen supers + customisations and designer values; saved and
  replicated.
- **Movement moves** beyond walking: dash/run, air jumps, fast fall, dodges, and a **variable-height jump**
  (continuous, not just short/full hop) with **independent rise and fall gravity**.
- **Piggyback** ("a cavalluccio"): a player rides on a teammate. **Pair moves** need both, some actions are only for the
  rider, some only for the carrier. Input still open; the function must be designed in from the start.
- Later, not designed yet: picking up and throwing objects of different sizes (section 13b).
- Platform-fighter rules in 3D: hitstun, hitstop, knockback, DI on the movement plane, SDI, tech, shield, parry, grab.
  Also PvE, so these get tuned for 3D and for many enemies.
- Hitboxes that do not depend only on the skeleton, authored easily by designers.
- GAS best practices, easy replication, a reusable plugin, modular and extensible.

*Decisions taken with the user:* engine-agnostic **Core** + **NPP adapter** (a stock-GAS adapter may come later);
hitboxes on a **frame-data timeline in the move asset**; the time unit is the **NPP fixed-tick frame**; knockback model
**pluggable, Smash-like by default**; players and AI use **the same pipeline**; **no lock-on**; friendly fire per game
mode, with **per-relationship hit variants**; scale target **40+ enemies** at once; **loadout = primary kit + additional
weapons + super slots**, skins separate; stats are GAS attributes, weapons modify them while active.

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
| Command tag | Gameplay tag naming a command (`Command.Attack.Directional`, `Command.Special.CrouchNeutral`...). The identity of a slot everywhere outside input matching: bindings, cancel windows, AI, UI, events, quests. |
| Slot | Pattern + context + priority, identified by its command tag. Says *how* a command is input (e.g. *Attack + tilt, grounded*). |
| Binding | Command tag → move or any GAS ability, inside a weapon or a character move set. |
| Move | Data asset with frame data and tracks, executed by a generic GAS ability. |
| Weapon | Set of bindings, stat modifiers while active, passives, tuning, visuals. Role: primary or support. |
| Primary kit | Character class: primary weapon, base stats, kit move set (weapon-independent), meters, super slot count, allowed weapons and supers. |
| Super | Weapon-independent move placed in a super slot of the loadout. |
| Meter | Resource attribute (EX, super...) gained in combat and spent as GAS cost. |
| Skin | Cosmetic look, outside the simulation, compatible with kits by tags. |
| Loadout | Primary kit + additional weapons + supers + customisations + designer values. |
| Link | Authoritative relation between two entities with a role each: grab (grabber/grabbed), mount (carrier/rider), later held object. |
| Pair move | Move involving both linked entities: the initiator's move plus a partner move started through the link. |

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

### 5.6 Authoritative combat events (*Decision*)

Simulation code re-runs on the client during re-simulation, so anything that must count once (quests, achievements,
statistics, analytics, kill feed) must not listen to simulation callbacks. The combat model publishes **authoritative
events** only where they cannot be rolled back: on the server, from `FinalizeFrame`-time state that the server
produced.

- `UGSTN_CombatEventSubsystem` (world subsystem, server): `Broadcast(const FGSTN_CombatEvent&)` and
  `Listen(FGameplayTagQuery, delegate)`. An event is `{ FGameplayTag EventType; FGameplayTagContainer Tags; int32
  Frame; TWeakObjectPtr<AActor> Instigator, Target; float Magnitude; }`.
- `Tags` carries everything a listener may filter on: command tag, move tag, weapon/kit tags, relationship, link role,
  context tags (airborne, on wall...). Example quest: "10 × `EventType = Combat.Event.CommandExecuted` with
  `Command.Attack.CrouchDirectional` and `Weapon.Role.Support` and `Combat.Context.Airborne`".
- Event types: `Combat.Event.CommandExecuted` (M2), `MoveFinished`, `WeaponSwitched`, `SuperUsed` (M3), `HitLanded`,
  `HitReceived`, `KO` (M4), `LinkFormed/Broken`, `PairMove` (M6–M7).
- Clients that need feedback (UI flashes, tutorial prompts) use replicated results or predicted presentation, never
  these events. A client-side mirror can be added later from replicated server events if a feature needs it.

## 6. Input

### 6.1 Input command (`FGSTN_CombatInputCmd`, NP model input)

| Field | Type | Notes |
|---|---|---|
| `HeldButtons` | uint32 | Held at sample time. |
| `PressedButtons` | uint32 | Pressed since the previous sample: a tap shorter than one frame is not lost. |
| `StickX`, `StickY` | int8 | Move stick **as on the controller** (raw, +Y up), quantized to [-127, 127]. *(M2)* |
| `CameraYaw` | uint8 | Control-rotation yaw at sample time, 256 steps. The simulation derives the world-plane stick from it, so every direction space is available deterministically. *(M2)* |
| `ForcedSlot` | uint16 | 0 = none. Lets AI/scripts request a slot without synthesising inputs. *(M2)* |

Exists today with the first two fields (M0). Serialized packed.

### 6.2 Input sources

`IGSTN_CombatInputSource` produces the command on the controlling machine: `UGSTN_PlayerCombatInputSource` (Enhanced
Input; buttons are the `UInputAction`s of the input config; taps caught through the action's Started event, not by
polling) and `UGSTN_AICombatInputSource` (BT/StateTree tasks: Press, Hold, Release, PerformSlot via `ForcedSlot`).
Test scripting (`SetScriptedButtons`, `TapScriptedButtons`) exists on the component today.

### 6.3 Directions in 3D (*Decision*)

- Default direction of a move is the **character's facing**; the stick on the movement plane overrides it.
- **"Down/up" are not stick directions**: the down/up variants of the main attacks use the **crouch button held** as a
  modifier (section 9.1). The stick only expresses directions on the movement plane.
- `FGSTN_DirectionClassifier` (pure, Core) classifies the stick every frame in **three spaces**, each as an 8-way
  direction (`Up, UpRight, Right, ... , Neutral`):
  - **`Facing`**: relative to the character (Up = forward, Down = back). For inputs like "towards / away from where I
    look".
  - **`Stick`**: the **physical stick on the controller** (Up = pushed up), independent of character and camera — the
    "global" stick direction (*Decision*, confirmed with the user). For spins, codes (supers) and any input that must
    not change when the character or the camera turns.
  - **`World`**: absolute level axes (Up = a configured world yaw). Optional, for fixed-camera or side-view sections.
  - Separately from these categories, the **world-plane direction** of the stick (stick rotated by the camera yaw) is
    what aims attacks (`AimPolicy`): "where the stick points on screen" becomes a world direction.
- Plus a **zone** from the magnitude, the same in every space: `Neutral` (dead zone) → `Aim` (light tilt: aims a
  "standing" attack without making it directional) → `Tilt` (directional). A **flick** flag marks entering `Tilt` from
  `Neutral`/`Aim` within `FlickFrames` (smash inputs). The world yaw (256 steps) is kept for aiming.
- Facing comes from Mover's simulated state at end of F-1. The project's `UGSTN_MoverSimStateTracker` is project code:
  in M2 it moves into `GSTNCombatNP` (the project then uses the plugin's).
- Each move has an `AimPolicy` (`KeepFacing | SnapToStick | SnapIfAimOrTilt`, max turn angle, last frame turning is
  allowed): a directional attack goes **towards the stick's world direction**; rotation goes through Mover, not the
  actor.

### 6.4 History (*Decision*, in sync state)

The review showed the history cannot be rebuilt from inputs alone (the server fills missing inputs, facing comes from
corrected state), so a **compact event log is part of the combat sync state**:

- `ButtonEvents`: ring of the last 16 events `{Frame, Button, bRelease, bConsumed}`.
- `DirectionEvents`: ring of the last 16 direction changes `{Frame, StickDir, FacingDir, WorldDir, Zone, bFlick,
  WorldYaw}` — a new event when any of the three directions or the zone changes (turning the character or camera with
  the stick held adds one event per 45° sector).
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
- Standard conditions: `Press`, `Release` (negative edge), `Hold` (also "modifier held", e.g. crouch), `Direction`
  (with a space and a zone range: "stick neutral" or "tilted"), `Motion` (direction sequence with leniency, e.g.
  back→forward), `Spin` (a rotation of N degrees starting **anywhere**, e.g. a 360° stick spin), `Chord`, `Charge`,
  `Mash`, `AnyOf`. Direction-based conditions choose their space (6.3). Later: `Custom` (const Blueprint class).
- The **matcher** searches backward from the terminal step (bounded backtracking over the rings) and returns the anchor
  of every step plus the button events it used (for consumption).
- Every condition reports a **specificity** (third priority criterion, 8.2) and a **canonical input** (the minimal
  input that satisfies it), used by the conflict validator.

## 8. Buffer, priority and startup override

Per frame, in the combat tick: (1) append this frame's events to the history; (2) apply the **reset rules**; (3) if
the character is actionable, a **cancel window** is open, or the current move is inside its **startup override
window** (8.3), resolve the candidate commands (8.2) and request the winner; (4) consume the button events it used
and the losers of its simultaneous group.

### 8.1 Buffering

- **Time buffer**: the terminal press is valid if unconsumed and its age (frames since the press, hitstop frames
  excluded) ≤ `BufferFrames`.
- **Hold buffer**: also valid past the window while the button has been held **continuously** since that press, up to
  `MaxHoldBufferFrames` (0 = unlimited). Per slot: on/off.
- **Direction policy** per slot: `AtPress` (directions checked at the anchor) or `AtExecution` (checked at the frame the
  move comes out).
- **Consumption**: one press triggers at most one command.
- **Reset rules** = list of `TInstancedStruct<FGSTN_BufferResetRule>` at global → weapon → move level: on a frame event
  (hit received, move activated, landed, hit confirmed, state entered), on tag added, after N idle frames, custom.
  Each flushes all or some buttons (marks their presses consumed, which also ends their hold buffer).
- Defaults in `UGSTN_CombatInputSettings` (DeveloperSettings); overrides per slot and per move.

### 8.2 Priority between commands (*Decision*)

Several commands can match on the same frame (attack and special pressed together, a buffered attack and a newer
special, crouch variants...). The resolver orders them deterministically:

1. **Press group, most recent first** (`BufferConflictPolicy = MostRecent`, configurable to `HighestPriority` or
   `Oldest`). The terminal presses of the matching commands are grouped: presses within `SimultaneousFrames` (default
   2) of the newest one form the newest group. Only commands anchored in that group compete; older presses lose (a
   player who pressed attack and then changed their mind and pressed special gets the special).
2. **Tier** of the command, from the command tag root, configured in the settings (`CommandTiers`: tag → tier), e.g.
   `Command.Super` 400 > `Command.Special` 300 > `Command.Strong` 200 > `Command.Attack` 100, movement and defensive
   roots as the game needs. Attack and special pressed together → the special, as in Smash.
3. **Specificity** of the pattern (automatic): inside a tier, crouch + direction > crouch + neutral > direction >
   neutral; a chord command (attack + special) beats each single button.
4. **Manual offset** on the slot, for the rare case the rules above do not express.

Then, in that order: the first command whose context holds and that **can execute** wins. A command that matches but
cannot execute **reserves** its terminal press for this frame (lower commands cannot steal it) unless it sets
`bFallThroughWhenUnavailable` (EX versions without meter fall through to the normal version).

**Losers are consumed**: when a command wins, the unconsumed presses of its press group whose buttons are in the
settings' `ExclusiveButtons` (default attack, strong, special, super) are consumed too, so the losing attack does not
come out a few frames later from the buffer.

The validator reports the conflicts these rules leave: same tier, same specificity, same press group possible,
overlapping contexts.

### 8.3 Startup override window (*Decision*, Smash-style interrupts)

Smash Ultimate lets the first frames of a move be replaced: jabs can be cancelled into a jump within 1 frame, tilts and
smashes within 2 (attack cancel into a short-hop aerial; with a forward tilt turning the character first, this gives
the "instant back air"), and the jump squat can be interrupted by up smash or up special. The window never overlaps an
active hitbox. Generalised here:

- Every move has a **startup override window** of `OverrideFrames` from its first frame (default per command tier in
  the settings, e.g. jab-like 1, other attacks and specials 2; per-move override; 0 disables).
- Inside the window the resolver runs although the character is in a move. A command may **replace** the move if it
  matches the move's `OverrideInto` query on command tags: by default "any command of a higher tier", plus explicit
  entries (jump from ground attacks = attack cancel; up attack / up special from the jump squat).
- **The replaced move leaves no trace except its turn**: its cost is committed, its events fired, its movement and
  hitboxes started only from the end of the window (`DeferredCommitFrame`); the validator forbids anything earlier.
  The replacing move keeps the facing the first one set, which is how the instant back air falls out naturally.
- **Chords without input delay**: attack pressed → the attack starts at once; special pressed within the window → the
  attack + special command (higher specificity and tier) replaces it. No command waits for a possible second button.
  For that, chord-capable commands need `OverrideFrames ≥ SimultaneousFrames`. The old **chord grace** (wait before
  starting a single-button command) stays as an option, off by default, for moves that must not show even one frame
  before a chord.
- Presentation: remote proxies may show 1–2 frames of the replaced move before the switch (as in Smash); montages
  use a short blend-in.

## 9. Command tags, slots and conflicts

- **Command tags** (*Decision*): every slot is identified by a gameplay tag under `Command.` (hierarchical:
  `Command.Attack.Neutral`, `Command.Special.CrouchDirectional`, `Command.Air.Strong.Neutral`, `Command.Movement.Dash`,
  `Command.Super.Slot1`, `Command.Link.Mount`...). The tag, not the input, is what the rest of the game refers to:
  bindings (command → move), cancel windows and override windows (tag queries), loadout customisations, AI requests
  (`RequestCommand(Tag)`), UI and tutorials, events and quests (5.6). Changing how a command is input touches only
  its slot.
- The hierarchy replaces the former slot categories: "cancel into `Command.Special`" accepts every special; tiers
  (8.2) are assigned to roots.
- **Buttons** keep their own tags (`Input.Combat.Button.*`, as Lyra's input tags); bits and indices exist only inside
  the simulation and on the wire.
- Moves, weapons, kits, supers and hit types carry asset tags too (`Move.*`, `Weapon.*`, `Kit.*`, `Super.*`,
  `Combat.HitType.*`), so events and queries can filter on them.
- `FGSTN_InputSlotSpec`: command tag + pattern + **context requirements** (`FGameplayTagRequirements` on the character's
  tags: grounded, airborne, on wall, link role...) + manual priority offset + buffer and override overrides.
- `UGSTN_InputSlotAsset` (PrimaryDataAsset) wraps one spec for sharing; `UGSTN_InputSlotSet` lists slots (the plugin
  ships a standard set); weapons may also declare **inline slots**. A new command = a new tag + a slot (asset or
  inline entry), no code.
- **Tag sources** (exception to the project's "native tags only" rule, *Decision*): the plugin's standard command,
  button and event tags are native; game-specific commands created by designers live in a dedicated tag ini
  (`Config/Tags/GSTNCombatCommands.ini`) so they need no code. Everything else stays native.
- **Conflict validation** (`IsDataValid` on slot sets, later weapons, kits and loadouts): every slot's canonical input is
  run through every other slot's matcher. Conflicts the priority rules (8.2) do not resolve → **error**
  (ambiguous); a higher-priority slot always matching another slot's canonical input → **warning** (shadowed). Two
  slots with the same command tag in one set → error.
- Project contexts: Mover tags (`Mover_IsOnGround`...) and `State.Movement.WallSliding` → wall attacks.

### 9.1 Standard command set (*Decision*)

Three families (normal attack, strong attack, special) × grounded/airborne × four input shapes, all anchored by the
family's button press (one step):

| Shape | Analogue (Smash, reference only) | Extra conditions | Aim policy |
|---|---|---|---|
| `Neutral` | jab / neutral special | `Direction(Facing, any, MaxZone = Aim)` | `SnapIfAimOrTilt` (a light tilt aims it) |
| `Directional` | side tilt / side special | `Direction(Facing, any of the 8, MinZone = Tilt)` | `SnapToStick` (the move goes where the stick points) |
| `CrouchNeutral` | up tilt / up special | `Hold(Crouch, 0)` + `Direction(Facing, any, MaxZone = Aim)` | `SnapIfAimOrTilt` |
| `CrouchDirectional` | down tilt / down special | `Hold(Crouch, 0)` + `Direction(Facing, any of the 8, MinZone = Tilt)` | `SnapToStick` |

Command tags: `Command.Attack.<Shape>`, `Command.Strong.<Shape>`, `Command.Special.<Shape>` grounded and
`Command.Air.Attack.<Shape>`, `Command.Air.Strong.<Shape>`, `Command.Air.Special.<Shape>` airborne (context
`Combat.Context.Airborne`); 24 commands. Plus chord commands (e.g. `Command.Combo.AttackSpecial.<Shape>`: `Chord(Attack,
Special)` + the shape's conditions, higher tier), supers (10.4), movement (11b) and link commands (12b).

- In 3D "side" means **any** tilted direction, the attack turning towards it; commands that care about forward vs back
  relative to the character use a `Facing` mask (e.g. `FacingForward`) instead of "any of the 8".
- Crouch versions win over plain ones by specificity; a crouch press up to `SimultaneityFrames` after the attack press
  still counts. The crouch button is both a movement input (Mover stance) and a combat modifier: slots read the
  **button held state from the history**, not Mover's crouch tag (a frame late, and blockable).
- Strong attacks: a strong button by default (the family is a separate tier); a flick-based variant (`bRequireFlick`)
  is available to kits that prefer smash-style strong attacks.
- Spin- or code-based inputs (supers, special moves) use the `Stick` space so they do not depend on where the
  character faces.

## 10. Kits, weapons, supers, meters, loadouts, skins

### 10.1 Data model

```
FGSTN_LoadoutData (what a player owns and saves)
 ├ PrimaryKit ──────────► UGSTN_PrimaryKitDefinition  "class": base stats, kit move set, meters, rules
 │                          └ PrimaryWeapon ─► UGSTN_WeaponDefinition (Role = Primary)
 ├ AdditionalWeapons[] ─► UGSTN_WeaponDefinition (Role = Support; for now 1)
 ├ Supers[SuperSlotCount] ► UGSTN_SuperDefinition  (weapon independent)
 ├ SlotCustomisations      per weapon: slot → allowed alternative move
 ├ DesignerValues          TMap<FGameplayTag, FGSTN_LoadoutValue>
 └ Skin ────────────────► UGSTN_SkinDefinition  (cosmetic, outside the simulation)
```

- **Binding**: slot → **move** or **any GAS ability class** (e.g. `GA_Jump` → buffered jump and jump-cancel for free),
  with optional buffer/priority overrides and a meter cost.
- **`UGSTN_PrimaryKitDefinition`** (PrimaryDataAsset, primary type `GSTN_PrimaryKit`): display data; `PrimaryWeapon`;
  **base stats** `FGSTN_KitStats` (applied as the attribute init GE, section 10.2); **kit move set** (bindings that do
  not depend on the weapon: jump, dodge, shield, grab, weapon switch); passive abilities; **meter setup**
  (`FGSTN_MeterSetup` per meter: max bars, points per bar, gain rules); `SuperSlotCount` and the super input scheme
  (10.4); `AllowedAdditionalWeapons` and `AllowedSupers` (tag queries on the weapon/super asset tags);
  `CompatibleSkins` (tag query); hurtbox set; locomotion anim layer.
- **`UGSTN_WeaponDefinition`** (PrimaryDataAsset, `GSTN_Weapon`): `Role` (Primary/Support tag), `MoveSet`,
  `AllowedAlternatives` per slot, `ParentWeapon` (inherits and overrides the move set), **stat modifiers while active**
  (a GE class + SetByCaller values, 10.2), passives while active, `FGSTN_WeaponTuning` + `Parameters`
  (SetByCaller for its moves), switch-in/out moves, visuals and anim layer (cosmetic).
- **`UGSTN_SuperDefinition`** (PrimaryDataAsset, `GSTN_Super`): the super move (a move definition or ability), meter
  cost, super tags (`Super.*`), `InputCode` (used only by the per-super input scheme), optional cooldown, kit/weapon
  restrictions (tag requirements).
- **Precedence** when the same slot is bound twice: super slots are separate slots; between kit and active weapon the
  **weapon wins**; the validator reports any remaining overlap.

### 10.2 Stats (*Decision*)

- Stats are **GAS attributes** (`UGSTN_MovementAttributeSet`: walk speed, acceleration, braking, ground friction, air
  control, jump velocity, gravity scale, max fall speed, weight; extendable), so they are predicted and rolled back
  with the ASC state, modified only by GEs, and visible to every system.
- Kit base stats = the kit's attribute init GE. Each weapon's stat modifiers = an **infinite GE applied while the
  weapon is active** (added/removed by the combat model's requests when the active weapon changes, inside the ASC
  tick). Buffs and debuffs stack as ordinary GEs.
- **Mover reads them through a permanent movement modifier** `FGSTN_StatsMovementModifier` (queued once at spawn).
  Mover's stock modes read `UCommonLegacyMovementSettings`, a shared settings object that is not rolled back (the stock
  stance modifier writes it on start/end). The stats modifier instead **rewrites those settings in `OnPreMovement` of
  every simulated frame** from the ASC's current attribute values. The ASC ticks before Mover in the same frame, also
  in re-simulation, so Mover always sees the values of the frame it simulates; nothing depends on state outside the
  sync states.
- Jump height: `UGSTN_JumpImpulseMoveCreator` reads the jump velocity attribute. Project wall modes read their own
  tuning structs; any wall stat that must vary goes through the same modifier pattern later.
- *Verify in M3:* the modifier's writes during re-simulation (harness: weapon switch + forced reconcile, speeds
  compared server/client).

### 10.3 Meters (EX bars)

- **`UGSTN_MeterAttributeSet`**: `ExMeter`, `ExMeterMax`, `SuperMeter`, `SuperMeterMax` (two generic meters; the kit
  names them and sets bars × points per bar). A kit that needs more meters adds an attribute set; the meter API refers
  to meters by `FGameplayAttribute`, so no code assumes two.
- **Spending** = standard GAS cost: `UGSTN_GE_MeterCost` (instant, SetByCaller `Combat.SetByCaller.MeterCost`) set as
  the move ability's cost; `CanActivateAbility` checks it as usual, so the resolver's `CanExecute` rejects an
  unaffordable EX move.
- **Gaining** = GEs applied by the hit pipeline (on hit dealt, hit taken, blocked, whiff) with rates from the kit's
  `FGSTN_MeterSetup`, plus optional passive regen GEs.
- **EX moves** are ordinary slots with a more specific pattern (e.g. special + extra button, or a chord) bound to the
  EX version with a meter cost, and `bFallThroughWhenUnavailable = true`: without meter the normal version comes out.
- UI reads the attributes (replicated with the ASC state; interpolated on proxies).

### 10.4 Supers (weapon independent)

- The loadout fills `SuperSlotCount` super slots (kit-defined, e.g. 3) from the kit's `AllowedSupers`.
- Input is still open, so the kit picks a **super input scheme**:
  - `FixedSlots`: `UGSTN_SuperInputScheme` defines one slot spec per super slot (e.g. hold a modifier + direction);
    super slot *i* (command `Command.Super.Slot<i>`) triggers whatever super the loadout put there.
  - `PerSuperCode` (stratagem style): each super's `InputCode` pattern is its slot (command = the super's own tag
    under `Command.Super`) (e.g. hold the super button, then a
    direction sequence, then release). Expressible with the standard conditions (`Hold` + `Motion` + `Release`).
  - While a code is being entered the scheme may grant `Combat.State.SuperInput` (a context tag other slots and
    movement can react to).
- Super slots are compiled with the kit and weapon slots and validated together **at loadout validation time**
  (conflicts between the chosen codes are a loadout error, reported to the player's UI).
- Supers are available regardless of the active weapon unless their own requirements say otherwise; cost from the
  super meter.

### 10.5 Compiled table, switching, revision

- **Compiled table**: when a loadout is applied (server, not predicted, replicated) the bindings of the kit, **all**
  weapons and the supers are compiled in deterministic order (source: kit, weapon index, super slot; then
  command tag name) and their abilities granted. Each entry knows its source, so the resolver only considers the kit, the
  **active** weapon and the supers.
- **Weapon switching** is a kit move (`UGSTN_GA_WeaponSwitch`, a move with frame data): at its swap frame the combat
  model sets `ActiveWeaponIndex` and requests the stat GE swap. Predicted and rollback-safe; switch slots are ordinary
  slots (cycle, or one per weapon).
- **No `SourceObject` on specs** (*Decision*): a few `UGSTN_GA_Move` specs, the move index lives in the combat sync
  state. `SourceObject` replication breaks when the asset is not loaded on the client, and per-move specs on every AI
  inflate the ASC state.
- **`LoadoutRevision`** in the combat sync state: no predicted activation while client and server revisions differ
  (the table is replicated outside NPP).

### 10.6 Loadout lifecycle and skins

- **`UGSTN_LoadoutDefinition`** (designer preset, `GSTN_Loadout`) and **`FGSTN_LoadoutData`** (runtime, serialisable,
  references by PrimaryAssetId).
- **`UGSTN_LoadoutComponent`** on the PlayerState (survives respawn): `ServerRequestLoadout` validated by
  `UGSTN_LoadoutRules` (kit allows the weapons and supers, slot customisations are allowed alternatives, super codes do
  not conflict), replicated data, applied to the pawn on possession or respawn: specs, kit init GE, primary weapon stat
  GE, meters, compiled table, new revision. Changing the loadout mid-life is not predicted.
- **`UGSTN_LoadoutSubsystem`** (LocalPlayer) + `USaveGame` for saving and loading.
- **`UGSTN_SkinDefinition`** (`GSTN_Skin`): mesh, materials, anim class, VFX overrides, tags for kit compatibility.
  Chosen per player, replicated on the PlayerState, applied by a cosmetic component; never read by the simulation
  (hurtboxes come from the kit, not the skin mesh).
- Asset Manager primary types: `GSTN_PrimaryKit`, `GSTN_Weapon`, `GSTN_Super`, `GSTN_Move`, `GSTN_InputSlot`,
  `GSTN_Loadout`, `GSTN_Skin`.

## 11. Moves

- **`UGSTN_MoveDefinition`** (PrimaryDataAsset), the frame-data source of truth: `TotalFrames`,
  `FirstActionableFrame`, montage + section (played with `PlaySyncedMontage`; root motion → Mover), `AbilityClass`
  (default `UGSTN_GA_Move`, subclass for special moves), cost and cooldown through standard GAS settings, `AimPolicy`.
- **Tracks** = `TArray<TInstancedStruct<FGSTN_MoveTrack>>`, extensible: `Hitbox`, `HurtboxModifier`
  (intangible/invincible/armor), `CancelWindow` (into a command tag query, condition OnHit/OnBlock/OnWhiff/Always,
  jump-cancel), `Movement` (velocity curves, gravity, facing lock → one queued layered move via GE), `Tags`, `Event`
  (gameplay event / projectile / cue at frame N), `Charge` (loop while held), `LandingLag`/auto-cancel for aerials,
  buffer overrides.
- **Startup override** (8.3): `OverrideFrames`, `OverrideInto` (command tag query), `DeferredCommitFrame` (= the end
  of the window): cost, events, movement and hitboxes start there. Cancel windows also target command tag queries.
- **`UGSTN_GA_Move`**: the GAS shell (tags, cost, cooldown, block/cancel, cues, `CommitAbility` at the deferred commit
  frame). No timers of its own: reads frame and state from the combat component; ends when the move ends, is
  interrupted or is replaced.
- The move frame lives in the combat sync state (`MoveStartFrame` + hitstop-paused frames), so hitstop freezes it.
- Animation: the move frame is the truth; the synced montage follows (same tick, paused in hitstop). GASP keeps motion
  matching for locomotion, moves use the montage slot; anim layer per weapon; GAS cues for VFX/SFX.

## 11b. Jump and movement moves

### Variable jump (*Decision*)

- Jump height is **continuous**: it depends on how long the jump button stays held, anywhere between a minimum hop
  and the full jump, not two fixed stages.
- Physics as three gravity phases plus caps, all **movement attributes** (10.2) so kits, weapons and effects tune them:

  | Attribute | Phase |
  |---|---|
  | `JumpVelocity` | Initial upward speed. |
  | `JumpRiseGravityScale` | Rising **while the jump button is held** (and for at most `MaxJumpHoldFrames`). |
  | `JumpCutGravityScale` | Rising after the button is released (or after `MaxJumpHoldFrames`): cuts the jump short. |
  | `FallGravityScale` | Descending (independent from rising). |
  | `MaxFallSpeed`, `FastFallSpeed`, `FastFallGravityScale` | Terminal speeds and fast-fall. |
  | `MinJumpHoldFrames` | Frames before a release starts cutting (the minimum hop). |
  | `AirJumpCount`, `AirJumpVelocity` | Multi-jump. |
  | `JumpSquatFrames` | Ground startup before take-off (0 = immediate). |

- **Jump state lives in the combat sync state** (`JumpPhase`, `JumpStartFrame`, `bJumpCut`, `AirJumpsUsed`,
  `bFastFalling`); "held" is read from the input history (the jump press still held continuously), so it is
  rollback-safe and buffered like any input.
- **Gravity reaches Mover through the stats modifier** (10.2): in `OnPreMovement` it picks the phase's gravity scale
  from the combat state of the same frame (the combat model ticks before Mover) and rewrites Mover's gravity override
  and fall-speed caps. Mover's gravity override is a component field, not rolled back, which is why it is rewritten
  every simulated frame.
- The jump itself is a kit move: `UGSTN_GA_PlatformJump` (plugin) — jump squat frames, impulse from `JumpVelocity`,
  air jumps from `AirJumpCount`, buffered and cancel-window aware like every slot. Wall jumps stay in the project's
  wall modes; the project's `GA_Jump` remains for characters without a combat kit.
- Fast fall input: *Open*. Default proposal: crouch pressed while airborne and past the apex (crouch already means
  "down" in the slot set, 9.1).

### Movement moves

- Movement options are **moves in the kit move set** (command tags under `Command.Movement`), so they get frame
  data, buffering, cancel windows, meter costs and weapon-independence for free: dash/run start (ground flick), air
  dash or directional air dodge, roll, spot dodge, air jumps, fast fall.
- What they need beyond attacks: movement track options (`MomentumMode`: keep / reset / blend the current velocity;
  `bCancelOnLand`; landing lag), intangibility windows (`HurtboxModifier`, so dodges arrive with the hit system), and
  for continuous states (run) a move that loops while a condition holds (`Charge` track pattern).
- Mover modes stay the place for continuous locomotion (walking, falling, wall modes); moves only layer velocity on top
  or override it for their frames.

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
- **Grabs** are links (12b): a `Grab` hitbox creates a grabber/grabbed link; throws are slots active only for the
  grabber role. **Needs the link spike before building.**

## 12b. Links: piggyback, grabs, held objects (*Decision*: one system)

A **link** is a relation between two entities, each with a role: `Combat.Link.Mount` (carrier / rider),
`Combat.Link.Grab` (grabber / grabbed), later `Combat.Link.Held` (holder / object). One system serves all three.

### State and authority

- Each entity's combat sync state has `FGSTN_LinkState { LinkType; Role; PartnerId; LinkFrame; LinkSerial; }`.
- Links are **created and broken by the server** only, through the same inbox mechanism as hits (12.4): the request
  reaches the partner at F+1, the partner's resolution step accepts or refuses, both states are written. On clients
  the partner learns it through a correction (about one rollback, like a hit).
- The initiator may **predict its own side** when the link type allows it (mount: yes, so mounting a teammate feels
  immediate; grab: per design). A refused prediction is corrected.
- Role tags are projected (5.4): `Combat.Link.Role.Rider`, `.Carrier`, `.Grabber`, `.Grabbed`. **Role-only actions are
  plain slots with a context on those tags** (rider-only attacks, carrier-only throws): no special code.

### Piggyback (mount)

- **Mounting**: the rider's mount move has an interaction volume (a hitbox with hit type
  `Combat.HitType.Interaction.Mount`, relationship `Ally` only). The carrier's pipeline step `LinkRequest` accepts if
  its kit allows being mounted, it is not already linked, not in hitstun, and the rider's `Weight` ≤ its
  `MaxCarryWeight` attribute. Input for mount/dismount: *Open* (standard set reserves `Link.Mount`, `Link.Dismount`
  with no default pattern; the kit sets them).
- **Carrier**: moves normally and controls movement; carries the rider's weight as a GE while linked
  (`GE_CarryLoad`: speed/jump modifiers scaled by the rider's weight) and gets the carrier context tag; kit flags decide
  which carrier moves remain allowed.
- **Rider**: action state `Riding`; its Mover goes to a `UGSTN_RidingMovementMode` that does not move it (collision
  with the carrier off). Its **simulated transform is parked**: everything positional while riding (presentation
  attach, hurtboxes, hitbox `Root` space, aim origin) derives from the **carrier's** transform at the mount socket, as
  known on that machine (server: authoritative; carrier's client: predicted; rider's client: interpolated). This avoids
  a rider that constantly mispredicts its own position against an interpolated carrier. The rider keeps its own
  camera and stick; its stick aims its moves.
- **Dismount**: rider input, carrier action (drop/throw), configured hits (`bHitBreaksLink` per role), death,
  distance safety. The server picks the exit position and teleports the rider (one correction).
- **Hits while mounted**: both keep hurtboxes; rules decide whether the carrier shields the rider from some angles
  and which role's hitstun breaks the link.

### Pair moves

- `UGSTN_PairMoveDefinition`: initiator role, initiator move, partner move, `PartnerStartOffsetFrames`, link outcome
  (keep / break / swap roles), requirements on both.
- The initiator executes its move predicted; the **partner move starts through the link inbox** on the server (the
  partner's client is corrected). Design pair moves so the partner's part tolerates that delay: the partner is
  launched, thrown, boosted or plays a reaction (e.g. carrier throws the rider, rider jumps off the carrier for a boosted
  jump).
- *Open:* if playtests show the partner delay is too visible on the partner's screen, the next step is forward
  predicting the partner on the initiator's client during a pair move (needs ForwardPredict LOD for that pair; the fork's
  ASC does not run on simulated proxies today, section 2 fact 5).

### Grabs and held objects

Grabs (M6) are links with the hostile relationship and the grabbed playing the parked role (follows the grabber's
socket); throws are grabber-role slots. Held objects (13b) are links whose partner is a server-simulated object.

## 13. Many enemies (40+)

Two paths with the same Core logic:

- **Fighters** (players, bosses, elites): the full NP model above.
- **Minions**: `UGSTN_MinionCombatComponent`, **server only**, no NP model per entity. `UGSTN_MinionCombatSubsystem`
  ticks them in batch at the fixed tick (moves, hitboxes, hurtboxes, knockback from Core), with LOD (far/inactive skip
  queries and debug) and the world subsystem's broadphase. Compact replicated state (FastArray, reduced rate), clients
  interpolate + hit presentation. Player hits on minions resolve on the server with lag compensation; minion hits on
  players go to the player's inbox (server), so a correction on the player's client.
- Promoting a minion to fighter = swapping the component.

## 13b. Held and thrown objects (later, not designed)

Requirement noted, out of scope until after M7. Constraints already known, so the earlier milestones do not close
the door: holding is a link (12b, `Combat.Link.Held`) with an object size class, the object's own simulation is
suspended while held; throwing is a move whose `Projectile`-space hitbox becomes the object's; object size classes
select the hold/throw move set (a kit or weapon move set keyed by size tag). Objects are minion-like (server-simulated)
unless a design needs prediction.

## 14. Editor and debug

- Move asset editor: preview viewport, frame timeline (ruler, frame stepping), one row per track, hitbox gizmos,
  `FGSTN_HitData` details, "Bake sockets" button.
- `IsDataValid`: frame ranges, overlapping windows, slot conflicts (section 9).
- CVars `gstn.Combat.Debug.Hitboxes/Hurtboxes/Buffer`, input display widget (notation of [combat-m1-core-input.md](combat-m1-core-input.md) §4, frames),
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
- Combat characters jump with the plugin's `UGSTN_GA_PlatformJump` bound to a kit slot (variable jump, 11b); the
  project's `GA_Jump` stays for characters without a kit (remove its `ActivationInputs` on any character that has
  both, or it would activate twice). Wall jumps stay in the wall modes.
- `UGSTN_MoverSimStateTracker` moves from the project into `GSTNCombatNP` (it is generic Mover code the plugin needs
  for facing and Mover tags); the project module then depends on `GSTNCombatNP` (M2).
- Wall modes: the "movement locked" extension point (12.6); wall stats through the stats modifier pattern (10.2).
- The project's attribute sets stay separate from the plugin's (`UGSTN_MovementAttributeSet`, `UGSTN_MeterAttributeSet`,
  `UGSTN_CombatAttributeSet`); the test attribute set remains test content.
- Game command tags in `Config/Tags/GSTNCombatCommands.ini` (9); CLAUDE.md's tag rule gets this exception.
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

Each milestone ends with a build, its tests and the harness run clean (testing.md), and with this document updated
where the implementation differs. Milestones are sequential: each spec assumes the previous ones are merged.

| # | Milestone | Content | Specification |
|---|---|---|---|
| M0 | Risk spike | **Done** (section 17). | — |
| M1 | Core input layer | `GSTNCombatCore`: samples, classifier, history, conditions, matcher, buffer, reset rules, slots, validation, Automation tests. | [combat-m1-core-input.md](combat-m1-core-input.md) |
| M2 | Input in the simulation | History and classifier state in the combat sync state, stick + `ForcedSlot` in the command, player/AI input sources, Mover state reader moved into the plugin, slot bindings → activation of any GAS ability, harness scenarios. | [combat-m2-sim-input.md](combat-m2-sim-input.md) |
| M3 | Moves, kits, weapons, loadouts, jump | `GSTNCombatGAS`, attributes (stats, meters), stats → Mover modifier (incl. jump gravity phases), `UGSTN_GA_Move` with synced montage, cancel windows, movement track, variable jump, air jumps, fast fall, dash, primary kit, support weapon and predicted switch, EX moves, supers, compiled table, `LoadoutRevision`, loadout component and save, skins. | [combat-m3-moves-kits.md](combat-m3-moves-kits.md) |
| M4 | Hits | Hurtboxes, hitboxes, snapshot, inbox, resolution pipeline, damage and meter gain, hitstop (Mover freeze, wall lock), hitstun/knockback/DI, teams and variants, lag compensation, hit presentation, dodges (intangibility). | [combat-m4-hits.md](combat-m4-hits.md) |
| M5 | Editor | Move editor, socket baking, validators UI, debug draw, input display. | [combat-m5-m9.md](combat-m5-m9.md) |
| M6 | Defence and links | Shield, parry, tech, SDI; link system spike, links, grabs/throws. | [combat-m5-m9.md](combat-m5-m9.md) |
| M7 | Piggyback and pair moves | Mount link, riding mode, role-only slots, carry load, pair moves. | [combat-m5-m9.md](combat-m5-m9.md) |
| M8 | Minions | Server subsystem, LOD, compact replication. | [combat-m5-m9.md](combat-m5-m9.md) |
| M9 | Stock GAS adapter | Optional. | [combat-m5-m9.md](combat-m5-m9.md) |

