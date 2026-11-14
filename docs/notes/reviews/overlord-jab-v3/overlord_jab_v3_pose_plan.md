# Overlord jab 1 / jab 2 v3: pose plan (ANIM-JAB-5, 2026-11-12)

Why v3: the owner rejected v2 in game ("pitiful", "extremely static"). v2 only satisfied the numeric checks. This plan is
derived from the two theory docs (`docs/notes/anim-theory/principi-animazione-corporea.md` = T1,
`animazione-picchiaduro-action.md` = T2), the owner rules in `cards/README.md`, and the owner's own acted reference
(`media_1794165889394_34973340.mp4`, frame grid looked at once: 0.0-1.2 s jab 1, 1.2-3.0 s jab 2, 3.0 s fist clench, 3.2-3.8 s rise).
Owner direction 2026-11-12: very pushed poses, readable from far away, silhouette first; jab 2 takes a small step forward.

Axes as in the cards: +X character's left, +Y forward (the target), +Z up. Frames are 60 fps anim frames from 0.
Timing is the owner's and does not change: jab 1 = 29 f, active 6-8, FAF 22; jab 2 = 53 f, active 9-11, FAF 26.

## What was wrong in v2 (seen in `docs/notes/reviews/overlord-jab-v2/`), by principle
1. Squat with wide feet from f2 to the end, pelvis and feet frozen: no weight shift (T1 §4), vertical line of action (T1 §11).
2. Chamber reached at f2 then frozen to f5, the whole arc in one frame: no acceleration chain, "teleported" (T1 §2.1, §4.2, §5).
3. Contact silhouette: arm at 45° in front of the chest, short, seen against the torso; trunk barely turned (T2 §2.2, §4).
4. Off arm glued to the belly with fanned fingers; head locked; nothing overlaps, nothing settles (T1 §6, §7).
5. Jab 2: same frozen squat, the "lunge" is a lean; the observe pose is small and upright; no step.

## Shared rules for both cards (each maps to a theory point; the reviewer checks them one by one)
- R1 Stance: tall and narrow like the idle (elegant boss). Knee bend comes only from weight shifts and from the jab 2 lunge.
- R2 Weight (T1 §4.1): the pelvis moves over the support foot 2-4 f before the other foot unloads or lifts.
- R3 Chain (T1 §4.2, §7.2): pelvis starts first, chest +1-2 f, shoulder/elbow +1-2 f, wrist last and fastest. Never all together.
- R4 Strike spacing (T1 §2.1, README "largest step ON the first hitbox frame"): hand travel per frame grows geometrically,
  the last interval (onto the first active frame) is the longest, about 55-65% of the arc; no ease-in before contact.
- R5 Line of action (T1 §11): a C leaning away from the target in the chamber, reversed into a taut C toward the target on contact.
- R6 Contact pose is a clean key (T2 §2.2): striking arm fully extended and clear of the torso, chest side-on so the game
  camera sees shoulder line + arm as one long line; nothing mid-overlap on that frame.
- R7 After the first active frame nothing moves more than 10 cm per frame until the last active frame (owner, `hitstop_snap`).
  The "release of energy" T2 §2.3 asks for therefore starts on the frame AFTER the last active one. Declared conflict: owner rule wins.
- R8 One overshoot of 5-15% on the striking arm, settle in 4-8 f, then a moving hold drifting 2-5% (T1 §6, §8). No second swing.
- R9 Off arm and head: active counterweights with their own timing, 1-3 f late, one small overshoot (T1 §7.3, §11 twinning).
  Head stays on the target (T1 §12 "teste mobili") except where the design says he looks at his hand.
- R10 Breakdowns favour a key (70-80% or 20-30%), never the 50% point (T1 §3). Wrist paths are arcs with no cusp (T1 §9).
- R11 Hands: villain hand = flat, fingers straight and together (README). Belly hand flat and together, NOT fanned.
  Jab 2 claw = fingers spread and hooked. No interpenetration (hand 3 cm off belly/chest).
- R12 FAF (T2 §1): jab 1 is at near-idle with near-zero velocity at f22. Jab 2 is in the held "observe" pose at f26 by design
  (owner), velocity near zero; everything after f26 is a tail that any action may cut.
- R13 Keys: sparse, per control where needed (hand, head, off arm keyed alone on their own frames); curves edited per segment.

## Jab 1: backhand slap (right hand), left forearm folded on the torso
| f | Pose | Pelvis / legs | Trunk / head | Right arm (strikes) | Left arm |
|---|---|---|---|---|---|
| 0 | idle | exact idle | exact idle | idle | idle |
| 2 | break (favours chamber, 70%) | pelvis 3 cm back, 2 cm down, starts turning left; weight goes to the rear (left) foot, right heel lifts | chest follows 1 f late; chin dips behind the rising right shoulder | elbow leads up toward the target, hand drags across the chest | starts folding, 2 f behind the right |
| 4 | CHAMBER (extreme) | pelvis yaw about 35° left, 5 cm back over the left foot, right foot drawn back 8 cm on the toe, both knees soft | chest a further 40° left (back partly to the target), spine leaning 8-10° away from the target; head turned right over the shoulder, eyes on target | hand high beside the left jaw/ear, 3 cm off; elbow at chin height pointing at the target | forearm landing flat on the belly |
| 5 | drive (breakdown, hips first) | pelvis already back to about 10° left and 4 cm forward; right foot plants flat 10 cm forward of idle (small stomp) | chest lags at about 25° left of contact | elbow has opened to about 40% toward the target, hand still trailing, wrist bent back | elbow starts pulling back |
| 6 | CONTACT (first active) | weight on the right (front) foot, right knee bent 15-20°, left leg long, left heel up; pelvis 6 cm forward, 2 cm down | chest side-on: left shoulder, right shoulder and hand on one line toward the target; spine 5° toward the target; chin UP, head turned to the target (contempt) | fully extended toward +Y at target face height (about shoulder height +8 cm), back of the hand leading, wrist still cocked 15° | elbow pulled back past the back line as counterweight, forearm on the belly |
| 7 | flick (active) | drifts 1 cm | drifts | hand +6-8 cm along the arc, wrist flicks through (hand rotates about 25° past the forearm) | settles |
| 8 | overshoot (last active) | pelvis yaw 5° past contact | chest 5° past | arm about 8° past the target line, outward | 1 cm |
| 12 | disdain hold (moving) | weight stays front, body 1.5 cm taller than contact | chin a little higher | arm back on the target line, wrist relaxed, hand drooping 10° | still |
| 13-22 | recovery | pelvis rises and unwinds first (f13-17); right foot steps back to idle f15-19 (3 cm lift, lands f19) | chest 1-2 f after pelvis, head last | lowers elbow first, hand drags, arrives f21 | unfolds 2-3 f after the right, arrives f22 |
| 22 | FAF | within 1 cm / 2° of idle, velocity near zero | | | |
| 28 | idle | exact idle | | | |
Jab 2 cancel leaves from the hold f9-13: jab 2 f0 must equal jab 1 at f12.
Spacing of the right hand f4→f6: about 35% on f4→f5, 65% on f5→f6. Chamber f2→f4 keeps moving (no frozen frames).

## Jab 2: villain claw slap with a step forward, then he looks at his hand
Step: the LEFT foot steps, the root advances `StepForwardCm = 30` (data, provisional, owner decides) between f4 and f9.
The rear (right) foot is planted in the world during the advance, so in root space it travels 30 cm backward f4→f9 at the
same rate as the root. At the end the right foot steps up to the idle stance: the character ends 30 cm ahead, no slide back.
| f | Pose | Pelvis / legs | Trunk / head | Right arm (strikes) | Left arm |
|---|---|---|---|---|---|
| 0 | = jab 1 f12 | weight on the right foot | chest side-on, chin up | extended on the target line | on the belly |
| 3 | break | weight settles fully on the right foot, left foot unloads and lifts; pelvis turns right | chest turns right 1 f late, head stays on the target | arm swings back and UP, elbow leading | leaves the belly, reaches forward as aim/counterweight |
| 6 | WINDUP (extreme) | left foot in the air mid step (knee up 10 cm), pelvis 4 cm back and 3 cm UP on a nearly straight right leg | chest about 50° right (left shoulder to the target), spine arched back 10°; head on target | claw high behind the head: hand above head height, behind the right shoulder, elbow out and high, fingers spread | extended forward-down toward the target |
| 8 | drive (hips first) | left foot LANDS heel first 40 cm ahead in the world; pelvis already square and dropping | chest lags 25° | elbow leads forward-down, hand trails above the shoulder | whips back toward the hip |
| 9 | CONTACT (first active) | deep lunge over the left foot: left knee 50-60°, right leg long behind, right heel up, pelvis 10 cm down and over the front foot | chest 30° past square (right shoulder driven forward), spine 30° forward; head low, eyes on target | fully extended forward-down on a descending diagonal, hand at target chest height, palm down-in, claw open | thrown back behind the hip, elbow slightly bent |
| 11 | last active | sinks 1.5 cm | drifts | hand +8 cm per frame at most along the diagonal | drifts |
| 13 | lowest point / rake through | pelvis lowest (12 cm), absorbing the step (T1 §4.4: low point 3-5 f after landing) | spine 40° forward, head follows the hand | hand raked through to low-left in front of the left knee: wide spacing f11→f13 (release) | starts returning |
| 20 | OBSERVE (held to FAF) | lunge kept, pelvis back up 3 cm (one overshoot) | spine 25° forward and twisted left; head tilted down, turned to the hand | forearm vertical in front of the face, 25 cm from it, palm toward the face, fingers hooked | hanging low beside the left thigh, slightly behind |
| 26 | FAF | moving hold: +1 cm rise | | fingers curl a little more | |
| 27-37 | tremor | still | still, tension | hand rotation tremor, period 4 f, 5° decaying to 0.5°; forearm 20% of it | still |
| 38-42 | clench | | head lifts first, eyes back to the target | fingers close into a fist (owner's video, 3.0 s) | |
| 40-52 | rise | right foot steps up to idle f41-47; pelvis rises f40-49 | chest 1-2 f after pelvis | lowers elbow first, opens to idle hand by f50 | arrives f51 |
| 52 | idle | exact idle | | | |
Spacing of the claw f6→f9: about 10% f6→f7, 30% f7→f8, 60% f8→f9. The windup f0→f6 never stops.

## Checks that may contradict the plan (theory and the eye win; exemptions must say why)
- `planted_feet`: jab 1 right foot f2-5 and f15-19; jab 2 left foot f3-8, right foot f41-47 and its root-space travel f4-9.
- `co_motion`: none expected (every limb moves on every segment). `legibility`: punch rules ("fist not distinct") do not apply to slaps.
- `hitstop_snap`: must PASS as is (R7).
