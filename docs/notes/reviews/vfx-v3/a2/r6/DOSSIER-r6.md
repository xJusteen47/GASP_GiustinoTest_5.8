# Reference dossier, landing dust A2 r6 (2026-11-19)

Owner on r5: "still looks fake" + DK Bananza clip (owner_clip_2059.mp4). Breakdown and ranked causes (Italian):
`docs/notes/vfx-studies/dk-clip-2059/studio_dk_clip_2059.md` (images in the same folder). Causes: 1 dust unlit (shading painted in
sprite space), 2 a single layer (DK: light flash, shock ring, sparks, stretched air jet, debris, motes), 3 too uniform (same frame,
perfect ring), 4 no motion stretch early, 5 clean flipbook.

r6 targets (this round): fix 2 (impact light flash), 3 (grit debris layer), 5-variation (staggered births, mixed sizes, disc spawn),
then fix 1 (dust lit by the scene). NOT in this round: streaks/motion stretch (4), richer flipbook (6).

Owner rules (scope):
- Realistic dust (every dust event): one burst from a central impact pushed radially along the floor; grows while fading to alpha 0;
  NEVER shrinks. Owner 2026-11-19 21:02Z: this rule is for smoke/dust ONLY; solid grit may shrink and vanish at the floor.
- Size max = skirt as wide as the shoulders and below the knee (`../owner_size_target.png`, A1b early frame).
- Colours: r3 values kept on purpose (owner decisions 1-2 of `../r3/REVIEW-A2r3.md` still open): do not fail on hue alone.
- Perf target Switch 2-class: layers must stay cheap.
- Owner smoke reference IMG_9543: `docs/notes/vfx-studies/fumo-owner-IMG9543/` and `../r5/DOSSIER-r5.md` (key facts there still hold).
- Checklist D1-D20: `docs/notes/stile/stile-sheet-vfx-review.md` section D.

Previous round (baseline, Fable PASS narrow, owner "still fake"): `../r5/REVIEW-A2r5.md`, sheets `../r5/consec_r5d_{dark,light}_f154-177.jpg`,
`../r5/side_r5d_*`, `../r5/zoom_r5d_*`; frames Saved/VfxV3/a2r5d/{dark,light}/jump/{game,side}/fNNN.png.

r6 build (captured = b2): frames Saved/VfxV3/a2r6/b2/{dark,light}/jump/{game,side}/fNNN.png, 1920x1080, 60 fps, touchdown v156 dark /
v155 light. Sheets here: `consec_r6b2_*` (game view crop, f153-176), `side_r6b2_*`, `zoom_r6b2_*` (feet, f155-166).
`consec_r6b1_dark` / `zoom_r6b1_dark` = first pass (flash 3x weaker, grit white and floating), kept for the record.
Changes: Flash = Niagara light, 220 cm, 4 frames, warm; Skirt 13 puffs 16-40 cm + Lumps 5 puffs 30-50 cm born 1.5 frames later; grit 9
dark lit pieces, ballistic; dust material Default Lit translucent, Volumetric Directional with the flipbook normal (Unreal Assistant:
`ASSISTANT-lit-dust.md`). Same shockwave, drag, fade and size cap as r5.
