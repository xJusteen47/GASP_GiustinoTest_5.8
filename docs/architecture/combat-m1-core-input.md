# Combat M1 — Core input layer (implementation specification)

Part of the combat design ([combat.md](combat.md)): read sections 5–9 there first. This file says exactly what to
build in M1, how it is tested and when it is done. Section numbers below are local to this file; "combat.md §N" points
to the architecture.

> **Revision 2026-10-18** (after M1.1 was committed): slots are identified by **command tags**, the resolver uses
> **tiers, press groups and loser consumption**, chord grace is off by default (startup override in M3 replaces it).
> Affects M1.2 onwards: settings, slot spec, resolver, validation, standard set, tests. See combat.md §8.2, §8.3, §9.

## 1. Scope

In: module `GSTNCombatCore`, every pure type and algorithm of combat.md §6.3, §6.4, §7, §8, §9; developer settings; slot
assets and validation; Automation tests. Out: NP integration and input command changes (M2), Enhanced Input sources
(M2), moves/weapons (M3), editor UI (M5). Nothing in `GSTNCombatNP` or the project changes in M1, except adding the
module to `GSTNCombat.uplugin`.

**Hard constraint:** `GSTNCombatCore` must not depend on GameplayAbilities, AbilitySystemSimulation,
NetworkPrediction or Mover. Every function is deterministic: integer frames, no wall-clock time, no randomness, no
iteration over unordered containers.

## 2. Files

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
    Input/GSTN_InputScript.h          FGSTN_InputScript: builds histories from the notation of section 4 (tests, validator, debug)
    Input/GSTN_CombatInputTags.h      native tags of the standard buttons: Input.Combat.Button.Attack/Strong/Special/Crouch/Shield/Jump
    Input/GSTN_CombatCommandTags.h    native standard command tags (combat.md §9.1): Command.Attack/Strong/Special.<Shape>,
                                      Command.Air.*, Command.Combo.AttackSpecial.<Shape>, Combat.Context.Airborne
    Input/GSTN_StandardSlots.h        UGSTN_StandardSlots::Build(): the standard slot set of combat.md §9.1 in code
                                      (tests use it; an editor action creates the asset from it)
  Private/
    GSTNCombatCoreModule.cpp
    Input/*.cpp
    Tests/GSTN_InputTests_*.cpp       WITH_DEV_AUTOMATION_TESTS
```

Conventions from CLAUDE.md apply (prefix, export macro `GSTNCOMBATCORE_API`, header doc comments explaining intent and
determinism constraints, tuning as `UPROPERTY(EditAnywhere/EditDefaultsOnly)` with `Category = "Area|Section"`,
`ClampMin`).

## 3. Types

**Enums (`GSTN_CombatCoreTypes.h`)**

- `EGSTN_StickDirection : uint8` — `Neutral, Up, UpRight, Right, DownRight, Down, DownLeft, Left, UpLeft`. The
  meaning depends on the space it was classified in: in `Facing` space Up = forward of the character, Down = back; in
  `Stick` space Up = stick pushed up (away from the camera); in `World` space Up = the settings' world reference yaw.
  `FGSTN_DirectionMask` = uint16 bitmask over these, with helpers (`AnyTilted` = the 8 non-neutral directions,
  `FacingForward` = Up|UpLeft|UpRight, ...). `EGSTN_StickDirection` is a `Bitflags` enum (bit = 1 << value), so
  condition properties holding a direction set are `int32` with `Bitmask` metadata and edit as a checklist; helpers are in
  `namespace GSTN_DirectionMasks`. `EGSTN_BufferConflictPolicy` (`MostRecent, HighestPriority, Oldest`) lives with the
  other enums.
- `EGSTN_DirectionSpace : uint8` — `Facing, Stick, World` (combat.md §6.3).
- `EGSTN_StickZone : uint8` — `Neutral, Aim, Tilt` (ordered: comparisons "at least Tilt" are valid).
- `EGSTN_DirectionPolicy : uint8` — `AtPress, AtExecution`.
- `EGSTN_OverrideBool : uint8` — `Default, On, Off` (per-slot override of a settings default).

**Settings (`UGSTN_CombatInputSettings`, Project Settings > Plugins > GSTN Combat Input)**

| Property | Default | Meaning |
|---|---|---|
| `DefaultBufferFrames` | 8 | Time buffer. |
| `bHoldBufferByDefault` | true | Hold buffer for slots that do not override it. |
| `MaxHoldBufferFrames` | 0 | 0 = unlimited while held. |
| `SimultaneousFrames` | 2 | Presses within this many frames of the newest terminal press form one press group (combat.md §8.2). |
| `BufferConflictPolicy` | `MostRecent` | Which press group competes: `MostRecent`, `HighestPriority` (all groups, priority only), `Oldest`. |
| `CommandTiers` | `Command.Attack` 100, `Command.Strong` 200, `Command.Special` 300, `Command.Combo` 350, `Command.Super` 400, and the airborne roots `Command.Air.Attack/Strong/Special` with the same values as their grounded twins (they are not under the grounded roots) | Tier per command tag root (`TMap<FGameplayTag, int32>`); a command's tier is the one of its nearest configured ancestor, found by walking `RequestDirectParent()` (deterministic, no map iteration); unlisted = 0. |
| `ExclusiveButtons` | Attack, Strong, Special | Button tags whose presses in the winner's press group are consumed with it. |
| `bChordGraceByDefault` | false | Legacy wait before single-button commands (combat.md §8.3); off because the startup override handles chords. |
| `ChordGraceFrames` | 3 | Only used when chord grace is on. |
| `DefaultSimultaneityFrames` | 3 | Tolerance inside a step (e.g. direction vs press). |
| `StickDeadZone` / `AimZone` | 0.2 / 0.5 | Normalised magnitude thresholds: below dead = Neutral, below aim = Aim, else Tilt. |
| `FlickFrames` | 3 | Max frames from Neutral/Aim to Tilt for a flick. |
| `DiagonalHalfAngle` | 22.5° | Sector half-width for diagonals (8 equal sectors by default). |
| `WorldReferenceYaw` | 0° | World yaw that counts as Up in `World` space. |
| `DefaultResetRules` | (empty) | Global reset rules. |

Values are designer defaults, to be tuned in M2 playtests.

**Input config (`UGSTN_CombatInputConfig`)** — `Buttons: TArray<FGSTN_CombatButtonDef>`, max 32; each
`{ FGameplayTag Tag; TObjectPtr<UInputAction> Action; }`. The array index is the bit. `IsDataValid`: ≤ 32 entries,
unique tags, unique actions. `FindButtonIndex(Tag)`. Conditions reference buttons **by tag** and are resolved to bits
when a slot set is bound to a config (unknown tag → validation error), so slot sets are reusable across configs.
Standard tags (native, `GSTN_CombatInputTags.h`): `Input.Combat.Button.Attack`, `.Strong`, `.Special`, `.Crouch`,
`.Shield`, `.Jump`; projects add their own.

**Sample (`FGSTN_InputSample`)** — what one frame provides to the history:
`int32 Frame; uint32 Held; uint32 Pressed; uint32 Released; EGSTN_StickDirection StickDir, FacingDir, WorldDir;
EGSTN_StickZone Zone; bool bFlick; uint8 WorldYaw (256 steps; aim and exact angles)`. Built in M2 from the command + classifier; in M1 by `FGSTN_InputScript` and tests.
`Pressed` includes taps (`Cmd.PressedButtons`) and held edges; a button both pressed and released inside one frame
produces a press then a release event on the same frame.

**Classifier (`FGSTN_DirectionClassifier`)** — pure static `Classify(FVector2D RawStick, float CameraYawDeg,
float FacingYawDeg, const FGSTN_ClassifierState& Prev, int32 Frame, const UGSTN_CombatInputSettings& S,
FGSTN_ClassifierState& Out)` → the three directions, zone, flick, world yaw. `RawStick` is the stick as on the
controller (+Y up); the world-plane stick is `RawStick` rotated by the camera yaw; the facing-relative stick is the
world stick rotated by `-FacingYaw`. Zone and flick come from the magnitude, identical in every space.
`FGSTN_ClassifierState { EGSTN_StickZone Zone; int32 LastNeutralFrame; }` lives in the sync state in M2 (needed to
detect flicks after a rollback). A flick is the frame the stick enters `Tilt` with `Frame - LastNeutralFrame <=
FlickFrames`, i.e. it left `Neutral` no more than `FlickFrames` ago (tracking the last frame in *any* low zone would
make every entry into `Tilt` a flick). The classifier returns an `FGSTN_StickSample` (defined in `GSTN_InputSample.h`
together with the per-frame sample, M1.3) and the new state through an out parameter. Angles are degrees, 0 = up,
clockwise; a boundary angle belongs to the cardinal direction. Helpers: `QuantizeStickAxis/DequantizeStickAxis`
(127 = 1), `QuantizeYaw/DequantizeYaw` (256 steps). Quantized input (int8 stick, uint8 camera yaw) is converted back before
classification so client and server classify the same values.

**History (`FGSTN_InputHistory`)**

```
FGSTN_ButtonEvent    { int32 Frame; uint8 Button : 5; uint8 bRelease : 1; uint8 bConsumed : 1; }
FGSTN_DirectionEvent { int32 Frame; EGSTN_StickDirection StickDir, FacingDir, WorldDir; EGSTN_StickZone Zone;
                       bool bFlick; uint8 WorldYaw; }
FGSTN_InputHistory {
  TStaticArray<FGSTN_ButtonEvent, 16> ButtonEvents;   + uint8 ButtonHead, ButtonCount
  TStaticArray<FGSTN_DirectionEvent, 16> DirectionEvents; + uint8 DirectionHead, DirectionCount
  uint32 HeldButtons;
  FGSTN_DirectionEvent Current;   // direction/zone at the newest frame (also when unchanged)
}
```

Operations: `Append(const FGSTN_InputSample&)` (press events in button index order, then releases; a direction event
only when any of the three directions or the zone changes, or a flick happens — so turning the character or the camera
while holding the stick adds at most one event per 45° sector); iteration newest → oldest; `GetDirectionAt(Frame, Space)` (from the
last direction event with `Frame <= F`; at `Now` from `Current`); `IsHeldContinuouslySince(Button, Frame, Now)` (no release event of that button
after the press at `Frame` and the bit is in `HeldButtons`); `Consume(EventIndex)`; `FlushPresses(Mask, UpToFrame)`.
`NetSerialize(FArchive&, int32 CurrentFrame)`: counts + per event `uint8 delta = min(Current - Frame, 255)` + payload;
events with delta 255 are dropped on read. `operator==` for `ShouldReconcile`. Ring overflow drops the oldest.

*Implementation notes (M1.3):*

- `FGSTN_DirectionEvent` is `{ int32 Frame; FGSTN_StickSample Stick; }` (the sample type holds the three directions, zone,
  flick and world yaw, so the event and the per-frame sample share it). Events are addressed by **slot** (physical ring
  position 0..15, stable while the event lives): the matcher's `UsedButtonEvents` and `ReservedEvents` use slots;
  `GetButtonSlot(Age)` converts from age (0 = newest).
- Events older than 254 frames are dropped by `Append` itself (so serializing with 8-bit deltas loses nothing and an idle
  state does not change). The newest stick event is the current stick state, so it is never dropped: it keeps living with
  `Frame = AncientFrame`. Consequence: a hold buffer cannot outlive 254 frames even with `MaxHoldBufferFrames = 0`.
- `IsHeldContinuouslySince(Button, PressFrame)` has no `Now` parameter (the held mask is the latest frame's).
  `GetStickAt(Frame)` returns the stick state at a frame (flick flag only on its own frame); `GetDirectionAt` is its
  per-space shorthand.
- Wire format: `[buttonCount][directionCount | flags][held, packed]([worldYaw])([frame])` then newest-first events, 2 bytes
  per button event and 4 per stick event. The stick of the latest frame is rebuilt from the newest stick event (plus the
  world yaw byte). Idle: 3 bytes; both rings full: exactly 100 bytes (measured by `History.SerializedSize`). `NetSerialize`
  takes the frame to write deltas from; `NetSerializeParams` carries no frame, so **M2 must provide one** (e.g. a sync
  state field that stays equal to the latest appended frame, or serialize relative to a frame the state already has).
- `Append` records presses from `Sample.Pressed` and releases from `Sample.Released` as given. A tap shorter than a frame
  is a press and a release of the same frame, so M2 builds `Released = (History.Held | Pressed) & ~Held`.

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
| `Hold` | `Button`, `MinFrames` | `Button` held continuously for ≥ `MinFrames` ending at the anchor (± simultaneity). `MinFrames = 0` = "held at the anchor" (modifier buttons such as crouch). Never consumes, does not need an unconsumed press. | 5 |
| `Direction` | `Space` (default `Facing`), `Directions` mask, `MinZone`, `MaxZone`, `bRequireFlick` | Direction in `Space` at the anchor (± simultaneity) is in the mask, with `MinZone ≤ zone ≤ MaxZone` (and flick if required). "Stick neutral" = mask `Neutral` + all, `MaxZone = Aim` (a light tilt only aims). | 5, +3 if `MinZone = Tilt`, +2 if flick |
| `Motion` | `Space`, `Sequence: TArray<FGSTN_DirectionMask>`, `MaxFramesBetween`, `MaxTotalFrames`, `MinZone` | Direction events (in `Space`) matching the sequence in order, gaps ≤ `MaxFramesBetween`, whole motion ≤ `MaxTotalFrames`; other events in between are allowed (leniency). Anchor = frame of the last element. | 5 per element |
| `Spin` | `Space` (default `Stick`), `MinDegrees` (360), `MaxFrames`, `Rotation` (Clockwise / CounterClockwise / Either), `MinZone` (Tilt), `MaxSkippedSectors` (1) | Reading direction events backward from the anchor within `MaxFrames`: a chain of sector steps all in the same rotation sense (each step 1 to 1 + `MaxSkippedSectors` sectors of 45°), zone ≥ `MinZone`, totalling ≥ `MinDegrees`. **Any starting sector.** Anchor = the chain's last event. | 5 per 45° |
| `Chord` | `ButtonMask` (all of), `WithinFrames` | Unconsumed presses of every button in the mask, all within `WithinFrames` of each other. Anchor = newest. | 10 per button |
| `Charge` | `Directions` mask, `MinFrames`, `MaxReleaseFrames` | Direction in the mask held for ≥ `MinFrames`, left no more than `MaxReleaseFrames` before the anchor. | 15 |
| `Mash` | `Button`, `Count`, `WithinFrames` | `Count` unconsumed presses of `Button` within `WithinFrames`. | 5 × Count |
| `AnyOf` | `Conditions` (instanced) | Any child holds (first in order wins). | max of children |

Only `Press`, `Release`, `Chord` and `Mash` use (and so consume) button events; `Hold` and direction conditions never
consume. `Spin` and `Motion` are anchor conditions; combined with a `Press` they are usually two steps (spin, then the
press within `MaxGapFrames`), or one step when the press may come during the last part of the spin.

**Pattern (`FGSTN_InputPattern`)** — `Steps: TArray<FGSTN_InputStep>`; `FGSTN_InputStep { TArray<TInstancedStruct<
FGSTN_InputCondition>> AllOf; int32 SimultaneityFrames = -1 (= settings); int32 MaxGapFrames = 15 (to the next step);
}`. Validation: each step has exactly one anchor condition; at least one step. `GetSpecificity()` = sum of conditions.
`FGSTN_PatternMatch { TArray<int32, TInlineAllocator<4>> StepAnchors; TArray<uint8, TInlineAllocator<8>>
UsedButtonEvents; int32 TerminalPressEvent = INDEX_NONE; bool bViaHoldBuffer; }`.

**Matcher (`FGSTN_InputMatcher`)** — `static bool Match(const FGSTN_InputPattern&, const FGSTN_InputHistory&,
const FGSTN_MatchParams&, FGSTN_PatternMatch& Out)`. `FGSTN_MatchParams { int32 Now; int32 BufferFrames; bool
bHoldBuffer; int32 MaxHoldBufferFrames; EGSTN_DirectionPolicy DirectionPolicy; int32 DefaultSimultaneityFrames;
uint32 ReservedEvents (bitmask over the 16 button events); int32 PausedStart = INDEX_NONE; int32 PausedEnd; }`.
Frames inside `[PausedStart, PausedEnd)` (the last hitstop window, used from M4) do not count towards a press's age.
Algorithm:

1. Terminal step: candidate anchors newest first, restricted to presses whose age (`Now - Frame` minus paused frames) is ≤ `BufferFrames`; if `bHoldBuffer`, also
   older presses that are still held continuously (and within `MaxHoldBufferFrames` when non-zero) — then
   `bViaHoldBuffer = true`.
2. With `DirectionPolicy = AtExecution`, non-anchor direction conditions of the terminal step are evaluated at `Now`
   instead of the anchor.
3. Each earlier step searches `[Anchor(next) - MaxGapFrames, Anchor(next)]`, newest first; backtrack on failure.
   An event used by a later step cannot be reused by an earlier one.
4. Bound: at most 16 candidates per step, depth = step count; abort and fail past 4096 evaluations (assert in dev).

*Implementation notes (M1.4):*

- **Buttons are bound through `FGSTN_ButtonMap`** (tag -> bit, built by `UGSTN_CombatInputConfig::MakeButtonMap()` or
  `FGSTN_ButtonMap::MakeStandard()`), passed in `FGSTN_MatchParams::Buttons` and looked up at evaluation (at most 32 integer
  compares per reference), instead of resolving conditions in place when a slot set is bound: the same slot data serves
  every config and nothing mutates shared assets. A compiled form can come with the M3 table if profiling asks. Unknown
  tag = mask 0 = the condition never holds (the validator reports it through `GetReferencedButtons`).
- **Condition interface** adds `IsDirectionCondition()` (judged at the execution frame under `AtExecution`) and
  `GetReferencedButtons()`; `AppendCanonicalInput(Script, Map, Frame)` takes the map to turn tags into masks. Conditions are
  built with `TInstancedStruct<>::Make<T>(value)` (the unqualified `Make(value)` overload slices to the base type).
- **Step anchor**: the *first anchor-capable* condition of `AllOf` is the anchor (`FindAnchorIndex`); a step needs at least
  one (the spec said exactly one). Any other anchor-capable condition (a `Spin` next to the `Press`) must have its own
  reference event within `SimultaneityFrames` of the anchor. Anchor conditions list candidates newest first: a candidate made
  of button events is passed by excluding its events, a stick based one by moving the search limit one frame back.
- **Terminal buffer rule** applies to the anchor frame of any terminal condition (a motion alone fires within
  `BufferFrames` of its end); the hold buffer only to terminal presses (`TerminalPressSlot`: the newest press of a press,
  chord or mash). `Release` conditions use unconsumed releases only.
- **Hold** is "down at some frame of `[anchor - Sim, anchor + Sim]` (not after Now) for at least `MinFrames` at that
  frame"; a button is down from its press frame up to, not including, its release frame, so a tap is never down.
- **Direction** is "in the set and zone range at some frame of the same window"; the flick flag only on its own frame.
- **Motion** elements are *entered* at the first stick event of a run satisfying them; gaps and the total are measured
  between entries; neutral elements ignore `MinZone`. **Spin** merges consecutive stick events with the same sector, a
  stick event below `MinZone` or neutral breaks the chain. **Charge** gains `Space` (default Facing) and `MinZone`
  (default Tilt); its run lasts until the first stick event outside the set, and a run still going counts as released
  "in the future". Defaults: motion 12/30 frames, spin 30 frames, charge 30 frames held / 8 released.
- **AnyOf** used as an anchor reports the newest candidate among its children (first child on a tie), not simply the first
  child that holds, so candidate enumeration stays newest first.
- `FGSTN_MatchParams::FromSettings`, `FGSTN_InputMatcher::GetAge` (paused frames between the event and now excluded) and
  `FGSTN_InputPattern::IsStructurallyValid` are part of the API; an invalid pattern never matches.

**Reset rules (`FGSTN_BufferResetRule`)** — base fields `uint32 ButtonMask` (0 = all). Interface
`bool ShouldFlush(const FGSTN_BufferFrameEvents& Events, const FGSTN_InputHistory& H, int32 Now) const`.
`FGSTN_BufferFrameEvents { FGameplayTagContainer EventTags; FGameplayTagContainer TagsAdded; }` is filled by the
caller (M2: from combat events and ASC tags; M1: tests). Standard rules: `OnEvent(EventTag)`, `OnTagAdded(Tag)`,
`AfterIdleFrames(Frames)` (no button or direction event for `Frames`). Flush = `FlushPresses(Mask, Now)`, which ends
time and hold buffering of those presses. `ApplyResetRules(History, Rules, Events, Now)` is a static pure function;
rule lists concatenate global → weapon → move.

**Slots (`GSTN_InputSlot.h`)**

- `FGSTN_InputSlotSpec { FGameplayTag Command (meta = Categories="Command"; unique within a set; the slot's identity
  everywhere: bindings, cancel windows, AI, events); FGSTN_InputPattern Pattern; FGameplayTagRequirements Context;
  int32 PriorityOffset = 0; int32 BufferFramesOverride = -1; EGSTN_OverrideBool HoldBuffer = Default;
  EGSTN_OverrideBool ChordGrace = Default; EGSTN_DirectionPolicy DirectionPolicy = AtPress;
  bool bFallThroughWhenUnavailable = false; }`.
- `FGSTN_CommandPriority { int32 Tier; int32 Specificity; int32 Offset; }` with lexicographic `operator<`;
  `GetPriority(const UGSTN_CombatInputSettings&)` on the spec. No single summed number: a higher tier always wins.
- `UGSTN_InputSlotAsset : UPrimaryDataAsset { FGSTN_InputSlotSpec Spec; }` (primary type `GSTN_InputSlot`).
- `UGSTN_InputSlotSet : UDataAsset { TArray<FGSTN_InputSlotEntry> Slots; }` where an entry is either an asset reference
  or an inline spec. `IsDataValid` runs the validator.

**Resolver (`FGSTN_SlotResolver`)** — `static FGSTN_SlotResolution Resolve(TConstArrayView<const FGSTN_InputSlotSpec*>
Slots, const FGSTN_InputHistory&, const FGameplayTagContainer& OwnerTags, int32 Now, const UGSTN_CombatInputSettings&,
TFunctionRef<bool(int32 SlotIndex)> CanExecute)`. The caller passes only the candidate slots allowed right now (M2:
all when actionable; M3: those allowed by a cancel window or the startup override query). Result `{ int32 SlotIndex =
INDEX_NONE; FGSTN_PatternMatch Match; TArray<uint8, TInlineAllocator<8>> EventsToConsume; bool bDeferredForChord; }`.
Steps (combat.md §8.2):

1. Match every candidate whose `Context` holds (each with its own buffer params; reserved events excluded).
2. **Press group**: with `MostRecent`, find the newest terminal press among the matches; keep only matches whose
   terminal press is within `SimultaneousFrames` of it (`Oldest`: mirror; `HighestPriority`: keep all).
3. Order the kept matches by `FGSTN_CommandPriority` descending, ties by slot order (the validator reports real ties).
4. Walk them: `CanExecute` → winner. Not executable → reserve its terminal press (later candidates sharing it are
   dropped) unless `bFallThroughWhenUnavailable`.
5. Chord grace, only for slots where it is on: before accepting a single-press winner, if a higher-priority slot with a
   `Chord` containing that button has a partial match and the press is younger than `ChordGraceFrames`, return
   `bDeferredForChord` with no slot.
6. `EventsToConsume` = the winner's `UsedButtonEvents` + the unconsumed presses inside the winner's press group whose
   button is in `ExclusiveButtons` (the losers). The caller consumes them on activation (M2: in the sync state).

*Implementation notes (M1.5):*

- **`Context` is an `FGSTN_TagRequirements { RequireTags, IgnoreTags }`** (defined in `GSTN_InputSlot.h`), not
  `FGameplayTagRequirements`: that type lives in the GameplayAbilities module, which the Core must not depend on. Same
  semantics (all required present, none ignored present).
- **Reset rules**: a rule's button filter is a tag container (empty = all buttons, bound through the button map);
  `FGSTN_BufferResetRules::Apply(History, Buttons, Rules, Events, Now)` is applied once per list (global, then weapon, then
  move). `AfterIdleFrames` counts from the newest button or stick event. Rules also report `GetWatchedTags()` (the tags the
  owner must track for `OnTagAdded`, M2 section 6).
- **Resolver**: `Resolve(Slots, History, FGSTN_ResolveParams, Settings, CanExecute)`; the params carry the button map,
  the owner tags, `Now` and the optional hitstop window. Slots that match are ranked by `FGSTN_CommandPriority`; the press
  group is the matches whose terminal frame is within `SimultaneousFrames` of the newest (`Oldest`: of the oldest);
  `CanExecute` is called at most once per slot and only for slots that matched. A slot that cannot execute reserves its
  terminal press and the slots below it are matched again without it (so they may still use an older press). Consumption:
  the winner's events plus every unconsumed press of an exclusive button within `SimultaneousFrames` of the group frame
  (the winner's own frame with `HighestPriority`). Chord grace defers only a winner whose terminal anchor is a plain `Press`,
  whose slot has the grace on, whose press is younger than `ChordGraceFrames`, when a higher ranked slot in context ends in
  a `Chord` with that button and has not matched.
- `UGSTN_InputSlotSet::GetSpecs()` gives the specs in order for the resolver and the validator (`IsDataValid` arrives
  with the validator in M1.6).

**Validation (`UGSTN_InputSlotValidation`)** — `FindConflicts(Slots) → TArray<FGSTN_SlotConflict {A, B, Kind
(Ambiguous | Shadowed), Message}>`: build each slot's canonical script (`FGSTN_InputScript` from
`AppendCanonicalInput`), run the resolver over every other slot on that history (context ignored, unless both contexts
are provably disjoint: one requires a tag the other forbids). Both match and their `FGSTN_CommandPriority` is equal →
Ambiguous (error); a higher priority always matching → Shadowed (warning); duplicate command tags in one set → error.

*Implementation notes (M1.6):*

- **Validator**: `FindConflicts(Slots, Settings)` (and an overload for a set asset) returns `FGSTN_SlotConflict { SlotA, SlotB,
  Kind, Message }` with kinds `Ambiguous` (error), `Shadowed` (warning, `SlotA` is the shadowed slot), `DuplicateCommand`
  (error) and `InvalidSlot` (error: no command tag, malformed pattern, canonical input that cannot be played). It matches
  each slot's canonical input against the other slots' patterns **with the matcher** (not the resolver: reservation and press
  groups do not matter for a static check), with each slot's own buffer overrides, comparing `FGSTN_CommandPriority`.
  Buttons get bits from the set's own button tags sorted by name, so no input config is needed. Only canonical inputs are
  explored: it cannot find a conflict that no canonical input reaches.
- **`UGSTN_InputSlotSet::IsDataValid`** reports the findings (errors invalidate, shadows warn). A `CallInEditor` button
  **Fill With Standard Slots** replaces the slots with `UGSTN_StandardSlots::Build()` so a designer can start from or inspect
  the standard set; plugin content is git-ignored (combat.md section 3), so such assets are test content until the
  exception is added.
- **`UGSTN_StandardSlots::Build()`** returns the 24 family commands (per family: grounded then airborne, each Neutral,
  Directional, CrouchNeutral, CrouchDirectional) and the 4 chord commands. Grounded slots forbid
  `Combat.Context.Airborne`, airborne ones require it (provably disjoint contexts); crouch is `Hold(Crouch, 0)`, so a crouch
  press up to the simultaneity tolerance after the attack press still selects the crouch version. Priorities in a tier:
  Neutral 15 < Directional 18 < CrouchNeutral 20 < CrouchDirectional 23 (chords: +10 on each).

## 4. Input notation (`FGSTN_InputScript`)

Used by tests, the validator and later the input display. Space-separated tokens `<frame>:<item>`:

- Buttons by index or config tag name: `+A` press, `-A` release, `*A` tap (press and release on the same frame).
- Directions are the **physical stick** (`Stick` space), numpad layout: `8` up, `2` down, `4` left, `6` right,
  `7 9 1 3` diagonals, `5` neutral; zone suffix `a` (aim) or `t` (tilt, default); `!` flick. `<frame>:@<deg>[a|t]`
  puts the stick at an exact angle (0 = up, clockwise), for spins and aim tests.
- Directives `<frame>:cam=<deg>` and `<frame>:face=<deg>` set the camera and facing yaw from that frame (default 0 and 0:
  the character faces away from the camera). The builder runs the real classifier, so facing and world directions are
  derived exactly as in the game.
- Example: `0:face=90 0:4t 6:6t! 7:+A` = stick left, then flick right and press A, with the character turned 90° from
  the camera.
- A trailing `!` makes the stick token a flick **for the real classifier**: the stick is released on the previous frame
  (a `!` at frame 0, or on a neutral stick, is an error), so the flag in the history comes from `FlickFrames`, not from
  the notation.
- `FGSTN_InputScript` is a value: `Parse` adds events, `Advance(History, Settings, UntilFrame)` plays frames up to a
  frame and continues where it stopped (copying the script forks the session, the rollback stand-in of the determinism
  tests); `Build(Notation, Settings, History, LastFrame)` is parse + play to the last token. Buttons resolve by letter
  (`A` = bit 0), `#n`, or the last part of the tag name through the script's button tags (default: the six standard
  buttons in that order). `Describe(History, MaxEvents)` prints a history in this notation (stick events as numpad key +
  zone letter, `!` for a flick, `?` for a frame too old to know); M2's `ToString` uses it.
- `Build(const FString&, FGSTN_InputHistory&, int32& OutLastFrame)` fails loudly on syntax errors and impossible
  sequences (pressing a held button, releasing a released one): it logs an error and returns false.
- The input display (M5) shows the same notation, with the facing-relative direction as a secondary line.

## 5. Tests (Automation, names `GSTNCombat.Core.Input.<Area>.<Case>`)

| Area | Cases (expected result) |
|---|---|
| History | Ring overflow keeps the newest 16; serialize → deserialize equality; events older than 255 frames dropped; same-frame press+release recorded in order; `GetDirectionAt` returns the last change at or before the frame. |
| Classifier | Dead/aim/tilt thresholds exactly at and around the limits; the same raw stick with camera 0° and 90° gives the same `Stick` direction and different `World` directions; facing 90° maps the stick correctly in `Facing` space; turning the character while holding the stick adds one event per sector; flick within `FlickFrames`, none when slower; quantized input classified identically to its float. |
| Conditions | One positive and one negative case per condition and field (window edges inclusive, consumed press ignored, reserved press ignored, negative edge needs the minimum hold, motion with noise between elements passes, motion over `MaxTotalFrames` fails, chord outside `WithinFrames` fails, charge released too early fails, mash count; `Hold` with `MinFrames = 0` matches a modifier pressed up to `SimultaneityFrames` after the anchor press; `Direction` with `MaxZone = Aim` rejects a tilt; `Spin` clockwise and counter-clockwise from every starting sector, with one skipped sector, fails at 315°, fails when slower than `MaxFrames`, `Either` accepts both senses). |
| Buffer | Press at `Now - BufferFrames` matches, at `-BufferFrames-1` fails, matches again when a paused window covers the extra frame; held press matches past the window (`bViaHoldBuffer`), fails after release, fails past `MaxHoldBufferFrames`; `AtPress` vs `AtExecution` give different results when the direction changes after the press. |
| Resolver | Motion+A beats Dir+A beats A on one history; a consumed press does not fire again next frame; unavailable higher slot reserves the press (no lower slot fires) and with `bFallThroughWhenUnavailable` the lower one fires; context filtering. **Priority:** Attack and Special on the same frame → special (tier); Special 1 frame after Attack (within `SimultaneousFrames`) → special; Attack 4 frames after Special with `MostRecent` → attack, with `HighestPriority` → special; Attack+Special chord command beats both singles in the same group; a crouch-directional attack loses to a neutral special (tier before specificity); after the winner, the losing attack press is consumed (nothing fires next frame); a non-exclusive button (e.g. Jump) in the same group is not consumed. **Chord grace** (turned on for one slot): defers, then resolves A+B, or A alone after `ChordGraceFrames`; off by default. |
| Reset | `OnEvent` flushes only masked buttons; `AfterIdleFrames`; a flush ends hold buffering. |
| Standard slots | With the standard command set of combat.md §9.1 (24 commands + one chord command) on scripted histories, for each family: stick neutral + A → neutral attack; light tilt (aim zone) + A → neutral attack (aimed); tilt + A → directional attack; crouch held + neutral + A → crouch neutral; crouch held + tilt + A → crouch directional; crouch pressed 2 frames after A → still the crouch version; airborne context selects the `Command.Air.*` ones; the validator reports no conflicts on the set. |
| Validation | Two identical patterns with equal priority → Ambiguous; A vs Dir+A → no conflict (different priority) and A not reported shadowed; a slot fully covered by a higher one → Shadowed; disjoint contexts → no conflict. |
| Determinism | Same script resolved twice → identical results; copy the history mid-script, resolve the rest on both copies → identical (rollback stand-in). |

Run headless:
`UnrealEditor-Cmd.exe "<project>/GASP_Giustino2.uproject" -ExecCmds="Automation RunTests GSTNCombat.Core;Quit" -unattended -nullrhi -nopause -log`
(or the Session Frontend). Add the command to testing.md.

## 6. Acceptance criteria

1. The project builds (Development Editor) with `GSTNCombatCore` listed in `GSTNCombat.uplugin`; Core's Build.cs has
   none of the forbidden dependencies (section 1).
2. All tests of section 5 pass headless.
3. Every public type has a header doc comment on intent and determinism, at the density of the existing headers.
4. A standard slot set can be authored in the editor with the condition structs (instanced struct pickers work) and
   `IsDataValid` reports conflicts. (Plugin content needs the `.gitignore` exception of combat.md §3; until then create it
   under `/Game/GAS/` and tell the user.)
5. This document is updated where implementation differs (types, defaults), and testing.md gets the test command.

## 7. Notes for the implementer

- Keep the history and every result type POD-like and small: in M2 they are copied every frame into the sync state.
- Never iterate `TMap`/`TSet` in matching; never use floats in frame logic (the classifier converts once).
- The resolver will be called every frame for every fighter: avoid allocations in the hot path (inline allocators,
  static arrays).
- No changes to `GSTNCombatNP` or project code in M1; the spike keeps working as is until M2 replaces its
  button→ability mapping with the resolver.
