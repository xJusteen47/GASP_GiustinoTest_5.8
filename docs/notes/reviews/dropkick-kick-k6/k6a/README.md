# Dropkick K6 (laptop, 2026-11-21): review pack k6a, K5 round 3 vs K6

Branch claude/project-thread-jx1fv2. K5 (round 3, PASS) is untouched and is the BASE row of every grid (same cameras as k5r3).
Gameplay timeline and hitboxes untouched (TL_Overlord_DropkickTBK6 = copy of TBR4 with the K6 montage). PIE 1 player.
Built from: python cards/overlord/dropkick/k6/make_k6_cards.py, animpipe author/build/check on startup_k6, settle_k6, loop_k6,
setup_dropkick_kick_variant.py (DROPKICK_KICK_ROUND=K6, DROPKICK_KICK_SETTLE=K6). Curves: overlord_dropkick_k5_curves.md, section K6.

## Owner asks (11-21) and what K6 does
1. Weight, follow-through, secondary motion; arms PRESS in toward the body into the cross, then relax a little:
   a4 press (clavicles +12 deg, wrap 3 cm deeper, chest curls 3 deg; elbows 3-4 cm toward the midline), a8 relax, a11 gather,
   a15 the lying fold clamps, then lets go in the settle. Head lags the tuck at a4 and overshoots it at a8.
2. YES to an overshoot after the strike: new 12 f SETTLE clip = a16-a27 (replaces the first loop repetition, montage still 90 f):
   pelvis carried 5 deg past the lock (peak a17-a18), toes -60, chest out; head flicks past the smug attitude 2 frames later (a20)
   and nods back under it; arms loosen last; rebound a21-a23; = loop f11 at a27.
3. (18:39Z, scope strike + settle + fall) torso BACK and bent, legs FORWARD and never below the body's plane (Big Band j.HK):
   pelvis +8, hips flexed 12, trunk curled 8: legs 6.7 deg ABOVE the horizontal (K5: 13.6 below), hips the lowest point.
Identity traits to check first: legs together and straight from a15, arms crossed (upright a4-a13, lying from a14), smug in the fall.

## Files
- grid_<seq>_<view>.jpg: a0-a21, K5 rows over K6 rows (yellow labels). tiles_*: a4 press, a8 relax, a15 strike, a17 settle peak, 480 px.
- grid_high_settle.jpg: high press, a14-a24 every frame, az90 and az315, K5 over K6 (cameras at 160 cm: the side view crops the head).
- mp4: k5_* / k6_* whiff and hit (az90, az315, game, realspeed and slow4x), k5high_* / k6high_* (press at 700 cm: settle + fall).
  Every mp4 starts at a-20 with "a<N> v<frame>" burned in. The 480 cm press lands ~a19-a20 = settle f3-f4: only the high press
  shows the whole settle and the K6 fall loop.
- assert_lines.txt: ASSERT OK on all three K6 runs, each with its own spec (review k5r3 note: no "empty spec" line).

## Checks (Saved/AnimPipeline/animpipe/overlord_dropkick_*_k6_check.txt)
clearance PASS on the three clips: startup min 0.07 cm (lowerarm_r|spine_03 @ f4, the press), settle 0.16, loop 1.03.
First build failed clearance on settle/loop (-0.10 cm, the trunk curl): the lying fold now rides 2 cm further off the chest.
Other FAILs as K5 (wanted or tool): legibility (matplotlib missing / loop KeyError), contact_faf, hitstop_snap 14->15 (the snap).

## Known, not this variant
- HITBOX (owner's): at the strike the feet are ~34 cm higher relative to the hips than K5.
- Landing clip: forearms spring up one frame before the ground (code/landing queue); Gemini also flags the whiff landing splay.
- Gemini notes not taken: spine wave in the tuck, forearm drag at a14, 24 f loop sway (docs/notes/reviews/dropkick-kick-k5/gemini-k6.md).
