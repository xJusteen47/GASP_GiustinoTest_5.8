# Reference dossier, landing dust A2 r5 (2026-11-19)

Owner references and rules, in priority order:
1. Owner reference video IMG_9543 (snow landing, DK-Bananza-like game, phone recording of a TV, 30 fps). Breakdown in Italian with the
   9-point table: `docs/notes/vfx-studies/fumo-owner-IMG9543/studio_fumo_breakdown.md`; frames: `zoom_B_f74-f100.jpg` (landing B, contact
   f77-78), `zoom_A_f44-f67.jpg` (landing A, contact f47), `keyframes_annotati.jpg`.
   Key facts: born on the contact outline (feet/hands), pushed radially and flat; 6-8 lobes ~1/3-1/2 torso; almost all travel in the first
   ~0.07 s, nearly still by ~0.15 s; lobes grow ~1.5x and never shrink; nearly opaque to ~+0.2 s then gone in ~0.15 s; total 0.4-0.45 s;
   lit tops ~floor value, shaded sides/base darker and cooler than the floor; round lumps, no hair; separate lobes merge into one ring.
2. Owner rules for realistic dust (scope: every dust event): one burst from a central impact pushed radially outward along the floor
   (not independent puffs, r4/r3 feedback); grows while fading to alpha 0; NEVER shrinks (2026-11-19, measured:
   `Tools/GASNPPTest/vfx_size_monotonic.py`, result `size_r5b.txt` here); size max = a skirt as wide as the shoulders and below the knee
   (A1b early frame, `../owner_size_target.png`).
3. Colours: r3 values kept on purpose (owner decisions 1 and 2 of `../r3/REVIEW-A2r3.md` still open): do not fail r5 on hue alone.
4. Checklist D1-D20: `docs/notes/stile/stile-sheet-vfx-review.md` section D. Previous review: `../r3/REVIEW-A2r3.md`, critic `../r3/CRITIC-A2r3.md`.

r5 changes (r5b = the captured build): push origin fixed (Origin Offset; probe Saved/VfxV3/a2r5/probe/capture.log: skirt v ~(300,0,52),
vz/vh 0.15), flipbook erosion removed (no shrink), clumps fade, ring 8 -> 12 cm, speed 600 / drag 10, SmokeReal FadeStart 0.45, flipbook
threshold 0.24. Captures: Saved/VfxV3/a2r5b/{dark,light}/jump/{game,side}/fNNN.png (1920x1080, 60 fps, touchdown ~f157).
