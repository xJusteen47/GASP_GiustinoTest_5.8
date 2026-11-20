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

## Update 19:15Z: no stamp (owner: "stomp" meant the run STOP)
Trait 1 of runstart-identity.md (0.3 s stomp) is withdrawn for the run start. New proposal, `setup_run_start_ramp.py`
(not applied yet, paper model `ramp_preview.py` / `ramp_preview.png`, the model reproduces the v4r3 capture):
- Start Stamp Frames 21 -> 0: the capsule moves from frame 1.
- Stage 1: AccelRampFrames 62 (~1 s), RampStartFraction 0.15: ease-in, ~85 cm/s at 0.2 s, 450 at ~0.6 s, no snap.
- Stage 2: AccelRampFrames 30, RampStartFraction 0.25: 450 -> 585 over ~0.25 s instead of a step.
- Model: t50 0.46 s (v4r3 0.52), stage 1 top 0.6 s, steady 1.41 s; the flat run phase 0.6-1.2 s is the stage design
  (Stages[0] FramesToNextStage 40): shorten it by hand if the run should flow straight into sprint 1.
Hand-tuning fields (DA_Locomotion_Default > Run Stages): Start Stamp > Frames; Stages[0|1] > Acceleration Ramp > Accel Ramp
Frames, Ramp Start Fraction; Stages[0] > Acceleration, Frames To Next Stage.
Needs from a pose thread before applying (else the feet slide in the first 0.3 s): AS_GSTN_RunStart_v4r* without the
planted stomp step: frantic cadence from the first contact, stride growing with the ramp (~0.85 m/s at 0.2 s, 4.5 m/s at
0.6 s), root motion off or matching; capture and Fable review together with this data change.
