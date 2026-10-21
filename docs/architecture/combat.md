# Combat system (`GSTNCombat` plugin)

Design document for a fighting-game / platform-fighter combat layer on top of GAS, simulated by Network Prediction
(the GAS-NPP fork, see [gas-npp.md](gas-npp.md)). Source: `Plugins/GSTNCombat/`.

Status (2026-10-19): **M0 (risk spike) done and verified**, M1 implemented, M2 in progress; the rest is design. This
file is the architecture; each milestone has its own implementation specification (section 18). Sections marked
*Decision* are settled; *Open* items are listed where they apply.

> **Revision 2026-10-19 (combat abilities, *Decision*):** "moves" are no longer a concept of their own. Everything a
> character does is a GameplayAbility: `UGSTN_GA_Base` (plugin base: activation context, locks, frame clock) and
> `UGSTN_GA_BaseCombat` (adds a frame-data **timeline**: hitboxes, cancel windows, movement...). An input command is
> one activation source among GAS triggers, effects, timers, AI and other abilities; abilities started without input
> are **adopted** by the combat model one frame later (section 11). The number of commands is not fixed: kits start from
> the default set and add or remove commands, and a command may stay unbound (9.1). **Lock tags** `Combat.Lock.*`
> express "can move but not attack" and similar (11.4). The M4–M9 specifications still say *move* for a timeline
> ability.

> **Revision 2026-10-19 (input plugin, *Decision*):** the input layer (sections 6–9, M1 and the input half of M2) moves
> to its own plugin **`GSTNInput`** ([input.md](input.md)), usable without the combat plugin. `GSTNCombat` depends on
> it and embeds its simulation in the combat model. Input tags lose the combat prefix (`Input.Button.*`,
> `Input.Context.*`, `Input.Lock.Command*`, `Input.BufferEvent.*`, `Input.Event.CommandExecuted`). The rules of
> sections 6–9 are unchanged.

## 1. Goals

- Fighting-game input: designer-defined combinations (button, direction, motion, chord, charge, hold, release, mash),
  a **time buffer** and a **hold buffer** (a press stays valid while the button is held until the ability can come out, as
  in Super Smash Bros. Ultimate), standard buffer resets plus designer-defined ones.
- **Weapons** made of **combat abilities** (ordinary GameplayAbilities: attacks with hitboxes, multi-hits, heals on
  allies, stances, camera-aimed projectiles, minion spawns...) bound to **commands** (input slots, e.g. Attack + Back).
  How many commands exist is up to the game: the plugin ships a default set, kits add or remove commands, and a command
  may stay unbound. New slots for non-standard combinations are created without code and are checked against the
  other slots for conflicts.
- Abilities also start **without input**: GAS triggers (gameplay events, tags), effects, timers, AI, other abilities
  (e.g. an automatic attack 4 s after an event).
- **Primary kit**: the character's "class" (as in a class-based MMO): a primary weapon plus base stats (speed, jump
  height...) and kit-wide abilities. Independent of the look (**skins** are cosmetic and combine freely).
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
hitboxes on a **frame-data timeline in the combat ability** (inline or a shared timeline asset); the time unit is the **NPP fixed-tick frame**; knockback model
**pluggable, Smash-like by default**; players and AI use **the same pipeline**; **no lock-on**; friendly fire per game
mode, with **per-relationship hit variants**; scale target **40+ enemies** at once; **loadout = primary kit + additional
weapons + super slots**, skins separate; stats are GAS attributes, weapons modify them while active.

## 2. Constraints from the fork (verified in code)

| # | Fact | Consequence |
|---|---|---|
| 1 | `FAbilitySimSyncState` is hard-coded; `InternalSimulationTick`/`FillSyncState` are not virtual. | Combat state lives in **its own NP model** on a component. |
| 2 | Sort priorities: ASC = `PreKinematicMovers - 2` (48), Mover = 50. | Combat model at **47**: before the ASC and Mover. |
| 3 | Fork input: 5 bools per `UInputAction`, no axes, no history; `TryActivateAbilitiesFromInput` tries only the first matching ability; `GetSynced*` getters read the wrong frame during resim. | Combat has **its own input command**; combat abilities never use `ActivationInputs`. |
| 4 | GE durations and `UWaitForDurationPredictionTask` use absolute sim time; NPP has no per-entity freeze. | **Every combat timing is a frame count in the combat sync state** (pauses in hitstop). GEs only for attributes, tags, cues. |
| 5 | Nothing runs on simulated proxies (`SimulatedProxyNetworkLOD=Interpolated`): `ApplyGameplayEffectSpecToSelf` and `TryActivateAbility` return early. | Victim reactions are server-authoritative; the attacker predicts only its own side. |
| 6 | Predicted montages exist (`UNetMontageSimulator`, `PlaySyncedMontage`, `PauseCurrentSyncedMontage`); root motion goes to Mover through `FLayeredMove_SyncMontageRootMotion`. | Timeline abilities play synced montages; hitstop pauses them. |
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
| `GSTNCombatCore` (Runtime) | Core, CoreUObject, Engine, GameplayTags, **GSTNInputCore** | Combat content that is **pure and deterministic**, built on the input core: the six standard button tags, the default command set (`UGSTN_StandardSlots`, command tags), later hit shapes and knockback models. No GAS, no NPP. Covered by Automation tests. The input layer itself is `GSTNInputCore` ([input.md](input.md)). |
| `GSTNCombatGAS` (Runtime) | Core, GameplayAbilities, GameplayTasks | Native tags, `UGSTN_CombatAttributeSet`, damage GE and execution, hit effect context, weapons, loadouts, loadout component. Compiles against stock GAS and the fork (same module name). |
| `GSTNCombatNP` (Runtime, **exists**) | Core, GAS, NetworkPrediction, AbilitySystemSimulation, Mover, **GSTNInputNP** | **Fork adapter**: combat NP model (embeds the input simulation of `GSTNInputNP` and is its host), simulation component, `UGSTN_GA_Base` and `UGSTN_GA_BaseCombat`, freeze/knockback layered moves, hit resolution. Input command, sources, tracker and binding set come from `GSTNInputNP`. |
| `GSTNCombatEditor` (Editor) | all | Timeline editor (timeline assets and inline timelines; viewport), socket baking, slot conflict validator UI, debug draw. |

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
| Slot | Pattern + priority, identified by its command tag. Says only *how* a command is input (e.g. *Attack + tilt*); independent of context. |
| Binding | Command tag + context requirements → ability class (optionally a timeline override), inside a weapon or a kit. One command can have several bindings (grounded, airborne, on wall...) or none. |
| Combat ability | Child of `UGSTN_GA_Base`: a GameplayAbility the combat layer knows (activation context, locks, frame clock). Started by a command or by anything GAS offers. |
| Timeline ability | Child of `UGSTN_GA_BaseCombat`: a combat ability with a frame-data timeline run by the combat model. Called *move* in older text and in the M4–M9 specs. |
| Timeline | Frame data + tracks (hitboxes, cancel windows, movement, tags, events): inline in the ability or a shared `UGSTN_CombatTimeline` asset. |
| Lock tag | `Input.Lock.Command[.<root>]` blocks commands, `Combat.Lock.Movement` blocks movement input; granted by abilities and effects. |
| Adoption | The combat model taking over a timeline ability that started without a command, one frame after its activation. |
| Weapon | Set of bindings, stat modifiers while active, passives, tuning, visuals. Role: primary or support. |
| Primary kit | Character class: primary weapon, base stats, command set and kit bindings (weapon-independent), meters, super slot count, allowed weapons and supers. |
| Super | Weapon-independent combat ability placed in a super slot of the loadout. |
| Meter | Resource attribute (EX, super...) gained in combat and spent as GAS cost. |
| Skin | Cosmetic look, outside the simulation, compatible with kits by tags. |
| Loadout | Primary kit + additional weapons + supers + customisations + designer values. |
| Link | Authoritative relation between two entities with a role each: grab (grabber/grabbed), mount (carrier/rider), later held object. |
| Pair move | Action involving both linked entities: the initiator's ability plus a partner ability started through the link. |

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
**Lock tags** (`Combat.Lock.*`, 11.4) are the opposite case: granted by abilities and effects with ordinary counts
(`ActivationOwnedTags`, GE granted tags); the combat model only reads them.

### 5.5 Rules for combat simulation code (extend gas-npp.md)

1. All timers of the combat model and of timelines are frame stamps in combat sync state (`EndFrame`/`StartFrame`,
   plus paused-frame counters for hitstop), never GE durations or wait tasks. Other abilities may use the fork's wait
   tasks (absolute time, hitstop does not pause them) or the **combat clock** (11.5) when they must pause.
2. No `HasAuthority`/resim branches; server-only behaviour comes from where the instance ticks (inbox, section 12.4).
3. Prefer **end-frame stamps over countdowns**: an idle entity's state then does not change frame to frame (cheap
   delta serialization, section 15).
4. Every sync state field is in `NetSerialize`, `ShouldReconcile`, `ToString` and `Interpolate`.

### 5.6 Authoritative combat events (*Decision*)

Simulation code re-runs on the client during re-simulation, so anything that must count once (quests, achievements,
statistics, analytics, kill feed) must not listen to simulation callbacks. The combat model publishes **authoritative
events** only where they cannot be rolled back: on the server, from `FinalizeFrame`-time state that the server
produced.

- `UGSTN_SimEventSubsystem` (world subsystem, server): `Broadcast(const FGSTN_SimEvent&)` and
  `Listen(FGameplayTagQuery, delegate)`. An event is `{ FGameplayTag EventType; FGameplayTagContainer Tags; int32
  Frame; TWeakObjectPtr<AActor> Instigator, Target; float Magnitude; }`.
- `Tags` carries everything a listener may filter on: command tag, ability tags, weapon/kit tags, relationship, link role,
  context tags (airborne, on wall...). Example quest: "10 × `EventType = Input.Event.CommandExecuted` with
  `Command.Attack.CrouchDirectional` and `Weapon.Role.Support` and `Input.Context.Airborne`".
- Event types: `Input.Event.CommandExecuted` (M2), `TimelineStarted` (with the activation source: command or
  adopted), `TimelineFinished`, `WeaponSwitched`, `SuperUsed` (M3), `HitLanded`,
  `HitReceived`, `KO` (M4), `LinkFormed/Broken`, `PairMove` (M6–M7).
- Clients that need feedback (UI flashes, tutorial prompts) use replicated results or predicted presentation, never
  these events. A client-side mirror can be added later from replicated server events if a feature needs it.

## 6. Input

The input layer is the **`GSTNInput` plugin** ([input.md](input.md)): this section and sections 7–9 keep its rules;
input.md says where each piece lives and what the host (the combat model) adds.

### 6.1 Input command (`FGSTN_InputCmd`, NP model input)

| Field | Type | Notes |
|---|---|---|
| `HeldButtons` | uint32 | Held at sample time. |
| `PressedButtons` | uint32 | Pressed since the previous sample: a tap shorter than one frame is not lost. |
| `StickX`, `StickY` | int8 | Move stick **as on the controller** (raw, +Y up), quantized to [-127, 127]. *(M2)* |
| `CameraYaw` | uint8 | Control-rotation yaw at sample time, 256 steps. The simulation derives the world-plane stick from it, so every direction space is available deterministically. *(M2)* |
| `ForcedCommand` | uint16 | 0 = none. Lets AI/scripts request a command without synthesising inputs. *(M2)* |
| `AimYaw`, `AimPitch` | uint16 | Camera aim (65536 steps) for camera-aimed abilities (11.6). Produced only when the input config enables camera aim; serialized only when changed. *(M3)* |

Exists today with the first two fields (M0). Serialized packed.

### 6.2 Input sources

`IGSTN_InputSource` produces the command on the controlling machine: `UGSTN_PlayerInputSource` (Enhanced
Input; buttons are the `UInputAction`s of the input config; taps caught through the action's Started event, not by
polling) and `UGSTN_AIInputSource` (BT/StateTree tasks: Press, Hold, Release, RequestCommand via `ForcedCommand`).
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
- Facing comes from Mover's simulated state at end of F-1. `UGSTN_MoverSimStateTracker` lives in the
  input plugin (`GSTNInputNP/Mover/`; the project uses the plugin's).
- Each timeline ability has an `AimPolicy` (`KeepFacing | SnapToStick | SnapIfAimOrTilt | Camera`, max turn angle,
  last frame turning is allowed): a directional attack goes **towards the stick's world direction**; `Camera` turns to
  the camera's aim (11.6); rotation goes through Mover, not the actor.

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
no timeline ability holds the character, a **cancel window** is open, or the running timeline is inside its **startup
override window** (8.3), resolve the candidate commands (8.2) and request the winner; (4) consume the button events it
used and the losers of its simultaneous group.

### 8.1 Buffering

- **Time buffer**: the terminal press is valid if unconsumed and its age (frames since the press, hitstop frames
  excluded) ≤ `BufferFrames`.
- **Hold buffer**: also valid past the window while the button has been held **continuously** since that press, up to
  `MaxHoldBufferFrames` (0 = unlimited). Per slot: on/off.
- **Direction policy** per slot: `AtPress` (directions checked at the anchor) or `AtExecution` (checked at the frame the
  move comes out).
- **Consumption**: one press triggers at most one command.
- **Reset rules** = list of `TInstancedStruct<FGSTN_BufferResetRule>` at global → weapon → ability level: on a frame event
  (hit received, ability activated, landed, hit confirmed, state entered), on tag added, after N idle frames, custom.
  Each flushes all or some buttons (marks their presses consumed, which also ends their hold buffer).
- Defaults in `UGSTN_InputSettings` (DeveloperSettings); overrides per slot and per timeline.

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

Steps 2–4 are the **default list of priority criteria** (*Decision*, 2026-10-19, [input.md](input.md) §4.1): their
order is a Project Settings list and a new criterion is one small C++ struct. Step 1 (press group) stays a separate,
time-based stage before them.

Then, in that order: the first command that **can execute** wins — it has a binding whose context requirements hold
right now (9.2) and that binding's ability can activate. A command that matches but
cannot execute **reserves** its terminal press for this frame (lower commands cannot steal it) unless it sets
`bFallThroughWhenUnavailable` (EX versions without meter fall through to the normal version).

**Unbound commands** (*Decision*): a command with no binding in the current kit, active weapon and supers is legal and
is **not a candidate at all**: it neither executes nor reserves a press. A command that is bound but **locked**
(11.4) cannot execute this frame and reserves its press like any other, so the press stays buffered (time or hold
buffer) until the lock ends.

**Losers are consumed**: when a command wins, the unconsumed presses of its press group whose buttons are in the
settings' `ExclusiveButtons` (default attack, strong, special, super) are consumed too, so the losing attack does not
come out a few frames later from the buffer.

The validator reports the conflicts these rules leave: same tier, same specificity, same press group possible.
Context overlaps are a binding question (9.2).

### 8.3 Startup override window (*Decision*, Smash-style interrupts)

Smash Ultimate lets the first frames of a move be replaced: jabs can be cancelled into a jump within 1 frame, tilts and
smashes within 2 (attack cancel into a short-hop aerial; with a forward tilt turning the character first, this gives
the "instant back air"), and the jump squat can be interrupted by up smash or up special. The window never overlaps an
active hitbox. Generalised here:

- Every timeline has a **startup override window** of `OverrideFrames` from its first frame (default per command tier
  in the settings, e.g. jab-like 1, other attacks and specials 2; per-timeline override; 0 disables).
- Inside the window the resolver runs although a timeline ability holds the character. A command may **replace** it if
  it matches the timeline's `OverrideInto` query on command tags: by default "any command of a higher tier", plus explicit
  entries (jump from ground attacks = attack cancel; up attack / up special from the jump squat).
- **The replaced ability leaves no trace except its turn**: its cost is committed, its events fired, its movement and
  hitboxes started only from the end of the window (`DeferredCommitFrame`); the validator forbids anything earlier.
  The replacing ability keeps the facing the first one set, which is how the instant back air falls out naturally.
- **Chords without input delay**: attack pressed → the attack starts at once; special pressed within the window → the
  attack + special command (higher specificity and tier) replaces it. No command waits for a possible second button.
  For that, chord-capable commands need `OverrideFrames ≥ SimultaneousFrames`. The old **chord grace** (wait before
  starting a single-button command) stays as an option, off by default, for abilities that must not show even one frame
  before a chord.
- Presentation: remote proxies may show 1–2 frames of the replaced ability before the switch (as in Smash); montages
  use a short blend-in.

## 9. Command tags, slots and conflicts

- **Command tags** (*Decision*): every slot is identified by a gameplay tag under `Command.` (hierarchical:
  `Command.Attack.Neutral`, `Command.Special.CrouchDirectional`, `Command.Strong.Directional`, `Command.Movement.Dash`,
  `Command.Super.Slot1`, `Command.Link.Mount`...). The tag, not the input, is what the rest of the game refers to:
  bindings (command → ability), cancel windows and override windows (tag queries), loadout customisations, AI requests
  (`RequestCommand(Tag)`), UI and tutorials, events and quests (5.6). Changing how a command is input touches only
  its slot.
- The hierarchy replaces the former slot categories: "cancel into `Command.Special`" accepts every special; tiers
  (8.2) are assigned to roots.
- **Buttons** keep their own tags (`Input.Button.*`, as Lyra's input tags); bits and indices exist only inside
  the simulation and on the wire.
- Abilities carry their ordinary `AbilityTags` (identity for events, cancels and queries); weapons, kits, supers and
  hit types carry asset tags (`Weapon.*`, `Kit.*`, `Super.*`, `Combat.HitType.*`), so events and queries can filter on
  them.
- **Commands are context-free** (*Decision*): a command is an input decorated with meaning ("attack with the stick
  neutral"), the same in the air, on the ground or on a wall. Whether the character is airborne decides which **binding**
  of the command runs (9.2), never which command was input.
- `FGSTN_InputSlotSpec`: command tag + pattern + manual priority offset + buffer and override overrides. No context.
- `UGSTN_InputSlotAsset` (PrimaryDataAsset) wraps one spec for sharing; `UGSTN_InputSlotSet` lists slots (the plugin
  ships a standard set); weapons may also declare **inline slots**. A new command = a new tag + a slot (asset or
  inline entry), no code.
- **Tag sources** (exception to the project's "native tags only" rule, *Decision*): the plugin's standard command,
  button and event tags are native; game-specific commands created by designers live in a dedicated tag ini
  (`Config/Tags/GSTNCommands.ini`) so they need no code. Everything else stays native.
- **Conflict validation** (`IsDataValid` on slot sets, later weapons, kits and loadouts): every slot's canonical input is
  run through every other slot's matcher. Conflicts the priority rules (8.2) do not resolve → **error**
  (ambiguous); a higher-priority slot always matching another slot's canonical input → **warning** (shadowed). Two
  slots with the same command tag in one set → error.
### 9.1 Default command set (*Decision*)

This is the **default set**, shipped as a slot set asset generated from `UGSTN_StandardSlots`. It is a starting point,
not a requirement (*Decision*, 2026-10-19): a kit starts from it, adds commands (inline or shared slots) and removes
the ones it does not use (10.1); any command may also stay unbound (8.2). A character can have fewer or more
activatable abilities than the defaults.

Three families (normal attack, strong attack, special) × four input shapes, all anchored by the family's button press
(one step). The same commands serve the ground and the air (bindings decide, 9.2):

| Shape | Analogue (Smash, reference only) | Extra conditions | Aim policy |
|---|---|---|---|
| `Neutral` | jab / neutral special | `Direction(Facing, any, MaxZone = Aim)` | `SnapIfAimOrTilt` (a light tilt aims it) |
| `Directional` | side tilt / side special | `Direction(Facing, any of the 8, MinZone = Tilt)` | `SnapToStick` (the move goes where the stick points) |
| `CrouchNeutral` | up tilt / up special | `Hold(Crouch, 0)` + `Direction(Facing, any, MaxZone = Aim)` | `SnapIfAimOrTilt` |
| `CrouchDirectional` | down tilt / down special | `Hold(Crouch, 0)` + `Direction(Facing, any of the 8, MinZone = Tilt)` | `SnapToStick` |

Command tags: `Command.Attack.<Shape>`, `Command.Strong.<Shape>`, `Command.Special.<Shape>`: 12 commands. Plus chord
commands (`Command.Combo.AttackSpecial.<Shape>`: `Chord(Attack, Special)` + the shape's conditions, higher tier),
supers (10.4), movement (11b) and link commands (12b). A kit typically binds each of them twice (grounded and
airborne), e.g. `Command.Attack.Neutral` → jab on the ground, neutral air in the air.

- In 3D "side" means **any** tilted direction, the attack turning towards it; commands that care about forward vs back
  relative to the character use a `Facing` mask (e.g. `FacingForward`) instead of "any of the 8".
- Crouch versions win over plain ones by specificity; a crouch press up to `SimultaneityFrames` after the attack press
  still counts. The crouch button is both a movement input (Mover stance) and a combat modifier: slots read the
  **button held state from the history**, not Mover's crouch tag (a frame late, and blockable).
- Strong attacks: a strong button by default (the family is a separate tier); a flick-based variant (`bRequireFlick`)
  is available to kits that prefer smash-style strong attacks.
- Spin- or code-based inputs (supers, special moves) use the `Stick` space so they do not depend on where the
  character faces.

### 9.2 Bindings and context (*Decision*)

- A binding is `{ Command; Requirements; Ability; optional timeline override }` (any GAS ability class; the
  override only for `UGSTN_GA_BaseCombat` children, so one ability class can serve several weapons' timelines). `Requirements` (`FGSTN_TagRequirements`: required and
  ignored tags, a Core type with the semantics of `FGameplayTagRequirements`) is checked against the character's
  **context tags** of the frame: the owner's gameplay tags (end of F-1, rolled back), Mover's simulated tags (tracker),
  and context tags the combat model projects from them so bindings never depend on Mover tag names:
  `Input.Context.Grounded`, `.Airborne`, `.OnWall`, plus link roles (`Combat.Link.Role.*`) and states
  (`Combat.State.*`).
- **Selection**: among a command's bindings whose requirements hold, the one with the most required tags wins (an
  on-wall binding requiring `Airborne` + `OnWall` beats an airborne one); a tie is a validation error. No binding holds →
  the command cannot execute this frame (8.2: its press is reserved, or the next command gets it with
  `bFallThroughWhenUnavailable`).
- **Validation** of a binding list: two bindings of the same command whose requirements can hold together and have the same
  number of required tags → error. A command with no binding is legal (unbound, 8.2): no diagnostic.
- Role-only actions (rider, carrier), weapon-only, state-only (while charging, while shielding) are all just binding
  requirements; no command is duplicated for them.

## 10. Kits, weapons, supers, meters, loadouts, skins

### 10.1 Data model

```
FGSTN_LoadoutData (what a player owns and saves)
 ├ PrimaryKit ──────────► UGSTN_PrimaryKitDefinition  "class": base stats, commands, kit bindings, meters
 │                          └ PrimaryWeapon ─► UGSTN_WeaponDefinition (Role = Primary)
 ├ AdditionalWeapons[] ─► UGSTN_WeaponDefinition (Role = Support; for now 1)
 ├ Supers[SuperSlotCount] ► UGSTN_SuperDefinition  (weapon independent)
 ├ SlotCustomisations      per weapon: command → allowed alternative ability
 ├ DesignerValues          TMap<FGameplayTag, FGSTN_LoadoutValue>
 └ Skin ────────────────► UGSTN_SkinDefinition  (cosmetic, outside the simulation)
```

- **Binding**: command + context requirements → **ability class** (any GAS ability, e.g. `GA_Jump` → buffered jump and
  jump-cancel for free; a timeline ability may get a timeline override), with optional buffer/priority overrides
  (9.2). Meter costs are the ability's GAS cost.
- **`UGSTN_PrimaryKitDefinition`** (PrimaryDataAsset, primary type `GSTN_PrimaryKit`): display data; `PrimaryWeapon`;
  **base stats** `FGSTN_KitStats` (applied as the attribute init GE, section 10.2); **command set** (the default set
  plus added slots minus removed commands, 9.1); **kit bindings** (weapon-independent: jump, dodge, shield, grab,
  weapon switch); **triggered abilities** (granted, started by GAS triggers or other abilities, never by a command);
  passive abilities; **meter setup**
  (`FGSTN_MeterSetup` per meter: max bars, points per bar, gain rules); `SuperSlotCount` and the super input scheme
  (10.4); `AllowedAdditionalWeapons` and `AllowedSupers` (tag queries on the weapon/super asset tags);
  `CompatibleSkins` (tag query); hurtbox set; locomotion anim layer.
- **`UGSTN_WeaponDefinition`** (PrimaryDataAsset, `GSTN_Weapon`): `Role` (Primary/Support tag), `Bindings`,
  `AllowedAlternatives` per command (ability classes), `ParentWeapon` (inherits and overrides the bindings), **stat modifiers while active**
  (a GE class + SetByCaller values, 10.2), passives while active, `FGSTN_WeaponTuning` + `Parameters`
  (SetByCaller for its abilities), switch-in/out abilities, visuals and anim layer (cosmetic).
- **`UGSTN_SuperDefinition`** (PrimaryDataAsset, `GSTN_Super`): the super ability, meter cost, super tags (`Super.*`), `InputCode` (used only by the per-super input scheme), optional cooldown, kit/weapon
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
  the ability's cost; `CanActivateAbility` checks it as usual, so the resolver's `CanExecute` rejects an
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
  **active** weapon and the supers. Removed commands and unbound commands produce no entry.
- **Timeline registry**: every (timeline ability class, timeline override) pair the loadout can run (bound ones, the
  kit's triggered abilities, passives), in deterministic order. Its index is what the combat sync state stores for the
  running timeline; a timeline ability outside the registry cannot be adopted (logged, validation error on the kit).
- **Weapon switching** is a kit timeline ability (`UGSTN_GA_WeaponSwitch`): at its swap frame the combat
  model sets `ActiveWeaponIndex` and requests the stat GE swap. Predicted and rollback-safe; switch slots are ordinary
  slots (cycle, or one per weapon).
- **One spec per ability class, no `SourceObject`** (*Decision*, revised 2026-10-19): every combat ability is its own
  class (usually a Blueprint child), so GAS triggers can start it and it knows its own timeline; the combat sync state
  stores the registry index of the running timeline. `SourceObject` stays unused (its replication breaks when the asset
  is not loaded on the client). Cost: one spec per ability, about 30–60 per fighter, acceptable; minions use their own
  path (section 13).
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
- Asset Manager primary types: `GSTN_PrimaryKit`, `GSTN_Weapon`, `GSTN_Super`, `GSTN_CombatTimeline`, `GSTN_InputSlot`,
  `GSTN_Loadout`, `GSTN_Skin`.

## 11. Combat abilities (*Decision*, 2026-10-19)

### 11.1 Classes

- **`UGSTN_GA_Base`** (`GSTNCombatNP`, derives from `UNpGameplayAbility`): base of every ability the combat layer
  knows. Gives the **activation context** (11.5), the **combat clock** and `WaitCombatFrames` (11.5), the trigger
  button's held state and `bEndOnTriggerRelease` (stances, hold-to-aim), camera aim (11.6), and the interrupt policy
  (11.7). Otherwise an ordinary GAS ability: tags, costs, cooldowns, block/cancel, cues, triggers. Non-attack abilities
  (heals, buffs, stances, aimed shots, minion spawns) derive from it directly.
- **`UGSTN_GA_BaseCombat`** (child): adds a **timeline**, executed by the combat model: hitboxes and the timeline frame
  live in the combat sync state, so hitstop freezes them. The timeline is either **inline** on the ability
  (`FGSTN_CombatTimelineData`) or a shared **`UGSTN_CombatTimeline`** asset (same struct); a binding may override it
  (9.2), so one ability class serves several weapons. Designers subclass in Blueprint and fill the timeline.
- Examples: multi-hit = several hitbox keys or a hitbox with a re-hit interval in one timeline; a heal on allies = a
  hitbox whose `Ally` relationship variant applies a heal GE (12.3); a minion spawn = an `Event` track or plain
  ability logic; a stance = `UGSTN_GA_Base` with `bEndOnTriggerRelease` and lock tags (11.4).
- The project's `UGSTN_GameplayAbility` may later derive from `UGSTN_GA_Base`; until then the two coexist.

### 11.2 Activation sources

- **Command** (input or `ForcedCommand`): the combat tick resolves it and requests the activation through the ASC
  pre-tick (5.2); a timeline ability starts **in the same frame**.
- **Anything else**: GAS triggers (gameplay events, owned tags), effects that grant or activate, AI calling
  `TryActivateAbility`, other abilities, timers. A plain combat ability simply runs. A **timeline ability is adopted**:
  activated at F, it marks itself as waiting (`Combat.Ability.AwaitingAdoption`) and does nothing else; the combat tick
  at F+1 adopts it (frame 0 of its timeline = F+1) and from then on it runs exactly like a command-started one. One
  start path, deterministic; cost **one frame** (16 ms) for automatic activations, accepted (*Decision*).
- Adoption looks only at the loadout's **timeline registry** (10.5). Several abilities waiting in the same frame: the
  lowest registry index wins, the others are cancelled.
- Rules: no `AbilityTriggers` on `Combat.State.*` tags (5.4). Triggers from **another entity's** events (e.g. "when I
  am hit") are server-authoritative like every victim reaction (12.7): on the owning client they arrive as a
  correction. Predicting them is not planned (*Decision*).
- *Verify in M3 (spike):* the fork runs `AbilityTriggers` (gameplay event and owned tag) inside the simulation and
  re-runs them exactly once in re-simulation.

### 11.3 Timelines

- **`FGSTN_CombatTimelineData`**, the frame-data source of truth: `TotalFrames`, `FirstActionableFrame`, montage +
  section (played with `PlaySyncedMontage`; root motion → Mover), `AimPolicy`, startup override (8.3: `OverrideFrames`,
  `OverrideInto`, `DeferredCommitFrame` = the end of the window: cost, events, movement and hitboxes start there),
  buffer reset rules, tracks.
- **Tracks** = `TArray<TInstancedStruct<FGSTN_TimelineTrack>>`, extensible: `Hitbox`, `HurtboxModifier`
  (intangible/invincible/armor), `CancelWindow` (into a command tag query, condition OnHit/OnBlock/OnWhiff/Always,
  jump-cancel), `Movement` (velocity curves, gravity, facing lock → one queued layered move via GE), `Tags`, `Event`
  (gameplay event / projectile / cue at frame N), `Charge` (loop while held), `LandingLag`/auto-cancel for aerials,
  buffer overrides.
- The timeline frame lives in the combat sync state (start frame + hitstop-paused frames). The ability has no timers of
  its own: it reads its frame from the combat component and ends when the timeline ends, is interrupted or replaced.
- Animation: the timeline frame is the truth; the synced montage follows (same tick, paused in hitstop). GASP keeps
  motion matching for locomotion, timelines use the montage slot; anim layer per weapon; GAS cues for VFX/SFX.

### 11.4 Lock tags (*Decision*)

- **`Input.Lock.Command`** blocks every command; **`Input.Lock.Command.<root>`** blocks the commands under
  `Command.<root>` (`Input.Lock.Command.Attack` → every `Command.Attack.*`). Designer command roots get their lock tag
  in the same tag ini (9). A locked command reserves its press (8.2), so it stays buffered.
- **`Combat.Lock.Movement`** blocks movement input: the stats modifier (10.2) zeroes input-driven speed and
  acceleration, the wall modes' "movement locked" extension point (12.6) reads it too; gravity and layered moves still
  apply.
- Granted like any tag: `ActivationOwnedTags` of an ability ("can move but not attack" = an ability owning
  `Input.Lock.Command.Attack`, `.Strong`, `.Special`), GEs, or the timeline's `Tags` track for a window.
- A running timeline holds the character on its own until `FirstActionableFrame` (internal state, not a tag; projected
  as the informative `Combat.State.Busy`); cancel and override windows open it for the command queries they name.
  Hitstun (M4) is internal state too.

### 11.5 Activation context and combat clock

- **Activation context** (`FGSTN_ActivationContext`): source (`Command`, `Forced`, `Adopted`, `Other`), command tag,
  frame of the terminal press, trigger button. Written by the combat tick with the request (or at adoption), read by the
  ability at activation and kept in its sync vars.
- `IsTriggerButtonHeld()` reads the button's held state of the current frame from the combat history (written at 47,
  read at 48: same frame); `bEndOnTriggerRelease` ends the ability on release.
- **Combat clock** = frame minus the total hitstop-paused frames of the owner (a counter in the combat sync state).
  `WaitCombatFrames(N)` waits N live frames, paused in hitstop; the fork's wait tasks keep absolute time. Example:
  "4 s after X, attack automatically" = an ability triggered by X that waits 250 combat frames, then sends the event
  that triggers the attack (adopted, 11.2).

### 11.6 Camera aim

- `AimYaw`/`AimPitch` in the input command (6.1), 16 bits each, produced from the control rotation when the input
  config enables camera aim.
- The ability reads the aim of the **current frame** from the combat component (the command being simulated, also in
  re-simulation); `AimPolicy = Camera` turns the character to the aim yaw.
- Aim origin is the character (muzzle or eye socket from the kit), not the camera: the camera position is not part of
  the simulation. *Open:* camera-trace convergence for third-person crosshairs.
- Projectiles use `Projectile`-space hitboxes; hitscan needs a `Ray` hit shape and server lag compensation (M4).

### 11.7 Interrupts (*Decision*: per ability)

- `InterruptPolicy` on `UGSTN_GA_Base`, for activations that are not commands: **`SkipIfBusy`** (default: fails while a
  timeline ability holds the character) or **`InterruptCurrent`** (cancels the running timeline ability on activation;
  the newcomer is adopted next frame).
- Finer rules use the ordinary GAS block/cancel tags. Commands follow cancel and override windows (8.3).

## 11b. Jump and movement abilities

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
- The jump itself is a kit timeline ability: `UGSTN_GA_PlatformJump` (plugin) — jump squat frames, impulse from `JumpVelocity`,
  air jumps from `AirJumpCount`, buffered and cancel-window aware like every slot. Wall jumps stay in the project's
  wall modes; the project's `GA_Jump` remains for characters without a combat kit.
- Fast fall input: *Open*. Default proposal: crouch pressed while airborne and past the apex (crouch already means
  "down" in the slot set, 9.1).

### Movement abilities

- Movement options are **timeline abilities in the kit bindings** (command tags under `Command.Movement`), so they get
  frame data, buffering, cancel windows, meter costs and weapon-independence for free: dash/run start (ground flick), air
  dash or directional air dodge, roll, spot dodge, air jumps, fast fall.
- What they need beyond attacks: movement track options (`MomentumMode`: keep / reset / blend the current velocity;
  `bCancelOnLand`; landing lag), intangibility windows (`HurtboxModifier`, so dodges arrive with the hit system), and
  for continuous states (run) a timeline that loops while a condition holds (`Charge` track pattern).
- Mover modes stay the place for continuous locomotion (walking, falling, wall modes); timelines only layer velocity on
  top or override it for their frames.

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
  relative to the root; per-timeline overrides in the `HurtboxModifier` track (bakeable). `UGSTN_CombatWorldSubsystem`
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

`HitstopStartFrame/EndFrame` for attacker and victim: freezes the timeline frame, the combat clock and buffer expiry, pauses the synced
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
- **Forward prediction of simulated proxies** (spike S-FP, 2026-10-19; report in `Saved/Notes/spike-sfp-report.md`,
  not versioned). Feasible with small fork patches, **not worth enabling as it stands**: with dedicated server + 2
  clients at 60/15 ms and 2 % loss the observer saw a remote dash 40–50 ms earlier (73–82 ms instead of 108–132), which
  is mostly the interpolation buffer removed (`FixedTickInterpolationBufferedMS` = 100). Costs: every new input of the
  remote player arrives about one RTT late and costs the observer a rollback of ~14 frames of every predicted entity
  (~5 rollbacks/s per active remote player); ~0.03–0.04 ms per predicted entity per re-simulated frame, multiplied by
  the rollbacks; a 2–3 m single-frame jump at each unpredicted burst action (correction smoothing does not reach the
  proxy's mesh). Predicted activation on the proxy gave no measurable gain (dropped).
  **Policy** (*Decision*, 2026-10-19): everything Interpolated; first lower
  `FixedTickInterpolationBufferedMS` to the smallest value that stays smooth under the target jitter (most of the gain,
  no rollbacks). Forward-predict only once the client-side hit inbox (12.4, predicted attacker → local victim) and
  proxy correction smoothing exist, and then only for the 1–2 entities fighting the local player at close range
  (aggressor, target, boss), per entity with hysteresis; never minions or distant players. Re-evaluate with the M4 hit
  pipeline: the deciding number is how many hits on the local player stop being corrections.
  Fork patches a production version needs: per-entity LOD (today it is global; Mover reads the global setting in 3
  places), repeated-input preparation on proxies, correction smoothing for forward-predicted proxies, client-side hit
  inbox. **Fork bug found**: the server never sends attribute base values to simulated proxies
  (`FAttributeSyncData::NetSerialize` in `EffectsDataTypes.cpp`); harmless while proxies are interpolated, required
  (unconditionally, not behind a cvar: it changes the wire format) before any forward prediction; deferred until then
  (*Decision*). The spike code stays in the fork, inert with its cvars at 0 (`np.Spike.*`).
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
  plain bindings with requirements on those tags** (rider-only attacks, carrier-only throws): no special code, no new
  commands.

### Piggyback (mount)

- **Mounting**: the rider's mount ability has an interaction volume (a hitbox with hit type
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
  ticks them in batch at the fixed tick (timelines, hitboxes, hurtboxes, knockback from Core), with LOD (far/inactive skip
  queries and debug) and the world subsystem's broadphase. Compact replicated state (FastArray, reduced rate), clients
  interpolate + hit presentation. Player hits on minions resolve on the server with lag compensation; minion hits on
  players go to the player's inbox (server), so a correction on the player's client.
- Promoting a minion to fighter = swapping the component.

## 13b. Held and thrown objects (later, not designed)

Requirement noted, out of scope until after M7. Constraints already known, so the earlier milestones do not close
the door: holding is a link (12b, `Combat.Link.Held`) with an object size class, the object's own simulation is
suspended while held; throwing is a timeline ability whose `Projectile`-space hitbox becomes the object's; object size
classes select the hold/throw bindings (kit or weapon bindings keyed by size tag). Objects are minion-like (server-simulated)
unless a design needs prediction.

## 14. Editor and debug

- Timeline editor (assets and inline timelines): preview viewport, frame timeline (ruler, frame stepping), one row per track, hitbox gizmos,
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
- `UGSTN_MoverSimStateTracker` lives in `GSTNInputNP` (it is generic Mover code the input layer needs for facing and
  Mover tags; moved from the project in M2, then from `GSTNCombatNP` with the input split); the project module depends
  on `GSTNCombatNP` and `GSTNInputNP`.
- Wall modes: the "movement locked" extension point (12.6); wall stats through the stats modifier pattern (10.2).
- The project's attribute sets stay separate from the plugin's (`UGSTN_MovementAttributeSet`, `UGSTN_MeterAttributeSet`,
  `UGSTN_CombatAttributeSet`); the test attribute set remains test content.
- Game command tags in `Config/Tags/GSTNCommands.ini` (9); CLAUDE.md's tag rule gets this exception.
- Docs: this file; a future skill `add-gstn-combat-ability`.

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
| M1 | Core input layer | `GSTNInputCore` (moved out of `GSTNCombatCore` in S1, [input.md](input.md) §8): samples, classifier, history, conditions, matcher, buffer, reset rules, slots, validation, Automation tests. | [combat-m1-core-input.md](combat-m1-core-input.md) |
| M2 | Input in the simulation | History and classifier state in the combat sync state, stick + `ForcedCommand` in the command, player/AI input sources, Mover state reader moved into the plugin, command bindings → activation of any GAS ability, unbound commands, command lock tags, activation context, harness scenarios. | [combat-m2-sim-input.md](combat-m2-sim-input.md) |
| M3 | Combat abilities, kits, weapons, loadouts, jump | `GSTNCombatGAS`, attributes (stats, meters), stats → Mover modifier (incl. jump gravity phases, movement lock), `UGSTN_GA_Base`, `UGSTN_GA_BaseCombat` and timelines with synced montage, adoption of triggered abilities, combat clock, camera aim input, cancel windows, movement track, variable jump, air jumps, fast fall, dash, primary kit, support weapon and predicted switch, EX moves, supers, compiled table, `LoadoutRevision`, loadout component and save, skins. | [combat-m3-moves-kits.md](combat-m3-moves-kits.md) |
| M4 | Hits | Hurtboxes, hitboxes, snapshot, inbox, resolution pipeline, damage and meter gain, hitstop (Mover freeze, wall lock), hitstun/knockback/DI, teams and variants, lag compensation, hit presentation, dodges (intangibility). | [combat-m4-hits.md](combat-m4-hits.md) |
| M5 | Editor | Timeline editor, socket baking, validators UI, debug draw, input display. | [combat-m5-m9.md](combat-m5-m9.md) |
| M6 | Defence and links | Shield, parry, tech, SDI; link system spike, links, grabs/throws. | [combat-m5-m9.md](combat-m5-m9.md) |
| M7 | Piggyback and pair moves | Mount link, riding mode, role-only bindings, carry load, pair moves. | [combat-m5-m9.md](combat-m5-m9.md) |
| M8 | Minions | Server subsystem, LOD, compact replication. | [combat-m5-m9.md](combat-m5-m9.md) |
| M9 | Stock GAS adapter | Optional. | [combat-m5-m9.md](combat-m5-m9.md) |

