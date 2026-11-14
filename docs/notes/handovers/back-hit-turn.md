# Back-hit turn (thread "Turn victim on back hits"): handover, 2026-11-14 ~22:45Z

Branch `claude/project-thread-a5idak`. Owner ask: on a back hit (hitstand, knockdown) the victim faces the attacker on the FIRST reaction
frame and keeps the facing after the get-up; temporary until back-hit/launch clips exist.

## Root cause and fix (08338b8d, fix attempt 1 of this thread)
- `FGSTN_SetFacingEffect` wrote the yaw only into the Mover sync state. `UWalkingMode::SimulationTick` skips the component move when the
  step neither moves nor turns from its start state (which already had the new yaw), and `CaptureFinalState` reads the rotation back from
  the component: the old yaw returned on every frame at rest. Hence: hitstand never turned, knockdown turned on its first moving frame.
- Fix: the effect also sets the updated component's rotation (no sweep, TeleportPhysics), as `FTeleportEffect` moves the component.
  Deterministic; on resim the component is restored from the sync state first. `[HitTurn]` logs now Verbose.
- aa698b45 (orientation intent held at the turn yaw) is kept: harmless, and keeps walking from turning back on that frame.
- Built in _ue59; tests GSTNCombat.NP.Hits + Timeline + StartupSteer: 87/87 pass.

## Capture (react-hook-6, docs/notes/reviews/react-hook-6/, README there)
- back_hitstand: yaw 0 -> 180 on v36, the first Hitstun frame, while frozen; holds 180 to the end.
- back_knockdown (run alone): yaw 180 on v28, the first Tumble frame (8 frames earlier than react-hook-5); holds through KD, get-up, end.
- Capture-script note: after the hitstand the victim now faces the attacker, so in the same run the knockdown placement overshot (382 cm,
  miss). Run back_knockdown alone, or fix the behind placement to turn first.
- Needs the rule-10 cloud review (PASS/FAIL) before it goes to the owner. Turn is a one-frame snap by design.

## Virtual desktop 2 (owner request)
- `Tools/GASNPPTest/vd2_mover.py`: run it first in the editor boot script (`runpy.run_path(..., run_name="vd2_mover")`). Every 0.5 s it
  moves all top-level windows of the editor process to desktop "Programmazione" (GUID 94a28a1c-..., env VD_TARGET_GUID) with the public
  IVirtualDesktopManager (works only in-process). Worked first try (hr=0), Play windows included; the window may flash on desktop 1 at start.

## State left
- _ue59 on claude/project-thread-a5idak, built; Unreal closed; PM_BoostJump/PM_ThrowRider reverted; no maps saved. The animation thread's
  untracked files under Tools/AnimPipeline/cards/overlord/ untouched.

Cost: this thread ~120k tokens Opus (~$7) incl. 1 build, 1 test run, 2 captures.
