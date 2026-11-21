# turnarounds-3 (desktop, 2026-11-21): variant 27 = pivot step then w2 rev; 180 L built, review r1 FAIL, game blocker found
Branch claude/runstart-generic-desktop (from 84cc0057). Lock _ue59 21:30-22:20Z, editor only via timed scripts (never opened by hand).
DA_Locomotion_Default (owner's uncommitted change) untouched. Re-check of the previous handover (rule 11): v26 clip fails the new gates.
## Built
- runstart_pivot_compose.py (+ _all, _gates, _dump, _sheet; runstart_clip_joints_ue.py): pivot frames from the stock idle entry,
  turn-side foot steps out open and lands turned, other foot pivots on its ball, head/chest lead, torso upright (w_up cubic), arms
  tucked through H+2, then the w2 rev (C0 1.25 = step-foot stance, pivot foot lands rev f3). Root yaw = pelvis yaw (FRAC).
  Recipe: python runstart_pivot_all.py 180L ; UE: RST_SUFFIX=p RST_K=27 RST_KEYS=180L RST_FALLBACK=f RST_LO1=26.5 RST_LO2=36.5
  Save-Assets runstart_turn_ue.py (env in the SAME command: a separate call loses it and rebuilds the f clips). Divergence 21g.
- Gates (coordinator): ankle gap, toe, bone length, elbow/knee/hand zigzag; on the composer clip and the UE clip. v27 OK,
  v26 FAIL (gap 4.2 cm f12, hand 10.7) -> Saved/AnimPipeline/runstart/gates_*.json.
- Captures 960 px (PIE_CAPTURE_SIZE / -Size), now log hips/chest facing and root bone yaw per frame. ASSERT window 88-132 OK.
## Review r1 (cloud, FAIL) and what r2 found
- Item 1 gioco: round 1's fast yaw (8 f) was lost in the 0.2 s MM blend-in (Assistant, docs/notes/research/unreal-assistant-turn-yaw.md).
- r2b (FRAC 0,.05,.15,.33,.55,.75,.88,...): the body now follows the clip at full weight (hips 130 deg in v85-v93), then STALLS at
  ~-130..-141 for v93-v101 while the clip asks -158..-178: once the body reaches the capsule's facing it never passes it, and the
  capsule ends its turn slowly (-122 at v93, -155 at v99, -163 at v104). Table: docs/notes/reviews/turn3-v27/180a_r2/facing_v82-111.csv.
  So the last ~40 deg follow the capsule: side-on, leaning, R leg folded behind v94-v100 (grid27_az180.jpg). Same in r1.
- Pose items 2 and 4 (lean before H, open arms) are fixed on the clip (sheet, grids v84-v92); item 3 depends on the stall.
## Open (decision needed, see the coordinator message)
- Game: what keeps the body from leading the capsule (Steering target = capsule facing, or Offset Root Bone Max Rotation Error)
  and the capsule's slow end (Mover turning). Options: A steering off / Animated Target Time for turn starts (GASP divergence),
  B faster Mover turn end (feel, owner), C clip on the capsule pace (rev only after the capsule ends, ~v104: slow).
- 180 R / 090 L / 090 R not built; C++ debug drop-down does not list 27 yet (cvar works).
COSTO: ~13 $ | ~35 min desktop | 0 token review
