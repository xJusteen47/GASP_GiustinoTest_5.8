# L1 · Measuring animation (`P-MEASURE`)

Level 1. How to turn the criteria into evidence. Measurement never replaces looking; it tells you **where** to look
and it catches the lifeless signatures that a quick look at a clean clip misses.

### P-MEASURE-01 — Derive the judgement in four steps
1. **Intent:** what the move is for (gameplay function, feeling, mass class from the profile).
2. **Prediction:** what the principles predict for that intent (which IDs apply, which ranges).
3. **Observation:** what the frames and the numbers show (frame numbers, view).
4. **Difference and why:** where 2 and 3 differ, and the likely cause (pose, data, system).
A number outside a range is not a FAIL by itself; a FAIL needs step 4.

### P-MEASURE-02 — The isolation report (every clip, before in-game review)
Run `tools/animkb/kb_motion.py metrics <clip.bvh>` (FBX: `tools/animkb/fbx_to_bvh.py` first) and attach the json:

| Field | Criterion | Indicative reading |
|---|---|---|
| `stop_sync_share` | `P-OVERLAP-01`, `P-LIFE-01` | mocap 0.00-0.25; lifeless 0.88-0.96; > 0.5 FAIL |
| `even_spacing_run_max` | `P-SPACE-02` | hands/head 2-11 f in mocap; > ~12 look again |
| `arc_bulge_median` | `P-ARC-01` | hands/head 0.03-0.10 in mocap; < 0.01 on long moves = straight |
| `dead_frames` | `P-HOLD-01` | 0 in mocap; unintended runs ≥ 3 f FAIL |
| `moving_hold_frames` | `P-HOLD-02` | context: holds should be moving holds |
| `foot_contacts` | `P-CONTACT-01/02` | each foot has plants in locomotion; slide within a few % of height incl. roll |
| `pelvis_pop_ratio` | `P-ARC-02`, `P-GAME-04` | spikes far above the clip's own median = pop |
| `pelvis_vertical_range` | `P-WEIGHT-03/04` | context for dips and absorption |

Limits: clips shorter than ~0.5 s give too few speed minima for `stop_sync_share`; judge those by eye with the overlap
figure. Units are % of standing height and frames at 60 fps, so numbers compare across skeletons.

### P-MEASURE-03 — Calibration data (how the indicative ranges were obtained)
Five free motion-capture clips (CMU 02_05 punch, 16_08 run and sudden stop, 16_05 forward jump, 16_41 run with 90°
turn, 09_01 run), first frame (T-pose) removed, measured as they are and after a **lifeless rebuild**: key poses every
0.2 s, every channel eased between keys with smoothstep. Results in `figures/metrics-cmu-vs-lifeless.json`.
The rebuild is not a straw man: it is what a script that "interpolates a few poses" produces, and it passes foot,
loop and pop checks about as well as the mocap.

### P-MEASURE-04 — Figures to make for a review
`kb_motion.py figures <clip> --vs <reference> --kind K`:
- `overlap`: angular speed along the arm chain (lockstep vs staggered peaks), `P-OVERLAP`, `P-LIFE-01`;
- `spacing`: spacing ruler per effector, `P-SPACE`, `P-LIFE-02`;
- `contact`: toe height and speed + pelvis height, `P-CONTACT`, `P-WEIGHT-04`;
- `weight`: trunk mass centre against the span between the feet, `P-WEIGHT-01`;
- `arcs`: effector paths, `P-ARC`;
- `strip`: side-view stick figures every N frames (timing and poses at a glance).
`--vs` takes a reference clip of the same class (free mocap, a licensed library clip, an approved earlier version).

### P-MEASURE-05 — Video still decides
The in-game video from the game camera and from the side is the final evidence (`P-GAME-08`). Numbers that pass with
a video that looks wrong mean a criterion is missing: write it down and add it here with a source.

## Figures in this folder

| File | Shows | Criteria |
|---|---|---|
| `figures/A-overlap-punch.png` | punch: mocap staggered peaks vs lifeless lockstep | P-OVERLAP-01/02, P-LIFE-01 |
| `figures/A-spacing-punch.png` | punch: spacing rulers | P-SPACE-01/03, P-LIFE-02 |
| `figures/A-contact-stop.png` | run stop: contacts and pelvis | P-CONTACT-01/02, P-WEIGHT-04 |
| `figures/A-contact-jump.png` | jump: absorption after contact | P-WEIGHT-03/04, P-LIFE-05 |
| `figures/A-weight-stop.png` | run stop: mass centre vs feet | P-WEIGHT-01 |
| `figures/A-arcs-turn.png` | run turn: paths | P-ARC-01 |
| `figures/A-strip-*.png` | stick-figure strips (stop, punch, jump) | timing, poses |
| `figures/D-spacing-charts.png` | five spacings, same timing | P-SPACE-01/03 |
| `figures/D-favouring.png` | breakdown placement | P-SPACE-04 |
| `figures/D-overlap-chain.png` | lockstep vs successive breaking | P-OVERLAP-02 |

Motion data: CMU Graphics Lab Motion Capture Database (mocap.cs.cmu.edu), BVH conversion by B. Hahne (cgspeed);
both state the data may be used freely, the database asks for acknowledgement: "The data used in this project was
obtained from mocap.cs.cmu.edu. The database was created with funding from NSF EIA-0196217."
