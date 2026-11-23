# Wall slide animation states

Source: `Source/GASP_Giustino2/{Public,Private}/WallMovement/Animation/`, tests `Private/WallMovement/Tests/GSTN_WallAnimTests.cpp` (`GSTN.Wall.Anim.*`).
General pattern (layers decoupled from the main graph, skin variants): [anim-layers.md](anim-layers.md).

## What the layer gets

`UGSTN_WallAnimInstance::WallAnim` (`FGSTN_WallAnimState`), updated every frame on the game thread from the Mover presentation state:

| Field | Meaning |
|---|---|
| `Phase` | `None`, `Enter` (just grabbed, `EnterDuration`), `Active`, `Exit` (just left, `ExitDuration`) |
| `SlideState` | `Idle` (still or straight down), `SlideLeft`, `SlideRight`, `Skid` (fast, wins over the sides) |
| `PhaseTime`, `SlideStateTime` | time in the current phase / loop state |
| `bIsActive` | phase is not `None`: the layer wants the pose |
| `bIsClimbing` | the mode is the climb v2 |
| `LateralSpeed`, `VerticalSpeed`, `PlaneSpeed` | velocity on the wall: right positive, up positive (gravity-up, any gravity direction) |
| `LateralAlpha`, `VerticalAlpha` | the same scaled to [-1, 1] for blend spaces |
| `ExitModeName` | mode that took over on leaving (floor, air, wall jump variants of the exit) |

"On a wall" = the active Mover mode is a `Settings.WallModeClass` (default `UGSTN_WallTraverseModeBase`: slide and climb v2). A slide <-> climb hand-over in place
is not an exit. Legacy modes are ignored unless the class is set to `UGSTN_ControlledWallMovementMode`.

## Rules

```
Skid      if PlaneSpeed >= SkidEnterSpeed (750), kept while >= SkidExitSpeed (650); never while climbing (bSkidWhileClimbing)
Left/Right if |Lateral| >= LateralEnterSpeed (120), kept while >= LateralExitSpeed (80) and the sign is the same
Idle      otherwise
```

Defaults sit around the slide tuning: `TerminalDownSpeed` 600 (a steady slide down is Idle, not Skid), `HorizontalControlMaxSpeed` 350, `MaxEntrySpeed` 1500.
The brake input is not in the sync state, so the skid is a speed rule; a fast entry brakes down through it naturally.

## Layer graph (placeholder clips until the final ones)

Built and kept up to date by `Tools/GAS/setup_wall_anim_layer.py` (idempotent, headless; it backs up the two shared Blueprints to `Saved/Backups/GAS`
before changing them), through the editor module `GSTNAnimationEditor` (`UGSTN_AnimGraphAuthoringLibrary`). Assets in `Content/Blueprints/Animation/`.
`--check` prints every graph and the compiler messages without changing anything.

```
ALI_GSTN_Wall (anim layer interface): layer WallLayer
ABP_GSTN_WallLayer (parent GSTN_WallAnimInstance, implements ALI_GSTN_Wall), layer WallLayer:
  state machine WallPhases, entry Enter
    Enter   --(Phase == Active)--> Loop      0.15 s
    Loop    = inner state machine SlideStates on SlideState, entry Idle: Idle / SlideLeft / SlideRight / Skid,
              a transition between every pair (12), rule SlideState == <target>, 0.2 s
    Loop    --(Phase == Exit)----> Exit      0.15 s
    Enter   --(Phase == Exit)----> Exit      0.1 s
    Exit    --(Phase == Enter)---> Enter     0.1 s  (re-grab during the exit)
    Loop    --(Phase == Enter)---> Enter     0.1 s  (only reachable with ExitDuration 0: Active -> None -> Enter shows no Exit)
  every rule is Property Access WallAnim.<field> -> enum == -> result (thread safe); standard crossfades inside the layer
```

| State | Placeholder (GASP sample, `/Game/Characters/UEFN_Mannequin/Animations/`) | Loop |
|---|---|---|
| Enter | `Jump/M_Neutral_Jump_F_Land_Stand_Light_Lfoot` | no |
| Exit | `Jump/M_Neutral_Jump_F_Start_Stand_Lfoot` | no |
| Idle | `Jump/M_Neutral_Jump_Loop_Fall` | yes |
| SlideLeft | `Walk/M_Neutral_Walk_Loop_LL` | yes |
| SlideRight | `Walk/M_Neutral_Walk_Loop_RR` | yes |
| Skid | `Slide/M_Neutral_Slide_FootOut_Loop` | yes |

Chosen to make each state recognisable in PIE, not to look right. Replace them in the layer Blueprint (or in `CLIPS` of the script and run it again: an existing
sequence player is retargeted to the new clip, nothing else changes); a blend space on `LateralAlpha` / `VerticalAlpha` in a state is a hand edit.

Main ABP (`SandboxCharacter_Mover_ABP`): reparented to `GSTN_CharacterAnimInstance` (plugin GSTNAnimation) and implements `ALI_GSTN_Wall` (its own `WallLayer` graph
is empty: it is only the default a linked class replaces, and the blend never shows it, since `bOverrideLayerActive` is false while no wall layer is linked). The seam
sits in front of the **Pose History** node, not the Output Pose:

```
... Procedural_Feet -> Procedural_FullBody --[False]--> Blend Poses by bool (bOverrideLayerActive; True/False blend time = OverrideBlendTime; inertialization)
                    Linked Anim Layer WallLayer --[True]-->        |
                                                                   v
                                                             Inertialization -> [Control Rig of the limbs on walls goes here] -> Pose History -> Output Pose
```

- In front of the Pose History so motion matching records the pose that was shown (otherwise the history is GASP's while the wall pose is on screen, and the first
  match after the wall is made against the wrong past); after the GASP procedural layers (foot placement, full body), which must not act on the wall pose.
- The Inertialization node serves the blend's own request: the GASP Dead Blending node is upstream of the seam and would never see it.
- **Control Rig for the limbs on walls (later thread)**: one node between that Inertialization node and the Pose History, gated by `bOverrideLayerActive` (or a
  weight from the wall layer), so it sees the final wall pose and the history records its result. The layer graph and the seam do not change shape for it.

The layer class comes from data: an anim set `DA_AnimSet_Wall` (`SetTag` `Anim.Set.Wall`, `LayerClass` `ABP_GSTN_WallLayer`) in the character's default collection
`DA_AnimSetCollection_SandboxCharacter`, on the `UGSTN_AnimSetComponent` "AnimSets" of `SandboxCharacter_Mover`; heavy or female variants are child layers in sets of
the same slot, picked by the skin's collection (anim-layers.md part B).

Variant test (review item 2e, `setup_wall_anim_layer.py --variant`): `ABP_GSTN_WallLayer_Heavy` (child of the wall layer, asset override Idle ->
`Jump/M_Relaxed_Jump_Loop_Fall`), `DA_AnimSet_Wall_Heavy`, and a second rule in the collection, `When` = `Anim.Trait.Build.Heavy`, priority 10. The default character
has no trait, so it keeps the base layer; add `Anim.Trait.Build.Heavy` to the component's `DefaultCosmeticTags` to see the switch.

Checked headless (2026-10-26, wall slide A/B harness lanes `a_flat` and `b_convex90`, listen host + 1 client): the layer is linked on every copy of the pawn (host and
client worlds), `WallAnim.Phase` goes None -> Enter -> Active -> Exit -> None with `bOverrideLayerActive` following it, `SlideState` SlideRight while the stick
runs the character sideways at 350 cm/s; no anim Blueprint warning in the log. Not seen there: Skid (the lanes stay under 750 cm/s) and the state machine's current
state, which Python cannot read; check both with the ABP debugger in PIE (review item 3).
