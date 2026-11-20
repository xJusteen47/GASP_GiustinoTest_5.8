# Checklists by move type (generic)

Each list names what to check for that move type and the criteria behind it. Numbers are indicative (60 fps); a
project profile scales them. These are **guiding questions**, not rules: the rules are the cited IDs.
Every list also includes the isolation report (`P-MEASURE-02`) and the LIFE axis of the rubric.

## Locomotion

### Start (idle → walk/run)
- Weight shifts over the stance foot before the first foot lifts (P-WEIGHT-02); a dip or lean loads the push (P-WEIGHT-03).
- The trunk leads the direction; arms counter-swing from the first step, with lag (P-OVERLAP-02).
- Acceleration is visible in the spacing: first steps short and powerful, then lengthening (P-SPACE-01).
- Root accelerates with the body, not linearly (P-GAME-02); feet do not slide at the speeds the game uses (P-CONTACT-01).
- First frame reads as a change on input (P-ANTIC-04, P-GAME-05).

### Stop (run → idle)
- Braking leans back and lowers the centre; the braking foot plants ahead of the body (P-WEIGHT-04).
- Momentum carries the upper body forward past the feet, then back: overshoot and settle (P-SETTLE-01, P-OVERLAP-05).
- Root decelerates with the body; no slide over planted feet (P-GAME-02, P-CONTACT-01).
- End pose balanced (P-WEIGHT-01); feet re-plant one at a time into the idle stance (P-CONTACT-03, P-GAME-06).
- Arms and head settle after the body (P-OVERLAP-05).

### Skid / slide (intended)
- The slide is marked on the clip as intended, with start and end (P-CONTACT-01 exception).
- Body leans against the direction of travel; the centre is low; the sliding foot leads (P-WEIGHT-04).
- The end of the slide becomes a plant or a push-off, not a freeze (P-HOLD-01).

### Pivot / turn
- The body turns in order: head/eyes, then chest, then pelvis, then feet (P-OVERLAP-02).
- The pivot foot plants and rotates on the ball without translating (P-CONTACT-01).
- The centre of mass shifts toward the new direction before the push (P-WEIGHT-02).
- Paths of hands and head curve through the turn (P-ARC-01).

### Run / walk loop
- Contacts, down, passing, up are visible (P-WEIGHT-03/04); pelvis rises and falls with the steps.
- Hips and shoulders counter-rotate (P-WEIGHT-05); no twinning between arms (P-POSE-04).
- Loop closes with velocity, secondary motion continuous across the seam (P-GAME-04).

### Jump (take-off)
- Load (dip) before the push; the lowest point precedes the take-off (P-WEIGHT-03).
- Arms swing up with the push, leading the body (P-OVERLAP-02).
- Airborne centre follows a parabola, horizontal velocity fixed at take-off, unless the game drives it (P-ARC-04/05).

### Landing
- First contact on the foot (or feet) with the body still travelling down; the lowest point comes after contact
  (indicatively 3-5 f realistic, shorter stylised) (P-WEIGHT-04).
- Arms and head continue down after the body stops (P-OVERLAP-05).
- No forward pop or stall after the contact (P-SPACE-06).
- Recovery to a usable pose; feet re-plant one at a time if the stance changes (P-CONTACT-03, P-GAME-06).

### Return to idle
- The body arrives through a settle, not a blend to a frozen pose (P-SETTLE-01, P-HOLD-02).
- Feet adjust one at a time with lift and plant (P-CONTACT-03).
- Idle is a moving hold: breathing/balance drift, asymmetric (P-HOLD-02, P-POSE-04).

## Actions

### Attack (any strength)
- Anticipation present (compressed for light attacks) and in the opposite direction (P-ANTIC-01..03).
- Power comes from the ground up for strong attacks (P-WEIGHT-06), with lead and lag along the chain (P-OVERLAP-02).
- The striking part accelerates into contact; largest step on or just before the active frame (P-SPACE-03).
- Impact pose reads frozen from the game camera and the side; striking part out of the torso outline (P-POSE-01/03).
- Hit-pause freezes the impact pose, not a passing pose (P-GAME-07).
- Follow-through, then recovery to a pose the next action can start from (P-SETTLE-02, P-GAME-06).
- Anticipation bends the line of action against the strike; the spine whips C → reverse C (P-ANTIC-05, P-POSE-07).
- Strike pose held long enough to read, no 1-2 f flicker (P-TIME-04); heavy strikes: dense wind-up, short burst, dense contact (P-SPACE-08).
- Recovery has a designed breakdown and a small rebound (P-SETTLE-04); startup/active/recovery markers on the clip (P-GAME-13).
- Light vs heavy: heavier = longer anticipation, hold and recovery, bigger arcs, deeper weight shift; tiers separable by timing (P-TIME-02, P-ANTIC-02, P-GAME-14).

### Hit reaction
- First frame already shows the hit (P-ANTIC-04).
- The body goes where the hit sends it; the hit part leads, the rest drags (P-OVERLAP-02).
- Contacts absorbed; the centre's path in the air plausible (P-WEIGHT-04, P-ARC-04).

### Idle / held pose
- Moving hold, not frozen and not noisy (P-HOLD-02); asymmetric, contrapposto (P-WEIGHT-05, P-POSE-04); balanced (P-WEIGHT-01).
