# Reaction re-bake (Fable approved, M1)
- `animpipe/ue_rig.py`: new `_unit_scale(dst)`; `_lock_lengths` now writes scale (1,1,1) on every locked track and calls `_unit_scale` for the rest (root, pelvis, ik_*, VB).
- The 8 game clips `/Game/GASNPPTest/Combat/Reaction/AS_React_*` were fixed in place with the SAME function (`Tools/GASNPPTest/react_rebake_m1.py`)
  instead of re-running the whole Cascadeur/Control Rig build: translation and rotation keys are rewritten with their own values, so the result equals a
  re-bake with the fixed `_lock_lengths` (pelvis frame-0 shift 0.0000 cm, lengths unchanged). DA_ReactionAnims untouched. Old versions are in git history.
- Verification (every track x every frame, 93 tracks): `react_rebake_log.txt` (log Saved/Logs/ci/react_rebake.txt, script react_rebake_m1.py):
  max |scale-1| = 0.00000 on all 8 clips (before: head 1.15, thigh 1.12, foot 0.89).
- Grids: `reactions_3q_front.png` and `reactions_side.png` (az 315 and 270, camera dist 230, fov 55, centre 0,15,80; rows = clips, columns = the same
  6 evenly spaced frames, 4-frame clips show all 4). The standalone capture level has no visible floor (black background), so no floor in the images.
