# PIE perf probe 2 (2026-11-19, desktop _ue59, branch claude/locomotion-finish-4 = pie-mp-perf merged and built)

Probe: Tools/GASNPPTest/pie_perf_probe.py (Run-Timed -Preset Capture), listen server, 3 players, L_Test_Hub, 30 s after 12 s warm-up,
no input. Numbers: Saved/PiePerf/summary.txt (python Saved/PiePerf/csvstat.py <csvprofile> <name>), warnings counted from
Saved/PiePerf/<name>_run.log.ue.log (`Select-String "Warning:"`).

| run | Run Under One Process | FrameTime avg | GameThread avg / p95 | GPU avg | NP warnings in ~40 s |
|---|---|---|---|---|---|
| after_one_b | on (3 worlds in one editor) | 333.5 ms | 247.6 / 271.5 ms | 57.5 ms | 43 "Lag Compensation Rewind Exceeded" |
| after_multi | off (host process only) | 334.2 ms | 190.7 / 284.8 ms | 27.6 ms | 18 |

- The editor no longer hangs at startup (4 runs, all ESITO OK): the earlier hangs were not reproduced.
- FrameTime is a flat 333 ms (3 fps) in every run: the editor runs on virtual desktop 2, never in front, and is capped in the
  background. `set /Script/UnrealEd.EditorPerformanceSettings bThrottleCPUWhenNotForeground 0` and t.IdleWhenNotForeground 0 did
  not lift it (2 attempts). The slate tick dt is clamped at 125 ms, so the probe's own "fps 8.0" line is wrong (90 frames in 30 s).
- So the absolute fps cannot be measured off-screen; the game thread cost can: ~190-250 ms of game thread per frame with 3 players.
  One process costs ~30% more game thread on the host than the host of a multi-process run, and twice the GPU (3 viewports).
- The warning flood is not the cause: with the throttled log (pie-mp-perf) the NP warnings are ~1 line/s, mostly the rewind one.
- No "before" (unpatched) run: the patch only rate-limits log lines, and the counts above show the log is not the cost.
Next: a foreground run (owner present, editor in front) or an Insights look at Saved/PiePerf/after_one_b/pie_one.utrace to split
the ~240 ms game thread (expected: 9 motion-matching ABPs + NP/Mover resims).
