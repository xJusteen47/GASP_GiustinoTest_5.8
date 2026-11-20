# L1 · Working from reference (`P-REF`)

Level 1. Most lifeless animation is invented: a few poses imagined from a text description, interpolated. Animators
work from reference (filmed performance, motion capture, footage of real bodies) and then push it. This file is the
procedure and its checkable criteria.

### P-REF-01 — Motion that imitates real bodies starts from reference
- **Rule:** any move that imitates real motion starts from a reference clip (footage, an acted take, motion capture, an
  approved earlier version), never from poses invented from a description.
- **Measure:** the move's brief names the reference and says which poses and timings came from it.
- **Severity:** FAIL (process) for new moves.

### P-REF-02 — Plan before posing
- **Rule:** write the intent in one line, the context (what comes before and after, so the first and last poses hand
  off), and the rhythm (strong and weak beats) before the first key.
- **Severity:** WARN (process).

### P-REF-03 — Reference fits the target
- **Rule:** the reference is sharp, stable, whole body in frame, from a camera with depth (three-quarter, plus a second
  angle when possible), at a known frame rate (higher for fast actions), with props of similar weight; several takes,
  the best parts edited into one clip.
- **Severity:** WARN.

### P-REF-04 — Keys at events, not at intervals
- **Rule:** key poses are picked where something happens: contacts, extremes of a weight shift, the moment the
  direction or the intention changes. Keys at regular frame intervals are the signature of sampling, not of choosing.
- **Measure:** key frames vs detected contacts/extremes of the reference; intervals between keys are irregular.
- **Severity:** FAIL when keys are evenly spaced (feeds `P-LIFE-01/02`).

### P-REF-05 — Keep the mechanics, push the timing and the poses
- **Rule:** keep from the reference the weight transfers, contacts, the order and offsets between body parts and their
  timing relationship. Push or cut: soft or slow realistic timing (tighten per phase, not by one global ratio),
  fidgeting, any motion that hides the main read; exaggerate arcs, anticipation and contrast while staying grounded.
- **Measure:** frame count per phase vs the reference (ratio per phase); offsets between parts preserved.
- **Severity:** WARN (FAIL if the result is more even than the reference: `stop_sync_share` higher than the reference's).

### P-REF-06 — Body first, details after
- **Rule:** block the root, spine and limbs first; props, hands, face and secondary parts after.
- **Severity:** WARN (process).

### P-REF-07 — Breakdowns before splines; stepped hides timing
- **Rule:** add the breakdowns for every weight event, anticipation, overlap and settle before switching to splines, and
  check a rough spline preview; stepped playback hides the real timing.
- **Severity:** WARN (process); the result is judged by `P-SPACE-04` and `P-LIFE-02`.

### P-REF-08 — Fresh eyes
- **Rule:** review the reference and the result flipped, without sound, and by a second viewer who has not worked on it.
- **Severity:** WARN (process); in agent pipelines, the blind critic plays the second viewer.

## Procedure (agents)
1. Intent, context, rhythm (`P-REF-02`). 2. Reference found or recorded (`P-REF-01/03`). 3. One edited clip of the
best takes. 4. Keys marked at events (`P-REF-04`); offset so the first key is frame 1. 5. Stepped blocking, body first
(`P-REF-06`). 6. Breakdowns at every weight event (`P-REF-07`). 7. Rough spline preview. 8. Spline in chunks: fix pops,
arcs, contacts; add the overlap and contrast the reference lacks (`P-REF-05`). 9. Measure (`P-MEASURE-02`) and compare
with the reference's own metrics. 10. Polish, then stop.

## Sources
Williams (reference and "don't copy it, use it"); Cooper (reference and motion capture in games); common practice
of body-mechanics curricula (sources.md).
