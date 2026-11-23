# Jab 1 + Jab 2 director round 1: brief for the animator

You are the animator. I am the animation director. Character: the Overlord, a heavy, arrogant villain. Stylised action
game in the Smash Bros family, camera mostly BEHIND and above the character, 60 fps, UE mannequin rig. Same pipeline and
way of working as `../../dropkick/director/` (read its `author_variants.py` and `strip.py`: reuse, do not edit them).

## The owner's new key poses (the base; read them first)
- `owner_keys_2026-11-15b.json`: every rig object at his Step keys, world cm, Cascadeur axes (Y up, +Z forward, the
  character's RIGHT is -X), rotations as 3x3 matrices. Includes finger joints, so the hand orientation is in there.
  His scene is 30 fps and the key numbers are only an ORDER, not the game timing. Ignore frame 140.
- `owner_keys_author.json`: the same keys converted to card/author space by `casc_to_author.py` (rig ratios 1.0).
  It carries no pelvis horizontal shift and no hand rotation: take those from the raw file.
- `owner_keys_2026-11-15b.png`: stick views (front / side / top). Redraw your own if you need other angles.
- Jab 1 = keys 0, 1, 5, 7. Jab 2 = keys 16, 19, 22.

What the keys say (my reading, measured):
- O0 idle. O1 wind-up: whole body shifted 20 cm to the LEFT, right hand carried across to the left side at jaw height
  (x +53, y 124), palm turned away, weight on the left foot. O5: right arm sweeping through the FRONT at shoulder height
  (hand 55 cm ahead), back of the hand leading, right foot lifted 15 cm. O7: arm fully extended out to the RIGHT
  (hand x -81, y 133), body travelled 17 cm right and 12 back, right foot planted wide (x -42). It is a big horizontal
  BACKHAND slap, left to right, with a step to the right.
- O16: pelvis down 16 cm, right knee lifted and reaching forward (foot 34 cm up, 30 ahead), right hand cocked back by
  the right hip, left arm thrown out left as counterweight. O19: deep lunge (pelvis 25 cm below idle, 23 cm forward),
  right foot planted 37 ahead, right PALM thrust forward at chest height (hand 79 cm ahead), fingers pointing right,
  palm to the target. O22: the arm has swept across to the LEFT (hand x +38), chest leaning in and forward, left arm
  counter-swung to the right.

## Identity (owner's; do not change, tell me if a pose cannot honour it)
Jab 1: (1) wide horizontal backhand, left to right, ending with the arm fully open to the right; (2) the wind-up is
immediate and big: hand across at the left jaw, weight left; (3) the right foot steps out right and the weight follows.
Jab 2: (1) low lunge STEP FORWARD on the right foot, the body really drops; (2) open-palm thrust forward, then the arm
carries across to the LEFT; (3) the recovery pulls the body back a little (not all the way).
Open hands in both (slaps, never fists). His key poses are hit exactly in variant A; B and C may push them (more twist,
more drop, more reach) but must stay recognisably the same poses from behind and from the side.

## Fixed game facts (owner places hitboxes and active frames: never touch `anim_active`, `faf_game`, hitboxes, timelines)
- Jab 1 card: frames 0-28, active frames 6-8, cancel (FAF) at anim f22. Enters from standing idle; frame 0/1 must already
  be a big readable change. The arm must be sweeping through the front on f6 and the slap must still be travelling
  through f8 (the game freezes on hit somewhere in 6-8: every one of those frames must be a strong pose).
- Jab 2 card: frames 0-52, active 9-11, FAF at anim f26. Chained from jab 1: jab 2 f0 = your jab 1 pose at f12. The palm
  must arrive on f9 and the sweep to the left happens through f11 and after.
- Everything in the move's character must happen before the FAF; after it, a calm return to idle.
- Schema: `../overlord_jab{1,2}_v3.json` (schema ONLY: those poses were rejected, legs broken and stiff follow-through;
  do not reuse them or anything in `../owner_r3/`). Solver: `animpipe/author.py`. Key every frame where timing matters,
  `part_delays` 0, overlap written into the keys.
- Jab 2 forward travel is ROOT MOTION (owner's decision): author the root's forward path as its own per-frame track
  (`root` in the card, cm forward per frame) and express the poses relative to it; feet planted in WORLD space must not
  slide. Check whether author.py / the build can already carry a root track; if not, say exactly what is missing in your
  answer and still deliver the track in the card. Recovery pulls the root back a bit (your choice how much).
  Jab 1: no root motion; the side step is in pose space and he ends where his feet are (state the final offset).

## What I want (intent; you choose the numbers)
- TIMING CONTRAST: fast into the wind-up, a short squeeze, then the strike covers most of its path in the last 2 frames
  before contact. No even spacing anywhere.
- CHAIN: feet/hips first, then chest, shoulder, elbow, wrist; each 1-2 frames after the other. The wrist FLICKS through
  last (it is a slap). Head stays on the target, stabilises before the chest.
- FOLLOW-THROUGH: the hand keeps travelling past contact on its ARC and decelerates; a small overshoot, a recoil, a
  settle. Never a statue, never a pump. Jab 1's open-arm end pose (O7) is a pose of contempt: let it breathe.
- WEIGHT: the steps are real steps (heel/ball contact, the body drops onto the foot and gives a little). Centre of mass
  over the support foot before the other lifts. Jab 2: the drop into the lunge powers the palm.
- LEGS: natural. Knees over toes, no locked or inverted knees, no feet sliding, pelvis never floating. This is where the
  last attempt failed: look at your legs frame by frame from the side AND from the front.
- ARCS everywhere, no straight lerps, no pops. Left arm always alive as counterweight, never covering the right hand
  from behind.

## Three variants (same identity, different acting): folders `director/A`, `director/B`, `director/C`, both jabs in each
- A "Clean": the owner's poses exactly, classic Smash economy. Tight 2-frame snap, small overshoot, quick settle.
- B "Whip": the most elastic. Hips lead clearly, big wrist lag and flick, biggest overshoot and recoil. Jab 2: the palm
  keeps leading the sweep to the left (forehand feel), deeper drop.
- C "Tyrant": heavy and contemptuous. A beat of menace in the wind-up, heavy step that lands with a give, little
  overshoot but a long weighty settle; jab 2 pulls back slowly, looking down at the target.

## Deliver per variant
1. `overlord_jab1.json`, `overlord_jab2.json` (full cards, same schema, plus the `root` track on jab 2).
2. `curves.md` (max 25 lines): per phase timing, easing, overshoot size, who lags whom by how many frames, hand spacing
   per frame around contact (cm), root path per frame for jab 2.
3. `strip_jab1.png`, `strip_jab2.png`: EVERY frame up to the FAF + 4, three rows per strip: side (forward right), BACK
   (camera behind), top. Thick limbs, right arm in a distinct colour, frame numbers, ground line, world space (root
   motion applied). Cells about 200 px, max about 2800 px wide (wrap rows if needed).
4. `onion_jab1.png`, `onion_jab2.png`: onion skin around the strike (jab 1 f2-f12, jab 2 f4-f15), back and top views,
   with the paths of the right hand, pelvis and head drawn.
Write your own script in `director/` for the strips. Look at your own strips and fix what does not match before
handing in. Answer me in at most 15 lines: per variant what you did, what you are unsure about, and the root-track
finding. Touch nothing outside `cards/overlord/jab/director/`. Do not run Unreal, Cascadeur or any kit/setup script.
