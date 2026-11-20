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
- Tests: `GSTN.Camera.Anchor.TicksBeforeTheView`, `GSTNCamera.Runtime.TicksBeforeTheView` (tick group guards).
## Desktop (Haiku/Sonnet, editor closed while building, ~3 $)
1. Merge `origin/claude/project-thread-hiq1zx` into the playtest branch (touches only Camera files, GSTN_CameraComponent, CombatSimComponent tick).
2. Build with Run-Timed (45 min). Tests: `GSTN.Camera+GSTNCamera+GSTNCombat.NP.BodyCapsule` (expect all pass, 2 new).
3. PIE L_Test_Hub, Overlord: jump in place, run + jump, dropkick, dair stomp landing, crouch in/out. Same inputs with `gstn.Camera.Anchor.Early 1`
   and `0`. Record both (EDITOR_WIN=full, owner hi-res), commit mp4 in docs/notes/reviews/camera-jitter-1/. Expect: 0 jitters, 1 stays smooth.
4. If 1 still jitters: `stat unit` (frame time spikes?) and `gstn.Camera.Anchor.Debug 1` (green sphere smooth?) -> if green is smooth and the
   view is not, GASP's rig evaluates elsewhere: ask the Unreal Assistant "when does a GameplayCameraComponent rig evaluate in 5.9 relative to
   tick groups, and does it read the component's world location each frame?" and log the answer here.
Risks: tick prerequisite cycle if a PostPhysics component of the pawn depends on the camera (none known; would log a cycle warning);
changing the GameplayCamera component's effective tick group if it ticks earlier (harmless, it then reads newer data).
COSTO: ~5 $ | 0 minuti desktop | 0 token review
