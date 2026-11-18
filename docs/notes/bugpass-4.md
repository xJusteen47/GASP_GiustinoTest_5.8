# Bug pass 4: input and camera C++ (cloud, review + 2 small fixes + 1 test), 2026-11-18

Scope (checkpoint 4, the last cloud one, of "bug fix e test su tutto"): branch claude/project-thread-3f1vdx, based on
claude/project-thread-pg3fen (e137330c, the newest camera code: tilt and shift independent, telephoto option; not yet in 98dml5).
Reviewed in full: Source/.../Camera/GSTN_CameraAnchor (anchor, air hold = jump-stable camera, aim crouch offset), GASNPPTest/GSTN_PlaytestCamera
(framing push, telephoto, shift, tilt-shift DOF / band / tilted plane, subsystem), GSTNInputNP GSTN_InputSimulator + GSTN_InputSource
(sampling, held/pressed, mapping contexts), GSTNInputCore GSTN_SlotResolver + GSTN_InputHistory (buffer, consume, hold, frame re-base).
Spot-checked only: matcher / conditions / reset rules (covered by the GSTNInput.* suites). Not reviewed: GSTNCamera plugin (its component is
inactive on the pawn since 1f60d450; GASP's GameplayCamera drives the view). Nothing compiled here (cloud).

## Findings (P1 visible/likely, P2 wrong but rare or latent, P3 minor)

| # | Sev | Where | Issue | Evidence | State |
|---|---|---|---|---|---|
| K1 | P3 | GSTN_PlaytestCamera.cpp ScreenV | The screen band centre ignored the Shift's off-center offset: with Shift on and Mode = ScreenBand, the sharp band sat where the unshifted character would be. Both are off by default. | ProjectNDC subtracts the offset, ScreenV did not | FIXED + test GSTN.Camera.TiltShift.ScreenVFollowsShift |
| K2 | P3 latent | GSTN_CameraAnchor.cpp Damper::Step (teleport test) | The snap test compared the character with the HELD camera. With AirRiseBand + MaxVerticalLag above SnapDistance (defaults 250 + 80 < 400: safe), a high jump read as a teleport: the camera popped onto the character and the air hold was lost until the landing. | code read | FIXED: the test measures the followed (held) point; no change at the default values. Test GSTN.Camera.Anchor.HighJumpInRiseBandDoesNotSnap |
| K3 | P3 feel | GSTN_CameraAnchor.cpp:71 + :145 | Any landing whose standing root differs > 1 cm from the take-off ground (slopes, stairs, smoothing) starts the landing catch up: LandingHalfLife 0.2 s instead of 0.07 and no lag clamp upward. Softer camera after jumps on uneven ground. | code read | OPEN, desktop feel check (timing). Option: threshold as data (e.g. 10 cm) |
| K4 | P3 latent | GSTN_CameraAnchor.cpp:25 + ResolveComponents | Class fallback ends at "CameraComponent", and UGSTN_CameraComponent derives from it: a pawn without GameplayCamera/SpringArm would get its INACTIVE GSTN camera moved (no effect, jitter fix silently off). Components are resolved once (:361), so a camera added later is never picked. | code read | OPEN, check with gstn.Camera.Anchor.Debug 1 that the green sphere drives the view |
| K5 | P3 perf | GSTN_CameraAnchor.cpp:250/320/370, IsOwnerAiming | Per tick: 2 FindComponentByClass(Mover), a reflection walk for WantsToAim, a controller loop. Small, local player only. | code read | OPEN (cache when touched next) |
| K6 | P3 data | Damper::Step | DeadBand larger than MaxVerticalLag: the lag clamp wins over the dead band (camera moves inside the band). | code read | OPEN, tooltip note only |
| K7 | P3 | GSTN_PlaytestCamera.cpp:127 | Band / plane materials are loaded once per camera modifier: a missing material is not retried, a material changed in the DA during PIE applies next session. | code read | OPEN, by design |
| I1 | P3 | GSTN_InputSource.cpp:56 | Held = GetActionValue().Get<bool>(): any non-zero analog value is held. RT (strong) / LT (crouch) without a dead zone or actuation trigger in the IMC would read trigger noise as presses. | code read | OPEN, asset check: IMC trigger mappings need a dead zone |
| I2 | P3 known | GSTN_InputSimulator.cpp:66 | Release and re-press inside one sim frame (16 ms) of a held button is lost (trade off of the held dodge fix cb8163ab, documented there). | code read | accepted |
| I3 | P3 | GSTN_InputSource.cpp:42 | ApplyMappingContexts runs every frame; if a GASP Blueprint re-adds IMC_Sandbox, each removal rebuilds the mappings and Enhanced Input fires Started again for held keys. Harmless since cb8163ab. | code read | OPEN, note |
| I4 | test gap | GSTN_InputSimulator.cpp:66 | The held re-press rule (owner bug: held dodge dodged twice) had no test. | grep of the suite | ADDED GSTNInput.NP.Simulator.HeldButtonStartedAgainIsNotAPress |

Checked and fine: critically damped spring (exact form, HalfLife 0 snaps, DeltaTime 0 holds); air hold edges continuous; StandingRootFromVisual vs
FGSTN_BodyCapsuleModifier and Mover's StanceModifier (both offset the visual by the capsule change, so crouch/tuck never move the anchor);
restore on inactive / EndPlay; ShiftView path; off-center sign vs UE's projection (M[2][0] = -Offset.X); telephoto scale and wall fallback;
tilted plane normal; PP blends re-added each frame; history frame re-base keeps held buttons; resolver press groups / reserve / consume.

Proof: K1, K2, I4 diffs: `git diff e137330c..HEAD -- Source Plugins`. Merge test into 98dml5 (5b22e56e): source merges clean, one docs conflict
in docs/notes/camera-tiltshift-1.md that comes from pg3fen itself.
