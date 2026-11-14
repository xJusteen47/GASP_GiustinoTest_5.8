# Combined branch for the owner's playtest (desktop _ue59, 2026-11-14)
Branch `claude/project-thread-comb59` = ebigxw (7ee2d5a3: launches/bounce/splat/knockdown, pad menu) + wif3ec (timeline crash fix, End Blend Out,
Hold Pose On End, Hitboxes outliner tab) + 77ctnl (per-move momentum slide). Both merges were clean (no conflicts, no binary picks needed).
**Done**: full editor build UE 5.9 (`-NoHotReloadFromIDE`, first try, no errors); headless `Automation RunTests GSTN`: 1591 passed, 0 failed
(was 1587; +4 new); `setup_momentum_slide_examples.py` run headless: GA_Example_Light and GA_Example_Strong written (2 .uasset in this commit).
**Not done / not verified**: no PIE, no dedicated server + 2 clients run (`gasnpp_test.py`), no look at the editor UI (Hitboxes tab, strong end
settings). The momentum-slide risks listed in momentum-slide.md (frame 0 order, FindRootMotion, ground check) are still open until played.
**Local state**: _ue59 checked out on this branch and built. My stash "ue59 local asset resaves..." holds the owner's earlier local resaves of
GA_Example_Light/Strong and PM_BoostJump/PM_ThrowRider (editor noise, not applied; `git stash list`). The editor-run tests also touched the two
Piggyback PM_* assets; I reverted those.
**Decisions**: stashed rather than discarded the owner's local asset edits; the ESEMPIO assets in this branch come from the script.
Cost: ~60k context, Sonnet 5.5, no subagents, about $2.
