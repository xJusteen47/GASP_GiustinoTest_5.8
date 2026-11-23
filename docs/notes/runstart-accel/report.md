# Run start: acceleration ramp, Overlord v4r3 vs references (2026-11-20, cloud, read only)

Question (owner 18:40Z): does the "takes a while to get going" feel of Astro Bot / DK Bananza come from a gradual
acceleration rather than a standstill at the start? Measure time to steady speed and the ramp shape; compare with ours.

## Overlord v4r3 (measured)
Source: `docs/notes/reviews/runstart-v4r3/metrics_v4r3.json` (per-frame actor_x, 60 fps capture, branch
claude/runstart-v3-pose-2 e8cc43aa). Command: `python docs/notes/runstart-accel/speed_curve.py <json> speed_v4r3.png`
(12-frame centred window; t=0 = f82, first frame the anim reacts to the stick). Plot: `speed_v4r3.png`.

| Phase | Time from stick | Speed | Cause in the data (defaults in GSTN_RunStageTypes.h) |
|---|---|---|---|
| Hold | 0 - 0.37 s | < 60 cm/s (10%) | Run start stamp: 21 f @62.5 Hz, AccelFraction 0.05, MaxStartSpeed 60 |
| Snap 1 | 0.37 - 0.6 s | 60 -> 450 (77%) in ~0.23 s | Stage 1 Acceleration 1800 cm/s^2, AccelRampFrames 0 (no ramp) |
| Shelf | 0.6 - 1.25 s | flat 450 | Stage 1 MaxSpeed 450; FramesToNextStage 40 (0.64 s) |
| Snap 2 | 1.25 - 1.4 s | 450 -> 585 (100%) | Stage 2 entered, same hard acceleration |
| Steady | > 1.4 s | 585 cm/s (9.75 cm/f) | (636 peaks = double sim tick 62.5 vs 60 Hz, not real) |

t10% = 0.37 s, t50% = 0.52 s, t90% = 1.30 s. Distance after 1.0 s: 251 cm (41% of a full-speed second).
Shape = hold -> snap -> shelf -> snap. The total is long (1.3 s), but the speed arrives in two steps, each in ~0.2 s:
the eye reads "stands still, then is already running". Asset values not read (binary): the 450 shelf and the 0.64 s step
match the code defaults exactly, so the profile is most likely on defaults.
Also known (handover runstart-v3-pose-2): on base 1 the clip enters at 0.167 s, so part of the stomp is skipped.

## References
- Astro Bot: NO video found. Searched project files (uploads: 5 owner videos = 2 DK Bananza, 3 Smash Ganondorf; none
  Astro Bot), repo (only VFX/camera stills in reviews/ref-astrobot-*), desktop `Unreal Projects` (no video). YouTube is
  blocked from the cloud. Needs the owner's clip -> measure with the same method (track character vs ground per frame).
- DK Bananza `0aa3dec5` (19 s, 30 fps): jumps and punches in place, no clean run from standstill: not usable.
- Toad (Smash, earlier measure in memory): ~0.6 s of frantic in-place steps, then run (timing only).

## Options (pending the Astro Bot numbers)
A (recommended, data only, no code): keep the 0.33 s stamp (identity trait), replace both snaps with one continuous
  ramp: stage 1 AccelRampFrames ~30 + RampStartFraction ~0.3 (ease in), and a softer stage 1->2 (lower stage 2
  acceleration or NextStageSpeedFraction) so 450->585 blends over ~0.3 s. Target shape: speed rises from the end of
  the stomp to 90% in ~0.8-1.0 s with no flat shelf; the clip's cadence then reads as the "engine revving". Tune the
  numbers to the Astro Bot curve once measured. Cost ~4-6 $ (setup script + 1 capture + review).
B (owner decision, touches trait 1): let the capsule creep during the stomp (AccelFraction 0.05 -> ~0.2, MaxStartSpeed
  60 -> ~150) so there is no "standing still" phase; the stomp becomes a push-off. Same cost as A.
C Animation only: keep the capsule, make the clip carry the gradual feel (stride length grows, cadence already high).
  Cheapest on sim, but the capsule snap stays visible against the ground: not recommended alone.
Rule 14: run stages are our divergence from stock GASP Mover; any change needs its line in docs/notes/gasp-divergenze.md.

## Stock GASP Mover (Unreal Assistant, 2026-11-20, desktop; Saved/Assistant/replies.json, log Saved/Logs/ci/ask_accel.log)
Stock shapes standstill -> run with the walking mode's Acceleration / MaxSpeed smoothed by a critically damped spring (exponential
ease-in, no curve asset); the stock way to a gradual start is a longer acceleration smoothing time and/or lower Acceleration, no extra
ramp. A linear ramp on top is "compatible" but double-eases and can drift from the GASP trajectory used by Motion Matching (names not
confirmed by the docs; it asked for the header). Our run stages already replace the stock speed profile (divergence row 13).

## Applied (desktop, runstart-accel-1-desktop)
Stage values found in DA_Locomotion_Default differ from the code defaults: stage 1 470 cm/s / 2000 cm/s^2 with a 6 f / 0.3 ramp already,
stage 2 610 / 1400, stage 3 850 / 1200, stamp 21 f (setup_run_start_ramp.txt). Set: stage 1 ramp 31 f / 0.15, stage 2 30 f / 0.25.
Measured (capture Saved/RunStartAccel/measure, base 1, KIT_Test_Overlord, mv_run_start_v4s4, ASSERT: OK min 0.96; metrics_accel.json,
`REF=../reviews/runstart-v4r3/metrics_v4r3.json python speed_curve.py metrics_accel.json speed_ramp.png`, F0 82 same as v4r3):
t10/t50/t90 = 0.40 / 0.70 / 1.52 s (v4r3 0.37 / 0.52 / 1.30); distance after 1 s 183 cm (v4r3 251). Shelf at ~470 from 0.8 to 1.45 s
still there (FramesToNextStage 40), then stage 2. No owner video made.

## PAUSED (owner 19:12Z): the stomp was meant for the run STOP (see update below). DA_Locomotion_Default
restored to its git version (ramp NOT applied); script and measure kept for when the brief is confirmed.

## Update 19:15Z: no stamp (owner: "stomp" meant the run STOP)
Trait 1 of runstart-identity.md (0.3 s stomp) is withdrawn for the run start. New proposal, `setup_run_start_ramp.py`
(not applied yet, paper model `ramp_preview.py` / `ramp_preview.png`, the model reproduces the v4r3 capture):
- Start Stamp Frames 21 -> 0: the capsule moves from frame 1.
- Stage 1 (DA: 470 cm/s, 2000 cm/s^2, ramp 6 f / 0.3): AccelRampFrames 62 (~1 s), RampStartFraction 0.15: ease-in, ~95 cm/s at 0.2 s,
  470 at ~0.57 s, no snap.
- Stage 2 (DA: 610 / 1400): AccelRampFrames 30, RampStartFraction 0.25: 470 -> 610 over ~0.25 s instead of a step.
- Model (DA values, ramp_preview.py): t50 0.43 s (current 0.50), stage 1 top 0.57 s, steady 1.34 s; the flat run phase 0.6-1.2 s is the stage design
  (Stages[0] FramesToNextStage 40): shorten it by hand if the run should flow straight into sprint 1.
Hand-tuning fields (DA_Locomotion_Default > Run Stages): Start Stamp > Frames; Stages[0|1] > Acceleration Ramp > Accel Ramp
Frames, Ramp Start Fraction; Stages[0] > Acceleration, Frames To Next Stage.
Needs from a pose thread before applying (else the feet slide in the first 0.3 s): AS_GSTN_RunStart_v4r* without the
planted stomp step: frantic cadence from the first contact, stride growing with the ramp (~1 m/s at 0.2 s, 4.7 m/s at
0.57 s), root motion off or matching; capture and Fable review together with this data change.
