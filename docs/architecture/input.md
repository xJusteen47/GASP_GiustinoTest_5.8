# Input plugin (`GSTNInput`)

Fighting-game style input for any Unreal project: designer-defined commands (button, direction, motion, chord, charge,
hold, release, mash, spin), time and hold buffering, reset rules, priority between commands, and command → GameplayAbility
bindings with context requirements. Simulated and rolled back by the Network Prediction fork when used with GAS-NPP.
Source: `Plugins/GSTNInput/`.

The combat plugin (`GSTNCombat`, [combat.md](combat.md)) is built on top of it. This file describes what the input
plugin owns, what it leaves to its host, and how the code moves out of `GSTNCombat` (section 8). The input behaviour
itself (classifier, history, grammar, buffer, priority, bindings) is specified in combat.md §6–§9 and in the M1/M2
specifications; their rules do not change, only where the code lives and some names.

Status (2026-10-19): *Decision*, design only. The code is still in `GSTNCombatCore/Input/` and `GSTNCombatNP`.

## 1. Goals

- Usable without the combat plugin: a game that only wants buffered commands driving GameplayAbilities enables
  `GSTNInput` alone.
- Usable without GAS-NPP for the pure part: `GSTNInputCore` depends on neither GAS nor Network Prediction nor Mover.
- No game vocabulary inside: no "attack", "special", "crouch", hitstun or combat tags. Buttons, commands and their
  tiers are data the game provides.
- One NP model per entity: a host simulation (the combat model) **embeds** the input simulation instead of running a
  second model next to it.

## 2. Modules

| Module | Depends on | Contents |
|---|---|---|
| `GSTNInputCore` (Runtime) | Core, CoreUObject, Engine, GameplayTags, DeveloperSettings; private EnhancedInput (only `UInputAction` references) | Today's `GSTNCombatCore/Input/` minus the combat content: core types, samples, direction classifier, history, conditions, patterns, matcher, buffer reset rules, slots, slot sets, resolver, validation, binding requirements and selection, input script, settings, input config, generic tags, the authoritative event stream. Automation tests `GSTNInput.Core.*`. |
| `GSTNInputNP` (Runtime) | GSTNInputCore, GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, NetCore, Mover; private EnhancedInput | Input command, embeddable input sim state and simulator, command binding set, input sources (player, AI), context providers, the Mover sim-state tracker, the standalone input component, activation context and the interface abilities use. |

`GSTNInput.uplugin` plugin dependencies: GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, EnhancedInput,
Mover. Prefix stays `GSTN_`; export macros `GSTNINPUTCORE_API`, `GSTNINPUTNP_API`; log category `LogGSTNInput`.

`GSTNCombat` declares a plugin dependency on `GSTNInput`; `GSTNCombatCore` depends on `GSTNInputCore`, `GSTNCombatNP`
on `GSTNInputNP`.

## 3. Tags (*Decision*)

The input plugin declares only roots and the tags its own code produces or reads. Games declare the rest (natively, or
in the tag ini of section 3.1).

| Tag | Declared by | Meaning |
|---|---|---|
| `Input.Button` (root) | GSTNInputCore | Root of logical button tags; tag pickers filter on it. The buttons themselves are game data. |
| `Command` (root) | GSTNInputCore | Root of command tags; `CommandTiers` and lock mapping work on its children. |
| `Input.Lock.Command` | GSTNInputCore | Locks every command; `Input.Lock.Command.<root>` locks `Command.<root>.*` (combat.md §11.4 rule). Per-root lock tags are declared by whoever declares the root. |
| `Input.Context.Grounded`, `.Airborne`, `.OnWall` | GSTNInputCore | Context tags written by the Mover context provider (5.2); binding requirements use them. |
| `Input.BufferEvent` (root), `.AbilityActivated`, `.Landed` | GSTNInputCore | Frame events for reset rules; other systems declare theirs under the same root (combat: `.HitReceived`, `.WeaponSwitched`). |
| `Input.Event.CommandExecuted` | GSTNInputCore | Authoritative event (section 6). |

Renamed from the combat names: `Input.Combat.Button.*` → `Input.Button.*`, `Combat.Context.*` → `Input.Context.*`,
`Combat.Lock.Command*` → `Input.Lock.Command*`, `Combat.BufferEvent.*` → `Input.BufferEvent.*`,
`Combat.Event.CommandExecuted` → `Input.Event.CommandExecuted`. `Combat.Lock.Movement` stays a combat tag (it is read by
the combat stats modifier).

### 3.1 Designer tags

Game-specific buttons, commands and their per-root lock tags live in **`Config/Tags/GSTNCommands.ini`** (replaces the
planned `GSTNCombatCommands.ini`, which was never created): the one exception to the project's native-tags rule.

## 4. `GSTNInputCore`

Every type keeps its behaviour, tests and header comments. Renames:

| Today (`GSTNCombatCore`) | In `GSTNInputCore` |
|---|---|
| `GSTN_CombatCoreTypes.h` | `GSTN_InputCoreTypes.h` (same enums and masks) |
| `GSTN_CombatCoreLog.h`, `LogGSTNCombatCore` | `GSTN_InputLog.h`, `LogGSTNInput` |
| `UGSTN_CombatInputSettings` ("GSTN Combat Input") | `UGSTN_InputSettings` ("GSTN Input"), same fields, `config = Game` |
| `UGSTN_CombatInputConfig`, `FGSTN_CombatButtonDef` | `UGSTN_InputConfig`, `FGSTN_InputButtonDef`; gains `MoveAction` (2D stick action) and `bSendCameraAim` (combat.md §11.6) |
| `GSTN_CombatInputTags.h` (`GSTNInputTags`) | `GSTN_InputTags.h` (`GSTNInputTags`): the generic tags of section 3 only |
| `UGSTN_CombatEventSubsystem`, `FGSTN_CombatEvent` (planned in M2 §7b) | `UGSTN_SimEventSubsystem`, `FGSTN_SimEvent` (same API) |

Stay in `GSTNCombatCore` (combat content built on the input core): the six standard button tags (now
`Input.Button.Attack/Strong/Special/Crouch/Shield/Jump`, in `GSTN_CombatButtonTags.h`), `GSTN_CombatCommandTags.h`
without the context tags, `UGSTN_StandardSlots` (the default fighting-game command set, combat.md §9.1) and its tests.

### 4.1 Priority criteria (*Decision*, 2026-10-19)

The order "tier → specificity → manual offset" of combat.md §8.2 is no longer hard-coded: it is the **default list of
priority criteria**, and both the order and the criteria are configurable.

- **`FGSTN_PriorityCriterion`** (USTRUCT base, used as `TInstancedStruct`, like conditions): `virtual int32 Score(const
  FGSTN_PriorityContext&) const`, where the context is the slot spec, its pattern and the settings. Higher score wins.
  Built-in criteria: `_Tier` (from `CommandTiers`), `_Specificity` (from the pattern), `_ManualOffset` (the slot's
  `PriorityOffset`).
- **`UGSTN_InputSettings::PriorityCriteria`** (`TArray<TInstancedStruct<FGSTN_PriorityCriterion>>`, max 8): the
  ordered list. Default `[Tier, Specificity, ManualOffset]` = today's behaviour. Reordering or removing is a Project
  Settings change; a **new criterion is one small C++ struct** with one function (e.g. "chords first", "a given button
  first").
- **`FGSTN_CommandPriority`** becomes the vector of the criteria's scores (`TArray<int32, TInlineAllocator<8>>`),
  compared lexicographically; ties still go to slot order. It is computed once per slot when a slot set or table is
  built (criteria must not depend on the frame), so the resolver's hot path does not change, and the loser consumption
  and the conflict validator keep comparing priorities as today.
- **Frame-dependent ordering** (which press is newer) stays in the press-group stage, configured by
  `BufferConflictPolicy` and `SimultaneousFrames`; it runs before the criteria, as now.

Generic code that names combat content today, to be cleaned in the move:

- **Settings defaults**: `CommandTiers` and `ExclusiveButtons` start empty in `UGSTN_InputSettings`. The combat tiers
  (Attack 100, Strong 200, Special 300, Combo 350, Super 400) and exclusive buttons (Attack, Strong, Special) go into the
  project's `Config/DefaultGame.ini` section `[/Script/GSTNInputCore.GSTN_InputSettings]`; combat.md §8.2 lists them
  as the recommended values.
- **`FGSTN_ButtonMap`** builds only from an input config (no hard-coded six-button order).
- **`FGSTN_InputScript`**: the button letters are a parameter (no default to the combat buttons). Tests pass their own.
- **Tag pickers**: `meta = (Categories = "Input.Button")` and `"Input.BufferEvent"` instead of the combat roots.
- **Tests**: generic tests use test-only native tags (`Input.Button.Test.A`… declared in the test utilities under
  `WITH_DEV_AUTOMATION_TESTS`), renamed `GSTNInput.Core.Input.*`. `StandardSlots`, the native-tag test of the combat
  tags and the settings test of the combat defaults move to `GSTNCombatCore` as `GSTNCombat.Core.*`.
- Comments that cite combat.md sections keep citing them (the rules live there) but say "combat.md" explicitly.

## 5. `GSTNInputNP`

### 5.1 Simulation pieces (embeddable)

- **`FGSTN_InputCmd`** (was `FGSTN_CombatInputCmd`): `HeldButtons`, `PressedButtons`, `StickX/Y`, `CameraYaw`,
  `ForcedCommand`, later `AimYaw/AimPitch`; `NetSerialize`, `ToString`, `PrepareRepeatedInput`. A host model uses it
  directly as its NP input type.
- **`FGSTN_InputSimState`**: `History`, `Classifier`, `WatchedTagMask`, the frame-stamped request (`RequestedBinding`,
  `RequestButton`, `RequestFrame`), `LastActivatedFrame`. Not a sync state by itself: helpers `NetSerialize(Ar)`,
  `ShouldReconcile(Other)`, `ToString(Builder)` that the host's sync state calls for its embedded member.
- **`FGSTN_InputSimulator`** (stateless): `Tick(const FGSTN_InputTickArgs&, IGSTN_InputSimHost&)` runs M2 §5 steps
  1–7 for one frame (sample, history, watched tags, reset rules, locks, resolve, request, consumption, outbox entry) on
  the embedded state.
- **`IGSTN_InputSimHost`** (plain C++ interface, not a UInterface; the host implements it):
  `AppendFrameEvents(FGSTN_BufferFrameEvents&)` (extra reset events such as hits),
  `IsCommandAllowed(int32 CommandIndex)` (the host's own gating, e.g. combat timeline and cancel windows),
  `CanExecuteBinding(int32 BindingIndex)` (ASC `CanActivateAbility` by default), `AppendResetRules(...)`
  (host-level rules, e.g. the running timeline's).
- **`UGSTN_CommandBindingSet`** (DataAsset, was `UGSTN_CombatSlotBindingSet`): slots, bindings
  `{ Command; Requirements; Ability }`, reset rules. The combat model replaces it with its compiled loadout table (M3)
  through the same simulator calls.

### 5.2 Context and input sources

- **`UGSTN_InputContextProvider`** (abstract, `EditInlineNew`, held by the component): `GetFacingYaw()`,
  `AppendContextTags(FGameplayTagContainer&)`, `AppendFrameEvents(...)`, all from end-of-F-1 state.
  **`UGSTN_MoverInputContextProvider`** (default): facing and Mover tags from the tracker, `Input.Context.Grounded`
  / `.Airborne` / `.OnWall` from configurable mode-name lists, `Input.BufferEvent.Landed`.
- **`UGSTN_MoverSimStateTracker`** moves here (`GSTNInputNP/Mover/`, same class name and API).
- **`IGSTN_InputSource`** (was `IGSTN_CombatInputSource`), **`UGSTN_PlayerInputSource`**, **`UGSTN_AIInputSource`**:
  as M2 §4, reading buttons and the stick action from the `UGSTN_InputConfig`.

### 5.3 Abilities' view

- **`IGSTN_CommandSourceInterface`** (UInterface, implemented by every component that runs the input simulation):
  `GetActivationContext()`, `IsCommandButtonHeld(int32 Button)`, `GetAimRotation()`.
- **`FGSTN_ActivationContext`**, **`EGSTN_ActivationSource`** (`Command`, `Forced`, `Adopted`, `Other`) live here;
  the combat plugin's `UGSTN_GA_Base` finds the interface on the avatar, so it works with either component.

### 5.4 Standalone component

**`UGSTN_InputSimComponent`** (`UNetworkPredictionComponent`) for projects without the combat plugin: registers its
own NP model (`FGSTN_InputModelDef`, priority `PreKinematicMovers - 3`, before the ASC like the combat model), with
`FGSTN_InputCmd` and a sync state holding only `FGSTN_InputSimState`. Properties: input config, binding set, input
source (instanced), context provider (instanced), global reset rules. Executes requests in the ASC pre-tick
(`OnSimulationPreTick`), projects the context tags on the ASC, drains its outbox on the authority, implements
`IGSTN_CommandSourceInterface`.

A pawn uses **either** this component **or** the combat component (which embeds the same simulation); having both is a
validation error at BeginPlay (two models would read the same buttons).

## 6. Authoritative events

`UGSTN_SimEventSubsystem` (world subsystem, `GSTNInputCore`) is the stream of combat.md §5.6, generalised: same API
(`Broadcast`, `Listen(FGameplayTagQuery)`), same rule (outbox written in simulation, drained only on the authority
after `FinalizeFrame`). The input simulation publishes `Input.Event.CommandExecuted`; the combat plugin publishes its
`Combat.Event.*` through the same subsystem.

## 7. What stays in the combat plugin

Default command set and its tags, combat button tags, timelines, adoption, `Combat.Lock.Movement`, combat states,
hits, kits, loadouts. The combat component embeds `FGSTN_InputSimState` in its sync state, implements
`IGSTN_InputSimHost` (timeline windows, hit events, compiled table) and `IGSTN_CommandSourceInterface`, and uses
`FGSTN_InputCmd` as its input type.

## 8. Migration (do it now, before M2 continues)

The split is mostly moving files and renaming; M2's NP code is half written, so the sooner the better.

**Step S1 — `GSTNInputCore`** (one commit, builds and tests green):

1. Create `Plugins/GSTNInput/GSTNInput.uplugin` and module `GSTNInputCore` (Build.cs as section 2, module class, log).
   Enable the plugin in `GASP_Giustino2.uproject`.
2. `git mv` the generic files of `GSTNCombatCore/Public|Private/Input/`, `GSTN_CombatCoreTypes.h` and the generic tests
   into the new module; apply the renames of section 4; `GSTNCOMBATCORE_API` → `GSTNINPUTCORE_API` in moved headers.
3. `GSTNCombatCore` keeps `GSTN_CombatButtonTags`, `GSTN_CombatCommandTags`, `UGSTN_StandardSlots` and their tests; it
   depends on `GSTNInputCore`; `GSTNCombat.uplugin` declares the `GSTNInput` plugin dependency.
4. Add the combat values to `Config/DefaultGame.ini` (section 4).
5. Build; run `Automation RunTests GSTNInput` and `GSTNCombat`: the 58 existing tests all pass, split between the two
   prefixes (count them in the commit message).

**Step S1b — priority criteria** (one commit, after S1): section 4.1. With the default list every existing resolver,
validation and standard-set test passes unchanged; new tests: the list reordered (specificity before tier) changes the
winner of a chord-versus-higher-tier case, a test-only criterion struct changes a result, an empty list falls back to slot
order.

**Step S2 — `GSTNInputNP`**: the rest of M2 lands in `GSTNInputNP` instead of `GSTNCombatNP` (updated M2 spec):
`FGSTN_InputCmd` (move the working-copy fields of `FGSTN_CombatInputCmd`), `FGSTN_InputSimState` (move `History`,
`Classifier`, `WatchedTagMask`, `LastActivatedFrame` and the request out of `FGSTN_CombatSyncState`), simulator, host
interface, sources, context provider, binding set, tracker move, standalone component. `UGSTN_CombatSimComponent`
embeds them. The combat component's `MoveAction`/`ButtonActions` move to the input config: **tell the user before
re-setting them on `BP_GASNPP_TestCharacter`**.

Redirects in `Config/DefaultEngine.ini`:

- Tracker: both old names → `/Script/GSTNInputNP.GSTN_MoverSimStateTracker` (update the existing entry, add one from
  `/Script/GSTNCombatNP.GSTN_MoverSimStateTracker`).
- No asset of `UGSTN_CombatInputConfig` exists yet (checked), so the input classes need no redirects.

Tools: `setup_gas_assets.py` and `gasnpp_test.py` keep `GSTN_CombatSimComponent` (the class stays); the input config
asset is created by the setup script; testing.md lists the new test prefixes.

## 9. Planned: keyboard shortcuts for stick gestures (owner, 2026-10-25)

Owner idea: on keyboard the spin, a stick rotation, gets a **shortcut such as A-S-D without W**, a partial rotation that is easy to
roll with three fingers. *Proposal*, not built:

- The `Spin` condition already counts rotation in degrees with a sense. A slot gains `DigitalMinDegrees` (default: its normal
  threshold), used when the direction samples in the window came from **digital** directions. A-S-D is left, down, right: 180°
  counterclockwise seen from above with forward at the top. D-S-A is the clockwise one. Eight-way samples (A, A+S, S, S+D, D) feed
  the same accumulator.
- The input source writes one bit per frame, `bDigitalDirection`, from the device that last changed the direction (keyboard or
  d-pad). The bit is part of the input frame, so history, prediction and rollback see the same value on every machine; the
  simulation never asks which device is plugged in.
- The same per-slot threshold serves other gestures that are hard on keys. A dedicated spin button stays possible
  ([spin-drill.md](spin-drill.md) §5).
- Questions KB.1–KB.2 in [open-questions-m4-m10.md](open-questions-m4-m10.md).

## 10. Charge inputs, Guile style (owner question, 2026-10-26)

The owner asked whether a tag added on crouch press and removed on release, read by an ability, is enough for Street Fighter charge
moves. **The plugin already has it, in a better place**: `FGSTN_InputCondition_Charge` (`GSTNInputCore`, test `ConditionCharge`) reads
the **input history**, which is part of the predicted state, so a resimulation after a rollback gets the same answer. A tag alone has
no duration, and a tag derived from Mover's crouch would be seen one frame late and is a Mover query (CLAUDE.md rules 1 and 5).

What the condition does today:
- `Directions`: a **set** of directions that all count as charging (for example back and down-back), in a chosen `Space`
  (`Facing` by default).
- `MinFrames` (default 30): how long the stick must stay inside the set. Standard charge moves use **40 frames** (about 0.65 s) and
  `MaxReleaseFrames` 8, set per move (owner answer CH.2, 2026-10-26).
- `MaxReleaseFrames` (default 8): the **tolerance after release**: the stick may have left the set at most this many frames before the
  move's anchor, so "charge back, then forward + punch" works.
- `MinZone`: how far the stick must be tilted to count.
- A charged **button** is `Release` with `MinHeldFrames`; a move that loops while a button is held is the timeline's `Charge` track.
- Charging is input only, so it continues while the character walks, crouches, blocks or attacks, as in Street Fighter.

Gaps, *Proposal*:
1. **The history keeps the last 16 direction events** (`FGSTN_InputHistory::MaxEvents`). A new event is recorded whenever the stick
   sector, the zone, the facing or the camera yaw changes the stick's direction, so a charge held while the camera turns, or while
   the stick wobbles between back and down-back, can push the start of the run out of the ring: the charge then looks shorter than it
   was and the move fails. Fix: a small **charge tracker** in the input state of `GSTNInputNP`. For each charge slot declared by the
   command set (at most 4), it keeps `uint8 AnchorAngle` (256 steps, item 2), `uint8 HeldFrames` (saturating at 255) and
   `uint8 FramesSinceRelease`, updated every frame from the current stick. It is part of the predicted input state, so rollback
   replays it; cost 12 bytes. The `Charge` condition reads the tracker when its slot is declared and falls back to the event scan
   otherwise.
2. **"Back" in 3D (owner answer CH.1, 2026-10-26 09:53)**: the back-charge works in 3D; the charge holds **as long as the stick stays
   within a limited angle**. In free movement the character turns towards the stick, so "back relative to facing" cannot be held;
   instead the charge is relative to **where it started**. *Proposal*: a new space `EGSTN_DirectionSpace::ChargeAnchor`. When the stick
   enters the tilt zone with no charge running, the tracker stores the stick's angle in `Stick` space (the physical stick, so a camera
   turn does not break the charge) as `AnchorAngle`. The charge keeps counting while the stick stays within `MaxDeviationDeg` of the
   anchor (50°, per slot, owner answer CH.4); past it the charge restarts with a new anchor. **The anchor drifts with time**
   (owner, 09:55): each frame it moves towards the stick by at most `AnchorDriftDegPerSecond` (default **45°/s**, a full turn in 8 s;
   per slot, 0 = no drift; owner CH.4 10:23: 90°/s is too fast for some moves, so a move that needs another rate declares its own
   slot), so a
   stick turned very slowly keeps the charge, even through a full 360°, while a quick turn breaks it. The release reads the anchor
   of the frame it happens on. The release step of the move reads
   its direction in the same space: "forward" means **opposite to the anchor** within its own tolerance, so "hold one way, then push
   the other way + punch" fires the move towards the push. Where facing is locked (the Duel camera, a lock-on) the classic `Facing`
   set (back and down-back) keeps working. Down-charges (crouch held) need neither.
   **Which way the move faces (owner, 09:54)**: while charging, the character usually faces the charge direction, so a move may also
   have its **hitboxes behind** the character instead of in front. That is the timeline's choice, not the input's: `AimPolicy =
   KeepFacing` with hitboxes authored behind the root (negative X in `Root` space) hits towards the push without turning;
   `SnapToStick` turns towards the push and hits forward (owner, 09:55: "it depends on the ability"). Both are data on the move
   (combat.md §6.3, `AimPolicy`).
3. **Side switches (CH.3, inferred from the owner's answers, 2026-10-26 09:56)**: standard charge moves use `ChargeAnchor` in the
   duel too, so the charge does not depend on facing and a cross-up does not erase it. The move still fires opposite to the anchor;
   whether it then turns towards the opponent is the move's aim policy, like any other charge move. A move that wants the Street
   Fighter rule (charge lost on a side switch) uses the `Facing` set instead; that is data on the command.

**Charged buttons (owner question, 2026-10-26 10:25 and 10:26; Cody's Zonk Knuckle and EX Zonk Knuckle)**: hold a normal attack
button for a while, release it to fire the special. Reference behaviour (Street Fighter IV data; Street Fighter V keeps the idea):
the plain Zonk charges on **any one punch** and has **three levels by hold time** (about 60, 90 and 120 frames: more damage and a
longer upper-body invulnerability); the **EX Zonk** charges on **two punches held together**, released together, about 60 frames,
costs EX meter, hits twice and is fully invulnerable during startup.

What exists today: `FGSTN_InputCondition_Release` with `MinHeldFrames` (the negative edge; `MaxHeldFrames` gives the tap slot on the
same button, and a tap slot and a hold slot never match the same release). The release does not need an unconsumed press, so the
press can fire a normal attack and the hold keeps charging through other moves, walking or blocking, as in Street Fighter. A release
during another move is buffered like any command. The EX meter fall-through already exists in the resolver (M3 C17–C18): without
meter, a command marked EX falls through to its plain version. Gaps, *Proposal*:
1. **The 16-event button ring**: if the press of the held button drops out of the ring, `Release` counts the hold as longer than the
   history (`MAX_int32`), which is right for a long hold but wrong when the press was pushed out early by **other button presses**:
   eight presses and releases of other buttons while charging push it out, and a 120-frame charge could fire after 30 frames. Fix:
   the charge tracker of gap 1 gets **button slots**: `uint8 HeldFrames` (saturating at 255, about 4 s) of the current press, one byte
   per declared button, up to 4 (so the three punches of a Zonk character fit next to its stick slots; tracker 12 + 4 = 16 bytes),
   predicted and rolled back. `Release`, `Hold` and the chord below read the slot when the button has one, and keep the event scan
   otherwise. Each button charges on its own, so releasing one of two held punches fires the plain Zonk and the other keeps charging.
2. **Any one of several buttons**: `Release` gets an optional `AlsoButtons` set (any one of them counts; the default stays one
   button), so one command covers "any punch" instead of three slots.
3. **Charge levels by hold time**: one command per level with `MinHeldFrames`/`MaxHeldFrames` bands (60–89, 90–119, 120+), each
   pointing at its own timeline or at one timeline with level data. The bands never overlap, so one release matches one level.
4. **EX: several buttons released together** (owner, 10:26): a new `ReleaseChord` condition, the mirror of `Chord`: every listed
   button (or any `Count` of a set, for "two of the three punches") released within `WithinFrames` (default 3) of each other, each held
   at least `MinHeldFrames`. Anchor = the newest release. Specificity 10 per button, so the EX chord (20) wins over a plain `Release`
   (10) on the same frames, as `Chord` does for presses. The EX command is marked EX, so without meter it falls through to the plain
   Zonk of the level the hold reached. Releases are never consumed, so the plain `Release` slots must not fire on the same
   releases: the resolver's existing one-activation-per-frame rule handles the same frame, and the plain slot's `MaxReleaseLagFrames`
   (new, default 0) stops it from firing a frame later from the second button of the chord.
5. **"Charge ready" feedback** (a flash per level): presentation reads the slots' `HeldFrames`; no simulation change.
6. The charge stays ready as long as the button is held (no automatic release). A move that fires by itself after a maximum hold uses
   a `Hold` condition with that `MinFrames` instead, or the timeline's `Charge` track (loop while held, Smash style).

Questions CH.1–CH.4 in [open-questions-m4-m10.md](open-questions-m4-m10.md); all answered. CB.1 answered (two-button release needed for EX).
