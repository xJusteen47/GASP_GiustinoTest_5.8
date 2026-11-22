# Review Overlord JAB 1 + JAB 2, round 2 (capture `Saved/AnimR2/jabfk2/var_jab3`)
Looked at my own sheets cut from the raw 60 fps frames (az0, az90, az180, az270, az315, az45; chain v21-v69 and solo `jab1` v18-v94) plus `frames.jsonl` body rows turned into world cm. `docs/notes/reviews/jab-fk-2/variant/` did not exist when I ran, so the ready-made sheets and mp4 were not viewed. Frame map in this capture: jab 1 a0 = v21, jab 2 a0 = v33 (hitches: v40 and v43 repeat a frame), jab 1 again a0 = v60.

| Jab 1 identity trait | Round 2 | Frames / view |
|---|---|---|
| 1 wide horizontal backhand L->R, ends arm open right | YES | v26-v29 az0, az90, az180 |
| 2 wind-up immediate and big, hand across left, weight left | YES | hand across by v23-v24, torso turned ~90 deg left v24-v26 |
| 3 right foot steps out right, weight follows | YES (was partial) | foot lifts 16 cm, lands v28 (a7) 25 cm wider than idle; pelvis +32 cm right |
| open hand, never fist | YES | v27-v33 az0, az315 |

| Jab 2 identity trait | Round 2 | Frames / view |
|---|---|---|
| 1 low lunge, step forward on the RIGHT foot, body drops | YES (was no) | knee lift v36-v41, lands v42 (a8) with dust, 41 cm step; pelvis -27 cm, actor +24 cm (az270, az0) |
| 2 open-palm thrust forward, then arm across to the left | YES | v44 az270/az45, across chest v46 |
| 3 recovery pulls back a little | YES, small | actor -1.8 cm v57-v59 |
| open hand, never fist | YES | v44-v45 az45 |

Fixed since round 1 (confirmed on frames): jab 1 hip pop at a0 (pelvis now 3, 6, 6 cm/frame), left-foot flick at a3-a5, return to idle (a23.6 hand and pelvis within 2 cm of idle, no cut); jab 2 feet no longer glide (left foot holds within 3.5 cm, right foot really steps), root eased (0.3, 1.5, 3.5, 6, 6, 3.5, 1.8, 0.4 cm/frame), chain v32->v33 is seamless (every joint < 0.2 cm), legs clean in all views. The z 100 -> 88 landing at v33->v34 no longer shows (feet stay, pelvis -4 cm). No crouch after the montage in either sequence of this capture (pelvis z 91-92 to the end).

## Verdict
- **JAB 1: FAIL (narrow).** Arm arc, contact, step and recovery pass; the wind-up feet and the dead hold would not pass a Smash / AAA frame-by-frame check.
- **JAB 2: PASS.** Identity complete, step reads, no pops, no slide. Items 5-6 below are polish, not blockers.

| # | Jab | Label | Frames | Problem | Fix |
|---|---|---|---|---|---|
| 1 | 1 | `posa` | v22-v26 (a1-a5) | right foot is dragged 23 cm inward at 1-5 cm height (11 cm/frame at a2, a3) and parks behind the left ankle, 8 cm apart: shins cross, shoes touch (az270, az315, az0 v24-v26) | keep the right foot planted a0-a3 (heel pivot with the hip turn only), lift at a4, same step out; or stop the gather >= 18 cm from the left foot with a 6+ cm lift |
| 2 | 1 | `posa` | v30-v37 (a9-a16) | the overshoot/settle is in the data (hand path 1-1.5 cm/frame, 4 cm total) but invisible: 8 identical frames, arm and wrist read as held (az0, az315) | overshoot the hand 8-10 cm past the hold at a8-a9 with wrist drag (~15 deg), settle back by a12, elbow breathing 5-8 deg |
| 3 | 1 | `posa` | v21-v26 (a0-a5), v88-v100 | "pinned" left foot is authored 6 cm behind the game idle foot: it slides back during the 0.1 s blend-in, and the game does a small corrective step after the move | put the pin on the game idle foot position (actor space x +1.5, y -15.3) |
| 4 | 1 | `posa` (advice) | v27-v28 (a6-a7) | at contact a6 the stepping foot is at its highest (16 cm up), lands one frame late | land the foot on a6 |
| 5 | 2->1 | `dati montage` | v58-v64 | jab 1 restart after FAF: left foot dragged 20 cm forward at <= 2.5 cm height, right foot 13 cm back on the ground | in jab 2 recovery a17-a24 step the left foot up under the body (6-8 cm lift) so the FAF pose has idle-relative feet |
| 6 | 2 | `posa` (advice) | v44-v45 (a9-a10) | palm contact pose still on screen 1 frame, then the hand travels 66 cm in one frame | hold the palm a9-a10, sweep from a11 |
| 7 | both | `gioco` | v33->v34; v60-v72; solo `jab1` v21-v38 | pawn floats 11.85 cm until first root motion (not visible now); 2.3 cm backward actor drift during the restarted jab 1; in solo `jab1` the mesh starts 40 cm ahead of the capsule and drains during the montage, whole body slides (capture artifact, judge jab 1 on the chain sequence) | ground the pawn at spawn; zero root-motion velocity on cancel; wait for mesh offset = 0 before pressing |
