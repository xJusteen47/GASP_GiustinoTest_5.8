# Handover VFX v3 hd1: owner 1080p videos + DK Bananza comparison (2026-11-19, laptop, Opus)
Branch `claude/project-thread-ii4go3` from ff43afbe. Owner (phone): "non si vedono per niente, risoluzione bassa"; is our dust at DK Bananza level?
## Videos (owner)
`docs/notes/reviews/vfx-v3/hd1/video/` 10 mp4, 1920x1080 H.264, 60 fps real speed, game camera, digital crop 1.2-1.5x around the fighter
(feet kept), label burnt in: 1 landing, 2 run+skid, 3 medium hit, 4 strong hit, 5 ground bounce, each `_chiaro` / `_scuro`. Sprint lines off.
Built by `Tools/GASNPPTest/vfx_owner_mp4.py` (ffmpeg from pip `imageio-ffmpeg`, installed on the laptop: `pip install imageio-ffmpeg`).
Sources: hd3 (attempt 2) for all but light hits = hd2 (attempt 2 does not touch them). `vfx_contact_sheet.py` makes the review grids (crop arg).
## Capture changes
`vfx_v3_capture.ps1 -Wide 1920x1080 -Views game` (+ `-Close`), `pie_capture_vfx.py` PIE_CAPTURE_W/H, PIE_EXPORT_VIEWS, PIE_CLOSE. The 'close' view
in 16:9 cuts the legs (FOV narrowed on the screen centre): use the mp4 crop instead. Game-only runs: 3 seq 343 s, 5 seq 444 s (hd2-*.out).
## Review (Fable, `docs/notes/reviews/vfx-v3/hd1/REVIEW-DK.md`, baseline + attempt 1 + attempt 2 final)
Baseline: all 5 NOT (white on white on the light floor). Final: landing, skid, strong hit COMPARABLE + readable both floors; medium hit NOT
(close), run start NOT/FAIL, ground bounce NOT (volume ok, cloud at torso height). No soot on light, no khaki on dark. Not a blanket "DK level".
## Applied (2 attempts, limit reached)
- Smoke material: `ShadeTint` (0.78,0.86,1.0) on the shaded side, full on bright floors, half on dark; `DarkSnow`/`SnowColor`: dark-floor
  dust leans 40 % to snow. MIs: SmokeDust/Light/Cloud light shade 0.60-0.66, dark lift 0.7-0.8 (values in `setup_presentation_vfx_v3.py`).
- Sizes/counts (`setup_presentation_vfx.py`): landing 10 x 150-210 cm, grains 12-28 cm everywhere, skid kick 6 x 140-190 wide, run start
  9 puffs flat cone, hit streaks 16x130 cm tilted up, star 240 cm, debris 22-35 cm; cue table Hit count 12, strong_count 7.
## Open (next thread, needs C++ or material rework)
1. gioco: GroundBounce cloud at the floor contact point, not the victim (sits at torso height).
2. gioco: hit star on its own unscaled age (0.083 s) + 180 cm soft orange glow (hitstop slows the system to 0.15).
3. dati/gioco: run start hidden by the runner: spawn 40 cm wide, side puffs +-45 deg.
4. posa/look: round snow-chunk grain sprite (now 4-point stars), debris shadow neutral grey; dark dust a bit flat/opaque (try dark lift
   SmokeDust 0.65, SmokeLight 0.6, dark OpacityMax 0.85); floor contact darkening; skid as a low wedge.
5. Jump.Ground logs `niagara=-`: NS_GSTN_JumpPuff is not in the cue table (check if intended).
Cost: ~45 min editor (7 runs, all ESITO OK, Saved/VfxV3/hd*-*.out), 3 Fable passes (~150k), ~10 $ estimated.
