# Handover smoke redo A1: landing dust look test (2026-11-19, laptop, Opus high)
Branch `claude/project-thread-ii4go3` from f5f3d7a2. Brief `docs/notes/reviews/vfx-v3/hd1/REVIEW-SMOKE-LOOK.md` option A, owner 1A.
## Built
- `Tools/GAS/vfx/make_vfx_puff_flipbook.py` -> `T_GSTN_VFX_PuffFB` 8x8, 4 variants x 16 frames: hard union of spheres, RG normal (y up),
  B crease occlusion, A crisp density; lobes drift, turn, die one by one. Needs `pip install numpy` (done on the laptop).
- `Tools/GAS/setup_presentation_vfx_puff.py`: `M_GSTN_VFX_Puff` (candidate (b) self-lit: unlit, wrap shading from the normal against
  `LightDir` in sprite space, cool `ShadeColor`, occlusion, `DarkBrightness`/`Brightness` by floor, DarkSnow, pivot shrink, depth fade 12),
  `M_GSTN_VFX_ContactDisc`; MIs PuffDust, PuffClump, ContactDisc. Candidate (a) lit TLM_SURFACE NOT built.
- `setup_presentation_vfx.py` `_puffs` + `land_ring`: 16 skirt puffs 25-55 cm, 8 wisps, 8 clumps (gravity), contact disc 220 cm, curl
  noise, view-depth sort, no sprite rotation. `setup_presentation_vfx_landring.py` rebuilds only that system (capture -Pre).
- `Tools/GASNPPTest/vfx_sbs_mp4.py`: 2x2 owner video, real speed then x0.25.
## Results
Video `docs/notes/reviews/vfx-v3/a1/video/A1_atterraggio_vecchio_nuovo_DK.mp4` (old light, new light, new dark, DK clip d0092+).
Fable high review + critic `docs/notes/reviews/vfx-v3/a1/REVIEW-A1.md`: light = clear step toward DK, dark = partly (fixed in the one
fix round, end of that file). Captures `Saved/VfxV3/a1/{light5,dark5}` (`a1-*.out` ESITO OK, ~3 min each).
## Traps
- TextureSample / ParticleColor default output is RGB: a ComponentMask on A fails the whole material ("Failed to compile ... Default
  Material" in run.log.ue.log, black quads). Use the "A" output pin.
- Laptop with the editor open has ~1 GB RAM free: quit it (MCP `quit_editor`) before a capture run.
- Unreal AI assistant: owner asked to consult it; the screen-control request for the editor window was declined/unanswered, and the
  unreal-mcp server has no assistant tool. Not consulted.
## Open for A2
`LightDir` is fixed in sprite space (gioco: set it from the sun in the presentation component); sprite rotation off because of that;
size/count dial after the owner's look; rest of the dust kit still uses M_GSTN_VFX_Smoke cards. Cost estimate: ~9 $ (Opus) + Fable review.
## A1b realistic round (owner on the A1 video: "size ok, too stylised; DK is realistic"; "fade to alpha 0 while growing")
`make_vfx_smoke_flipbook.py` -> `T_GSTN_VFX_SmokeFB` (soft billows, same channels) drawn by the same material through MI `SmokeReal`
(grows over the whole life, fades from 15 %); `land_ring` skirt/wisps use SmokeReal, sprites 60-120 cm (the soft shape fills ~half the
frame), life 0.5-0.95 s, slight rise. Capture `jump` extended to 200 frames (1 s after landing). Fable `REVIEW-A1b.md`: partly closer to
DK; flaw 1 (opaque core shrinking) fixed; open: flat lighting/uneven billows (posa), 16 equal balls on a ring and wisps as "ears" (dati).
Video `video/A1b_atterraggio_stilizzato_vs_realistico.mp4`. YouTube ref not analysed (Gemini: metadata only; download blocked here).
Stylised look kept as MI PuffDust. Cost: A1 ~9 $ + A1b ~6 $ (Opus) + 2 Fable reviews.
## Owner verdict 2026-11-19 15:33 (scope: landing dust look)
"Bella, mi piace" on A1b -> A1b (realistic, MI SmokeReal) is the base for A2. "Probabilmente troppo grande rispetto alla dimensione del
personaggio l'entità dell'effetto (la polvere)": the dust's overall extent is too big for the character. A2 start values (to try):
skirt sprites 60-120 -> 40-80 cm, ring 36 -> 26, speed 560 -> 420, wisps 25-50 -> 18-35 cm; then Fable flaws 2-3 of REVIEW-A1b
(shading Wrap 0.25 / RampLo 0.2 / RampHi 0.65 / OccStrength 0.8 / ShadeColor (0.60,0.63,0.72); flipbook k 14, blur 2, noise 0.42,
dens 0.55 + 0.75 billow; skirt 10 puffs uneven ring). Then convert the rest of the dust kit (REVIEW-SMOKE-LOOK A2).
