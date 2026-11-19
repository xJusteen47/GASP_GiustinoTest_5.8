# Speed-up (2026-11-19, desktop, branch claude/editor-speed-up) - PARTIAL, stopped at usage limit

## 1. Background throttle (all machines)
- Cause: UEditorEngine::GetMaxTickRate returns 3 fps when ShouldThrottleCPUUsage() (EditorEngine.cpp ~2592/5377, UE 5.9 source).
  ShouldThrottleCPUUsage returns false at once when FApp::IsUnattended() -> `-unattended` removes the cap. The old fix
  (`set ... bThrottleCPUWhenNotForeground 0`) did not work: 333 ms flat (pie-perf-probe-2.md).
- Run-Timed.ps1 -Preset Capture now adds by default: `-unattended -ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False`
  (no file edits, per run only) and every 5 s opts each UnrealEditor* process in its job out of Windows EcoQoS / timer-resolution
  throttling (SetProcessInformation ProcessPowerThrottling) + ABOVE_NORMAL priority. `-KeepThrottle` = old behaviour. `-ExtraArgs` added.
- Measure: Saved/SpeedUp/p1_before (KeepThrottle, 1 player PIE, 15 s): python dt flat 125 ms (clamped; real 333 ms per pie-perf-probe-2).
  p1_after: run was still going when the thread stopped -> next thread reads Saved/SpeedUp/p1_after/result.txt.
- Also fixed: Run-Timed -SelfTest exited 1 with all PASS (matched "FAIL-case").

## 2. Builds
- Incremental builds in _ue59 are 5-60 s (Run-Timed .esito files, 2026-11-19). The "20 min" was a fresh worktree:
  GASP_Giustino2_mpperf build 1207 s, 491 actions, 12 workers, log line "65.81 GB/67.17 GB committed" on 31 GB RAM = memory pressure.
- Advice: keep a pool of long-lived worktrees and `git switch` in them (incremental) instead of new worktrees; build with no other
  editor open; do not raise workers (bAllCores) on 31 GB. UBA local executor already on, adaptive unity already on.
- Power plan is Balanced: switching plan is a system setting, owner's choice (`powercfg /setactive SCHEME_MIN`).

## 3. Launch: warm editor startup 24.3 s ("Total Editor Startup Time", p1_before log). Not yet optimized.

## TODO (next thread)
- Read p1_after; re-run 3-player PIE probe one/multi process without throttle (Tools/GASNPPTest/pie_perf_probe.py, needs a boot in
  %TEMP% because of the space in the path), Insights top costs.
- One editor session for many captures: in-editor job server (queue folder, heartbeat, intercept QUIT_EDITOR via a proxy of the
  `unreal` module, unregister the job's tick callbacks) + client script with fallback to Run-Timed. Saves ~30 s per capture.
- Laptop: same Run-Timed change applies as is.
