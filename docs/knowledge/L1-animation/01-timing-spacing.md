# L1 · Timing and spacing (`P-TIME`, `P-SPACE`)

Level 1: general craft. Frames at **60 fps, counted from 1**; 24 fps equivalents in brackets (1 f @24 = 2.5 f @60).
**Timing** = how many frames an action takes. **Spacing** = how far a part moves from one frame to the next.
Two clips with identical timing and different spacing look completely different; most generated animation fixes timing
and leaves spacing to the interpolator. That is the single most common cause of "stiff" or "floaty".

## Units

| Duration | 60 fps | 24 fps | Typical content |
|---|---|---|---|
| 1/30 s | 2 f | ~1 f | a snap, an impact frame, a smear |
| 1/10 s | 6 f | 2-3 f | overlap delay between distant body segments |
| 1/6 s | 10 f | 4 f | fast anticipation, short settle |
| 1/4 s | 15 f | 6 f | quick head turn |
| 1/2 s | 30 f | 12 f | one walking step, a hop in place |
| 1 s | 60 f | 24 f | a weighted body turn |

"On twos" at 24 fps (12 drawings/s) corresponds to holding each pose 5 f at 60 fps; 15 poses/s = 4 f holds.

---

### P-TIME-01 — Contrast between fast and slow parts
- **Rule:** every action has at least one clearly fast part and one clearly slow part; the ratio between the fastest and
  the slowest phase is around **1:3 or more**.
- **Why:** an action reads through contrast (Williams). Equal-length phases read flat even with good poses.
- **Measure:** list the phases (anticipation, action, follow-through, settle) with frame counts. Peak speed of the leading
  part in the fast phase vs mean speed in the slow phase.
- **Broken looks like:** every phase 8-12 f; the move "plays" but nothing lands.
- **Severity:** FAIL for actions (attacks, starts, stops, jumps); WARN for idles and loops.
- **Exceptions:** mechanical objects, deliberate monotony.

### P-TIME-02 — Duration fits mass and intent
- **Rule:** heavier bodies take longer to start and stop, shorter to "commit" once going; stylised characters compress
  durations and widen contrasts.
- **Why:** perceived mass is mostly read from acceleration, not from the pose.
- **Measure:** indicative realistic adult ranges: walking step 30 f [12]; running step 16-20 f [6-8] with flight; take-off
  push 6-10 f after a 10-20 f load; landing absorption 6-12 f with the lowest point 3-5 f after contact; untrained punch
  12-18 f, trained fighter 2-3x faster. Stylised "toy" characters: about 20-40% shorter, longer holds.
  The project profile (level 2) sets the scale.
- **Broken looks like:** a big body that starts in 2 f and stops in 2 f (weightless); a small one that eases for 20 f (sluggish).
- **Severity:** WARN (FAIL when the owner's profile states the mass class).

### P-TIME-03 — Timing is judged in stepped (blocking) first
- **Rule:** before curves, the key poses and breakdowns must work in stepped playback.
- **Why:** if the timing fails stepped, splines will not save it; they only hide it.
- **Measure:** play keys stepped; the action is readable and the beats fall on the right frames.
- **Severity:** process rule for authors; reviewers ask for the blocking pass when a clip fails `P-TIME-01`.

---

### P-SPACE-01 — Spacing follows force, not the default curve
- **Rule:** spacing reflects what forces do: leaving rest = slow-out, arriving at rest = slow-in, being hit or thrown =
  sudden change, free fall = increasing spacing.
- **Why:** default splines put slow-in/slow-out on **every** key, so every pose is reached gently: nothing has weight or speed.
- **Measure:** spacing ruler of the leading part (`kb_motion.py figures --kind spacing`). Indicative ratios on the distance
  travelled per frame: standard ease, first and last interval ~1/3-1/4 of the middle one; strong slow-in, last three
  intervals each about half the previous one.
- **Broken looks like:** ticks crowd at **both ends of every segment** between keys (figure: lower ruler).
- **Severity:** FAIL when the crowding repeats at every key (`P-LIFE-02`).
- **Figure:** `figures/D-spacing-charts.png`, `figures/A-spacing-punch.png`.

### P-SPACE-02 — No even spacing on a moving body part
- **Rule:** a body part that is moving relative to the body does not keep equal steps for long.
- **Why:** equal steps = linear interpolation = remote-controlled look.
- **Measure:** `even_spacing_run_max` (consecutive frames with steps within ±6% of each other, limb relative to pelvis):
  free mocap 2-11 f on hands and head (measured, `P-MEASURE-03`); more than ~12 f is suspicious.
- **Broken looks like:** a hand gliding at constant speed to its target.
- **Severity:** WARN; FAIL if together with `P-LIFE-01`.
- **Exceptions:** the whole body travelling at constant speed (that is the root, not spacing); 1-2 f of a smear.

### P-SPACE-03 — Strikes accelerate into contact
- **Rule:** the striking part's largest step is on or just before the contact frame; no slow-in before an impact.
- **Why:** a punch that slows before contact does not hit.
- **Measure:** step lengths of the striking part in the last 4 f before contact are increasing; the last one is the largest
  of the action. Indicative: ≥40% of the path covered in the last 2 frames before contact for fast strikes.
- **Broken looks like:** the fist eases into the target, then the body stops.
- **Severity:** FAIL for attacks.
- **Figure:** `figures/D-spacing-charts.png` (last row).

### P-SPACE-04 — Breakdowns are never at the halfway point
- **Rule:** a breakdown favours one key (in time or in pose), adds an arc, or adds drag; it is not the 50% average.
- **Why:** the breakdown decides character: favouring B (70-80% at half time) = decisive; favouring A (20-30%) = hesitant
  then snapping.
- **Measure:** on the middle frame between two keys, how far along the path is the leading part?
- **Broken looks like:** every in-between is the arithmetic mean of its neighbours: the clip looks morphed.
- **Severity:** WARN; FAIL as part of `P-LIFE-02`.
- **Figure:** `figures/D-favouring.png`.

### P-SPACE-05 — Repeated keys need flat tangents
- **Rule:** two equal keys in a row (a hold) have flat tangents, otherwise the spline overshoots and drifts between them.
- **Measure:** during a hold the part's speed falls to near zero and does not reverse.
- **Broken looks like:** a "hold" that wobbles or bounces.
- **Severity:** WARN.

## Sources
Williams, *The Animator's Survival Kit* (timing and spacing charts, contrast); Thomas & Johnston, *The Illusion of Life*
(slow in and slow out, timing); measurements in `figures/metrics-cmu-vs-lifeless.json`.
