# Handover: camera P1a pitch lock (cloud diagnosis, 2026-11-21)

Branch `claude/project-thread-aq82ak` (from `claude/runstart-generic-desktop` bcee4afc). NOT compiled (cloud, no editor).

**Cause**: feedback loop. The wiring read the look input as "control rotation change" AFTER the GSTN camera had already written the control
rotation (WriteBack, TG_PostPhysics). The camera's own motion came back as input every frame (gain 1 integrator) and ran the pitch to the Free
clamps -70 / +20; the real stick input was overwritten before being read. Details: docs/notes/camera-platforming/p1a.md "Pitch lock fix".

**Fix**: plugin `ApplyLook` reads `LookInputSource::ControlRotation` before its write back (delta from its own last write; first frame = 0).
Wiring: P1a camera gets a transient profile with that source; AddLookInput feed and modifier delta removed.
New: `FGSTN_CameraMath::ControlRotationLookDelta` + test `GSTNCamera.Core.ControlRotationLookIsOnlyThePlayersChange`; cvar `gstn.Camera.LookLog`
(auto-listed in GSTN Debug Options under gstn.Camera.*); checker `Tools/GASNPPTest/camera_look_check.py`.

**Desktop recipe** (Sonnet/Haiku, ~20 min):
1. Merge this branch into the playtest branch, real incremental build (not Live Coding).
2. Automation: `GSTNCamera.Core` (all pass, incl. the new test).
3. Scripted run: `camera_jitter_run.py` with `CAMTRACE_CVARS="gstn.Camera.P1a 1;gstn.Camera.LookLog 1"`, 1 player.
4. `python Tools/GASNPPTest/camera_look_check.py` -> expect (1) 0, (2) <= 0.5 s, orbit pitch median about -25, ESITO PASS.
   With no stick in the script, (3) says "stick not tested".
5. Owner hand test in PIE (P1a 1, LookLog 1): stand 3 s, right stick up/down/left/right, run, jump. Camera must stay at about -25 when
   the stick is released and follow the stick; then re-run step 4 on the new csv: (3) > 0 inputs, 0 wrong way.
6. If the stick moves the pitch the wrong way: GASP's look sign differs; set the profile's bInvertLookY (do not touch the delta).

Proof of cause (read only, no run here): old wiring order = PC input (PrePhysics) -> camera write back (PostPhysics) -> subsystem delta (tickables)
-> modifier stores rotation (camera update). The Free clamp values -70/+20 are exactly the two locked pitches the desktop saw.

COSTO: 4 $ | 0 min desktop | 0 token review
