# PUSHBOX-PLAN-1 handover: plan for separate pushboxes (cloud, design only, 2026-11-15). DONE
Branch `claude/project-thread-45g6rf` from `claude/integ-dropkick-capsule` a95e17d4. No code, no build, no asset touched.
## Output
- `docs/notes/plans/pushbox-plan.md`: ECB ignores other fighters via pairwise `IgnoreActorWhenMoving` (no channel/project setting change),
  `FGSTN_TimelineTrack_Pushbox` (vertical cylinder, falls back to the Body Capsule track, then state set, then standing), soft capped
  horizontal push computed per fighter from the shared `FGSTN_StepSnapshot`, `UGSTN_BodyShapeSet` per state tag for knockdown/tumble/crouch,
  tests, checkpoints P1 (10-13 $) and P2 (7-9 $), 3 owner decisions (rec 1A 2A 3A).
## Checked in code (read only)
- Mover queries use `InitSweepCollisionParams` (`Plugins/EngineOverrides/Mover/Source/Mover/Private/MoveLibrary/MovementUtils.cpp` 544, 897-964)
  -> `MoveIgnoreActors` is honoured by sweeps, floor and depenetration. To confirm with the NP test in P1.
- Conflict: `Plugins/GSTNLink/Source/GSTNLinkNP/Private/GSTN_LinkPresentationComponent.cpp:278` sets ignore false on the previous partner;
  needs the ref-counted helper or it re-enables fighter blocking after every grab.
- Snapshot already carries Transform, bInHitstop, Reaction (`GSTN_StepSnapshot.h` 212-271): pushbox fields go there.
## Not seen
- dair capsule commit c2a3f3be is not on origin (local desktop only); plan assumes it uses the same Body Capsule track.
## Applied choices (rule 6)
- Fighters only in v1 (minions keep blocking + steering). Defaults Gain 0.25/frame, Cap 3 cm/frame, pushbox radius x0.8 of the capsule.
**Cost**: estimated ~3-4 $ (one Opus thread, ~60k context).
