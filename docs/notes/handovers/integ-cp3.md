# INTEG-CP3 handover (desktop, 2026-11-15)
Branch `claude/integ-cp3` (from anim-r2-int d311d243) = + p2xw3s (CP3 double shot) + qxjy3w (dropkick hold-until-landed; one conflict in
`GSTN_GA_BaseCombat.cpp`, both sides kept). `_ue59` built on it, Unreal closed. Not pushed.
**Fixes needed on 5.9 (3, one attempt each)**: `UK2Node_BreakStruct::CanBeBroken` not exported (link error; check dropped for Break);
`setup_double_shot_examples.py`: Python returns only the error text of a bool+out-error function (empty = success), and the GameplayAbilities BP factory
ignores `parent_class` (now plain BlueprintFactory: the old script made abilities with the wrong parent). Same factory line is in `setup_designer_examples.py` (not touched).
**Tests green**: MoveGraph, SimGraph, NP.MoveActions, Projectile.Core; after the 2nd merge 194 tests (HoldUntilLanded Core+NP, MontageSync, Timeline, MoveActions, Editor).
**Assets** (committed): 9 DoubleShot assets (saved 9, no ERROR; graph dump + log in `docs/notes/reviews/cp3-double-shot/`); dropkick kit + timeline rebuilt
(TL hold [15,26), land frame 59, total 91; handover expected [16,27)/60, 1 frame lower: check). AM_OverlordDAirV9 left dirty (never touched), PM_* reverted.
**Play (L_Test_Hub, Special, taps in render frames) from the frame rows, NOT watched by me**: A: single = 1 move (17 f), double (8 f apart) = one 31 f run (A1 then A2),
slow (40 f) = two separate moves. B: single 17 f, double 30 f (FAF 28), slow two singles. Videos `docs/notes/reviews/bp-double-shot/{A,B}_shot_*.mp4`.
The 15 cm shot is tiny: I did NOT confirm in the video that 1 vs 2 projectiles appear or that the second is faster -> the cloud review must look.
**Dropkick** (`docs/notes/reviews/dropkick-land-1/dropkick_whiff.mp4`, rows): press v42, loop montage 17-26 repeats in the air, height falls to 0 at v100,
land clip jumps to montage frame 61 at v104, no get-up (follow-up not set). Jab `jab1_jab2_faf.mp4` (+rows): first rendered montage label 0.00 on jab1 and
dropkick, then 2.0, 2.0, 4.0 (one repeated frame) then ~1.0 steps. Needs frame-by-frame PASS/FAIL review vs base (no base capture taken here).
**Not done**: PIE harness `abilities/forced_rollback/combat_spike` (not run); only the 3 anim checks above.
**Tools**: `pie_capture_move.py` has DoubleShot sequences + `PIE_CAPTURE_BUTTON`; `Tools/GASNPPTest/animr2/cap_one.sh` = one full capture (starts editor, waits 45 s after MCP, captures, quits).
**Cost**: Sonnet, ~1.5 h machine, ~$6 est.
