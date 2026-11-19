# Locomotion finish 4 (desktop _ue59, Opus, 2026-11-19 17:02-18:10 UTC) - dash PASS, perf numbers, run stop + skid causes found
Branch claude/locomotion-finish-4 (from cdacfc0d, claude/pie-mp-perf merged). Builds: 3, all ESITO OK (Saved/Logs/ci/build_loco4_*.log).
1. L3 dash r3: PASS (docs/notes/reviews/dash-r3/review_fable.md, Fable). Cause of "boost never fires": the command table keeps only
   commands bound to an ability; nothing binds Command.Movement.RunBoost (cap4 frames.jsonl combat.run.boostCmd = -1). Fix: query
   commands (FGSTN_KitCompileInput::QueryCommands -> slot from the kit's command set, no row; NP table adds them with 0 bindings;
   locomotion loadout extension adds the profile's BoostCommand). Key stays in input data (coordinator rule). Test
   GSTNCombat.Core.Table.QueryCommands; 65 tests OK (Saved/Logs/ci/test_loco4_1.log.ue.log, "Result={Success}" count).
   Non-gating from the review: dust too big (hides the lunge), 2 run-start frames before the load v43-45, flat late 2nd step.
   Not captured: L3 while already running. Capture: boost 3 frames after the stick (stick reaches the sim one frame after a press).
2. 3-PIE perf: no hang (4 runs). Background editor capped at 333 ms/frame, not liftable (2 attempts) -> real fps needs the editor in
   front. Game thread ~248 ms one process vs ~191 host multi; GPU 57 vs 28 ms; NP warnings ~1/s. docs/notes/pie-perf-probe-2.md.
3. Run stop drift: clip ends on M_Relaxed_Stand_Idle_Break_v08 stance, the game then blends ~45 f into M_Relaxed_Stand_Idle_Loop
   (feet 3.5/4.6 cm apart). docs/notes/reviews/runstop-v6/rest-idle-cause.md, options A/B/C (C recommended: hold feet through the
   Blend Stack hand-off). Not applied (pose round 4 -> owner, rule 10).
4. Skid: actor turns opposite ways in the two runs (180 reverse, side picked by float noise) in 45-90 deg/frame steps; clip turns
   one fixed way -> two poses; foot pop v137 on the yaw steps. docs/notes/reviews/skid-a5b/skid-yaw-cause.md. Fix not started.
5-6. Run start stomp/foot angle, air crouch landing rise: not started.
Tools: capture rows now log combat.run (stage, boost), combat.anim (playing clips + weight + time), yaw [actor, mesh].
Side task: Gemini camera research + review pushed on claude/project-thread-mu9mh4 (f6cc9e4a, 9609fed1).
Applied choices: boost via query command (not a button fallback); perf "before" run skipped (log counts show the log is not the cost).
Uncommitted, not mine: Plugins/GSTNPiggyback/Content/Test/PM_{BoostJump,ThrowRider}.uasset (were dirty at start).
Cost: ~1 h 10 desktop, 3 builds, 4 headless, 9 captures, 1 Fable review (~124k) + 1 Haiku agy runner (~85k).
Proof: each number cites its log/frames file above; summaries by Saved/RunStop7/rest.py, Saved/Skid/cap6/sk.py, Saved/PiePerf/csvstat.py.
