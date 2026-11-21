# Camera P1a: platform camera core in the GSTNCamera plugin (code + headless tests, cloud)
Thread "Build Mario-style camera code P1a", 2026-11-21, cloud, Opus high. Branch `claude/project-thread-w6l7mz` (from `claude/camera-mario-research` 03afc437, itself from `claude/project-thread-hiq1zx`).
Owner decisions this builds on: 19/11 1A 2A 3A (design.md: own plugin, gentle auto-follow with menu switch, plan replaces C0-C3); 21/11 (cmsg_01CRvUdrWm1dJrM4q54YDBLNHzCfFRDhNeEgz48URm4cHQ): **1 free by default, designer can lock or focus parts of the level without hard locks (replaces "Parallel")**, 2 no shared co-op camera for now, 3A P1a now in the cloud.
**Not compiled, not run** (no engine in the cloud): the desktop must build and run `GSTNCamera.*` first (list below).

## What was built
| Piece | Where | Notes |
|---|---|---|
| `UGSTN_CameraProfile` (Data Asset) | `GSTNCameraRuntime/Public/GSTN_CameraProfile.h` | Replaces `UGSTN_CameraSettings` (Project Settings and `Config/DefaultGSTNCamera.ini` deleted). Set on the component `Profile`; null = code defaults. Master switch is only `gstn.Camera.Enable`. |
| Free = default mode | `MakeDefault`, component fallbacks | Empty `DefaultModeTag` now means `Camera.Mode.Free` (was Follow). Own view tuning `FreeView` (pitch -18, FOV 85, arm 650: starting values, tune on video in P1b). |
| Asymmetric pivot | `FGSTN_CameraMath::StepPivotAsymmetric`, Free `DampPivot` | Catch-up 0.18 s while running away, settle 0.10 s when braking, vertical 0.2 s. |
| Horizontal dead zone (owner 21/11 01:37) | `FGSTN_CameraMath::StepDeadZone`, Free `AdjustFollow`, profile `Free.DeadZoneWidthCm` 80 / `DeadZoneDepthCm` 60 | Mario-style rectangle on the ground around the camera anchor, oriented with the camera: small steps inside it never move the camera; leaving it drags the anchor so the character sits on the edge, then the asymmetric lag smooths it. 0 = off per axis. Works with framing volumes, recenter and auto follow (the box turns with the view). |
| Vertical band | Free `AdjustFollow` | `FallFollowCm` 50 -> 120, plus follow down after 0.25 s of real falling; fall pitch proportional to falling speed (max 10 deg at 1500 cm/s). |
| Look-ahead | Free tuning | 150 -> 100 cm, x0.5 in the air (`LookAheadAirScale`). |
| Pitch curves | `FGSTN_CameraPitchCurves` (profile `FreePitchCurves`) | Arm multiplier by pitch (1.15 at -70, 1 at -18, 0.75 at +20) and FOV offset (empty). |
| Distance presets | `FGSTN_CameraDistancePresets`, user options | Near 0.8 / Medium 1 / Far 1.25 / Custom slider, eased 0.45 s, clamped by section `DistanceScaleLimit`, locked section = 1. Input `DistanceAction` (1D, +1/-1), BP `CycleDistancePreset`, `SetDistancePreset`. |
| Auto follow (2A) | `StepAutoFollow`, profile `AutoFollow` | Only after 1 s of running (>= 250 cm/s) and 1 s without look input, max 30 deg/s with ramp and ease, never past 120 deg (no spin towards camera), never in air/aim/opponents/`AutoFollowBlockTags`; option `bAutoFollow`. |
| Soft framing volume | `AGSTN_CameraBiasVolume` ("GSTN Camera Framing Volume") | Owner decision 1. Direction (volume forward or toward a focus point/actor), pitch, zoom, FOV, pivot shift to the focus; Soft (stick free, pull resumes after 1.5 s) or Firm (stick free within a range, wider at the edge). Weight smooth inside the volume (`FalloffCm`), blends on enable/disable, capped turn speed 45 deg/s, off in the air and outside a 100 deg engage cone, priorities. Sections still win (hard framing). |
| Recenter + Camera Assist | component `ApplyLook` | Tap recenter as before (0.25 s). Assist option: hold recenter >= 0.2 s and the move stick turns the camera (120 deg/s); `IsCameraAssistActive()` so the move code stops the character. |
| Stick latching | `LatchStickReference` / `ResolveStickReference`, BP `ResolveMoveReferenceRotation(MoveStick)` | After a camera-made turn (section, cut, respawn) the move reference stays the old one until the stick drops below 0.25 (or 3 s). Manual orbit, auto follow and framing never latch. |
| Player options + save | `FGSTN_CameraUserOptions` on the local player subsystem | Invert X/Y, sensitivity, distance, auto follow, assist; `SaveUserOptions`/`LoadUserOptions` (slot per controller id, loaded at start). |
Multiplayer: everything stays local presentation (per local player state, never replicated, nothing on a dedicated server; framing volumes evaluated per camera on its own pivot). 144 Hz: all smoothing is exponential/eased on real delta time, frame-rate independent (tested), reading the presented pivot.

## Decisions applied (rule 6, to confirm 13d)
1. Soft framing volume is a NEW actor beside `AGSTN_CameraSection` (sections = hard framing, kept); the "Parallel" mode is not built.
2. Framing volumes and auto follow act only outside sections and never in aim.
3. Locked sections ease the distance to 1 instead of snapping (old code snapped).
4. Bias pull acts on the player orbit (so the stick continues from where the camera is: no pop), view terms (zoom, FOV, pivot) blend via the volume weight.
5. `FreeView` separate from `Follow` tuning so the old far-follow mode keeps its values.
6. Camera Assist recenters on button RELEASE when the option is on (a hold must not also recenter); on press when off, as before.
7. No level-wide profile override actor yet (component profile + volumes cover P1a); add in P4 if needed.

## Desktop P0 must test (with P1a merged in the P0 branch)
Build: incremental build of the editor target, then `Automation RunTests GSTNCamera` (headless, `-nullrhi`): expected all `GSTNCamera.Core.*` and `GSTNCamera.Runtime.*` green; the 14 new tests are listed in the plugin README. Any compile error in P1a files = a code fix thread (cloud), not a P0 retry.
Then the original P0 brief (handoffs/camera-p0-brief.md) with these additions on `BP_GSTN_CameraP0Test`:
1. Adapter: subject = `UGSTN_CameraAnchorComponent` damped pivot (capsule-safe, never moved by capsule changes: crouch spam, FAir, landing must NOT move the camera). Anchor air hold OFF on this pawn (Free does it).
2. C1 tick order: anchor writes before `UGSTN_CameraComponent` evaluates, both <= TG_PostPhysics (rule from camera-tick-order). 144 Hz render, 62.5 Hz sim: run in a straight line and stop, no stepping or 2-level bounce (`gstn.Camera.Debug 1`).
3. C4: `ControlRotationPolicy WriteBack`; movement and aimed attacks follow the camera; then wire the move input to `ResolveMoveReferenceRotation` and check: run through a section cut while holding the stick, the character must keep its world direction until the stick is released.
4. Default mode is Free with no rule set; verify jump in place (pivot stays), jump onto a 1 m step (realign on landing ~0.6 s), walk off a ledge (follows down after 0.25 s), stairs (no bobbing).
4b. Dead zone: small steps and idle shuffles (jab in place, turning on the spot) must not move the camera; walking out sideways or forward it follows smoothly, no jerk at the edge. Try 0/0 (off) vs 80/60 vs 120/80 on video.
5. Asymmetric lag: sprint start (camera trails a bit) vs hard stop (settles quickly), no overshoot.
6. Distance: bind `DistanceAction` to d-pad up/down in a test mapping context; cycle Near/Medium/Far, change eases ~0.45 s, no pop.
7. Auto follow: run in a wide circle without touching the right stick: after ~1 s the camera starts turning behind at <= 30 deg/s; touch the stick: it stops at once and waits 1 s; option off: never turns; never turns while aiming.
8. Framing volume: place one on L_Test_Hub (Soft, VolumeForward) and one TowardFocus with a focus point; walk in: smooth pull after 1.5 s idle, stick free; Firm variant: stick limited to +-35 deg, never stuck; jump inside: no steering mid-air; overlapping section: section wins.
9. Camera Assist option on: hold recenter, left stick turns the camera; tap still recenters.
10. Capture (timed script, 15 min): current GASP camera vs P0/P1a camera, same inputs: run+stop, jump in place, jump on step, crouch in/out, stomp, a circle run (auto follow), walk into a framing volume. mp4 + grids in docs/notes/reviews/camera-p0/; Fable looks before the owner.
Still open from research: Astro Bot auto rotation conflict (8 frames from a video, P0 item); Gameplay Cameras 5.9 status (ask the editor AI assistant).

## Pitch lock fix (2026-11-21, cloud diagnosis, not compiled)

Symptom (desktop PIE, gstn.Camera.P1a 1): distance works, but the pitch sits at -70 (top down) running and idle, and at +20 after a jump; the stick does not turn the camera.

Cause (code reading, exact): the first wiring (bcee4afc) measured the look input in `UGSTN_PlaytestCameraSubsystem::Tick` as the control rotation change
since the modifier's last view. But the GSTN camera writes the control rotation back (`ControlRotationPolicy WriteBack`) in TG_PostPhysics, BEFORE
that measure. So the "delta" was the camera's own motion of this frame, fed back as look input next frame with gain 1: an integrator,
pitch(t+1) = pitch(t) + (pitch(t) - pitch(t-1)). Any first step (the hub controller's start pitch vs the Free default -25, or the air framing on a jump)
became a constant speed that ran until the Free clamp: MinPitchDeg -70, MaxPitchDeg +20 (FGSTN_CameraFollowTuning). The real stick input was
overwritten by the write back before it was read, so it was lost. LookFeed 0 removed the loop; the camera then sat at -25, which looks close to GASP's.

Fix: the plugin implements its `LookInputSource::ControlRotation` (it was declared but never read): inside `ApplyLook`, before the write back, the input
is the control rotation change since the camera's own write of the previous frame (`FGSTN_CameraMath::ControlRotationLookDelta`, test
`GSTNCamera.Core.ControlRotationLookIsOnlyThePlayersChange`). First frame or after a gap = no input (no jump). The playtest wiring gives the P1a
camera its own transient profile with that source (`gstn.Camera.P1a.LookFeed` 1 = ControlRotation, 0 = no look) and no longer feeds AddLookInput.
Log: `gstn.Camera.LookLog 1` -> `Saved/Logs/gstn_camera_look_<pawn>.csv`; checker `Tools/GASNPPTest/camera_look_check.py` (ESITO PASS/FAIL).

Seen, not changed (out of scope): the subsystem hands the camera its subject in a tickable (after the camera evaluated), so the P1a pivot is one frame
late; if P1a shows jitter on the run, that is the next suspect (fix: write the subject before TG_PostPhysics, rule of [camera-tick-order]).
