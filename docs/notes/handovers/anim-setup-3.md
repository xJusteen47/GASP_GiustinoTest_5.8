# Anim setup 3 (desktop _ue59, Opus, 2026-11-19 19:20-20:00 UTC) - clip gate calibrated, generic clip fixes, skid guard follow-ups
Branch claude/anim-setup-3 (from claude/anim-setup-2 5a0e2cd9 + merge claude/project-thread-c0lfu3 22e53000). 2 builds OK
(Saved/Logs/ci/build_setup3_{1,2}.log, "Result: Succeeded"), 4 headless exports, 3 PIE captures (1 player). Lock released.
1. Isolation check run on all locomotion clips + 13 GASP references (export: export_clips_ue.py, no args; check: clip_check.py run inside
   Blender 5.2, see clipcheck/README.md). First thresholds failed 8/13 GASP clips -> recalibrated (commit 556a53c7): feet judged on the
   ground frame (in-place rows need game_speed_cms), contact = ball <1 cm up AND <30 cm/s, drift >3.5 cm in 0.25 s, skate = flat foot
   travelling >=0.2 s, handoff FAIL 35 deg/45 cm (WARN 10/5), pelvis jolt 25000 cm/s2 at 30 fps, no MoveData_Speed required, root
   motion flag only fails when off on a travelling clip. All 13 GASP rows PASS (docs/notes/reviews/clip-check/summary.json).
2. Generic fixes (Tools/AnimPipeline/clipcheck/clip_filters.py + fix_clips_ue.py, dry run by default; rows with "fix" in clips.json):
   deflick (flicker on a still bone, one-frame spike, one-frame release eased forward), ease_snaps, ik_foot bake (animpipe code).
   Applied (owner ran fix_clips_write.bat: agent saves of versioned assets stay blocked even with the owner's ok in the thread):
   Dash ball_l f23 + ball_r f10, run start 8 finger _03 bones, run stop L/R ik_foot_l f10 (docs/notes/reviews/anim-setup-3/fix_report.json).
   Re-check: no pops, skates or drifts left in these 4 (summary.json). Run stop floor slide = design (slide_ok, runstop-v6 trait 2).
3. Still FAIL (design/pose, not fixed): hand-off gaps vs the neighbour pose: dash end vs run loop foot_l 49 deg/35 cm; run start
   start vs idle calf_r 83/36 cm, end upperarm_r 49/43 cm; run stop start lowerarm 93-99 deg, end vs relaxed idle upperarm_r 70 deg;
   skid start/end 44/40 deg (owner clip). Skid ball r -5.8 cm / heel -4 cm under the floor f15 (owner clip). Crouch exit ball r -1.5 f4.
   The *_r3_src rows are the old FBX files (history), not the game.
4. Skid pop guard (GSTN_AnimNode_BlendOutFootLock): fires only for a Free foot that was still (PopStillSpeed 60 cm/s), jump <=
   PopMaxDistance 20 cm, fade (1-t)^2. Evidence docs/notes/reviews/anim-setup-3/feet_*.txt (scratch feet.py on frames.jsonl):
   cap_skid2 = cap_skid_noguard on every frame except the pin release (release f138 10.2 -> 2.4, hold f137 10.4 -> 2.3 cm/frame).
   Entry freeze gone (left 2.5/7.0/17.3/22.0 vs base 3.0/5.2/12.7/19.3). Try 1 (no still gate) still froze f120-121 then 22-25 cm.
   Exit f147-149 31.7/20.5/32.7 cm/frame is in the BASE too: the skid -> run hand-back, not the guard (refactor item 3).
5. Run-Timed.ps1: negative exit codes crashed the [int] cast (fixed).
Decisions for the owner (consigliata first): (1) hand-off gaps: A generic "hand-off splice" in clip_filters (first/last ~0.1 s
   eased to the neighbour pose, as a variant per rule 13b) / B pose rounds per clip / C accept, judge in video. (2) skid floor: A floor
   clamp with leg IK in the fixer / B leave it to GASP foot placement. (3) saves: A owner adds an allow rule for headless saves in _ue59 /
   B keep the .bat route.
Not done: in-game video + Fable review of the 4 fixed clips (rule 10, next thread first), Unreal Assistant + ue59_editor_checks (no
editor session opened), run start stomp/47 deg foot (gated by run start hand-offs), air crouch landing (clips PASS: can start),
refactor 3-7 (ABP/DA edits = asset saves, see decision 3), camera P0. Cost: ~45 min desktop, no subagents.
