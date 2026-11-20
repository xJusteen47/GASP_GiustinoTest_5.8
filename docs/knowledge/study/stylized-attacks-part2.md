# Stylized Attack Animations, part 2 (Gnomon), study notes: lessons 07-12
Scope: private study notes, paraphrased, from the whisper transcripts of lessons 07-12 (heavy attack, timing and
spacing polish, effects, conclusion). Lessons 01-06 are in another note. Rig: a Maya biped with spheres as proxies,
tween machine, blend-ease tool, retime tool; one character, one heavy punch in a fighting-game style. Transcripts mis-hear
some words; numbers below are as heard. The instructor never states the working fps, so frame counts are in
"course frames" (fps unknown, do not convert to our 60 fps without checking the original video).
Tags: GENERAL = holds for any attack animation; STYLE = taste of the course or the fighting-game genre.
Mapping: existing ID, NEW (not covered), TOOL (software only).

## Lesson 07 - Heavy Attacks: Power & Commitment, part 1 (about 29 min)
- Heavy = weight and force, exaggerated; use "easy ease" (ease out of the wind-up, big gaps in the middle, ease in to the
  hit) so the eye rests on a few readable poses — 00:20-01:00 — GENERAL — P-SPACE-01, P-TIME-01
- Stretching the same punch from 20 to 40 frames makes it slower but heavier; time is the first lever for weight — 01:06 — GENERAL — P-TIME-02
- Rework the medium attack into the heavy one, keep the keys that still work, delete the rest, block in stepped — 01:27 — GENERAL (process) — P-TIME-03
- Number of anticipation frames is the biggest single driver of heaviness; 4 frames is not enough for a heavy, try 10 — 03:31 — GENERAL — P-ANTIC-02
- Reach the anticipation quickly from idle, spend the frames on holding the loaded pose, not on getting there — 05:01 — GENERAL — P-ANTIC-03, P-TIME-01
- Step into the punch instead of drawing back from standing: the step makes space and travel for the strike — 06:36 — STYLE — P-WEIGHT-03 (partly), NEW-weak
- Wind-up as an opposite line: chest and arm go back opposite to the later strike, a swing from one extreme to the other gives the power — 09:46 — GENERAL — P-WEIGHT-05, P-WEIGHT-06
- Weight transfer is shown on the feet: weight onto the back leg during the wind-up, then moves to the front leg on the step — 08:58, 20:19 — GENERAL — P-WEIGHT-02, P-WEIGHT-06
- The step must look good or the weight is not believable (lift, plant, settle) — 14:23 — GENERAL — P-CONTACT-03, P-WEIGHT-04
- Anticipation pose is more pushed, wider and held much longer than the medium's; exaggerate the extreme pose beyond what looks natural — 16:50, 18:59 — GENERAL (amount STYLE) — P-ANTIC-02, P-STRETCH-03
- Keep the fist/hand silhouette clear in the wind-up; raise head and clavicles so the full body extent reads — 15:35-16:43 — GENERAL — P-POSE-01
- Breakdown between wind-up and hit is a whip: curve goes reversed C, then bent, then S, then C; the character is deliberately "broken" (stretch, toe stretch, lead with neck and head, arm drops first) — 22:02, 24:23-26:47 — GENERAL (technique) — P-OVERLAP-02, P-STRETCH-02, P-POSE-02; the C-to-S-to-C sequence is NEW (see candidates)
- Several breakdown poses, not one, to sell the whiplash; hands go from open hand pose to fist across them — 21:12, 28:43 — STYLE — P-SPACE-04, P-LIFE-02
- Use limb/palm scale controls to push the breakdown (stretch the arm and hand for a clearer fist) — 03:09 (L08) — STYLE — P-STRETCH-02
- Reverse the poses at the end of the breakdown so shapes do not muddle (no pose looks like its neighbour) — L08 00:21 — GENERAL — P-POSE-05

## Lesson 08 - Heavy Attacks: Power & Commitment, part 2 (about 31 min)
- Hit pose is established early (frame 15 in the first block-in), pushed forward body, hips back, into a C shape — 00:55-02:08 — GENERAL — P-POSE-02
- The pose before the hit is the overshoot: wind back, step, land on the overshoot, then come back to the hit — 02:29-06:00 — GENERAL — P-SETTLE-01, P-SPACE-03
- Do not hit a wall: check the fist arc, the body lands and comes back slightly on the feet after contact — 07:09-07:43 — GENERAL — P-SETTLE-01, P-ARC-01
- Hold the hit pose roughly as long as the anticipation: anticipation held 11 frames, hit held about 7-8 — 08:06-08:32 — GENERAL (ratio) — P-GAME-07, X-08; mirror-hold of wind-up and impact is NEW (candidate)
- Heaviness comes from the extended frame count; extending the hold pushed the clip from about 26 to 28, with the return pose 6 frames later — 09:07-09:45 — GENERAL — P-TIME-02
- Tween and polish order: block, then in-betweens, start from the hips, clean the weight shift — 09:56-11:06 — TOOL — P-MEASURE-01
- Check front view as well as side view after posing in one view — 11:06 — GENERAL — P-POSE-03
- Recovery: rotate back, pelvis sticks, feet return to their idle contacts, arms not stacked on top of each other, add a toe pivot and a heel-first step — 14:06-21:09 — GENERAL — P-SETTLE-02, P-CONTACT-03, P-POSE-04
- Idle pose after a stepping attack: copy-world-space the body onto a shifted global so the clip ends where the attack stepped, not snapped back — 17:41-19:49 — TOOL (the principle that end pose must match the displaced root is P-GAME-02/06)
- Hold the final pose long enough (extra frame, "so it doesn't snap back"), neck/head keep a delay so the head does not pop — 20:29, 22:23 — GENERAL — P-SETTLE-02, P-OVERLAP-02
- Fist returns on an arc and eases in; a heavy fist coming back goes down then out; fist arc also checked from the front — 24:27-27:09 — GENERAL — P-ARC-01, P-SETTLE-02
- Result summary: more timing and more anticipation build a heavier impact; the next chapter cleans the jitter to separate light, medium, heavy — 30:28 — GENERAL — P-TIME-02

## Lesson 09 - Timing & Spacing, part 1 (about 26 min)
- Polish is a third phase: timing and spacing after blocking and poses — 00:07 — GENERAL (process) — P-TIME-03
- Replace the character by spheres on the fist, chest, cog, feet to see speed and weight without distraction — 00:28-01:16 — TOOL (the idea of judging a single point's spacing is P-MEASURE-01)
- Timing = number of frames, spacing = where the keys/poses fall; timing gives speed, spacing gives weight — 01:25-02:41 — GENERAL — P-TIME-02, P-SPACE-01
- Fewer frames means faster; less change per frame means heavier, more change per frame means lighter — 02:32-02:49 — GENERAL — P-SPACE-01 (the per-body-part form is NEW)
- Spacing vocabulary: linear (almost none), even (underwater slow motion), ease in (quick, light attack), ease out (more anticipation and weight), easy ease (both, heavy output and heavy catch, the heavy attack) — 02:49-03:32 — GENERAL — P-SPACE-01, P-SPACE-02
- A looping bouncing ball with the same spacing but changed timing loses or gains weight; time compression alone changes perceived mass — 03:44-04:47 — GENERAL — P-TIME-02
- Fist spacing for the heavy: small steps at the start (heavy, slow), then a giant ease out of about 3 frames of large gaps, then ease in to the hit; some overlap right before the hit pose — 06:27-07:41 — GENERAL — P-SPACE-03, P-OVERLAP-05
- Less change means more noticeable and heavier; the heavy attack has three noticeable (dense) places: the start, the contact, and the recovery — 10:11-10:22 — GENERAL — P-SPACE-01, P-HOLD-03; the "three dense places" form is NEW (candidate)
- Overshoot the hand then ease it back so it arrives and settles — 09:13-10:03 — GENERAL — P-SETTLE-01
- Parts of the body have different spacing: the fist ball moves far, the cog and chest move less distance and with smaller steps because they carry more mass; same timing, different spacing gives different weight — 11:57-14:22 — GENERAL — P-WEIGHT-06, P-OVERLAP-02
- COG: weight goes up then drops with space to read a landing on the foot carrying the weight — 12:16-12:47 — GENERAL — P-WEIGHT-04
- Plan: fix the spacing first using the proxy path, cut or add frames at the very end for timing — 15:15 — GENERAL (process) — P-TIME-03
- Weight moves forward while chest moves backward (opposing motion) — 17:24 — GENERAL — P-WEIGHT-05
- Feet: lift the heel, big knee bend pops, check the front view — 18:28-21:28 — GENERAL — P-CONTACT-03, P-POSE-03

## Lesson 10 - Timing & Spacing, part 2 (about 29 min)
- After the key poses, spline the whole thing to see it from start to end, then fix jitter, hips and chest that do not follow the fist — 00:27-03:00 — TOOL/GENERAL — P-LIFE-02
- Feet: make sure a stopped foot does not slide (check at each halt) — 03:49, 22:12 — GENERAL — P-CONTACT-01
- Delay the knee in recovery; keep the punch from covering the face; head leads into the punch — 04:24-05:04 — GENERAL — P-OVERLAP-02, P-POSE-01
- Add an in-between held for 2 frames and a blend ease when a return needs one more beat — 08:34-08:56 — GENERAL — P-HOLD-02
- Delay chest and head on the return so they follow the head arc, not lock together — 09:10-09:33 — GENERAL — P-OVERLAP-01/02
- Timing clean-up ("trim the fat"): the startup has too many frames; delete in-between keys and slide the good key poses earlier (examples: key 5 moved to 3, 6 to 4, shave one frame off every key) — 11:35-15:01 — GENERAL (method) — P-ANTIC-03, P-GAME-05
- Fewer breakdowns can be stronger: remove frame 12, keep 11 and 13, to move the impact earlier — 15:59-16:18 — GENERAL — P-SPACE-04, P-GAME-05
- Compress the chest on landing and let it breathe out — 16:37 — GENERAL — P-STRETCH-02, P-WEIGHT-04
- Impact timing: ease into the impact pose, remove passing frames 14 and 15 to land the impact at 13-16 — 18:08-18:57 — GENERAL — P-SPACE-03, P-GAME-05
- Rebound pose: after the hit, immediately come back to a pose, to be treated in the next chapter (not shown) — 20:58 — STYLE — NEW-weak
- Keep a small "keep alive" bend in the front arm, avoid a straight stiff arm — 19:39 — GENERAL — P-HOLD-02, P-LIFE-07
- A fist going bottom-hit, top-hit catches the eye; change the extension direction so it stays inside — 19:48 — STYLE — P-POSE-05
- Add a smidge of overshoot and settle at the end because the idle arrives too quickly; drag the arms past where everything settles so all do not land together — 24:08-25:11 — GENERAL — P-SETTLE-01/03, P-OVERLAP-01
- Do not let a breakdown fist arrive at the same time as the other fist; offset, avoid gimbal lock on the rotated arm — 25:23-26:08 — GENERAL — P-POSE-04, P-OVERLAP-01
- Conclusion: blocking to polish is not one shot; by splitting and timing parts you can make an attack quicker without losing the heaviness — 28:36-28:56 — GENERAL — P-GAME-05, P-TIME-02

## Lesson 11 - Effects & Enhancements for Attack Readability (about 28 min)
Reminder: the transcript talks about "smears" and "motion trails" in the intro only; no smear frame is built on screen.
Effects shown: impact explosion (free pack), a larger burst, camera shake, focal-length shake, speed lines, slow motion ramps.
Side column: A = animation side, V = VFX side, C = camera/presentation (outside both of our families).
- Enhancements give the eye more information to follow the action: freeze frames, slow motion, smears, trails, camera shake — 00:15-00:49 — GENERAL — V-INFO-01, X-08 — side: A+V+C
- Place the impact effect at the overshoot/contact pose, in front of the fist — 02:06-02:22 — GENERAL — X-01, V-IMPACT-01 — V
- Effect visibility is keyed on and off with linear steps, a lifetime of only a couple of frames, scale animated (held 2 frames, then reduced) — 02:51-04:20 — GENERAL — V-TIME-02, V-TIME-04 — V
- Effect timing: the instructor tried offsetting the burst later than the impact and preferred the earlier timing ("I kind of like it when it happened before" = before the offset, i.e. on the impact) — 06:21-06:43 — GENERAL — agrees with V-IMPACT-01 / X-01 (checked against the transcript 2026-11-19; an earlier note read it as "before the contact", which the transcript does not support)
- Animate the effect's translation and scale after the hit: push forward, push behind the fist, scale; moves quicker or slower; use it as a visual of the whiplash — 06:45-08:30 — GENERAL — V-SHAPE-05, V-SHAPE-07 — V
- A burst that reads like a gunshot, usable for a giant fist; reshaping and re-texturing a pack effect to the character — 05:15-05:47 — STYLE — V-SHAPE-01 — V
- Break up timing of the animation itself: bake the keys, scale the timeline (double it), use a retime tool to hold and ramp — 09:01-10:29 — TOOL (the concept is NEW: see candidates; also P-GAME-07)
- Hold the anticipation pose longer ("slow motion that comes in"), get into it a bit quicker, hold the hit frame for a while, then settle fast — 10:33-13:34 — GENERAL — P-ANTIC-02, P-GAME-07, P-HOLD-03 — A
- Squeeze the return/recovery to get back to idle quickly after the hold — 13:45-15:19 — GENERAL — P-GAME-05 — A
- Super move version: effect moved one more frame later (to feel impact), plus camera shake — 16:14-17:35 — STYLE — X-01 — V+C
- Camera shake begins on the impact, 3 alternate swings (right, left, right) already feel the hit; amplitude decays like a cardiogram, big then smaller — 17:46-18:51 — GENERAL — outside our families (C); relates to V-IMPACT-03 — C
- Shake axes: side-to-side, up-and-down (stomp feel), focal length (inside/z impact, reduced a lot); chose up-and-down for a stomp — 19:35-22:03 — STYLE — C
- Speed lines: trails oriented along the punch direction that read the direction; start ramping in at the slow-mo anticipation, more at the anticipation, extend past the point of impact — 22:19-26:06 — STYLE (genre) — V-SHAPE-05, V-TIME-01, X-03 — V
- Combine effect, shake, speed lines and after-effect; "tip of the iceberg"; hit boxes also named as a thing to add — 26:40-28:00 — GENERAL — X-02, V-LAYER-01 — V+C

Animation vs VFX split in lesson 11:
- Animation side: held anticipation (slow-mo in), held hit pose (frame freeze), fast punch, fast recovery; stretch and multiples/smears are only named, not shown. Maps P-GAME-07, P-ANTIC-02, P-STRETCH-02, P-ANTIC-03.
- VFX side: impact burst timing/position/scale (V-IMPACT-01, V-TIME-02/04, V-SHAPE-05/07), speed lines (V-SHAPE-05, V-TIME-01), layered combination (V-LAYER-01).
- Meeting point (X-*): X-01 impact frame shared, X-02 hit-pause designed (here as a long hold on the pose), X-03 charge on anticipation (speed lines ramp in at the wind-up), X-08 strength = timing (the whole lesson is long holds).
- Camera shake and the focal change are neither (presentation); our criteria have no family for it.

## Lesson 12 - Conclusion (about 2.5 min)
- Attack phases named: startup, action (active), contact, recovery — 00:55 — GENERAL — P-GAME-05
- Anticipation sets power; timing and spacing shape gameplay and responsiveness — 00:47 — GENERAL — P-ANTIC-02, P-GAME-05
- Push poses further and exaggerate timing; fighting games evolve with style — 01:12 — STYLE — P-STRETCH-03
- Good animators also play and understand how movement drives interaction; study real fights, good games, GDC talks — 01:29-01:57 — STYLE/process — P-GAME-05

## Numbers given
**Frame rate checked 2026-11-19:** the Maya scene plays at **24 fps** (visible in the playback options on frames `SA05_0500.jpg` and `SA08_0844.jpg`). Course frames × 2.5 = frames at 60 fps.

No fps stated anywhere in L07-12 (course frames).
| lesson mm:ss | what | value | fps |
|---|---|---|---|
| 07 01:06 | demo of timing change on the same attack | 20 to 40 frames | 24 (checked) |
| 07 01:40 | medium attack length, punch back to recovery | about 18 frames | 24 (checked) |
| 07 01:51-02:05 | heavy total length first set | 24 considered, 26 set (for safety) | 24 (checked) |
| 07 03:37-03:50 | medium anticipation vs heavy anticipation | 4 frames (not enough) vs 10 tried | 24 (checked) |
| 08 00:55 | first hit pose location | frame 15 | 24 (checked) |
| 08 08:11 | anticipation hold actually used | 11 frames | 24 (checked) |
| 08 08:26 | hit hold to match | 7 to 8 frames (not exactly 11) | 24 (checked) |
| 08 09:20-09:31 | return pose pushed later, total length | 6 more frames, to 28 | 24 (checked) |
| 08 22:16 | extra in-between in recovery | frame 29 (clip then past 28) | 24 (checked) |
| 09 07:05 | giant ease-out spacing before the hit | over 3 frames, "push even further" | 24 (checked) |
| 09 10:16-10:22 | noticeable (dense) places in a heavy | 3: start, contact, recovery | not applicable |
| 09 13:47 | fist ball vs pink body ball | similar timing, far more space for the fist (no ratio) | 24 (checked) |
| 10 08:47 | in-between hold | 2 frames | 24 (checked) |
| 10 10:14 | frames added at the end of the clip | to frames 34, 35 | 24 (checked) |
| 10 12:19-12:45 | startup trim examples | key 5 moved to 3, key 6 to 4, key 10 and 9 pulled up | 24 (checked) |
| 10 14:52 | shave all keys | by 1 frame | 24 (checked) |
| 10 16:06-16:18 | breakdowns removed | keep 11 and 13, delete 12 | 24 (checked) |
| 10 18:17-18:57 | impact trim | 18 easing in; 14 to 13; delete 15, key 16 | 24 (checked) |
| 11 03:53 | impact effect held on scale | 2 frames | 24 (checked) |
| 11 10:04 | timeline stretch for retiming | doubled (x2) | 24 (checked) |
| 11 16:14 | effect delayed for super move | 1 frame | 24 (checked) |
| 11 18:05 | camera shake swings | 3 (right, left, right) with decaying amplitude | 24 (checked) |

## Heavy attack summary (supported by L07-10)
| Phase | What the instructor does | Frames (course) | Weight cue |
|---|---|---|---|
| Startup / anticipation | step forward into a wide, over-extended wind-up, arm and chest back opposite to the strike, held long | 10-11 frames held, vs 4 on the medium | long hold, weight on back leg, opposite lines |
| Breakdown | whip C -> reverse C -> S -> C, stretch, lead neck/head, toe leaves ground, several poses | not stated (3 or so poses) | whiplash, big gaps, arms/fist stretched |
| Contact / hit | overshoot then hit pose, land on the front foot, body comes back slightly, hold | hit hold 7-8; hit about frame 15 of 26-28 | hold mirrors the wind-up, hips back, body forward |
| Recovery | slow ease in, arc on the fist, feet re-plant, toe pivot, overshoot and settle, offset arms | the rest, to about 34-35 after trimming | dense spacing at the end, delayed head/chest |
| Totals | heavy 26-28 vs medium 18; later trimmed to feel quicker without losing weight | | |
Spacing of the whole: dense (small steps) at the start, about 3 big frames in the middle, dense at the end (three dense places).

## Candidate new criteria (NEW)
GENERAL only.
| Proposed ID | Rule | Evidence | Measurable how |
|---|---|---|---|
| P-SPACE-06b | Heavy strike spacing has three dense zones (wind-up, contact, recovery) separated by a short burst of large steps (about 3 frames); everything else is subordinate | L09 10:11-10:22, 07:05 | Per-frame speed of the striking extremity: low, short peak, low; count frames above 50% of peak |
| P-SPACE-07b | Spacing differs by body part at the same timing: the heavier the part (cog, chest), the shorter its per-frame steps; the extremity travels far | L09 11:57-14:22 | Compare per-frame displacement of cog vs fist over the same frame range; ratio < 1 |
| P-ANTIC-05b | The hit hold mirrors the wind-up hold: heavy attack holds the contact pose about as long as, or somewhat shorter than, the anticipation (hit 7-8 vs wind-up 11) | L08 08:06-08:32 | Frames within a velocity threshold at the contact pose / at the anticipation pose |
| P-POSE-07b | The strike winds up and whips through a C, reverse C, S, C sequence along the spine (opposite lines), not a straight line | L07 09:46, 24:23-26:47 | Spine curvature sign per key pose; sign flips between wind-up and contact |
| P-GAME-11b | Perceived heaviness can be added by timing at runtime: ramp/hold the anticipation and hit frame, fast in between (retime), in addition to authored frames | L11 09:01-13:34 | Per-frame clip speed (retime curve) showing slow, fast, hold, fast |
| P-ANTIC-06b | Frame of the first keyed pose after idle is already near the wind-up (reach the loaded pose fast, spend frames holding it) | L07 05:01 | Frames from the first pose change to the anticipation pose vs hold length |
| X-09b | Impact effect lifetime is short (about 2 frames of hold on its peak scale) and its scale and position are animated after the hit, not static | L11 03:53, 06:45-08:30 | Effect peak hold frames and scale curve |
| X-10b | Camera/presentation shake starts on the contact frame and decays (3 swings or more); it is a separate layer from the animation and from the VFX | L11 17:46-18:51 | Shake onset frame vs contact frame; amplitude per swing decreasing |

## Workflow notes (TOOL)
- Spheres as proxies for fist, chest, cog and feet, with an arc tracker, to judge spacing without the body (L09).
- Block in stepped, then in-betweens, then spline, then trim frames; plan: spacing first, timing trim last (L07, L09, L10).
- Tween machine and blend-ease tool to adjust in-betweens; "copy world space" to carry the body to a shifted global (L08 17:41).
- Bake, scale the timeline x2, retime tool for holds and ramps; delete duplicate keys (L11 09:01).
- Effects: free packs, keyed visibility, linear steps; camera shake layers on translate Y, Z and focal length (L11).

## Frames worth extracting
| lesson mm:ss | what it shows | for which ID |
|---|---|---|
| 07 16:50 | new heavy anticipation pose vs the medium's (wide, pushed) | P-ANTIC-02, P-POSE-02 |
| 07 24:42-26:53 | breakdown shape sequence reverse C -> S -> C | P-POSE-02, candidate P-POSE-07b |
| 08 05:45 | overshoot pose with the foot planted and the arm wound back | P-SETTLE-01 |
| 08 07:29 | arc of the fist and the "wall" it must not hit | P-ARC-01 |
| 08 08:44 | hit pose held about 7 frames | P-GAME-07, candidate P-ANTIC-05b |
| 09 06:55 | sphere track of the fist with 3 large steps | P-SPACE-03, candidate P-SPACE-06b |
| 09 13:47 | two spheres with the same timing, different spacing | P-WEIGHT-06, candidate P-SPACE-07b |
| 11 02:22 | impact burst in front of the fist at the overshoot | X-01, V-IMPACT-01 |
| 11 12:55 | held hit frame after the retime | P-GAME-07, X-02 |
| 11 26:06 | speed lines passing the point of impact | V-SHAPE-05, X-03 |

## What contradicts or refines our criteria
- Course L07 03:44 ("10 anticipation frames for a heavy, 4 is not enough") vs ours P-ANTIC-03 (anticipation compressed in games): not a contradiction, but the course works at an unstated fps and in a fighting-game genre; our ratio P-ANTIC-02 (3:1 or more for a slam) is consistent: 10-11 against about 3-4 action frames. Refines: our numbers are in 60 fps frames, theirs are not convertible.
- Course L08 08:06 (hit hold about 7-8 frames, similar to wind-up) vs ours P-GAME-07 (hit-pause a few frames, frozen pose = impact pose): refines it; the course hold is authored in the animation, ours is a runtime pause; 7-8 course frames is long if it is a hit-pause, and compatible only if the animation also holds. We should keep the two separate (pose hold vs runtime hitstop) in X-02.
- Course L11 06:43: no contradiction with X-01 / V-IMPACT-01. The instructor reverted an offset and kept the effect on the impact frame (transcript checked).
- Course L11 03:53 (effect peak held 2 frames) vs ours V-TIME-02 (climax in the first quarter, hit open at maximum frame 1-2): consistent.
- Course L09 06:55 (large steps over about 3 frames in the middle) vs ours P-SPACE-03 (accelerate into contact): consistent; refines that a heavy strike accelerates into dense contact and then holds, it does not decelerate before contact.
- Course L09 02:49 (even spacing = underwater look) vs ours P-SPACE-02 (no even spacing): consistent.
- Course L10 15:01 (shave one frame from every key to quicken) vs P-GAME-05 (responsiveness): consistent, adds a method: trim passing frames, keep the loaded and contact poses.
- Course L11 smears and multiples: named but not built; nothing to confirm or refute P-STRETCH-02. No data on smear frame count.
- Our criteria have no family for camera shake and focal-length shake (L11 17:46); candidate X-10b.
