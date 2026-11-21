# crouch-mesh-pop-1 (cloud, Opus, 2026-11-21) — mesh flash on ZL crouch / stand

Branch `claude/project-thread-qncgfg` from `claude/runstart-generic-desktop` 5a83eb4b. NOT COMPILED (cloud, no editor).

## Cause (code reading, not yet measured)
- Crouch is ours: `FGSTN_BodyCapsuleModifier` (Mover `FStanceModifier` off). On crouch the actor moves down ~31 cm (86 -> 55 half height)
  and the base visual offset up by the same: the mesh must not move.
- Network Prediction draws the mesh at `base offset x interpolated actor` in OnWorldPreActorTick (FinalizeSmoothingFrame). The offset
  steps at once, the actor only by the interpolated fraction: for the rendered frames of the shift frame (2-3 at 144 fps) the mesh is
  (1 - alpha) x 31 cm off. `ApplyPresentation` (2026-11-17) corrects that, but only in TG_PostUpdateWork, AFTER the mesh animated
  (TG_PrePhysics). GASP's ABP (Offset Root, foot placement) reads the mesh's world motion: the pose of those frames carries the error -> flash.
- Old desktop trace (Saved/CamJitter3/v2/py_rows.csv, crouchspam, ~22 fps) shows mesh_z constant 0.15: the COMPONENT ends right, so the
  flash must be in the pose. Unproven until the capture below runs at 144 fps.

## Fix (attempt 1 of 2)
- `UGSTN_CombatSimComponent`: second tick `FGSTN_BodyCapsulePresentTickFunction` in TG_PrePhysics, prerequisite of the primary visual
  component's tick, runs `ApplyBodyCapsulePresentation()` before the anim. The PostUpdateWork call stays (idempotent, CorrectedAt guard).
- cvar `gstn.Mesh.CapsuleShiftBeforeAnim` 1 default / 0 old; in GSTN Debug Options (`gstn.Mesh.*` listed, test updated). Divergence row 23.

## Desktop recipe
1. `powershell -File Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\crouchpop-build.log` (new USTRUCT: real build) -> ESITO OK.
2. `powershell -File Tools\GASNPPTest\crouch_pop_timed.ps1 -Out Saved\CrouchPop\r1` (1 player, t.MaxFPS 144, crouch 0.25 s on/off,
   6 s with the fix then 6 s without). Prints the checker: `spam_fix` / `spam_old` mesh_step, root_step, spikes, crouches, fps + ESITO.
3. PASS = spam_fix mesh_step <= 0.5 cm, root_step <= 1 cm, spikes 0, crouches >= 6. spam_old should show the pop (if it does not,
   the capture does not reproduce the owner's flash: record a short video of ZL spam instead before a 2nd attempt).
4. Ask the Unreal Assistant: "does GASP's Offset Root node react to a one-frame mesh component move?" (rule 14).
5. If FAIL: attempt 2 candidates: alpha from the NP smoothing directly instead of the projection in `SmoothingCorrection`; or the
   anim crouch state (Mover.IsCrouching) arriving a frame after the capsule.

COSTO: ~3 $ | 0 min desktop | 0 token review
