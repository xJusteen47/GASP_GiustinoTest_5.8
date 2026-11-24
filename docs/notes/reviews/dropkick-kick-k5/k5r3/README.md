# Dropkick K5, pose round 3 of 3 (laptop, 2026-11-21): review pack k5r3

Branch claude/project-thread-cqad05 (from uvzm3k 79631bd6). Answers review k5r2b "Round 3". Gameplay timeline and hitboxes untouched.
Built from: python k5/make_k5_cards.py, animpipe author/build/check, setup_dropkick_kick_variant.py run(refresh=True).

## What changed (curves sheet: overlord_dropkick_k5_curves.md, section "Round 3")
- a14 to landing (startup 14-15, loop 0/6/11): LYING fold from k5/fold3.py. Upper arms down along the ribs, elbows out at the sides
  (18-22 cm from the spine, 8-16 cm forward), forearms on the chest, left over right. Offline arm block 11.3 cm above the sternum
  (round 2: 18.7) and below the top of the head in a side view (28 vs 33 cm above the pelvis; round 2: 41 vs 31). Key 14 is a
  transition fitted over its blends (14.6 cm).
- a4-a13 (upright fold, passed): unchanged.
- Head from a15: neck lifted, chin up, turned ~25 deg to his right (toward az90) and tilted ~25 deg; face 35 deg above the horizon
  (round 2: 54, at the sky). Loop f6 adds 3 deg chin and tilt and chest out: a held attitude, not a cycle (loop = 12 f = 0.2 s).
- Build: the animpipe build keeps only the card's pose frames, so a build key at f14 (smear) was added; without it the rig's own
  f11 -> f15 path put the right forearm 0.34 cm into the belly at f14. Clearance PASS, min 0.43 cm
  (Saved/AnimPipeline/animpipe/overlord_dropkick_startup_k5_check/clearance.txt; loop min 0.73).

## Files
- mp4: base / k5 whiff, k5 hit, k5high (press at 700 cm): az90, az315, game, realspeed and slow4x. EVERY mp4 starts at a-20 and has
  "a<N> v<frame>" burned in (review k5r2b capture note).
- grid_*: a0-a21, base over K5. tiles_*: a3, a14, a15, a18 at 480 px.
- grid_k5high_fall.jpg: the fall of the high press (side cameras at 160 cm in that run, the head leaves the az90 frame).
- closeup_loop.jpg: head and arms, 2x crops: whiff az90 a16 and a18, high az315 a20 and a26.
- assert_lines.txt: all assert lines. "FAIL no expectations (empty spec)" on hit and high = the capture ran with no spec (as round 2);
  the offline lines below them (capture_assert.py, window a2 -> landing) are the ones that count: ASSERT OK on all four runs.

## Known, not this round
- a19 of the 480 cm whiff: forearms spring up one frame before the ground = blend into the landing clip (other clip; code/landing queue).
- Overshoot at a15-a17 (OOMPH): not done; startup 16 f and loop 12 f are gameplay timing, an overshoot in the loop pulses every 0.2 s.
  Needs a one-shot segment after the strike (montage change): owner's call.
- Gioco: dummy never reacts, no hitstop (owner sets the hitboxes).
