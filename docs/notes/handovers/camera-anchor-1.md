# Camera anchor 1 (cloud, Opus, 2026-11-17) — NOT COMPILED
Owner 23:24Z: "la telecamera vibra su e giù quando faccio lo stomp o vado a terra". Branch `claude/project-thread-edmqov` from 98dml5 72d1afaf.

**Cause (from code, not from a capture):** GASP's camera (CameraAsset_SandboxCharacter, GameplayCameras: BoomArm + DampenPosition, no bone pivot) starts
from its camera component, which follows the actor root = capsule centre. FGSTN_BodyCapsuleModifier resizes the capsule (dair tuck, knockdown,
GroundedRestore growing back from the floor): the root jumps by the half height change while the mesh is kept in place. Body still, camera jumps.
Not the pelvis bone. Also the root steps at 62.5 Hz while the mesh is smoothed.

**Fix (presentation only, local only, GASP rigs untouched):**
- `Source/.../Camera/GSTN_CameraAnchor.h/.cpp`: `UGSTN_CameraAnchorComponent` (TG_PostUpdateWork) reads the "standing root" back from the drawn mesh
  (mesh world minus its template relative = unaffected by capsule shifts, includes Mover smoothing), damps it with a critically damped vertical
  spring along Mover's up, and moves the camera component (tag `GSTN_CameraAnchor`, else GameplayCamera, SpringArm, Camera by class name).
  Mode `ShiftView` (or no camera component) shifts the final view in `UGSTN_PlaytestCameraModifier` instead.
- Data: `FGSTN_CameraAnchorSettings` (UpHalfLife/DownHalfLife 0.07 s, DeadBand 0, MaxVerticalLag 80, HorizontalHalfLife 0, SnapDistance 400) on the
  component (per character) or `UGSTN_CameraAnchorSettingsAsset` at /Game/Blueprints/Data/DA_CameraAnchor_Default (optional; C++ defaults if missing).
- `UGSTN_PlaytestCameraSubsystem` adds the component to every locally viewed pawn without one. No BP wiring, no project settings.
- Console (A/B, debug only): `gstn.Camera.Anchor 0/1`, `gstn.Camera.Anchor.Debug 1` (red raw anchor, green damped, yellow actor root).
- Tests `GSTN.Camera.Anchor.*` (8, pure): step drop smooth/no overshoot, frame jitter 20 cm -> <5 cm, zero half life exact, lag clamp, teleport snap,
  dead band, damping only along Up, standing root unchanged by a capsule shift (3 yaws). Spring numbers pre-checked in Python, not in UE.

**Desktop steps (no editor open while building):**
1. Merge `origin/claude/project-thread-edmqov` into the build branch (touches only Camera/* and GSTN_PlaytestCamera.*; no overlap with z9mi0s/27htqa).
2. Build `Run-Timed.ps1` (45 min). Tests: `Automation RunTests GSTN.Camera.Anchor` (expect 8/8), then the usual full suite.
3. Optional: run `Tools/GASNPPTest/camera_anchor_setup.py` to create DA_CameraAnchor_Default (tuning as data).
4. PIE L_Test_Hub, Overlord: dair stomp landing + get knocked down. Record with `gstn.Camera.Anchor 1` and `0` (same inputs), `gstn.Camera.Anchor.Debug 1`
   once to confirm green sphere is smooth. Commit both mp4s in docs/notes/reviews/camera-anchor-1/.

**Risks / open:** if GASP's camera component is not found by class name the fallback ShiftView applies the raw correction under GASP's own
DampenPosition (small opposite transient). The log would show it: check `GSTN_CameraAnchor` component's mode in the details panel at runtime.
Damping feel (0.07 s) is a default, owner tunes. Dead band for jumps (Astro camera plan) is available but off (expeditions camera later).
