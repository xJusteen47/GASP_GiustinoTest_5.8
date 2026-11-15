# Tools/CI

`Run-Timed.ps1` is the only way to run a build, a test run or a capture (project rule 5). It starts the process itself inside a
Windows Job Object (suspended, assigned, then resumed), so on TIMEOUT, on a silent log (BLOCCATO) or if the wrapper dies, it kills the
whole tree itself, with no taskkill of foreign processes (works under Claude auto mode). Output goes to the `-Log` file; the console
gets at most 5 lines; the verdict is also in `<Log>.esito`. Exit: 0 OK, 1 FAIL, 124 TIMEOUT, 125 BLOCCATO.

    powershell -File Tools\CI\Run-Timed.ps1 -Preset Build   -Log Saved\Logs\ci\build.log
    powershell -File Tools\CI\Run-Timed.ps1 -Preset Test    -Log Saved\Logs\ci\test.log -Tests GSTN
    powershell -File Tools\CI\Run-Timed.ps1 -Preset Capture -Log Saved\Logs\ci\cap.log -Script Tools\GASNPPTest\hub_startup.py
    powershell -File Tools\CI\Run-Timed.ps1 -Kind Test -Log x.log -Exe "C:\tool.exe" -ArgLine "a b"   # generic

Limits: Build 45 min, Test 30, Capture 15 (`-MaxMinutes`/`-MaxSeconds` to lower). Silent log 10 min (`-SilentMinutes`), watched file
= the log, or `-WatchFile` (presets Test/Capture watch `<Log>.ue.log`, written by UE `-abslog`). Engine: `-Engine` or env
`UE_ENGINE_DIR`, default `C:\Program Files\Epic Games\UE_5.9\Engine`. Target for Build: `-Target` (default `GASP_Giustino2Editor`).
It does NOT check `_ue59/IN-USO.txt` or a running editor: the caller does.

Quick self-test (no engine, about 25 s): `powershell -File Tools\CI\Run-Timed.ps1 -SelfTest` must print 4 lines, all PASS
(OK, FAIL, TIMEOUT kills grandchild, BLOCCATO). Not yet run on Windows (written in a Linux cloud session).
