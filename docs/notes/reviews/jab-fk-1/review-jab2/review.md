# Review jab 2 Overlord, variant `jabfk` vs base `jdr1` — verdict: FAIL (big step up from base, not AAA yet)

Seen: 7 sheets in this folder (fixed world cameras, same video frames) plus `frames.jsonl` of both captures.
Note: the capture hitched at v39/v40 (same anim frame twice), so a9 = v43, not v42. Body numbers are actor-relative.

| Identity trait | New | Base | Frames / view |
|---|---|---|---|
| 1. Low lunge, STEP forward on the RIGHT foot, body drops | **NO (half)**: body drops 28 cm and travels 24 cm, but there is no right-foot step: the feet glide | no (spins in place) | v35-v46 az90 feet sheet, az270 |
| 2. Open-palm thrust forward, then arm across to the LEFT | yes: arm straight, shoulder height, open hand at v43; folded across chest by v48 | no (arms flail, body turns) | az270 v42-v48, az45 v43-v44 |
| 3. Recovery pulls back a little | yes, small: actor -2.3 cm, pelvis +4 cm in v56-v58, then cut by FAF | no | v55-v58 az90, loc in jsonl |
| Open hand, never a fist | yes | yes | az270 v43, az45 v42 |

- **Impact**: the thrust reads from the side (az270 v43) and the torso turn sells the sweep. The full extension is on screen for ONE frame (a9); at a10 the arm is already going down-left, so 2 of the 3 active frames show the sweep, not the palm.
- **Timing/spacing**: root moves 0.4 cm/frame a0-a5, then exactly 5.0 cm/frame a5-a9, then stops dead: linear between keys, no ease. The 90 degree torso turn happens in 2 frames (v43-v44). Hold a12-a20 is clean and still.
- **Arcs**: hand path cock (hip) -> forward -> down-left -> chest is a curve, not a line (az270 v38/v42/v43/v44/v48). With 2 in-betweens the sweep is more a snap than a readable arc; acceptable for a jab.
- **Weight/feet**: FAIL. Right foot is flat on the ground at its final spot from a3 (v36), not airborne until f8, so the O16 "right knee lifted forward" pose at a5 does not exist on screen (az270 v38: both feet down). Then both feet slide forward with the body: about 20 cm in v38-v44 (feet sheet: front shoe 400 -> 487 px, shoe = 125 px; jsonl: foot_r actor-relative constant while the actor moves 22 cm). The left foot also slides 12 cm after it lands (v41-v45). The foot that visibly lifts and steps is the LEFT one (v37-v41), ending 27 cm behind the right: narrow stance, not a deep lunge.
- **Legs**: PASS. No thigh/calf separation, no broken knee in any view (az0, az90, az180, az270, az45). Base had twisted legs.
- **Forward travel and end**: PASS. +24 cm by v45, 21.7 at the cancel, ends 19.4 cm ahead of the start in idle; no snap back. (Jab 1 after the cancel keeps drifting back 2.3 cm, v59-v70.)
- **Cleanliness (pops)**: FAIL. (a) v32->v33 feet teleport 17-29 cm, pelvis 17 cm: jab 2 f0 is not the pose jab 1 has when it is cut at a11, and jab 1 has the 16 cm root offset while jab 2 has none. (b) v33->v34 whole body drops in one frame: actor z 100 -> 88.15. (c) v58->v59 crouch to upright in one frame (pelvis +24 cm). (d) v83-v86 after jab 1 the character collapses 30 cm into a crouch in 3 frames and takes 40 frames to stand up.
- **Sink check (z 88.1)**: the visible character does NOT end lower (pelvis world z 92.2 start, 91.8 end). 88.15 = capsule half height 86 + 2.15: the END is the grounded state; at the start the pawn floats 11.85 cm with legs stretched down by foot IK, and the first root-motion move grounds it. Likely cause of pops (b) and (d); base never moves so it never shows. Cause not verified in code.

## FAIL items and fixes
1. `posa` v36-v45: feet slide, right-foot step missing. Pin planted feet in WORLD space (foot in component space = world pin minus root translation at that frame), not at a constant root-relative spot. Right foot: airborne a2-a8 with the knee lift of O16 at a5, lands a8 at its O19 world spot; left foot world-locked while it supports (a0-a8). After a8 the feet must counter-move the last 2 cm of root travel.
2. `posa` v38-v45: root curve linear. Ease it: slow out of a5, peak speed a6-a8, ease into a9-a12 (no 5 cm/frame then 0).
3. `posa` v43-v44 (advice, owner's key timing): palm contact pose lasts 1 frame. Hold O19 for a9-a10 and start the sweep at a11, or move the active window to a8-a10.
4. `dati montage` v32->v33 and v58->v59: no blend at the chain. Make jab 2 f0 match jab 1's pose at its cancel frame a11 (including the 16 cm root offset), or give AM_OverlordJab2V3 and the jab 1 restart a 2-3 frame blend in.
5. `gioco` v33->v34, v83-v86: pawn rests floating at z 100 and lands at the first root-motion move; foot IK pelvis offset then dips 30 cm at montage end. Ground the pawn at spawn in the capture (z 88.15) and re-capture; if the dip stays, reset the foot-IK pelvis offset when the montage ends.
6. `gioco` v59-v70 (minor): leftover backward drift during the next jab 1; zero the root-motion velocity on cancel.

Smash / AAA parity: no. Upper-body pose quality and identity are now there; sliding feet and the four pops would not pass.
