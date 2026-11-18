# Review Overlord JAB 1 round 3 + JAB 2 chain check (capture `Saved/AnimR2/jabfk3/var_jab_r3`)
Looked at 8 of my own sheets cut from the raw 60 fps frames (sequence `jab1_jab2_faf`: az0, az90, az180, az270, az315, az45, game; plus `jab1_jab2` az270 for the full jab 2 recovery; one r2-vs-r3 game-camera sheet) and at the `frames.jsonl` body rows (actor space, cm: x forward, y right, z up) of rounds 2 and 3. The ready-made grids and the mp4 were not opened (image budget). Solo `jab1` not judged, as agreed. Frame map: jab 1 a0 = v21, jab 2 a0 = v33 (v45 and v46 repeat v44), jab 1 again a0 = v61 (from v66 the montage runs 1.04 a per frame). In `jab1_jab2`: jab 2 a0 = v33, montage ends v83.

Two instances of jab 1 give different feet for the same authored frame (item 1). The restart (v61+) matches `plan_jab1_r3.json` to 0.1 cm, so the pose is judged there; the from-idle instance (v21+) is what the player sees first and is reported separately.

| Jab 1 identity trait | Round 3 | Frames / view |
|---|---|---|
| 1 wide horizontal backhand L->R, ends arm open right | YES | hand y -52 (a4) -> +83 (a9), forward peak x 55 at a6; v65-v69 az315, v66-v69 az90 |
| 2 wind-up immediate and big, hand across left, weight left | YES | hand across by a2 (v23, v63), pelvis 18 cm left at a2-a4; v22-v25 az270 |
| 3 right foot steps (crossover in front, then out right), weight follows | YES | cross lifts 7 cm, plants a4 27 cm in front of and 23 cm left of the left foot (v65 az315, v64-v65 az90); swings out 22 cm high at a6, lands a9 at y +42; pelvis +35 cm right a4 -> a9 |
| open hand, never fist | YES | v67-v73 az315, v70 az45 |

| Jab 2 identity trait | Round 3 | Frames / view |
|---|---|---|
| 1 low lunge, step forward on the RIGHT foot, body drops | YES | knee lift v36-v40, lands v41 (a8) with dust; pelvis -22 cm, actor +23 cm (v35-v45 az270) |
| 2 open-palm thrust forward, then arm across to the left | YES | thrust v42-v43, across chest v45+ (az270) |
| 3 recovery pulls back a little | YES | actor -6 cm over a22-a32 (`jab1_jab2` v58-v67) |
| open hand, never fist | YES | v43 az270 |

## What got fixed since round 2 (confirmed)
- **Crossover (owner feedback, replaces r2 item 1).** Reads as a real step on the clean instance: left foot back a0-a2 (4-6 cm up), right foot lifts at a2, 7 cm up at a3, planted at a4, off again at a5. No ground drag, the left foot is planted from a2 to a21 so there is never a hop, no shin or knee clash visible in az315, az90, az270 (ankles 35 cm apart at the cross, 19 cm when the foot passes at a3). The foot in the air at contact (a6) and landing at a9 is in the owner's own keys (key 5 foot 14 cm up, key 7 planted), so r2 item 4 is closed by the owner's choice.
- **Overshoot and hold (r2 item 2).** Visible now: hand goes 9-10 cm past the hold at a10 (x -3.2 against 5-6), comes back by a12 with a 2 cm rebound, wrist and fingers change between v69, v71, v73 (az315) and the hand shifts about 10 px in az90 v70-v73. Only a13-a16 is near-still (1.5 cm), 4 frames instead of 8. Passes.
- **Start and end on the idle spots (r2 item 3).** a0 feet = idle within 0.2 cm; after the move left foot 1.0/-15.1, right -8.7/14.7 against idle 1.3/-15.4 and -8.9/14.8: no corrective step, no blend-in slide in the data (v85-v109). Return steps staggered and lifted 5-7 cm (v77-v85 az315).
- **Jab 2 first frame.** v32 -> v33: hand, pelvis identical (0.0-0.1 cm). Seamless.
- **Jab 2 recovery (r2 item 5).** Left foot now really steps: 6-7 cm up, v58-v65, lands on the idle spot (1.5, -15.3); right foot follows to (-8.7, 14.7); end = idle, no step after. On the FAF restart (v58-v63) the left foot is lifted 5-6 cm the whole way instead of dragged.
- r2 item 7 float (pawn 11.85 cm high until first root motion): not in this capture (feet z -78.7 from the start).

**Slap more centred than round 2?** Not in the hand path: hand and pelvis are the same as round 2 within 1 cm at a4-a8 (a6 hand 55/+15 in both rounds, arc y -52 to +83). What changed is the body under it: at contact the right foot is under the centre line (y +6) instead of already out right (y +20, landing +36 at a7), so the contact silhouette is narrow and centred and the body opens right only after the hit (v67 az315/az90). If the owner still reads the hit as off to the right, the arm arc itself must move (item 4).

## Verdict
- **JAB 1: PASS (pose).** Identity complete, crossover reads, overshoot visible, idle-to-idle clean. Smash / AAA parity on the clean instance: yes for the pose. In game from idle it is not there yet because of item 1 (`gioco`), which must be fixed before the owner judges it in play.
- **JAB 2: PASS.** Still passes in the chain; recovery step and seam are better than round 2. Items 5-7 are polish.

| # | Jab | Label | Frames | Problem | Fix |
|---|---|---|---|---|---|
| 1 | 1 | `gioco` | v22-v37 from idle (a1 -> jab 2 a4); same in `jab1_jab2` | only when jab 1 starts from idle, both feet are offset about 13 cm back and 12 cm left of the authored spots while pelvis and hands are exact (a2 left foot -34.9/-15.3 instead of -22.0/-3.4; a4 cross foot -8.9/-40.9 instead of 5.1/-26.4). The left step becomes 36 cm instead of 26 and the planted left foot then creeps about 15 cm back to its spot at 1-2 cm/frame (v23-v37; about 10 px in az270 v27-v32). Restart v63+ has 0.0 offset; round 2 did not show it | runtime leg layer (foot lock / foot placement / leg IK) when leaving idle, not the sequence: turn it off or snap it for the montage slot, then recapture. Check first by sampling the sequence alone at a2 |
| 2 | 1 | `posa` (advice) | a4 (v65, v25) | the crossed foot is on the ground for one frame only (lands a4, 10 cm up at a5), one frame after the body reaches the owner's key 1 at a3: reads as a tap more than a weight step | land the cross at a3 with the body key and lift at a5 (2 planted frames), or move the body key to a4 |
| 3 | 1 | `posa` (advice) | a0-a4 (v21-v25) | two steps in four frames: left foot 26 cm in 2 frames, right foot 43 cm in 2 frames. Clean poses frame by frame, but at speed it is a shuffle | if the owner finds it busy: shorten the left back-step to about 15 cm; keep the cross |
| 4 | 1 | `posa` (owner question) | a4-a9 | hand arc in actor space is unchanged from round 2 (forward peak at y +15, hold at y +82): the "hits too far right" remark is answered only by the feet | if still off-centre for the owner: turn the swing about 10 deg left (peak at y 0, hold at y about 65) in keys 5 and 7 |
| 5 | 2 | `posa` (advice) | a29 (`jab1_jab2` v64) | recovery steps overlap: left foot still 3 cm up while the right is already 4 cm up, one frame with both feet off the ground | start the right-foot step at a30 (plan key 28 -> 30) |
| 6 | 2 | `posa` (advice) | v42-v43 (a9-a10) | unchanged from r2 item 6: palm contact on screen 1 frame, then the hand travels 66 cm in one frame | hold the palm a9-a10, sweep from a11 |
| 7 | 2 | `posa` (advice) | v47-v57 (a12-a22) | low hold is static for 10 frames (hand drift 3 cm), same kind of dead hold fixed on jab 1 | small settle: hand overshoot 6-8 cm at a12, back by a15, chest breathing |
| 8 | both | `gioco` | v44-v46; v61-v72; az0 v68+ | montage time stuck at 10.99 for 3 frames (hitch, both sequences); 2.3 cm backward actor drift during the restarted jab 1 (as r2 item 7); capture: az0 camera is fixed and the actor walks into it after the lunge (lens inside the head from v68), game camera shows the fighter about 40 px tall, useless for a pose check | find the 2-frame stall at jab 2 a11; zero root-motion velocity on cancel; move az0 back 1.5 m and add a closer game-camera crop |
