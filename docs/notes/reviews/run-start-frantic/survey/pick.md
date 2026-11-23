# Run start: which clip, what timing (owner request 2026-11-18 23:41)

Owner: "take the run where arms and legs move frantically for some time; the Toad video gives the timing".

**Clip pick: `standard_run` (Male Locomotion Pack).** It is the only one of the 3 where *both* arms and legs
move big: arm swing 124/106 deg (run_2: 20/46, sword run: 111/30 because the shield arm is held).
Its cadence (2.73 steps/s, cycle 0.73 s) is the slowest, so frantic comes from play rate, not from the clip:
at x1.9 the cycle is 0.39 s = ~5.2 steps/s. (`run_clips.csv`, `standard_run_grid.png`.)

**Toad timing** (`docs/notes/reviews/ref-smash-run/toad_a_closeup.jpg`, commit da69f433, 30 fps video,
one look at the grid):
- frame 24 (0.80 s): still standing.
- frames 26-36 (0.87-1.20 s): limbs blurred, almost no travel = scramble in place, ~0.33 s = ~21 sim frames at 62.5 Hz.
- frames 36-54 (1.20-1.80 s): travelling, limbs still blurred, dust/water trail = frantic run, another ~0.6 s.
- Total frantic phase ~0.9 s (~58 sim frames).

**Conflict with the brief** (`docs/notes/mosse/run-start-stop-skid-brief.md` §1): the brief ties the scramble to sim
speed (ends at ~90% target speed, ~13 sim frames = 0.2 s at full stick). Toad's is ~4x longer and the first
third happens almost in place. Matching it means either a fixed-time presentation layer or a sim change
-> owner decision (see report).
