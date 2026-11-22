# Dossier: Ganondorf SSBU initial dash (locomotion move, L3 boost dash) - 2026-11-19

Method: docs/notes/review/dossier-riferimento.md (branch claude/project-thread-5gguxu). Built from the earlier note
(project files handoffs/dash-ganon-ref/NOTE.md) plus one look at grid A. Images of the game stay in project files, never in the repo.

## Source
- Video: owner's phone filming the Switch, project files `handoffs/crouch-ref/ganon_9541.mov`. Phone 30 fps, game 60 fps -> 1 phone
  frame = 2 game frames. Our sim 62.5 Hz, clips 60 fps: 1 game frame ~ 1 clip frame (difference ignored, under 1 frame over 25).
- View: side, hand-held, the camera pans with the dash. No fixed scale (see "Not measurable").
- Grids: `handoffs/dash-ganon-ref/dash_A_turn_right_9541_t7.50-8.04_30fps.jpg` (clean side view, every phone frame),
  `dash_B_turn_left_..._30fps.jpg` (same dash back, closer, agrees within 1 phone frame), `overview_9541_6fps.jpg`.

## Timing (game frames, counted from 1 at the first frame that moves; grid A)
| frames | phase | what is seen |
|---|---|---|
| 1-2 | turn | body swings round, knees start to bend (both usable dashes are turnaround dashes) |
| 3-6 | LOAD | compact: knees deep, torso hunched forward, arms tucked; dust burst at the feet |
| 7-8 | push-off | rear leg drives, torso tips forward |
| 9-12 | LUNGE (held) | torso ~45-50 deg forward, near-airborne; front thigh up and forward, knee bent; rear leg straight back; fists forward-low; long dust trail |
| 13-16 | extended stride | torso ~35 deg, legs still wide, the front foot reaches down |
| 17-20 | catch | front foot lands under the hips, the body rises a little |
| 21+ | run | torso ~25 deg, normal run stride |
Rhythm: short compact load (~4 f), explosive push (2 f), lunge held ~4 f as one silhouette, then a heavy settle into the run.

## Shapes and silhouette
- Load = a compact ball (everything inside the feet). Lunge = one long diagonal line from the rear heel through the back to the head,
  with the front knee as a second, shorter spike forward: it reads as an arrow from the side. From behind it reads as a low, wide
  stance with the head forward of the shoulders.
- The silhouette of the dash is the lunge, not the run: it must be the most extreme and the longest-held pose of the clip.

## Sizes (relative to the body)
- Lunge: rear leg close to straight, ~45-50 deg from vertical, foot off the ground or on the toe; ankle-to-ankle split about
  0.6 body heights. Pelvis lower than in the run.
- Load: pelvis drop well under the run's (the knees deep), roughly a sixth of the body height.

## Movement and curves
- Load: fast in (2-3 f), no hold. Push: fastest change of the clip (ease-out into the lunge). Lunge: a MOVING hold (small drift,
  the legs keep spreading a little), not frozen. Stride and catch: ease-in to the front foot contact, then the body rises over
  ~3 f (the weight goes through the foot). No float: no frame where the body goes up without a foot pushing.
- Follow-through (Ganon): the cape flies horizontal in the lunge. Our character has no cape: the head and fists carry the
  overlap (head 1-2 f late on the lean changes, fists settle into the run swing 2-3 f after the catch).

## Identity traits (rule 13a)
1. Compact load before the push (knees deep, hunched, arms tucked), with a dust burst at the feet.
2. The lunge: strongly pitched torso (~45-50 deg) with a wide leg split, rear leg straight back, fists low and forward; held ~4 f.
3. Heavy settle into the run: the body rises as the front foot catches under the hips, no float.

## Where the reference is weak or does not apply to us
- Both usable dashes are TURNAROUND dashes; ours starts from idle facing the stick (the facing is snapped by the sim), so the
  turn frames 1-2 become the start of the load. A turnaround variant can be added later from the same keys.
- Ganondorf's cape gives most of the follow-through; we have none (see above).
- Smash moves the character from the first frame; our sim reaches stage 2 top speed (610 cm/s) in ~4 frames (BoostDash x8),
  so during the load the feet slide forward with the body: this matches Smash (the dust sells the slide) and is kept.
- Style: owner "Ganondorf inside Astro Bot", medium-heavy: push the extremes (lunge pitch, load depth), keep the weight in the catch.

## Not measurable here
- Distance and speed: the camera pans and the phone moves; the clip follows our sim's speed instead (root motion = sim, warping on).
- Exact limb angles: phone blur on frames 5-8; the numbers above are ranges, not measures.
