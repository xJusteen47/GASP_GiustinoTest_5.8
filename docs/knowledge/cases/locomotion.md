# Worked cases (level 3): locomotion

Examples that apply level 1 (general criteria, `knowledge/animation/` in the skills repo) and level 2
(`../profile-gstn.md`) to our locomotion. **Not rules.** If a case teaches something general, promote it to level 1 with
a source, or to the profile as an owner decision. Numbers at 60 fps, frames from 1.

## Case L-1 · Run stop (the failure that motivated this base)
- **What happened (2026-11-17..19):** stop and start clips were built by scripts that interpolated a handful of key
  poses; reviews checked foot slide, end stance, curves and pops, and passed clips the owner found lifeless.
- **What level 1 predicts:** `P-LIFE-01` (all joints ease into the same keys), `P-LIFE-02` (ease on every key),
  `P-WEIGHT-04` (braking must lower and lean back, the lowest point after the braking plant), `P-SETTLE-01`
  (upper body overshoots past the feet, then settles), `P-OVERLAP-05` (arms and head settle last), `P-GAME-02` (root
  decelerates with the body). The rebuild of CMU 16_08 (a free run-and-stop mocap) with keys every 0.2 s shows the
  same symptoms: `stop_sync_share` 0.90 vs 0.11 for the mocap, contacts reduced from 3 to 1 per foot
  (`figures/A-contact-stop.png`, `metrics-cmu-vs-lifeless.json` in the skills repo).
- **Profile on top:** medium-heavy mass → braking deeper and longer than a light character; toy timing → crisper end hold;
  return to idle re-plants one foot at a time.
- **What a passing version shows:** stop_sync_share well under 0.5; a visible down-and-back weight shift; upper-body
  overshoot 4-12 f after the feet plant; arms settling last; root speed curve following the pelvis; isolation report
  attached; side + behind video.

## Case L-2 · Run start ("0.3 s stomp + frantic run")
- **Level 1:** `P-WEIGHT-02/03` (shift and load before the push), `P-ANTIC-03/04` (compressed but present; frame 1 is a
  new pose), `P-SPACE-01` (short powerful first steps, then lengthening), `P-GAME-02` (root accelerates with the body).
- **Profile:** the stomp is the anticipation; it must read from behind (the camera) as a downward load, not a pause.
- **Typical failure:** a stomp that is a held pose (`P-HOLD-01` dead frames) followed by eased steps of equal length.

## Case L-3 · Skid and turn (intended slide)
- **Level 1:** `P-CONTACT-01` exception (slide marked on the clip), `P-WEIGHT-04` (lean against travel, low centre),
  `P-OVERLAP-02` (head → chest → pelvis → feet in the turn), `P-HOLD-01` (the end of the slide becomes a push, not a freeze).
- **Profile:** designer marker for the slide window; turn side from data, not from the clip (setup rule).

## Case L-4 · Landing
- **Level 1:** `P-WEIGHT-04` (lowest point after contact), `P-OVERLAP-05` (arms/head continue down), `P-CONTACT-03`
  (stance changes one foot at a time), `X-05` (dust born on the contact frame at the contact point).
- **Profile:** landing lag near hard-cut from aerials; the absorption must still read in the few frames available,
  so it is carried by pose contrast (squash) rather than by length.

## Case L-5 · Return to idle
- **Level 1:** `P-SETTLE-01`, `P-HOLD-02` (idle is a moving hold), `P-CONTACT-03`, `P-POSE-04` (no twinning in the idle).
- **Profile:** owner rule: never both feet airborne.
