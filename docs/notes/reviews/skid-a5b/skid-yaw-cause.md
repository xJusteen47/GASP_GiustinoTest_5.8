# Skid: why two poses for the same input, and the right foot pop (locomotion-finish-4, 2026-11-19)

Capture Saved/Skid/cap6 (KIT_Test_Playtest, mv_skid_release + mv_skid_hold, stick (0,1,0) then (0,-1,0) at v115). Rows now log
yaw = [actor, mesh] (world) and combat.anim. Summary: python Saved/Skid/cap6/sk.py <frames.jsonl>.

- Same clip both times: SS_180_Turn_Root180 from v123-124 (t 0.10) to ~v145 (t 0.47), then BS_Relaxed_Lean_Head/Run.
- The ACTOR turns the opposite way in the two runs (gioco): release 0 -> 90 (v136) -> 135 -> 157.5 -> 180 (v139), hold
  0 -> -45 (v132) -> -135 (v136) -> -157.5 -> 180 (v138). An exact 180 reverse has no preferred side, so the rotation picks one by
  float noise. The clip turns one fixed way, so once body and clip agree and once they fight: the two poses.
- The actor yaw moves in 45-90 deg steps per frame (release v135 -> v136: 0 -> 90). The mesh follows smoothed (-0.7, 22.7, 46.1,
  69.6, 91.5 on v136-140): the right foot pop v137 (x -14 -> -8 -> -17) sits on those steps.
- Hand-back: release goes to M_Relaxed_Run_Stop_F_Lfoot (v160+), hold to M_Neutral_Run_Loop_F (v152+); the 30 cm slide is in
  that hand-off (not re-measured here).
Next (one fix, gioco): during the skid, latch the turn side to the clip's side (and spread the 180 over the clip's turn frames,
no 45-90 deg steps), e.g. in FGSTN_LayeredMove_RunSkid / the orientation the skid hands to Mover. Then recapture both runs.
