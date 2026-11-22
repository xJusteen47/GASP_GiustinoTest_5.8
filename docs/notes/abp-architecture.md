# ABP_GSTN_Locomotion: how clips are picked (draft, run-start-handoff-1, 2026-11-19)

Status: DRAFT from the asset dumps of earlier threads (Saved/Routing/graphs.txt, dump_compact.txt, nested.txt, route_states.txt),
not yet checked node by node. The next thread completes it (sections marked TODO) before any state is re-routed.

## AnimGraph, output backwards (graphs.txt)
Root <- GroundContactFix (ours) <- PoseSearchHistoryCollector <- Inertialization <- ... <- Procedural_Feet layer (CR_Biped_FootPlacement)
<- BlendOutFootLock (ours) ... <- OffsetRootBone (Translation Interpolate, halflife 0.2, max error 30 cm; Rotation Accumulate)
<- DeadBlending <- Slot DefaultSlot (montages) <- AimOffset / AdditiveLeans / Procedural_PreLayering layers <- **BlendListByInt**:
- index 0: **MotionMatching** node (BlendTime 0.5): the MM setup;
- index 1: TwoWayBlend (alpha 1) of the **"State Controller" state machine** (through Inertialization) and **BlendStack_3** (blend time 0,
  QuadraticInOut, with a blend profile). This is the setup GASP Mover runs by default (`DDCvar.LocomotionSetupMover` = 1).

So, as the owner says, the Mover setup does NOT pick clips by motion matching. The state machine outputs no pose: each state's
On State Entry writes the Blend Stack's next clip from the chooser `CHT_GSTN_LocomotionAnimations` (copy of
`CHT_MoverCharacterAnimations_PoseMatch`), keyed by the state name, with nested tables (Idle Transitions / Loops / Breaks,
Locomotion Transitions / Loops, Slide Loops, ...). The chooser takes a `PoseHistoryReference` parameter: the "PoseMatch" tables pick
the clip AND its start time by pose search against the pose history (this is how GASP's starts, stops and pivots enter at a matching
foot phase). TODO: confirm per nested table which rows have a pose-match column and which output a fixed start time and blend time.

## Our states (route_states.py, abp-state-routing memory)
Run Start, Run Stop (L/R), Run Stop Blend, Skid, Skid Exit Run, Dash, Crouch Enter/Idle/Exit: On State Entry =
`UGSTN_LocomotionStateLibrary::SetBlendStackFromStateName` -> a row of `CHT_GSTN_OwnStates` with a **fixed** clip, start time and
blend time. No pose match, so the entry frame never depends on the current foot phase. Exits are timed rules on our C++ getters
(clip time, sim flags), e.g. Run Start: run start time < 0, or hand-off flag (v3, commit 3a624e42), or clip end - 0.1 s.

## Differences from the GASP pattern (each one breaks a hand-off)
1. Entry: fixed start time instead of pose match (GASP's starts/stops/pivots pick the frame by pose).
2. Root motion: ours is missing or wrong (clip check: run start 0 cm, run stop 125 cm vs ~360 cm of the capsule, skid 45 cm), while
   GASP's clips carry the real displacement that its warping, OffsetRootBone and the trajectory read.
3. Exit: our timed rules vs GASP's own transition rules (TODO: read them: notify "Transition: Animation Finished", time remaining,
   speed / MovementState).
4. Sim velocity: our run stages (stamp, stage acceleration, stop, skid) shape the speed curve; GASP's clips assume its own.

## Plan (proposed to the owner 2026-11-19)
Align our states to GASP's own pattern for starts, stops and pivots: chooser rows with a pose-match column (start frame by pose
history), clips with root motion consistent with the sim speed profile (DA_Locomotion_Default), exit rules modelled on GASP's.
Keep a forced entry only where the design needs it (the 0.3 s stomp). TODO next thread: confirm sections above with the
GSTNAnimToolsets (GetStateMachine, chooser dump) and ask the Unreal Assistant to explain the stock Mover flow.
