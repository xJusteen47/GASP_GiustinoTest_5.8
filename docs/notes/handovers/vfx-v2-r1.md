# Handover VFX v2 "Astro Bot" (2026-11-18, laptop + UE 5.9 editor, Opus medium, Fable 5.2 high review)
Branch `claude/project-thread-aait2t` from `claude/project-thread-vcepyw` e7757181. Brief `docs/notes/stile/vfx-stile-v2-astrobot.md` and style sheet `stile-sheet-vfx-review.md` (branch f95wnc); owner 1A, 2A, 3A.
**Fable r3: PASS on everything the captures show** (`docs/notes/reviews/vfx-v2/review-fable-r3.md`); r1 and r2 FAIL, 2 fix attempts used.
## Done
- Meshes by script, no Geometry Script: `Tools/GAS/setup_presentation_vfx_meshes.py` writes OBJ, imports Ball 88, TorusZ/TorusX 96, Gem 64, Capsule 48 tri into `/GSTNPresentation/VFX/Meshes`; checks axes (bounds) and winding against the engine sphere.
- Materials: `setup_presentation_vfx_toy.py`: `M_GSTN_VFX_Toy` (Default Lit, Masked, size over life by WPO, Fresnel rim, flat energy emissive) + `M_GSTN_VFX_ToyShadow` (flattened on the floor, dark blue) + 6 looks. `MF_GSTN_Rim` does not exist on this base: Fresnel built inline.
- Systems (`setup_presentation_vfx.py`): all 8 on mesh renderers; only the hit flash is still a sprite. Shared by every kit (rebuilt in place, same table).
- C++ `UGSTN_PresentationComponent`: `HitstopVfxTimeScale` 0.15 (after `HitstopVfxFreeFrames` 2), one-shots only; ground plane: one trace per event sets `User.GroundPoint/GroundNormal/GroundHeight`; cue fields `TowardCamera` and `SustainMinSpeed`. Library: `AddUserVector`, `AddMeshRenderer`, `MeshWindingSign`.
- Bounces: stock Collision module, CPU "Analytical Planes", restitution 0.55, max 2. Debris = strong hits only (3 accent gems, ~0.87 s).
## Choices applied (rule 6)
- Owner playtest: hit +45 cm and wall/splat +30 cm toward the camera; sprint lines stop under 450 cm/s (the dots on a walk); "dust only on run start and skid" read as locomotion: jump keeps its ring, no balls; landing/impact balls kept (coordinator's reading).
- The hit has 4 emitters (flash, ring, shards, debris), 3 mesh: over the cap of 2 only on a strong hit (debris empty otherwise). Persistent debris pool (24) NOT done (only for 3B). No low preset, no mesh LODs (~30 tri). Erosion dropped on meshes: all pieces pop out by shrinking.
- Shadows go straight down world -Z, not `GSTN_DownAxis`.
## Open
- Not captured: second kit, hit S/M/L, shaded hub spot, skid vs Toad ref, knockdown, jump take-off, far camera, hitbox range. Owner playtest.
- Game/capture, not VFX: victim hidden behind the attacker in the game camera; wall sequence hangs 30+ frames with trail balls still (trail is not slowed by the VFX code: game hitstop or replay). Dark flash seen once in r2 from the game camera, gone in r3: watch the first hit of a session (PSO?).
- Optional (Fable): Gem MidTime 0.3 -> 0.1 if shards look pale in the hitstop.
- Editor quits hang after a boot script: Run-Timed ends BLOCCATO; use -SilentMinutes 2 for setup boots. `Tools/GASNPPTest/vfx_frames_sheet.py` is a stray untracked copy (rm blocked): delete.
## Proofs (rule 11)
- Build `Saved/Logs/ci/v2-build3.log.esito` OK; setup `Saved/Logs/ci/v2-setup.txt` (5 meshes 0 wrong, 8 systems 0 errors, 13 table rows).
- Captures `Saved/PolishV2/r{1a,1b,1c,2a,3a}/vfx1/capture.log` (`finish: ok`); grids `docs/notes/reviews/vfx-v2/r{1,2,3}/`.
Cost estimate: ~38 $ Opus + ~9 $ Fable.
