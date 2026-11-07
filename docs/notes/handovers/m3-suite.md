# R3-1 M3 suite stability (laptop, 2026-11-06)

Branch claude/project-thread-2jy4k8 from b72c608. Build green (UBT editor, Development). No C++ or harness change was needed.

Result: the M3 suite (first 28 names of SUITE_DEFAULT up to `slide_jump`, plus `loadout_change` = 29 scenarios) passed
**29/29 on four consecutive runs**: listen host + 2 clients at 60/15 ms 0 % loss, three passes (p1-p3) and one dedicated server + 2 clients (d1).
Each run restarts the editor (fresh PIE, nullrhi). Reports: Saved/R31/report-{p1,p2,p3,d1}.txt (untracked).

Why the HANDOVER 14 flakes did not show up: not diagnosed, no failure to diagnose. Likely already fixed by later harness work
(lane reset with `lanes_settled`, apex/settle changes in M4-M7 commits). Not proven: 4 passes is evidence, not a guarantee.
Open from the gate, NOT done: `spike_triggers` T3, O-CameraYaw (command yaw 0 vs 64 in reconcile dumps), S-MaxSpeed. Not touched; they are
diagnostics outside the 29 scenarios. R3-2 canary (`Tools/Canary/make_canary.py`) also not done.

Added: `Tools/GASNPPTest/m3_suite_driver.py` (headless driver; env R31_TAG, R31_MODE=listen|dedicated, R31_ONLY=a,b).
Recipe: copy it to a path without spaces (TEMP), run `UnrealEditor.exe <uproject> -nullrhi -unattended -nosplash -ExecCmds="py <abs/driver>"
-abslog=<file>`; it starts PIE, runs `run_suite`, writes the report, QUIT_EDITOR. ~8-10 min per run. It rewrites the PIE ini
(setup_editor); I restored PlayNetMode=PIE_ListenServer, PlayNumberOfClients=3.
Not done: no map saved, nothing pushed to feature. Cost: about 1.5 build + 4 editor runs, token cost est. ~$1.5 (well under budget).
Next for R3-1 (cheap): decide with the owner if the three leftover diagnostics are worth a thread; else mark the gate met.
