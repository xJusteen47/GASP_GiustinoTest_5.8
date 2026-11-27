# Handover: calmer greybox run sway (thread "Rendere run start e corsa capsula come Mario", 2026-11-24)

Branch `claude/project-thread-u1p2ha` (from main 5c9e05e5).

Owner asks: (1) the run start does not feel like Mario's; (2) the capsule run sways too much, violently at the third run stage.

(1) Not the data of the kit: KIT_Test_Overlord / Playtest / OverlordDropkick all point at `DA_Locomotion_Default` and have the same
stats (WalkSpeed 600, JumpVelocity 1350, read from the uassets). The owner's "much better with the Overlord" was a kit vs no-kit
comparison (no collar = no kit = no locomotion profile = stock GASP start). Owner 20:32 Rome: keep the kit's start as reference;
retune the ramp only if it still feels off after this round. Measured start on run-sway-1 (frames.jsonl): stage 1 plateau 451 cm/s,
t10/t50/t90 0.15 / 0.32 / 0.47 s; stage 2 at 4 s (586), stage 3 at 5 s (~817).

(2) Cause, in `UGSTN_GreyboxAnimInstance` (the capsule is rigid in locomotion, so the sway is all procedural):
- cadence = speed / StrideLength without a cap: 5 Hz at 850 cm/s with full rock and twist;
- the lean read the raw per-anim-frame acceleration (sim and render rates differ; v^2/r at sprint) into an under-damped spring.
Fix (presentation only, all in ShapeSettings, each switchable back): MaxSwayCadenceHz 3.2 (0 = off), SprintSwaySpeed 800 /
SprintSwayScale 0.55 (1 = off), SprintForwardLeanDeg 6, AccelerationFilterHz 6 (0 = raw); base amplitudes lowered (bob 0.07->0.05,
rock 7->4.5, nod 4->3, twist 8->5). ABP_GSTN_GreyboxCapsule has no ShapeSettings overrides, so the C++ defaults apply.

Evidence: desktop build OK (376 s), tests GSTNCombat.NP.Greybox + GSTNAnimation.Core.Greybox 5/5; videos
`docs/notes/reviews/greybox-capsula/run-sway-1/` (gb_loco, gb_side, new gb_sprint: 5 s straight to stage 3 then a 90 deg sprint turn).
Stage 3 frames v466-512: steady forward lean, small rocks of a few degrees, no violent shake. Owner verdict in game pending.

Incident: this thread overwrote the capsule-ground thread's `_ue59/IN-USO.txt` at 20:37Z (owner had said UE was free); _ue59 was
given back on claude/project-thread-e8g12i @799a6740, that thread must rebuild. Memory: ue59-lock-check.

ESITO: PARTIAL code built, tested and captured on the desktop; owner verdict on the sway pending
SUBAGENTI: none (one Remote Control session on the desktop, same model)
COSTO: not measured | ~20 min desktop (build 376 s + captures) | 0 review tokens (capsule placeholder, no level-2 review)

## Round 2: run start driven by the sim + stage steps (2026-11-24/25) — RESUME POINT

Owner 2026-11-24 23:05 Rome: implement the consulting idea on the capsule (cadence from the stamp and the effort = forward acceleration,
effort lean), save the idea (docs/notes/run-start-drive-capsula.md), "almost a step" between run stages, capsule pose for the L3 dash.
Code: aeca46e8 (drive, effort/tell lean, dash pose, tests), a8651e58 + 7312c4a7 (setup_run_stage_steps.py also covers the per-kit
profiles BirdKnight/FlameFury/Voyager; STEPS_ANIM_SET=0 skips the mannequin anim set), main merged twice (b98d01e7: bCalm hit
reactions from #46/#49 also mute the drive, effort and dash pose). NOT compiled when the network went down (owner hotspot off ~00:30Z).

Next, in order:
1. Laptop, worktree GASP_Giustino2_cp1verify only (main copy holds another thread's VFX work): build, tests NP.Greybox + Core.Greybox,
   read the capsule ABP CDO ShapeSettings (overrides?), STEPS_MODE=dump, captures (cam follow mesh) gb_sprint / gb_loco / gb_dash (add
   gb_dash to pie_capture_move.py if missing) into docs/notes/reviews/greybox-capsula/run-drive-1/. Check docs/notes/greybox-capsula.md
   for a "Laptop run drive-1: stopped at step N" line first.
2. Desktop (`_ue59`, IN-USO.txt): the Voyager thread was asked to run setup_run_stage_steps.py (dump, then STEPS_ANIM_SET=0) before its
   setup_character_kits.py; check its output, then capture gb_sprint there to see the steps (the laptop has no owner Default data).
3. Owner verdict on video; then close PR #45.
