# Handover VFX v3 r3 (2026-11-18, laptop, Opus, coordinator extension +5-8 $)
Same thread as `vfx-v3-r2.md`, pushed as `claude/project-thread-ltu6q4`. Captures r5 light + dark (`Saved/VfxV3/r5*`, pack
`docs/notes/reviews/vfx-v3/r5/`), **Fable high: FAIL but close** (`r5/REVIEW.md`). Nothing goes to the owner. Stopped: context and budget.
## Done in r3
- "White ring + dashed vertical line" (r4 review) = the GSTNDepthMarker airborne marker (gameplay feature), not a VFX row: no change.
- `setup_presentation_vfx_v3.py`: SmokeDust OpacityMax 0.85, SmokeLight 0.7 / StartScale 0.6, ShadeMin 0.4, EmissiveLift 0.06; ORANGE (1, 0.22, 0).
- `setup_presentation_vfx.py`: run start 5 puffs 110-160 cm at 150 cm/s; skid 95-135 cm every 20 cm; landing 110-150 cm at 260; star 210 cm;
  streaks 12x85. Rebuilt in the r5 light run (log: "8 instances", "9 systems, 0 errors").
- Capture: hit_debris uses KIT_Example_Impacts (the KIT_Test_Hits jab is 31 hitstun frames at any percent, its strong misses: r4/r5 logs
  "Event.Hit ... mag="); jump camera follows up (`follow_z` 0.6, dist 700); ground_bounce dist 900; light backdrop 0.95.
- `vfx_v3_capture.ps1 -Pre a,b,c` takes several setup modules.
## PASS in r5
light/dark hit_debris, dark hit_medium (star character-sized, orange fringe, tiers differ, shards + puff on the strong hit), dark run_skid (notes).
## Remaining fixes (next thread, ~45 min editor + ~4 $ review)
1. look: smoke on the light floor reads as soot (dark grey). ShadeMin 0.4 -> 0.65-0.7, EmissiveLift 0.06 -> 0.12; contrast from opacity and
   a soft darker rim instead. Dust on dark floor still thin: on-screen opacity +1/3, add grains.
2. look: strong-hit / bounce cloud = 2-3 blobs of 20-40 cm: 5-8 sprites of 120-180 cm, low and wide, plus grains (NS_GSTN_Impact).
3. look: strong star stays near full size v35-v44 and hides both torsos: 30 % by frame 4, zero by frame 6 (FlashStar life/shrink, or the
   hitstop slowdown in UGSTN_PresentationComponent stretching it). Sprint lines have no blue fringe.
4. dati: skid dust shows at recovery (v167), none in the slide pose (v142): check the Skid.Enter event timing vs the skid pose.
5. cattura: light_jump ends airborne at v104 (frames 100 -> 160); dark mannequin invisible on the black sky (add a far backdrop wall on the
   side camera's axis); bounce sheet never shows ground contact (later picks); no sprint lines over the dark floor (450 cm/s gate?);
   second mannequin overlaps the run-start puff.
6. Owner (2026-11-18, "si A"): keep the landing dust, LIGHT. Not judged yet: no landing in the sheets (fix 5 first).
D1-D20 never scored: neither reviewer had the checklist path (the r1 design sheet names it; give it in the brief).
## Laptop notes
Zen DDC server may not start inside Run-Timed: start zenserver by hand first (see r2). 3 sequences per run, -Max 15. Cost r2+r3 ~20 $ (estimate).
