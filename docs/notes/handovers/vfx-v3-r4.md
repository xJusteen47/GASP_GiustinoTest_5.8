# Handover VFX v3 r4 (2026-11-19, laptop, Opus, brief ~45 min editor + ~4 $ review)
Branch `claude/project-thread-29srbd` from 6610b32b. Captures r7 light + dark (`Saved/VfxV3/r7*`, merged in `Saved/VfxV3/r7pack`), pack
`docs/notes/reviews/vfx-v3/r7/`. **Fable high: FAIL, close** (`r7/REVIEW.md`). Nothing goes to the owner. Stopped: 2 fix attempts used (rule 5).
## Done
- D1-D20 checklist: `docs/notes/stile/stile-sheet-vfx-review.md` section D (copied from `claude/project-thread-f95wnc`, where it lived alone). Now scored.
- Smoke (`setup_presentation_vfx_v3.py`): ShadeMin 0.8, EmissiveLift 0.5, soft rim (RimShade 0.8 / RimWidth 0.35), OpacityMax 1.0 dust / 0.9 light.
  Attempt 1 (0.7 / 0.14) still read mid grey on the light floor (self-check of `Saved/VfxV3/r6/light`, one grid per sheet).
- `setup_presentation_vfx.py`: star life 0.042 s (hitstop slows the system to 0.15 after 2 free frames); Impact 7 puffs 170-240 cm on a wide disc
  + 10 grains; skid = burst of 3 at Skid.Enter + SpawnRate 18 (per unit never fired: r5 log Skid.Enter v128, Exit v143, the v167 puff was the turn's
  RunStart v146); grains 5-8 cm; sprint fringe BLUE (0.05, 0.35, 1).
- Capture: jump 160 frames ("jump: land on v154", r7/light/capture.log); floor 80 m, far wall 45 m, side wall behind the subject; the dummy moves
  away in sequences without one; `_contact()` logs land/bounce, review sheets pick 4 frames around it; `vfx_v3_review.py` argv[4] "seq=v".
  The bounce z test fired in r6 (v105) but not in r7: forced `ground_bounce=105` (2nd GroundBounce is Hit + 74 frames in both run.log.ue.log).
- Removed the 9 unused v2 MIs (Ball, Drop, Flash, FlatBall, Ring, Star4, Star5, Streak, ThinRing) and VFX/Probe/NS_GSTN_Probe: no reference in any
  uasset (grep of the binaries) or script.
## PASS in r7
All 4 hit sheets (star gone after ~1 frame, accepted), landing dust light and dark, run_skid, dust on light is floor-coloured (no soot).
## Remaining (next thread, ~30 min editor + ~4 $)
1. gioco/cattura: no sprint lines in either sprint sheet. StageUp fires (mag 3, r7/light2 run.log.ue.log): check the 450 cm/s gate and that the
   sprint (pawn yaw -180 after run_skid) stays on the floor. A mannequin is still in frame v168-v242: the "moved away" dummy went to x 2100,
   so check whether that is the dummy or a parked player.
2. posa/look: impact cloud is right-sized but one flat blob. Use 5-7 shaded lobes with staggered birth (dark v76 is the target).
3. posa/look: on the dark floor the dust is cream and sticker-opaque. EmissiveLift 0.5 -> 0.2-0.25 and big cloud OpacityMax 0.8, which still has to
   read on light. Consider driving EmissiveLift from the surface brightness. On light: slightly stronger rim, since the puffs are faint.
4. note: on dark a fan of 5-7 streaks covers the victim's legs for 9 frames.
## Owner (2026-11-19)
"A": the smoke-column screenshot is a SHAPE reference (soft, billowy, volumetric), the dust stays the floor's colour. New clip + key frames:
`claude/ref-astrobot-vfx` fc9e5295, `docs/notes/reviews/ref-astrobot-vfx/` (copy used by the review in `r7/ref-clip/`, not committed).
## Laptop notes
Start zenserver by hand before the run. PowerShell runs must `Set-Location` to the root first (a relative `Tools/...` failed from a subfolder).
Cost: ~55 min editor (6 capture runs, 1 stopped), Fable review ~130k tokens; ~10 $ estimated.
