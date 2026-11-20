# Camera jitter 2: system diagnosis (desktop, Opus medium, 2026-11-20 22:45-23:20 local) — NOT REPRODUCED by script
Branch `claude/playtest-cam-render-skid` (merged `claude/project-thread-hiq1zx` 493df649). Built (Run-Timed, ESITO OK, Saved/Logs/ci/build_camjitter2.log).
## What ran
`Tools/GASNPPTest/camera_jitter_timed.ps1 -Out Saved\CamJitter\<run> -Clients 0|2 -CVars "..."` -> camera_jitter_run.py: PIE L_Test_Hub, KIT_Test_Playtest,
`gstn.Camera.Trace 1`, stand 5 s, walk back and forth 4 s (stick flips every 0.4 s), stand 1.5 s, full jump, Trace 0. Real frame time (no OverrideFPS).
Python sampler (py_rows.csv: POV, actor, mesh, GASP camera component) next to the C++ trace. Trace now writes one file per PIE instance
(gstn_camera_trace_<n>.csv for clients): with 3 players the old single file mixed three cameras.
## Result (docs/notes/reviews/camera-jitter-1/desktop-2/results.txt, from camera_jitter_summary.py + camera_trace_check.py per run)
| run | fps (median dt) | final view z zig-zags standing | walking | jump |
| 1 player, Anchor.Early 1 | 72-75 | 0 | 0 | 0 |
| 3 players, Early 1 | 20-24 | 0 (host and both clients) | 0 | 0 |
| 3 players, Anchor 0 | 19-22 | 0 | 0 | 1 x 3 cm (root step, expected without anchor) |
| 3 players, Early 1, Loco.Base 0 | 21-22 | 0 | 0 | 1 x 2 cm |
Pitch and yaw: 0 zig-zags everywhere; sweep_hit, anchor_on never flip; Mover.IsInAir flips only at the jump. The 10-25 cm two-height bounce of
the owner's video does not appear. Real fps with 3 players in one process: 19-24 (matches the video's ~20 unique fps); 1 player 72-75.
Checker now also reports "boom" (camera to drawn body distance) zig-zags: a pitched camera moving along its boom also reads as a vertical bounce.
## Conclusion
Not our camera stages under scripted input. What the script does NOT reproduce: the owner's real gamepad through Moonlight (look stick,
partial stick / AnalogWalk), his longer session and console toggles, the viewport he records. Rule 10: check the capture chain too.
## Options (owner/coordinator)
A (recommended, ~1 $): owner plays as usual, types `gstn.Camera.Trace 1` when the bounce shows, ~10 s, then `gstn.Camera.Trace 0`;
  a thread runs `python Tools/GASNPPTest/camera_trace_check.py` on Saved/Logs/gstn_camera_trace.csv: first column with kicks = culprit.
B (~3 $): extend camera_jitter_run.py with look-stick input, partial stick and 3 min session; speculative.
C (~1 $): same play recorded locally (Win+Alt+R / OBS) vs through Moonlight: if only the Moonlight copy bounces, it is the stream.
No fix implemented (nothing to fix found). Unreal Assistant asked (Saved/UE59/assistant_camjitter.json): no reply arrived before the editor closed.
First launch was a TIMEOUT (15 min, launcher regex bug, fixed; counts as one attempt). Editor reopened for the playtest at the end.
COSTO: ~4 $ | 21 minuti desktop (build 0.5 + 15 lost + 4 PIE runs) | 0 token review
