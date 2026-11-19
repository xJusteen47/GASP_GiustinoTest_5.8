# Handover smoke A2 r2 (2026-11-19, laptop, Opus high) - STOPPED at usage limit, not reviewed
Branch `claude/project-thread-inmtkm` from bad249b4 (ii4go3). UE 5.9 already installed and built on the laptop (BuildId 60351404), no plugin errors.
## Done
- Scripts: `setup_presentation_vfx.py` land_ring (Skirt max 90->65 cm, ring 30, life 0.40-0.50, rise 3, origin +6 cm over floor; Wisps 45-65 cm);
  `setup_presentation_vfx_puff.py` SmokeReal/SmokeHaze colour (DarkSnow 0.4, DarkBrightness 1.15, Brightness 0.8, cool LitColor, ShadeColor 0.40,0.40,0.45),
  owner idea EdgeTint (soft brownish edges, new material term, (1,1,1) = off), ContactDisc soft rim (1-e)^2 and MaxOpacity 0.10, PuffClump DarkSnow 0.3.
- `vfx_v3_capture.ps1 -Vfx 0` (VFX-off pass); `vfx_dust_measure.py` (on/off pixel diff: size, dust/floor value) - written, not yet run.
## Captures (Saved/VfxV3, 1920x1080, side+game, seq jump, GASP look via `-Pre hub_gasp_look`, hub umap restored after)
a2r2b/a1b_dark, a1b_light (A1b baseline under GASP light), off_dark, off_light (VFX off), a2_dark/a2_light (first r2 pass: too dark, vanished);
a2r2c/a2_dark, a2_light (second pass, current assets) - NOT looked at yet.
## Findings
- Decision A ineffective: light backdrop albedo 0.88 -> 0.62 still exposes at ~215/255 (auto exposure). Measured f161 floor crop: off_light 215,217,222 vs r1 213,215,220.
- b6e798c5 L_Test_Hub.umap does not load on the laptop 5.9 build either ("Impossibile trovare il mondo"): it was saved by the desktop preview build.
- First r2 pass: contact disc read as a black hole on dark (VFX-only, absent in off pass).
## Next
Look at a2r2c grid vs a1b, run vfx_dust_measure (box 660,700,1140,960), then blind critic + Fable high review; owner mp4 only on PASS.
Max 1 more colour attempt this round (2 used). Cost so far ~12 $ estimated.
## Resume 18:15 UTC: second pass checked (grid of a1b vs a2r2c, game view f160-f175, looked at once)
- Dark: dust now reads as a low blue-grey skirt at the feet, lighter than the floor, size well under A1b. Contact disc still slightly dark at f160.
- Light: dust nearly invisible again (only grit and a faint haze); A1b on light reads as clear grey smoke (larger, denser, longer).
- `vfx_dust_measure.py` numbers (Saved/VfxV3/a2r2c/measure_*.tsv, box 660,700,1140,960, thr 8) are NOT usable: the off pass's character
  pose/position differs from the on pass, so the diff mask includes the body (bbox stuck at the box size). Needs a mask that excludes the
  character (e.g. diff only below the ankle line, or same-run on/off frames).
- Two colour attempts used (rule 5). Proposed next: light floor only, more density (OpacityMax 0.75 -> 0.9, FadeStart 0.05 -> 0.2) and
  Brightness 0.8 -> 0.65, then the blind critic + Fable high review. Needs ~5 $ more (budget 15 $ reached).
## Pass 3 + review (round 2 of 3): FAIL
Pass 3 (light floor): Brightness 0.65, OpacityMax 0.9, FadeStart 0.2 -> captures Saved/VfxV3/a2r2d. Pack + reviews in docs/notes/reviews/vfx-v3/a2/r2/
(grids rows A1b/A2r2/OFF, zooms, CRITIC-A2r2.md blind Opus, REVIEW-A2r2.md Fable high).
- Closed: hard dark rectangle (the black oval is the character's own shadow, in the OFF pass too); late rise closed in part (top 76 px, gone by +20).
- Gap 1 dark: dust L 84-99 on floor L 32 (2.7-3.1x), neutral grey B/R 1.20 vs floor 1.45 -> white steam (critic agrees). Bracket DarkSnow 0.22/0.30 at DarkBrightness 1.0, target L 55-65, B/R >= 1.3.
- Gap 2 light: grey blot +0..+8 merged with the shadow, area 4-5x smaller than A1b, brown edges not visible. Warm ShadeColor (0.42,0.39,0.36), EdgeWidth 0.35 -> 0.6.
- Gap 3 height: top 68-76 px vs A1b early 48-53. Skirt max 65 -> 50 cm, ring 30 -> 36, or origin +6 -> 0.
- Round 3 = last before the owner decides (rule 10). Owner decision pending (recommended B): light floor A) accept a faint read, B) allow a darker warmer smudge.
- Round 3 must add: a real-speed mp4 (vfx_owner_mp4.py) for the reviewer; a body-free dust measure (OFF pose drifts +/- 5 px).
