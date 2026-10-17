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
- Directions, top-down numpad **relative to facing**: `8` forward, `2` back, `4` left, `6` right, `7 9 1 3` diagonals,
  `5` neutral; zone suffix `a` (aim) or `t` (tilt, default); `!` flick. Example: `0:2t 6:8t! 7:+A` = back, then flick
  forward and press A.
- `Build(const FString&, FGSTN_InputHistory&, int32& OutLastFrame)` fails loudly on syntax errors.
- *Open:* confirm this notation with the designers (it differs from the 2D fighting-game convention where 6 = forward).

## 5. Tests (Automation, names `GSTNCombat.Core.Input.<Area>.<Case>`)

| Area | Cases (expected result) |
|---|---|
| History | Ring overflow keeps the newest 16; serialize → deserialize equality; events older than 255 frames dropped; same-frame press+release recorded in order; `GetDirectionAt` returns the last change at or before the frame. |
| Classifier | Dead/aim/tilt thresholds exactly at and around the limits; facing 90° maps world stick correctly to Forward/Right; flick within `FlickFrames`, none when slower; quantized int8 input classified identically to its float. |
| Conditions | One positive and one negative case per condition and field (window edges inclusive, consumed press ignored, reserved press ignored, negative edge needs the minimum hold, motion with noise between elements passes, motion over `MaxTotalFrames` fails, chord outside `WithinFrames` fails, charge released too early fails, mash count). |
| Buffer | Press at `Now - BufferFrames` matches, at `-BufferFrames-1` fails, matches again when a paused window covers the extra frame; held press matches past the window (`bViaHoldBuffer`), fails after release, fails past `MaxHoldBufferFrames`; `AtPress` vs `AtExecution` give different results when the direction changes after the press. |
| Resolver | Motion+A beats Dir+A beats A on one history; a consumed press does not fire again next frame; unavailable higher slot reserves the press (no lower slot fires) and with `bFallThroughWhenUnavailable` the lower one fires; context filtering; chord grace defers then resolves A+B, and resolves A alone after `ChordGraceFrames`. |
| Reset | `OnEvent` flushes only masked buttons; `AfterIdleFrames`; a flush ends hold buffering. |
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
