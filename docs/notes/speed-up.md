# Speed-up (2026-11-19, desktop, branch claude/editor-speed-up) - PARTIAL, stopped at usage limit

## 1. Background throttle (all machines)
- Cause: UEditorEngine::GetMaxTickRate returns 3 fps when ShouldThrottleCPUUsage() (EditorEngine.cpp ~2592/5377, UE 5.9 source).
  ShouldThrottleCPUUsage returns false at once when FApp::IsUnattended() -> `-unattended` removes the cap. The old fix
  (`set ... bThrottleCPUWhenNotForeground 0`) did not work: 333 ms flat (pie-perf-probe-2.md).
- Run-Timed.ps1 -Preset Capture now adds by default: `-unattended -ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False`
  (no file edits, per run only) and every 5 s opts each UnrealEditor* process in its job out of Windows EcoQoS / timer-resolution
  throttling (SetProcessInformation ProcessPowerThrottling) + ABOVE_NORMAL priority. `-KeepThrottle` = old behaviour. `-ExtraArgs` added.
- Measure: Saved/SpeedUp/p1_before (KeepThrottle, 1 player PIE, 15 s): python dt flat 125 ms (clamped; real 333 ms per pie-perf-probe-2).
  p1_after (default, no throttle, same probe): 397 frames in 15 s, avg 37.8 ms, p50 37.3, p95 43.4 (26 fps) vs 46 frames before (~3 fps real).
  Source: Saved/SpeedUp/p1_{before,after}/result.txt line 'frames'. Startup 24.3 s -> 19.0 s ('Total Editor Startup Time' in run.log.ue.log).
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

## 4. 1 vs 3 players (anim-setup-1, 2026-11-19 ~18:25 UTC)
- p1_after really ran ONE player: result.txt 'set play_number_of_clients 1', 'PIE worlds: 1', and only UEDPIE_0 in run.log.ue.log. So 26 fps = 1 player.
- p3_one_after (PIE_PERF_PLAYERS=3, one process, -unattended, 15 s): 'PIE worlds: 3', frames 104, avg/p50/p95 125.0 ms = the engine's delta clamp, i.e. <= 8 fps (real may be lower). Source: Saved/SpeedUp/p3_one_after/result.txt line 'frames'.
- The probe sets the players per run (LevelEditorPlaySettings CDO, not saved); pie_capture scripts should do the same and force 1 player for single-character captures.
- Not measured: 3 players multi-process without throttle (owner's foreground 15 fps).

## 5. Why captures show low fps (anim-setup-2, 2026-11-19 ~19:05 UTC, owner question)
- The low fps the owner sees during a test run is the capture itself, not the game: pie_capture_move.py summons 6 SceneCapture2D
  (az0..az315, 480 px, final colour) plus the game view, renders all 7 every frame and writes each to PNG (GPU readback per view).
  Measured: mv_skid_release 210 frames in ~55 s, mv_skid_hold 300 frames in ~85 s = 3.5-4 fps real (Saved/AnimSetup2/cap_guard/
  capture.log, "sequence ... done" lines). The sim is unaffected (frames are counted, not timed); PIE already runs 1 player
  (PIE_CAPTURE_CLIENTS=0), -unattended and no background throttle (section 1).
- Cheaper, no code change: PIE_CAPTURE_VIEWS=az90,az0,game (3 views instead of 7) -> about 2x faster; one review needs at most
  2 side views + game. Further: write PNGs only for the frame window under review, or 320 px for numbers-only checks.
- Applied from now on in this thread: 3 views per capture.

## 4. Small editor window for low-res captures (owner 2026-11-20, skid-walkturn-2)
- Captures are 512-ish scene captures (pie_capture_move SIZE), so a full-size editor window only costs render time.
  `Tools/GASNPPTest/vd2_mover.py` (run by every *_boot_vd2.py and ask_assistant_boot.py) now shrinks each editor window of its
  own process to `EDITOR_WIN` (default `960x540`, restored first if maximized) and logs it to VD_LOG.
- Owner-facing hi-res videos (PIE_CAPTURE_HIRES viewport shots, screen recordings) take the real window size: set
  `EDITOR_WIN=full` for those runs. New capture boots must run vd2_mover.py (or do the same) so this stays the default.
