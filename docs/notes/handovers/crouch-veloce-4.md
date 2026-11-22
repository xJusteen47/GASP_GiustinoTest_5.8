# Crouch veloce 4: ground round 3 (desktop _ue59, Opus, 2026-11-19) - GROUND Fable PASS (narrow), sent to the owner
Branch `claude/locomotion-finish-1` (from claude/ue59-sync-1119 916338ad), commit 36c283a4. Brief: coordinator note "finisci tutto", item 1.

## Done
- `Tools/GAS/crouch/crouch_fk_build.py`: (a) pose report: clip key `"report": [frames]` logs bone positions in character axes
  (fwd, right, up cm) to Saved/Crouch/crouch_fk_build.log, lines "<clip> f<N>:"; (b) `solve_chain` two-bone IK with a pole direction;
  poses take `arm_ik` / `leg_ik` {"l": {"hand"|"foot": [f,r,u], "pole": [f,r,u], "rel_pelvis": bool}}; (c) plan flag
  `feet_from_source`: BUG before v4, source feet were filled after the poses, so a pelvis offset moved the feet too (v3 sank both
  balls 6 cm under the floor: v3 probe ball z -3 vs stand +3).
- `Tools/GAS/crouch/run_crouch_build.ps1 -Plan <json>`: headless build under Run-Timed (~10-30 s). Probe = plan copy with "dry_run".
- `crouch_plan_v4.json`: pelvis +3, back foot -4 fwd / +1 up / 17 deg toes-down, arms by IK (lead LEFT fist [33,-9,90], rear [22,12,78],
  elbows out), head -25, exit stand_over at f7 linear, stand at f12. Probe numbers (log line "AS_GSTN_Crouch_Idle f0"): back knee z 18
  (v3: 5), back ball z 2, heel (foot_r) z 17, hand_l (33,-9,90) vs head (16,-2,98). v3 had the raised fist on the RIGHT (hand_r fwd 20).
- Capture Saved/Crouch/v6cap (cr_ground, KIT_Test_Overlord, cvars gstn.FastCrouch.Mode 1), grids + mp4 in
  docs/notes/reviews/crouch-veloce-4/v6, Fable verdict review_fable_v6.md: PASS narrow, no posa/montage FAIL.

## Open
- Optional polish (posa, not blocking): fists 4 cm lower, lead fist 4-5 cm more forward; knee +4-5 if the owner still sees a kneel.
- Gioco (code queue, unchanged): camera horizon jump with the capsule at press/release; exit stride held then feet snap together.
- Reviewer limit: grids ~100 px per character, silhouette read only.
