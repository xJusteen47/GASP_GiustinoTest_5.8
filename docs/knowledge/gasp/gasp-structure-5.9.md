---
topic: GASP Mover (UE 5.9) animation structure and how to extend it
scope: SandboxCharacter_Mover_ABP, CHT_MoverCharacterAnimations_PoseMatch, Blend Stack, Pose Search columns, our ABP_GSTN_Locomotion diff
status: reference (research only, nothing in the project was changed)
written: 2026-11-20, thread gasp-structure-1, branch claude/gasp-structure-1
evidence: docs/knowledge/gasp/evidence/ (compact dumps), Tools/GAS/routing/gasp_structure_dump.py (headless, read-only)
keywords: GASP, Mover, state machine, chooser, PoseSearchColumn, pose match, motion matching, blend stack, root motion, OffsetRootBone, warping, foot placement, curves, notifies, EarlyTransition, custom clips, extension rules
---

# GASP Mover 5.9: how the animation is built, and how to extend it

How to read this file: section 0 is the one-screen answer. Each later section starts with **Verified** facts (read from the assets
by the headless dump, file named in brackets) and then **Assistant** notes (the Unreal Editor AI assistant, asked 2026-11-20,
replies in `evidence/assistant_replies.md`; useful but not checked line by line, treat as "very likely").
Asset paths are under `/Game/`. "Stock" here = `Blueprints/SandboxCharacter_Mover_ABP` and its chooser in the ue59 project, which
are Epic's except for our post-process nodes and layer slots listed in section 9.

## Index (for agents)
| # | Section | Answers |
|---|---|---|
| 0 | One-screen summary | "how does GASP Mover pick a clip?" |
| 1 | AnimGraph chain | node order, MM vs state-machine switch, cvars |
| 2 | State Controller state machine | states, aliases, every transition rule |
| 3 | Entry: SetBlendStackAnimFromChooser | what happens on state entry, OUT struct |
| 4 | Chooser tables | full table tree, columns, which tables pose-match |
| 5 | PoseSearchColumn and internal databases | how the clip AND start frame are picked |
| 6 | Exit rules and notifies | IsAnimationAlmostComplete, EarlyTransition notify, Re-Enter |
| 7 | Root motion, warping, play rate | what the clip root must match |
| 8 | Feet and curves | foot placement, contact curves, what a missing curve does |
| 9 | Our setup vs stock (diff) | ABP_GSTN_Locomotion, CHT_GSTN_* |
| 10 | Rules for adding custom clips/states | checklist |
| 11 | Open questions | what is still unverified |

## 0. One-screen summary
- GASP Mover runs the **state-machine setup** (`DDCvar.LocomotionSetupMover` = 1): a pose-less state machine ("State Controller")
  decides *which kind* of clip is needed; a **Chooser** picks the clip; a **Blend Stack** plays and blends it.
- Motion matching (the `MotionMatching` node, index 0 of a BlendListByInt) is the *other* setup and is not the visible pose in
  setup 1. What remains of "MM" in setup 1 is **pose search inside the chooser**: in most leaf tables a `PoseSearchColumn` picks,
  among the rows that pass the filters, the best clip **and its start frame** by comparing the clip to the pose history
  (PoseSearchHistoryCollector). This is the owner's "MM only refines".
- Only these leaf tables have **no** pose search (fixed start time from the table): Idle Loops, Idle Breaks, In Air Loops,
  Slide Loops/Transitions, Stand/Crouch Turn In Place, and the root's Debug Flying row.
- A state exits on: the clip being almost over (non-looping and <= 0.75 s left), no valid clip, an `EarlyTransition` notify window
  in the clip (flags `NotifyTransition_ToLoop` / `Re-Transition`), or a Re-Enter alias (direction/gait/stance change, pivot,
  facing change > 60 deg in the first 0.5 s).
- Clips carry **root motion that matches the capsule**, curves (`contact_l/r`, `phase`, `movedata_speed`, `enable_warping`, ...)
  and notify states (`PoseSearchBlockTransition`, `ExcludeFromDatabase`, `BranchIn`, Foley). Warping, steering, OffsetRootBone and
  dynamic play rate absorb small differences only.
- **Extension rule (recommended):** locomotion variants (start, stop, pivot/skid, landing) go in as **rows of the existing nested
  tables** so they get pose-matched entry and the stock exit rules; a **new state** only for new gameplay meaning (e.g. Dash), with
  its own row keyed by the state name.

## 1. AnimGraph chain
**Verified** [Saved/GaspStructure/stock_abp_graphs.txt (not committed, regenerate with the dump script); docs/notes/abp-architecture.md]: output backwards:
Root <- GSTN_GroundContactFix* <- PoseSearchHistoryCollector <- Inertialization <- Procedural_Feet layer (foot placement) <-
GSTN_BlendOutFootLock* <- OffsetRootBone <- DeadBlending <- Slot DefaultSlot (montages) <- AimOffset / AdditiveLeans /
Procedural_PreLayering layers <- **BlendListByInt (index = LocomotionSetup)**:
- index 0: `MotionMatching` node, databases from `CHT_PoseSearchDatabases_Mover` (setup 0, pure MM);
- index 1: TwoWayBlend (alpha 1) of the **State Controller** machine (through Inertialization) and **BlendStack** tagged
  `StateMachineBlendStack` (setup 1, default for Mover).
(* = our nodes, see section 9.) The Ragdoll machine (NoRagdoll / In Ragdoll / Ragdoll In Air / On Ground) sits on top.
- cvars read in `Update_CVarDrivenVariables`: `DDCVar.LocomotionSetupMover`, `DDCvar.MMDatabaseLOD`, `DDCVar.FootPlacementMode`,
  `DDCvar.OffsetRootBone.TranslationRadius`, `a.animnode.offsetrootbone.enable`, `a.AnimNode.FootPlacement.Enable/Debug`,
  `DDCVar.ThreadSafeAnimationUpdate.Enable`.
- `IsPivoting` itself switches on LocomotionSetup: composite "MM Pivot conditions" for 0, "SM Pivot Conditions" for 1, and it also
  requires MovementState == moving.

**Assistant:** in setup 1 the MotionMatching node is **not evaluated at all** once the BlendListByInt has settled (zero-weight
child: no Update, so `Update_MotionMatching`, `CHT_PoseSearchDatabases_Mover`, `MMDatabaseLOD` and `Update_MotionMatching_PostSelection`
do nothing and `CurrentDatabaseTags` goes stale). What still matters: the PoseSearchHistoryCollector (feeds the PoseHistoryReference
of every PoseSearchColumn query), the trajectory (FutureFacingDelta filters, the 60 deg exit rule, the trajectory channel of the
internal databases), and the pose search inside the chooser, run once per state entry. Setup 0 suits dense mocap coverage; for
stylised, keyed clips with few clips per gait the assistant recommends staying on setup 1, pose search only on stops and loco
transitions, deterministic rows (StartTime 0) for authored-timing starts and turns.
**Verified consequence:** `IsStarting` reads `CurrentDatabaseTags` (contains "Pivots"), so in setup 1 that part of IsStarting reads a
stale value; prefer the Blend Stack `Tags` (OUT struct) in any rule we add.

## 2. State Controller (states, aliases, transitions)
**Verified** [evidence/stock_state_machine.txt]: 10 real states, each with **only** an On State Entry function
(`OnStateEntry_<State>`), no exit/fully-blended functions, and no pose inside (anything placed inside a state is never seen).
| State | Entry function | Chooser key (StateName) |
|---|---|---|
| Idle Loop / Idle Transition / Idle Break | OnStateEntry_IdleLoop / _IdleTransition / _IdleBreak | same names |
| Locomotion Transition / Locomotion Loop | OnStateEntry_LocomotionTransition (also stores FutureFacingOnTransitionStart) / _LocomotionLoop | same |
| In Air Transition / In Air Loop | OnStateEntry_InAirTransition / _InAirLoop | same |
| Slide Transition / Slide Loop | OnStateEntry_SlideTransition / _SlideLoop | same |
| Debug Flying Loop | OnStateEntry_DebugFlyingLoop | root row, fixed clip M_Relaxed_Jump_Loop_Fall |
Aliases: `Idle -> Locomotion`, `Locomotion -> Idle`, `-> In Air`, `-> Grounded` (to a Conduit), `-> Slide`, `-> Debug Fly`,
and four `Re-Enter` aliases (names differ only by trailing spaces). Locomotion Transition also has OnUpdate_LocomotionTransition
(RInterpTo of FutureFacingOnTransitionStart, speed 4).

Transitions (all cross-fade 0.2 s in the machine; the visible blend is the Blend Stack's, section 3):
| From -> To | Rule |
|---|---|
| Idle->Locomotion -> Locomotion Transition | MovementState == Moving (enum 0) |
| Locomotion->Idle -> Idle Transition | MovementState == Idle (enum 4) |
| Conduit -> Locomotion Transition / Idle Transition | MovementState Moving / Idle |
| Locomotion Transition -> Locomotion Loop | (shared) IsAnimationAlmostComplete && state time > x; (shared) NoValidAnim or BlendStackInputs.Loop; `NotifyTransition_ToLoop`; Trj_CirclingTime > 0.25; abs(SlopeAngle) > 15 && state time > 0.75 |
| Idle Transition / In Air Transition / Slide Transition / Idle Break -> their Loop | the same two shared rules (almost complete, or no valid anim / looping clip) |
| Idle Loop -> Idle Break | state time > 3 s |
| Re-Enter -> Locomotion Transition | direction/stance/gait changed vs last frame (after min state time); IsPivoting && state time > x && !circling && MovementMode_Recent == grounded; abs(delta FutureFacing vs facing at transition start) > 60 && state time < 0.5 && not a looping clip && !circling; `NotifyTransition_Re-Transition`; abs(FutureFacingDelta change) > 200 |
| Re-Enter -> Idle Transition | stance changed; ShouldTurnInPlace && (turn-in-place steering curve < 0.1 after 0.3 s, or Spin tag and facing delta > 45, ...) |
| -> In Air -> In Air Transition | MovementMode == Falling (5); Re-Enter on direction change |
| -> Grounded -> Conduit | MovementMode == Walking (4) |
| -> Slide -> Slide Transition | MovementMode == Slide (6); Re-Enter on direction change |
| -> Debug Fly -> Debug Flying Loop | MovementMode == 10 |
("x" = a minimum state time read from a variable; the dump shows the pin as empty.)
Re-entering the same state is the normal way GASP picks a new clip (a new start/pivot) while moving: the alias re-runs the
entry function, which evaluates the chooser again.

## 3. State entry: SetBlendStackAnimFromChooser
**Verified** [evidence/stock_functions.txt]: every OnStateEntry_X calls `SetBlendStackAnimFromChooser(StateName, StateEnum,
ForceBlend=true)`, which:
1. stores StateName; gets `PoseHistoryReference` from the PoseSearchHistoryCollector node (anim node reference);
2. `EvaluateChooser` on `CHT_MoverCharacterAnimations_PoseMatch` with (ABP, OUT struct `S_CHT_MoverCharacterAnimations_OUT`,
   PoseHistoryReference) -> `ChosenAnim`; if none, sets `NoValidAnim` = true (that bool is itself an exit rule);
3. fills `BlendStackInputs` {Anim, Loop = IsAnimationAssetLooping(anim), StartTime, BlendTime, BlendCurve,
   BlendProfile = GetBlendProfileByName(OUT.BlendProfile), Tags} from the OUT struct, keeps the previous inputs;
4. resets `NotifyTransition_ToLoop` and `NotifyTransition_Re-Transition` to false;
5. if the new clip is not looping (or ForceBlend), calls `ForceBlendOnNextUpdate` on the Blend Stack node, so a re-entry with
   the same clip still blends in a fresh instance.
OUT struct fields: StartTime, BlendTime, BlendCurve (EAlphaBlendOption), BlendProfile (name), Tags. Stock values: loops/idles
blend 0.5-0.75 s QuadraticInOut, transitions 0.2-0.4 s, landings 0.2 s with profile `FastFeet_FastRoot`, StartTime 0 in every row.

## 4. Chooser tables
**Verified** [evidence/stock_chooser_tree.txt]. Root `CHT_MoverCharacterAnimations_PoseMatch`
(ExperimentalStateMachineData), parameters: ABP context (class `SandboxCharacter_Mover_ABP_C`, ReadWrite), OUT struct (Write),
PoseHistoryReference (Read). One column `StateName`; rows -> nested tables (all sub-objects of the same asset):
| Nested table (filter columns) | Leaf tables | Pose search |
|---|---|---|
| Idle Loops (Stance) | clips | no |
| Idle Breaks (Stance, AO.X range, Randomize) | 13 clips | no |
| Idle Transitions (Stance, MovementMode_Recent, Speed2D, ShouldTurnInPlace) | Stand/Crouch Mode Transitions, Stand/Crouch Turn In Place, Stand/Crouch Stops, Stand/Crouch Stance Transitions | yes except Turn In Place |
| Locomotion Loops (Stance, Gait) | Walk/Run/Sprint/Crouch Loops (MovementDirection) | yes |
| Locomotion Transitions (Stance, Gait, MovementDirection, MovementMode_Recent) | {Walk,Run,Crouch} x {F,B,LL,RL} Loco Transitions + Mode Transitions, Sprint Loco/Mode Transitions | yes |
| In Air Loops | 1 clip | no |
| In Air Transitions (MovementDirection, Speed2D) | Jump Idle / F / B / LL / RL (Velocity.Z, Speed2D, MovementMode_LastFrame) | yes |
| Slide Loops / Slide Transitions (MovementDirection[_LastFrame]) | clips | no |
- "Loco Transitions" leaves (e.g. Run F, 42 rows) filter on Speed2D range, FutureFacingDelta range, IsPivoting, MovementDirection_Recent
  and hold starts, refacing starts (90/180 L/R), gait transitions (walk->run, sprint->run), spins, box turns, pivots; Lfoot/Rfoot
  pairs of the same move are separate rows and the **pose search decides the foot**.
- "Mode Transitions" leaves (MovementMode_Recent, IsPivoting, JustLanded_Heavy, JustTraversed) hold landings (light/heavy) and
  traversal exits.
- "Stops" leaves (Speed2D, FutureFacingDelta, MovementDirection_Recent) hold stops and refacing stops; 36 rows standing.
- Disabled rows exist (bDisabled) as separators.
- Same family, CMC version: `CHT_CMCCharacterAnimations` + standalone `PSD_SM_CMC_*` databases. The 5.7 sample's Mover table was
  `CHT_MoverCharacterAnimations` with standalone `PSD_SM_Mover_{Loops,Spins,Stops,Transitions,TraversalTransitions}`; in 5.9 those
  databases live **inside** the `_PoseMatch` chooser (schemas `PSS_SM_Mover_*` still standalone).

## 5. PoseSearchColumn and the internal databases
**Verified** [Saved/GaspStructure/stock2_choosers.txt, regenerate with the dump script]: 38 leaf tables end with a `PoseSearchColumn` (input = PoseHistoryReference, parameter 2).
Each cell holds `RootChooser` (the root table), `Data` {AnimAsset = the row's clip, bUseSingleSample, grid sampling, BlendParamX/Y,
SamplingRange, bEnabled, bDisableReselection, MirrorOption, BranchInId} and `Database` = one of six internal databases (sub-objects
of the chooser): `PSD Locomotion Loops`, `PSD Locomotion Transitions`, `PSD Locomotion Stops`, `PSD Locomotion Spin Transitions`,
`PSD Locomotion Traversal Transitions`, `PSD Idle Transitions`. In stock every cell uses SamplingRange 0-0 (whole clip),
MirrorOption UnmirroredOnly, BranchInId 0, bDisableReselection false.

**Assistant:** the column is a *scoring* column: it searches only the clips of the rows that survived the filter columns and returns
the lowest-cost sample, i.e. **the clip and the frame**; the matched time is written through the column's own output binding
(StartTime, also mirror and cost) into the OUT struct, after the output column (columns write in order), which is why StartTime 0
in the table still gives a mid-clip start. Internal databases index like normal PSDs (schema Sample Rate, DDC, async build; while a
build runs the column can return nothing = NoValidAnim). The schema is the internal database's own Schema property; its bones must
be in the PoseSearchHistoryCollector. `SamplingRange` limits where a clip may be entered (main control for entry frames);
`BranchInId` != 0 only for entries generated by a `PoseSearchBranchIn` notify state; `bUseSingleSample` matters only for blend spaces.

## 6. Exit rules and notifies
**Verified**:
- `IsAnimationAlmostComplete` = Blend Stack current asset is NOT looping AND `GetCurrentAssetTimeRemaining` <= **0.75 s**
  [stock_functions.txt]. The 0.75 counts from where the clip actually started (pose-matched start time).
- `NotifyTransition_ToLoop` / `NotifyTransition_Re-Transition` are set through the ABP interface `BPI_SandboxCharacter_ABP`
  (`Set_NotifyTransition_ToLoop/ReTransition`) by the notify state **`BP_NotifyState_EarlyTransition`**
  (/Game/Blueprints/AnimNotifies, enums `E_EarlyTransition_Condition` and `E_EarlyTransition_Destination`, compares Gait). It is on
  23 GASP clips: box turns, pivots, spins, slide-outs (grep of Content/Characters/UEFN_Mannequin/Animations). Starts and stops do
  not carry it: they leave by "almost complete".
- `AnimNotify_Transition: ...` events in the ABP (Pivot, Turn In Place, to Idle, to Locomotion, State Changed, ...) are **state
  machine transition notifies** (debug/trace hooks on the transitions), not notifies inside clips; they only jump to the ubergraph.
- Notify states on GASP clips (counts over the GASP animation folder): `PoseSearchBlockTransition` 666 clips,
  `PoseSearchExcludeFromDatabase` 769, `PoseSearchBranchIn` 387, `PoseSearchOverrideContinuingPoseCostBias` 63,
  `PoseSearchModifyCost` 22, Foley `BP_AnimNotify_FoleyEvent_*` 1284. Example: M_Relaxed_Run_Start_F_Lfoot = BlockTransition + Foley;
  Run_Stop_F_Lfoot = BlockTransition + ExcludeFromDatabase + Foley; Run_Pivot_B_F_Lfoot = BlockTransition + EarlyTransition + Foley.

**Assistant:** Pose Search notify states never drive the state machine; they only decide which frames the pose match may return
(BlockTransition = playable but not an entry point; ExcludeFromDatabase = not indexed at all; ModifyCost = bias). Pitfalls:
(a) if the pose match starts the clip *after* its EarlyTransition window, ToLoop never fires and the state waits for "almost complete";
(b) a clip shorter than ~0.75 s (after its start time) is "almost complete" on the first update: exit at once or re-entry loops;
fix with a settle tail covered by BlockTransition, a per-state threshold, or a minimum state time; (c) set notify Trigger Weight
Threshold so a fading-out clip cannot fire it.

## 7. Root motion, warping, play rate
**Verified** (curves on stock clips, ABP functions): GASP starts/stops/pivots carry root motion plus `movedata_speed`,
`enable_warping` (starts, pivots), `enable_turninplacesteering` + `steeringtargettime` (stops). `Get_DynamicPlayRate` reads
`MoveData_Speed` and `Enable_Warping` from the current Blend Stack clip (play rate = capsule speed / clip speed, clamped by
Min/MaxDynamicPlayRate). OffsetRootBone: translation Interpolate (half-life from Get_OffsetRootTranslationHalfLife, radius cvar),
rotation Accumulate.

**Assistant:** the capsule (Mover sim) is the only truth; clip root motion is a *plan*. Layers that absorb the difference: selection
(trajectory cost), dynamic play rate (speed), stride warping (speed, clamped), orientation warping (direction), steering (facing),
OffsetRootBone (everything else, within its radius; Release when idle/in air/montage). A growing error is only hidden until the
radius, then the mesh is dragged = foot sliding. Concrete failures: stop clip 125 cm vs capsule 360 cm -> high trajectory cost, play
rate pegged at max (clip ends even earlier), stride pegged, mesh lags then is dragged, "almost complete" fires while the capsule still
brakes. Start clip with 0 cm root -> MoveData_Speed 0 (play rate undefined, pegged), no warping direction, no steering (mesh does not
turn), OffsetRootBone saturates in 0.1-0.2 s. **Rule: the clip root reproduces the capsule (same distance, speed profile, yaw, time);
style goes on the body, not on the root.** Either record the Mover capsule and animate over it, or tune Mover (accel/decel/turn
rate) so the sim matches snappy clips (stop distance = v^2 / 2a; 600 cm/s over 125 cm needs ~1440 cm/s^2). Never a zero-speed root
while the capsule moves (use a small creep, or hold the capsule in gameplay during the wind-up). Regenerate movedata_speed
(MotionExtractorModifier) and rebuild databases after any root edit.

## 8. Feet and curves
**Verified** (our docs/notes/gasp-curves.md, owner-confirmed rule): a curve a clip does not have reads 0, and curves blend like
poses, so a missing `Enable_*` turns the feature off and a missing `Disable_*` leaves it on. The foot placement runs in the
`Procedural_Feet` linked layer (CR_Biped_FootPlacement / Foot Placement node, `a.AnimNode.FootPlacement.*` cvars).
`AllowFootPinning` is an ABP **function** (reads MovementMode), not a curve.
Stock curves seen on locomotion clips: `contact_l`, `contact_r`, `phase`, `movedata_speed`, `enable_warping`,
`enable_turninplacesteering`, `steeringtargettime`, plus disable*footik / disablepelvisadjustment / distance_from_ledge on some.

**Assistant:** copy the full curve set of the equivalent GASP clip, same names; generate contact curves from foot speed
(MotionExtractorModifier, threshold, hand-fix smears); keep curve values stable at the first frames, at the exit frame (length -
0.75 s) and around notify windows, or the foot lock flips mid-blend (pop). Exact semantics of each foot curve: read the Get Curve
Value nodes in CR_Biped_FootPlacement and the Procedural_Feet layer (not documented).

## 9. Our setup vs stock (diff)
Ours = `/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion` + `CHT_GSTN_LocomotionAnimations` (copy of the _PoseMatch root, context
class switched to ABP_GSTN_Locomotion_C) + `CHT_GSTN_OwnStates`. [evidence/our_vs_stock_state_machine.txt, ours_chooser rows]
| Area | Stock | Ours | Effect |
|---|---|---|---|
| AnimGraph | chain of section 1 | same node set; both ABPs also carry our GSTN_* nodes (GroundContactFix, BlendOutFootLock, StanceOffset, CoveredLanding, DirectionalHitAdditive) and RunStart/Skid/Wall linked layers | the "stock" ABP in this project is not pristine: a clean copy must come from the sample |
| States | 10 states + aliases | + Run Start, Run Stop, Run Stop Blend, Skid, Skid Exit Run, Dash, Crouch Enter/Idle/Exit (+ aliases `-> Run Start`, `-> Run Stop`, `-> Skid`, `-> Dash`, `-> Fast Crouch`) | parallel path beside GASP's Locomotion/Idle states |
| Entry | SetBlendStackAnimFromChooser, nested tables with PoseSearchColumn | our states call C++ `UGSTN_LocomotionStateLibrary::SetBlendStackFromStateName` -> root rows -> `CHT_GSTN_OwnStates` (StateName + OUT only, **no PoseSearchColumn**, fixed StartTime: 0, Skid Exit Run 2.1833) | entry ignores the foot phase / pose history |
| Exit | almost-complete, NoValidAnim, EarlyTransition flags, Re-Enter aliases | timed rules on C++ getters (Skid: state time >= 0.42 and speed < 200; Run Start: time >= 0.767 or hand-off; Dash >= 0.28; Run Stop flags) | timing is hand-tuned per clip |
| Blend | 0.2-0.75 s, FastFeet_FastRoot on landings | 0.03-0.15 s (Skid 0.08, Dash/Crouch 0.03, Run Start 0.05, Run Stop 0.10, Run Stop Blend 0.35) | very short Blend Stack blends, pops absorbed by inertialization |
| Clips | root motion = capsule, full curve/notify set | run start 0 cm root, run stop 125 cm vs ~360 cm capsule, skid 45 cm; curves partly authored (gasp-curves.md); no Pose Search notify states | sections 7 and 8 failure modes apply |
| Nested tables | | unchanged (same row counts in every shared leaf table) | stock behaviour is intact for anything not routed to our states |

## 10. Rules for adding custom clips and states (recommended)
1. **Variant of an existing locomotion event** (start, stop, pivot/skid, landing, turn in place): add the clip as a **row in the
   matching leaf table** (Run F Loco Transitions for starts/pivots with IsPivoting, Stand Stops for stops, Mode Transitions for
   landings), copying the OUT cells (BlendTime, curve, profile, Tags) from a neighbour row and setting filter ranges that cover our
   speeds. It then gets the pose-matched start and GASP's exit rules. To force our clip over the mocap one, disable or narrow the
   mocap rows, or bias cost (ModifyCost), do not add a state. If a clip's authored timing must always play from frame 0, put it
   in a leaf table without PoseSearchColumn (like Turn In Place) or limit its SamplingRange; it still uses the stock exit rules.
2. **New gameplay meaning** (Dash, attack-driven moves): sim side as a Mover layered move; anim side a new state + alias + a root
   row keyed by the state name (no pose search, StartTime 0, like Turn In Place) or a montage in DefaultSlot.
3. **Every clip**: root motion matching the Mover capsule for that situation (section 7); the curve set of the equivalent GASP clip
   (section 8); Pose Search notify states: BlockTransition on tails and frames you must not enter mid-way, SamplingRange or
   ExcludeFromDatabase for anticipation that must play from the start; EarlyTransition notify where the clip may hand off early;
   length after the likely start > 0.75 s + blend, or a settle tail; Foley notifies.
4. **Copying ABP + chooser**: retarget the chooser context class (root and every nested table) to the copy's generated class,
   keep the Blend Stack node tag `StateMachineBlendStack` and the PoseHistory node reference used by functions, check the
   PoseSearchColumn `RootChooser` points to the copy, save and let internal databases rebuild.
5. **Verify** with Rewind Debugger (Chooser Evaluation track: which row passed; Pose Search track: candidates, cost per channel) and
   `a.AnimNode.OrientationWarping.Debug`, `a.AnimNode.StrideWarping.Debug`, OffsetRootBone debug circle (touching the edge = clip
   root does not match the capsule), NoValidAnim false in every state.

## 11. Open questions (not verified here)
- The PoseSearchColumn output binding (where the matched StartTime is written) is not exported by our chooser toolset dump: check the
  column header Details in the editor.
- Schemas assigned to the six internal databases and their channels (dump did not reach sub-object databases).
- Exact semantics of the foot curves in CR_Biped_FootPlacement and the stride/steering speed thresholds.
- Assistant question 7 (pitfalls of copying ABP + chooser) timed out; section 10.4 lists what we already know from earlier threads
  (abp-state-routing, unreal-mcp-59 memories: context class per table, "ContextData entry 0 expects..." log when it is wrong).
- Pose search vs authored timing for cartoony clips: the assistant suggests deterministic rows for starts/turns and pose search only
  for stops and loco transitions; this conflicts with nothing yet but is a design choice for the owner (keep pose-matched entry vs
  authored frame 0), to be decided per move.
