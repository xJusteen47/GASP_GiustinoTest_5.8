# BRIEF night-footlock: keep the feet planted while a montage blends back to idle (owner 2026-11-16: option B, blend-out only)
Machine: desktop (`_ue59`, IN-USO lock). Model: Opus high. Budget 8-12 $. Updates _ue59: yes (ABP + C++), last of the session builds it.
Base: `claude/night-knockdown` fb4e8327. Read only: `handovers/night-root.md`, memory `pipeline-root-and-stance`.
## Scope (owner + coordinator)
- ONLY the blend from a montage back to the game pose (get-up hold-blend 20 frames, HitStand 5-8, move FAF exits). The first-frame pop on
  montage start stays as it is. GASP locomotion foot placement must behave exactly as today when no montage is blending out.
- Do not lock/zero the root (night-root: it is compensated in the pelvis; zeroing draws every pose 16 cm off).
## Facts
- The slide is a stance mismatch, ~11-12 cm per foot: clips end in mesh-space feet (19.5,12.6)/(-14.2,-20.0), the game idle settles at
  ~(15.1,1.7)/(-15.5,-8.2). Measured in `reviews/night-root/rows_kd_*.txt`, `Saved/nightroot/move_before/jab1/frames.jsonl` (f43-90).
- ABP order: Slot(DefaultSlot) -> DeadBlending -> OffsetRootBone -> Procedural_Feet (linked layer, class not dumped yet) -> ...
- A planted foot cannot stay planted forever: the idle stance differs, so each foot must end with a short step (lift + place), not a slide.
## Plan
1. Dump the Procedural_Feet layer graph (`GSTN_AnimGraphAuthoringLibrary.describe_graphs` on its class): does it use FootPlacement with
   plant/unplant settings? If yes, preferred route: during blend-out only, raise the plant strength / unplant radius through a variable
   (alpha 1 only while the DefaultSlot weight is falling), so GASP's own planting holds the feet and releases them as a step.
2. Else a small isolated chain right after Procedural_Feet: two Two Bone IK on ik_foot_l/r to world targets captured at blend-out start,
   alpha from a C++ flag in `UGSTN_CharacterAnimInstance` (1 during blend-out, then per foot: release over ~6 frames with a 3-5 cm lift
   curve when the idle target is > 4 cm away). Flag 0 = node passes through: locomotion untouched.
3. Diff the ABP dump before/after (only the new nodes). Build, then in-game captures before/after with the logging already in
   `pie_capture_move.py` (`move_root_rows.py`) and `pie_capture_react_ingame.py` (`react_feet_rows.py`): knockdown_getup, hitstand, jab1,
   jab1_jab2_faf, dair_land (kit `KIT_Test_OverlordDAirV9`), dropkick_hit (`KIT_Test_OverlordDropkick`), walk_into, jump_only, plus
   starts/stops/pivot/turn in place/crouch if a capture exists. PASS = foot slide while the capsule is still < 2 cm on blend-out, and
   locomotion rows identical to before. Look at the frames around blend-out (rule 10) before reporting.
4. If neither route stays isolated, stop and list options (rule 5: max 2 attempts).
