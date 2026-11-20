# Camera jitter 1 (cloud, Opus high, 2026-11-20) — NOT COMPILED
Owner: camera jitters on jump, landing, dropkick, "everything". Branch `claude/project-thread-hiq1zx` from claude/runstart-v5-desktop (98c3b9f6).
## Cause (from code + a model, not from a capture)
UWorld::Tick updates the camera managers AFTER TG_PostPhysics and BEFORE TG_PostUpdateWork. `UGSTN_CameraAnchorComponent` ticked in
TG_PostUpdateWork: it moved GASP's GameplayCamera component after the view was already built. Next frame the actor root (capsule, steps at the
62.5 Hz sim rate, NOT smoothed; only the mesh is) drags the component before the view, so each view = last frame's correction + this frame's
raw root step. With the air hold the camera should stay still in a jump: instead it kicks by one sim step (v/62.5 = 14-24 cm when falling)
on every frame a step lands, then snaps back. Worse when falling fast (dropkick, landing), visible everywhere (running: horizontal).
Model: docs/notes/reviews/camera-jitter-1/jitter_sim.py -> jitter_sim.log (`python3 jitter_sim.py`): old order camera off target max
14.1 cm at 120 fps (28% of frames > 2 cm), 27.6 cm at 60 fps; new order 0.0. GASP's own DampenPosition may soften it in game (not modelled).
Same bug class in the unused GSTNCamera plugin: UGSTN_CameraComponent computed in TG_PostUpdateWork, GetCameraView served last frame's view.
## Fix
- Anchor ticks in TG_PostPhysics (`EarlyTickGroup`), after the pawn's other PostPhysics presentation ticks (prerequisites; cameras excluded),
  and the camera component's own tick (if any) waits for it. Before reading the mesh it runs the body capsule mesh correction
  (`UGSTN_CombatSimComponent::ApplyBodyCapsulePresentation`, new, idempotent; the combat tick in PostUpdateWork then changes nothing).
- A/B: `gstn.Camera.Anchor.Early 1` (default, new) / `0` (old order). `gstn.Camera.Anchor 0` = no anchor at all, as before.
- UGSTN_CameraComponent (GSTNCamera plugin) -> TG_PostPhysics (P0 brief spike C1 answered in code).
- Owner 20:51Z: custom camera API `UGSTN_CameraAnchorLibrary` (BP GSTN|Camera): Get Camera Pivot (capsule- and step-free, undamped),
  Get Damped Camera Pivot (anchor's damping). Designer doc docs/architecture/camera-anchor.md.
- Tests: `GSTN.Camera.Anchor.TicksBeforeTheView`, `GSTN.Camera.Anchor.LibraryNoCharacter`, `GSTNCamera.Runtime.TicksBeforeTheView` (tick group guards).
## Attempt 2 result (owner video 21:38, docs/notes/reviews/camera-jitter-1/owner-video-1.md): FAIL for what the owner sees
Desktop measured 0 kicks (Early 1) vs 13 (Early 0) on jumps, but the owner's video shows a different jitter: the camera height
bounces between two levels (~10-25 cm) every 1-3 frames while standing on flat ground, plus ~20 fps. Rule 5: stop here. Added
diagnostics only: `gstn.Camera.Trace 1` (CSV per frame, Saved/Logs/gstn_camera_trace.csv) + Tools/GASNPPTest/camera_trace_check.py.
## Desktop (next): build, PIE as the owner plays, `gstn.Camera.Trace 1`, stand still 5 s, walk in place, jump; `gstn.Camera.Trace 0`;
run the checker (5 lines) for Early 1 and Anchor 0. The first column with zig-zag kicks names the culprit. `stat unit` for the fps.
COSTO: ~8 $ | 0 minuti desktop | 0 token review
