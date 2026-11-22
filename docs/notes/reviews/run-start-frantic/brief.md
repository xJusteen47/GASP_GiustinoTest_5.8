# Brief: frantic run start (2026-11-18, owner 23:41-23:50Z)

Scope: run start from still (stick >= ~0.4), ALL kits via the shared locomotion profile (DA_AnimSet_Locomotion_Default).
Frames counted from 1. Sim 62.5 Hz; clip authored at 30 fps.
Supersedes: the procedural scramble (sped-up run loop, d5759018; off since 9f65c7fb because it read as a sped-up run) and
"run start = GASP M_Neutral_Run_Start_F_*" for this phase.

## Owner's words (scope)
- 23:41 "per il run start prendi la corsa dove si muovono braccia e gambe freneticamente per qualche tempo", timing from Toad.
- 23:42 run stays Ganon style, medium-heavy character; feel is built around the Overlord. -> Toad = TIMING only, not weight.
- 23:43 legs and arms still frantic; look in Mixamo first, else hand-made.
- 23:50 (all animations) fluid and cartoony while keeping the character: "a Ganondorf inside Astro Bot".

## Mixamo survey (survey/, commit 2eea2197): none fits
61 clips, 3 runs, all loops: standard_run 2.73 steps/s, big arm swing but an athletic sprinter; sword_and_shield_run 2.86 steps/s,
one arm locked; sword_and_shield_run_2 3.75 steps/s, arms almost still. None is a start, none has frantic arms AND legs, none
reads heavy. -> hand-made clip (owner 23:43 "sennò serve farla a mano").

## Timing from Toad (handoffs/ref-smash-run/toad_a_closeup.jpg, 30 fps phone video)
Still until video f24; legs blurred from f26; dust and blur until ~f44, then normal run. Frantic phase ~0.6 s = ~37 sim frames
(by eye on the grid, +-0.1 s). toad_b_far starts agree (~0.5-0.6 s).

## Identity traits (review checks these FIRST)
1. FRANTIC LIMBS: cadence ~5 steps/s (run loop ~2.7), arms pumping wide (elbows bent ~90 deg, fists, swing >= the run's),
   knees driving high: reads as "scrambling" in silhouette from the game camera.
2. HEAVY (Ganon): pelvis drops 4-6 cm on each contact, flat stomping feet, shoulders hunched, torso ~10-12 deg more forward than
   the run, head leading low. Cartoon amplitude (Astro Bot) on top, never weightless.
3. SHORT AND CLEAN: run pose from frame 1 (no walk-in), ~0.6 s / 3 steps, then hands over to the normal run with no pop.

## Key poses (clip 18 frames at 30 fps = 0.6 s; Fable may retime)
| Frame | Pose |
|---|---|
| 1 | Burst: torso low and forward, right knee driving up, left leg pushing back, left fist punched forward, right elbow back. Head down. |
| 3 | Right foot stomps flat, pelvis at lowest (-5 cm), arms at widest. |
| 6 | Passing, left knee up, arms swap with overshoot (2 frames late on the hands). |
| 9 | Left stomp, pelvis low. |
| 12 | Passing, right knee up. |
| 15 | Right stomp, slightly less low (speed rising). |
| 18 | Matches the run loop's phase at the same foot, lean back to the run's + 1-2 deg; blend 0.1-0.15 s to the run. |
Curves: stomps ease-in (heavy), lifts ease-out (snappy); arms and head 1-2 frames behind the pelvis (follow-through); one small
overshoot on the lean at the hand-over, no oscillation. Root: in place (the sim drives speed).

## Game side
- GASP ANIM rule: raw row in CHT_GSTN_LocomotionAnimations + RunStart state in ABP_GSTN_Locomotion (state alias from Idle), exit on
  clip end. Warping stays ON (owner rule): stride warping will shorten the steps at low sim speed; the review checks foot slide.
- Profile values: clip, stick threshold, blend out. Same for all kits.

## Order
1. Desktop, NO editor (Blender + agy, own worktree): author on the UE mannequin skeleton, render side + 3/4 grid, Fable judges poses.
2. When _ue59 is free: import, chooser row + state, build, capture base (GASP start) vs variant, Fable PASS/FAIL (rule 10).
3. Only PASS goes to the owner.
Estimate: authoring 8-10 $, integration 4 $, capture+review 3-4 $.

## Update 2026-11-19 (owner choice B + L3)
- Timing B (owner card 23:53Z): clip 27 frames at 30 fps = 0.9 s: f1-10 stamp almost in place (sim holds speed: StartStamp 21 frames,
  accel x0.05), f11-27 three frantic run steps. Sim code 69feafe1; anim reads GetAnimOwnerRunStartTime (-1 = leave the state).
- L3 (owner 00:05-00:07Z, scope: run start / boost): the frantic start is for precise platforming; in a fight L3 must SKIP it and launch at
  once, like Smash Ultimate's dash. Code: the boost works during the whole stamp, ends it (run start time -> -1) and multiplies the
  acceleration x8 for 6 frames (BoostDash, stage 2 top speed in ~4 frames). Anim: GetAnimOwnerBoostDashTime for a DASH state.
- Dash clip = NEW move, own brief later (identity traits to agree): Ganon-style initial dash, one explosive lunge from the first frame,
  no stamp; until it exists the dash state plays the normal run (no clip blocks the code).
- Dash ledge rule (owner 00:09Z, scope: L3 dash only): if the dash ends just past a platform edge the character does NOT fall, unless the
  player keeps running. NOT in 024f787f (not cheap: the run stage code has no floor probe). For the dash move thread: reuse
  UGSTN_CombatSimComponent::ProbeFloorAhead + FGSTN_LayeredMove_LedgeGuard (Ledge Check track, ujco9r) during the dash window; stop
  only while the stick is not still pushed toward the edge after the window. Open: dash window length (today the burst is 6 frames).
