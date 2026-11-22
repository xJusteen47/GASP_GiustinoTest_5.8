# Run start: blind review, stock vs Frantic (2026-11-20)

Reviewer: read-only, blind A/B (A = stock `M_Relaxed_Run_Start_F_Rfoot`, B = `AS_GSTN_RunStart_Frantic`).
Evidence: 6 grids (az90 / az0 / game, f84-132 step 3), `metrics_*.json` (clip weights per frame), `assert_*.json`.
Not used: the two mp4 (not opened). The metrics hold only clip weights and yaw: no root, capsule or foot data,
so skate and root coherence are judged by eye from the grids and are marked "not measured" where that matters.
Input starts at f82 in both captures (stamp 0). 60 fps, so the owner's 0.3 s stomp should end near f100.

## Verdict

Neither variant passes the identity. A is clean but has none of the identity; B has the stomp idea but is late,
frozen in the arms, too short, and exits into a wrong clip.
- Shippable today: A (stock) wins, on life and technical only.
- Identity: B is the only one that carries trait 1. It is worth keeping as the base of a v3, not as it is.

## 1. Identity traits

| Trait | A stock | B Frantic |
|---|---|---|
| 1. Stomp first, 0.3 s | FAIL | FAIL (partial) |
| 2. Then frantic run | FAIL | FAIL |
| 3. Clean hand-off to run loop | PASS (to f132; exit not captured) | FAIL |

A stock
- Trait 1 FAIL (posa). f84-105 the body stays upright with a small weight shift; no foot drive, no pelvis drop, no chest load.
  First readable change is the lean at f108, 26 frames (0.43 s) after input.
- Trait 2 FAIL (posa). f114-132 is a smooth mocap acceleration with long strides and a steady forward lean. Not frantic, not cartoony.
- Trait 3 PASS. Run_Start weight 1.00 from f101 to f200, stride continuous in az90 f114-132, no pop seen.
  The exit to the loop starts at f200 (BS_Run_Loop_F_Slopes 0.00), outside the grids: not verified.

B Frantic
- Trait 1 FAIL, partial.
  - PASS part (posa): the load reads. f87 arms up, f90-99 pelvis down and chest forward (az90), f102-108 foot lift (az90, az0 f105).
  - FAIL timing (posa): the foot comes down at about f108-111, clip time 0.45 s, 0.43-0.48 s after input. Target is 0.3 s (f100).
  - FAIL weight (posa): the lift at f102-108 is small and the plant has no squash or settle frame; pelvis is already low from f90,
    so the contact does not add a drop. It reads as a crouch that takes a step, not as a heavy stomp.
- Trait 2 FAIL (posa + dati montage). f114-120 shows small crouched steps with the torso upright and the arms locked in the same
  guard pose held since f87: no flail, no pump. From f121 the clip is already fading (0.78, then 0.48 at f124, 0.20 at f127),
  so the "frantic run" part of the capture is not this clip. The clip blends out from clip time 0.63 s: there is no room for a run section.
- Trait 3 FAIL (dati montage). f121-148 `SS_180_Turn_Root180` blends in (0.22 at f121, 0.57 at f125, peak 0.74 at f129, gone at f148)
  between Frantic and BS_Run_Loop_F_Slopes. A 180 turn clip on a straight forward start. Visible in az0 f123-132: wide stance,
  torso twisted and leaning sideways. Stock never shows this clip.

## 2. Life

| Item | A stock | B Frantic |
|---|---|---|
| Anticipation | FAIL (posa): none, f84-105 flat | PASS: f87-99 crouch and load |
| Response to input | FAIL (dati montage): idle still 0.21 at f90, Walk_Start in the mix f82-100 (max 0.20), Run_Start reaches 0.87 only at f97; pose moves at f108 | PASS: 0.72 at f84, pose changed by f87 |
| Arcs | PASS: smooth, f108-132 | FAIL (posa): arms static f87-120, head down and fixed |
| Weight | FAIL (posa): light, no drop | Partial: pelvis low f90-108, no impact frame at f108-111 |
| Follow-through | PASS (mocap overlap, arms f117-132) | FAIL (posa): nothing trails after the plant, arms and chest rigid |
| Root motion vs capsule | PASS by eye: lean and travel grow together f108-132 | FAIL by eye (gioco / dati montage): dust is left ~80 px behind between f111 and f120 while the pose is still a planted crouch; f123-132 the capsule runs at speed under an upright, turning pose |
| Foot skate | Not measured; none seen | Not measured; probable f111-120 (planted feet, moving capsule) |

Note (gioco): the dust puff appears at f111 in both variants, so it is driven by the movement start, not by the foot.
For the identity it should fire on the stomp contact.

## 3. Technical

- A: assert OK, min 0.99 on f100-200, coverage 1.0. Rule 10 holds for f100-200. What is before f97 (weight < 0.8) is idle + Walk_Start, not this clip.
- B: assert OK, min 0.69 (f122), coverage 0.92 on f98-122. Rule 10 holds only for f98-120.
  - f84-97 (the stomp wind-up) is outside the assert window. Logged weight there: 0.87 at f85, falls to 0.50 at f90, back to 0.97 at f97.
    The dip matches a re-selection at f90 (stock swaps Walk_Start to Run_Start on the same frame, stamp 7 to 9), probably a blend of the clip
    onto itself, but the log does not prove it. By rule 10 the wind-up frames f86-93 do not count until this is cleared (dati montage).
  - f121 onward is below 0.8: the review of trait 2 on B is a review of SS_180_Turn + run loop, not of Frantic.
  - The assert window f98-122 ends exactly where the wrong clip enters, so "ok: true" hides the hand-off failure.
- Both: yaw constant (0, -90) on all 121 frames; play rate slows at f108-112 in both (repeated clip times), same game behaviour.

## What a v3 needs

1. (posa) Stomp contact at clip time 0.25-0.30 s (f97-100), now ~0.45 s. Wind-up 6-8 frames, not 20.
2. (posa) A real lift and hard plant readable in az90: higher knee, pelvis bottoms on the contact frame, 2-3 frames of squash/settle, then push-off. Pelvis must not be already at its lowest before contact.
3. (posa) Add the frantic run section: clip at least ~1.5 s, short fast strides, arms flailing or pumping (now locked in guard f87-120), chest leaning forward while travelling.
4. (dati montage) Fix the exit: no `SS_180_Turn_Root180` at f121-148. End the clip on a run loop phase and check the chooser row / pose match candidates so the clip hands off to BS_Run_Loop directly.
5. (gioco / dati montage) Root motion and capsule: either the capsule holds for the stomp or the clip travels with it. Add root x and foot positions to the capture metrics so skate is measured, not guessed.
6. (gioco) Dust on the stomp contact frame, not on movement start (f111 today in both).
7. (dati montage) Re-capture with an assert window from f84 to the real clip end and explain the weight dip at f86-93, so rule 10 covers the stomp.
