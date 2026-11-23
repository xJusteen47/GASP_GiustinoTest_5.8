# surface feedback 1 (cloud, 2026-11-21): haptics + footwear x floor footsteps, C++/data side done, editor steps listed
Branch claude/project-thread-c2dfra from claude/runstart-generic-desktop 8cb52ea7. Brief: coordinator note (cloud, no editor, ~6-8 $).
Design and desktop steps: docs/notes/surface-feedback.md. Research: docs/notes/research/{haptics,surface-audio}.md on claude/project-thread-w299nb.
## Done (not compiled: no engine in the cloud)
- GSTNPresentationCore: UGSTN_SurfaceResponseTable (event x footwear x floor -> sound + rumble), UGSTN_SurfaceFeedbackComponent
  (floor trace, footwear, MetaSound inputs, dynamic rumble / Force Feedback asset, local pad only, 0.12 s same-foot dedupe),
  Presentation.Footwear.{Clay,Metal,Barefoot,ShoeSole} native tags, presentation component forwards accepted events (bSurfaceFeedback).
- Skin: UGSTN_SkinDefinition::Footwear; UGSTN_SkinComponent pushes it (GSTNCombatGAS now depends on GSTNPresentationCore).
- 6 cvars gstn.Audio.* / gstn.Haptics.*, listed in GSTN Debug Options (one control each), test extended.
- Tests GSTNPresentation.Core.Surface.Lookup / ParentLoop. Editor script Tools/Audio/create_surface_feedback_assets.py (8 rows, rumble only).
- DefaultGame.ini ProjectDefaultTable; divergence row 24 (stock foley hook, node added on desktop).
## Applied choices (rule 6)
- Rumble = dynamic pulse by default (works with no asset, scales with intensity); FFE asset optional per row.
- Footwear as a tag (not enum): new footwear = ini leaf + FootwearParamOrder, no code. Floor = EPhysicalSurface.
- Clay/no row = stock sound untouched; rumble independent of the sound switch.
## Open
- Desktop steps 1-9 in surface-feedback.md (build, script, Assistant question, hook node, MetaSound). Risk: PlayDynamicForceFeedback
  native overload name in 5.9 (one line). Hit rumble side and landing double: listed there for the owner.
COSTO: ~5 $ | 0 min desktop | 0 token review
