# pie-mp-perf-1 — PIE listen server, 3 players: 15 fps + warning spam

Branch `claude/pie-mp-perf` (worktree `GASP_Giustino2_mpperf`, base c09dfa04). Desktop, no editor, no _ue59 lock.

## What the evidence says
- No saved log has the owner's 3-player session. `GASP_Giustino2_ue59/Saved/Logs/*.log` (13 files, 18-19 Nov) contain only
  single-player PIE (`grep -c UEDPIE_2` = 0 on all). The last 3-client logs are in the old checkout `GASP_Giustino2/Saved/Logs`
  (12 Nov): there NetworkPrediction warnings are `ReconcileFromInputOffset` x12 plus 2 `LogGSTNSurface` join warnings, no flood.
  So the spam the owner sees was not captured; it is most likely the on-screen/Output Log warnings during PIE.
- The play settings (`_ue59/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` l.152-164): `PlayNetMode=PIE_ListenServer`,
  `PlayNumberOfClients=3`, `RunUnderOneProcess=True`, `bShowServerDebugDrawingByDefault=True`.
  One editor process ticks 3 worlds (server+2 clients), renders 3 viewports and runs 3 copies of every character
  (9 motion-matching ABPs, 9 NPP/Mover sims, client resims). That alone explains most of the 15 fps; it is not a real-game figure.
- Warnings that can fire every frame/packet in our NPP fork (all were plain `UE_LOG Warning`):
  `Client ID %d Discarded Update from Missing Delta State` (NetworkPredictionSerialization.h, 2 sites, per packet),
  `Independent Interpolation fell behind / starved` (per frame), `ReconcileFromInputOffset` (per offset change),
  `Desired Lag Compensation Rewind Exceeded` (2 sites, per rewind request). Low fps makes them fire more (starved buffers),
  and the flood in the Output Log costs frame time itself: a feedback loop.
- Not ours / not runtime: GASP ABP `FindConsoleObject` perf warnings (DDCvar.*, a.AnimNode.*; GASP content),
  ABP_GSTN_Locomotion "potentially thread-unsafe" compile warnings (215 in the 19 Nov log, editor compile only; the
  functions already carry `BlueprintThreadSafe` on this branch, so the warning should go after a rebuild + ABP recompile).

## Done (code)
- `NetworkPredictionLog.h`: `UE_NP_LOG_THROTTLED(Verbosity, Seconds, Fmt, ...)`, max one line per call site per second with
  the count of folded lines. Applied to the 6 sites above. Nothing is silenced, only rate-limited.

- Compile check: full `GASP_Giustino2Editor` build in this worktree, `Saved/Logs/ci/build.log.esito` = ESITO: OK (1207 s),
  `grep -cE "error C[0-9]|): error" Saved/Logs/ci/build.log` = 0.

## Repro attempt (19 Nov, editor thread, before the patch)
- `_ue59/Saved/PiePerf/before_one{,_r2}/run.log.esito`: both TIMEOUT (361 s, 481 s). Both `run.log.ue.log` end at
  `LogTurnkeySupport: Completed SDK detection: ExitCode = 0` (~1190 lines) and contain no DDC/Zen, dialog or error line,
  so the stall is not proven to be DDC. No project source was newer than `Binaries/Win64/UnrealEditor-GASP_Giustino2.dll`
  (find -newer, before 17:22), so a "modules out of date" box is unlikely; a modal box hidden by `-nosplash` on another
  desktop, or a second editor on the same project, is still possible. Next try: look at the screen/desktop 2 when it hangs.

## Left / next
1. Repro run (needs the editor slot, ~15 min; probe `Tools/GASNPPTest/pie_perf_probe.py`): same 3-player listen server, `stat unit`, `stat game`, `stat net` on the
   server window, Insights trace `-trace=cpu,frame,log,bookmark` 30 s, then the same with `RunUnderOneProcess=False`.
   Count warnings by category from the log. This tells PIE overhead apart from our tick cost.
2. Owner-side setting (recommended): Play > Advanced Settings: `Run Under One Process` off (each client its own process)
   and `Show Server Debug Drawing` off. Real multiplayer cost must be measured that way, not in one process.
3. If the trace shows Mover/NPP resim on top: look at forward-predicted sim proxies and the host send dividers (f0c7f995).
