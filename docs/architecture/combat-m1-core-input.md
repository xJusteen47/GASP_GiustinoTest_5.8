# Combat M1 — Core input layer (implementation specification)

Part of the combat design ([combat.md](combat.md)): read sections 5–9 there first. This file says exactly what to
build in M1, how it is tested and when it is done. Section numbers below are local to this file; "combat.md §N" points
to the architecture.

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
    Input/GSTN_CombatInputTags.h      native tags of the standard buttons: Input.Combat.Button.Attack/Special/Crouch/Shield/Jump
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
  `FacingForward` = Up|UpLeft|UpRight, ...).
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
| `ChordGraceFrames` | 3 | Single-button wait when a chord could complete. |
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
Standard tags (native, `GSTN_CombatInputTags.h`): `Input.Combat.Button.Attack`, `.Special`, `.Crouch`, `.Shield`,
`.Jump`; projects add their own.

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
`FGSTN_ClassifierState { EGSTN_StickZone Zone; int32 LastLowZoneFrame; }` lives in the sync state in M2 (needed to
detect flicks after a rollback). Quantized input (int8 stick, uint8 camera yaw) is converted back before
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

**Reset rules (`FGSTN_BufferResetRule`)** — base fields `uint32 ButtonMask` (0 = all). Interface
`bool ShouldFlush(const FGSTN_BufferFrameEvents& Events, const FGSTN_InputHistory& H, int32 Now) const`.
`FGSTN_BufferFrameEvents { FGameplayTagContainer EventTags; FGameplayTagContainer TagsAdded; }` is filled by the
caller (M2: from combat events and ASC tags; M1: tests). Standard rules: `OnEvent(EventTag)`, `OnTagAdded(Tag)`,
`AfterIdleFrames(Frames)` (no button or direction event for `Frames`). Flush = `FlushPresses(Mask, Now)`, which ends
time and hold buffering of those presses. `ApplyResetRules(History, Rules, Events, Now)` is a static pure function;
rule lists concatenate global → weapon → move.

**Slots (`GSTN_InputSlot.h`)**

- `FGSTN_InputSlotSpec { FName SlotName (unique within a set; referenced by cancel windows and customisations in M3); FGSTN_InputPattern Pattern; FGameplayTagRequirements Context; int32 Priority
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
- `Build(const FString&, FGSTN_InputHistory&, int32& OutLastFrame)` fails loudly on syntax errors.
- The input display (M5) shows the same notation, with the facing-relative direction as a secondary line.

## 5. Tests (Automation, names `GSTNCombat.Core.Input.<Area>.<Case>`)

| Area | Cases (expected result) |
|---|---|
| History | Ring overflow keeps the newest 16; serialize → deserialize equality; events older than 255 frames dropped; same-frame press+release recorded in order; `GetDirectionAt` returns the last change at or before the frame. |
| Classifier | Dead/aim/tilt thresholds exactly at and around the limits; the same raw stick with camera 0° and 90° gives the same `Stick` direction and different `World` directions; facing 90° maps the stick correctly in `Facing` space; turning the character while holding the stick adds one event per sector; flick within `FlickFrames`, none when slower; quantized input classified identically to its float. |
| Conditions | One positive and one negative case per condition and field (window edges inclusive, consumed press ignored, reserved press ignored, negative edge needs the minimum hold, motion with noise between elements passes, motion over `MaxTotalFrames` fails, chord outside `WithinFrames` fails, charge released too early fails, mash count; `Hold` with `MinFrames = 0` matches a modifier pressed up to `SimultaneityFrames` after the anchor press; `Direction` with `MaxZone = Aim` rejects a tilt; `Spin` clockwise and counter-clockwise from every starting sector, with one skipped sector, fails at 315°, fails when slower than `MaxFrames`, `Either` accepts both senses). |
| Buffer | Press at `Now - BufferFrames` matches, at `-BufferFrames-1` fails, matches again when a paused window covers the extra frame; held press matches past the window (`bViaHoldBuffer`), fails after release, fails past `MaxHoldBufferFrames`; `AtPress` vs `AtExecution` give different results when the direction changes after the press. |
| Resolver | Motion+A beats Dir+A beats A on one history; a consumed press does not fire again next frame; unavailable higher slot reserves the press (no lower slot fires) and with `bFallThroughWhenUnavailable` the lower one fires; context filtering; chord grace defers then resolves A+B, and resolves A alone after `ChordGraceFrames`. |
| Reset | `OnEvent` flushes only masked buttons; `AfterIdleFrames`; a flush ends hold buffering. |
| Standard slots | With the standard attack slots of combat.md §9.1 on scripted histories: stick neutral + A → neutral attack; light tilt (aim zone) + A → neutral attack (aimed); tilt + A → directional attack; crouch held + neutral + A → crouch neutral; crouch held + tilt + A → crouch directional; crouch pressed 2 frames after A → still the crouch version; the validator reports no conflicts on the set. |
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
