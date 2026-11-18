# Handover VFX v3 "Astro Bot semi-realistic" r1 (2026-11-18, desktop _ue59, Opus medium)
Branch `claude/vfx-v3` from `claude/project-thread-98dml5` 5b22e56e (contains v2). Brief `handoffs/vfx-stile-v3-categorie.md`, owner refs
`docs/notes/reviews/ref-astrobot-vfx/` (astro_1-13 + NOTE.md, from claude/ref-astrobot-vfx 570d4ead). Design sheet and plan:
`docs/notes/stile/vfx-stile-v3-design-sheet.md`. Budget 45 $ (up to 52), coordinator OK.
**State: built, NOT yet seen in game.** Both captures crashed the editor (capture tooling bug, found and fixed, see below): rule 5,
2 attempts used, stopped. No video, no Fable review yet.
## Done
- Textures by script `Tools/GAS/vfx/make_vfx_v3_textures.py` (plain python, numpy/PIL): Star (8 rays, white core R, fringe G), Streak
  (vertical, head at the top for velocity-aligned sprites), Smoke (2x2 puffs: density R, self-shading G, erosion B).
- `Tools/GAS/setup_presentation_vfx_v3.py`: M_GSTN_VFX_Flash (Unlit Translucent, white core + saturated fringe, Eye Adaptation Inverse
  x Brightness), M_GSTN_VFX_Smoke (Lit Translucent per-vertex volumetric, Depth Fade, random atlas cell, erosion), M_GSTN_VFX_ContactShadow,
  8 instances, capture-only M_GSTN_VFX_CaptureBackdrop. Setup log `Saved/Logs/ci/v3-setup.txt` (8 instances, 0 exceptions).
- `setup_presentation_vfx.py`: the 8 systems rebuilt (0 errors, `Saved/Logs/ci/m2-vfx-setup.txt`): hit = star + 4/6/8 streaks + 6 sparks
  (+3 small debris on strong); landing/impact/run/skid/jump = lit smoke + grains in User.SurfaceColor; sprint = thin cyan-white lines;
  tumble = dotted sparks. No tori, rings or glossy balls left.
- C++ `UGSTN_PresentationComponent`: User.SurfaceColor from the floor's Physical Material surface type (SurfaceColors map, default
  DefaultSurfaceColor), cvar `GSTN.Presentation.SurfaceColor r,g,b`; contact shadow = its own transient AStaticMeshActor (disc under the
  feet, 1 trace per SubmitSnapshot, shrinks/fades with height, destroyed in EndPlay). PhysicsCore dependency. Build `v3-build4.log` OK.
- Capture `pie_capture_vfx.py`: PIE_BACKDROP=light|dark (no-collision floor + far wall), PIE_SURFACE_COLOR (via the cvar), PIE_CAPTURE_SIZE.
  Launcher `Tools/GASNPPTest/vfx_v3_capture.ps1` (pie_capture_timed.ps1 runs pie_capture_MOVE, not this). Review pack `vfx_v3_review.py`.
## The crash (for the bug-checklist thread)
`Saved/VfxV3/r1/light/run.log.ue.log`, `r2/light/run.log.ue.log`: at the first sequence frame the driver set `default_surface_color` on
the pawns' presentation component with `set_editor_property` in Play -> the BP pawn reran its construction script (log: "table set
revision 0", "AttachTo: cannot attach root component GSTN_KitPlaceholderLook") -> ensure `ClientID < INDEX_NONE && ServerID >= 0`
(NetworkPredictionWorldManager.h, FNetworkPredictionProxy::Init lambda) x3 -> fatal `Index>=0 && Index<NumBits` in
`UNpAbilitySystemComponent::EndPlay` -> UnregisterInstance<FAbilitySystemModelDef>. Fork bug: a construction-script rerun of a live NP pawn crashes.
Fix on our side: the cvar (never write properties of live pawns from Python in Play).
## Next (new thread, ~25 min editor + ~8 $ Fable)
1. Capture light + dark: `Tools/GASNPPTest/vfx_v3_capture.ps1 -Out Saved\VfxV3\r3\<bd> -Backdrop <bd> -Surface ... -Seq hit_debris,hit_medium,ground_bounce,jump,run_skid,sprint`.
2. `python Tools/GASNPPTest/vfx_v3_review.py Saved/VfxV3/r3 docs/notes/reviews/vfx-v3/r3 docs/notes/reviews/ref-astrobot-vfx`: mp4 60 fps per view + sheets "astro_N | 8 game frames".
3. Look at it first (Opus), then Fable high: astro_N side by side FIRST, real speed, light and dark, then checklist D1-D20.
## Choices applied (rule 6)
Jump = 3 small light puffs (the ring had to go; owner's "dust only on run start and skid" conflicts: owner to confirm). Hit star rotates
randomly. Debris kept in the kit's accent colour (not the floor's). Hub has no surface types: default colour per level until mapped.
Cost so far: ~30 $ (estimate).
