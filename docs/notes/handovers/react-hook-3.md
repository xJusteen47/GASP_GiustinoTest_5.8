# REACT-HOOK-3 handover: playtest of the GroundBounce/Tech input fix (2026-11-14)
Branch `claude/project-thread-wkds3r`, code at 125897ce (no code change in this thread). _ue59 BUILT on it (UE 5.9, Development editor),
editor closed, no maps saved; dirtied PM_BoostJump / PM_ThrowRider reverted (IC_GASNPP_Test, KIT_Test_Hits were not dirtied).
Tests: `Automation RunTests GSTN` = **1596 success / 0 fail**.
Captures + numbers: `docs/notes/reviews/react-hook-3/` (README.md). Needs the rule-10 cloud review before the owner.
**Result: roll back FIXED.** Stick held behind from KD+4: yaw stays 180 for the whole lying phase and the roll; the get-up plays
AM_React_RollBack, travel 65 cm toward the head (x 472 -> 537), starts KD+28. Seen on the az90 sheet: backward roll over the head v108-v120.
**For the review:** RollBack travels 65 cm vs 80 cm for Roll (clip keys, anim data); after the roll the still-held stick turns the pawn
(normal walk, capture releases it 3 f after the get-up). Hitstand montage restart at v41 and frozen clip frame 2 still open (react-hook-2).
Other sequences match react-hook-2 (forward roll 79.4 cm, roll_late KD+52, bounce +17.8 cm at KD+5/6).
**Tooling note:** the mp4/sheet builder is not versioned (pie_capture_grid.py expects the 7-view move capture); this thread used a scratch
PIL+ffmpeg script: mp4 = az45 | az90 side by side at 60 fps, sheets every 2nd frame, 200 px cells, label `v a state`.
**Cost:** one desktop thread, ~60k context, one build + one test run + one 3 min capture; estimate ~$5.
