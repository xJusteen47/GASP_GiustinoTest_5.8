# Review Overlord JAB 1 — variant `jabfk` (FK copy of owner keys) vs base `jdr1`
Looked at 7 sheets (base on top, new below, same video frames): `s1`/`s2` az0, `s3` az90, `s4` az180, `s5` recovery, `s6` game, `s7` az315+az270 (new only). Numbers = `body` rows of `jabfk/.../frames.jsonl` (cm, actor space; base has no body rows).

| Identity trait | New | Frames / view |
|---|---|---|
| 1 wide horizontal backhand L→R, ends arm fully open right | YES | v26–v30 az0, az180, az270 (`s2`,`s4`,`s7`) |
| 2 wind-up immediate and big, hand across left, weight left | YES, hand lower than asked | across already at v22 (a1), held v23–v25; hand sits on left collarbone, ~13 cm under the jaw (az270 `s7`) |
| 3 right foot steps out right, weight follows | PARTIAL | weight: pelvis 37 cm to the right v24→v30 (clear, az180). Step: only ~10 cm, done in 1 frame v24→v25 |
| open hand, never fist | YES | v26–v30 az90, az270 |

- **Legs (the owner's complaint): fixed.** Base shows thigh/calf detached at v23–v25 and v29–v31 (az0, az180); new has no separation or broken knee in any view.
- **Impact**: a6 (v27) arm at full reach in front, shoulder height, open hand, torso unwound. Reads.
- **Timing/spacing**: hand travel per frame a3→a9 = 7, 29, 60 (contact), 47, 26, 9 cm: accelerates into contact, eases out. Snappy, Smash-like.
- **Arcs**: hand path seen from above is a round arc (radius ~60–70 cm), elbow leads at a5 (v26). No straight line. Left hand on hip is a good counterpose.
- **Weight/feet**: planted and stable v26–v32 (right foot creeps 2.7 cm inward over 6 frames, negligible).
- **Recovery**: open pose held a9–a16 completely still, arm lowers a17–a24, then see F3/F4.
- **Cleanliness**: pops at a0–a1 and a4 (F1, F2).

## Verdict: FAIL (narrow). The variant is clearly better than base and should replace it; arm, impact, arcs and legs pass. Feet at start-up and the end of the move do not pass a Smash / AAA frame-by-frame check.

| # | Label | Frames | Problem | Fix |
|---|---|---|---|---|
| F1 | `posa` | v20→v22 (a0–a1) | pelvis/head pop 16 cm back-right at a0 while feet stay, then at a1 right foot slides 14 cm back **on the ground** and left foot jumps 9.5 cm forward; pelvis goes 9 cm right then 11 cm left in two frames | f0 = game idle stance (feet and pelvis where the game leaves them); left foot pinned in position (rotation only) for the whole clip; no foot translation before f3 |
| F2 | `posa` | v24→v26 (a3–a5) | left foot flicks 20 cm back-left then 17 cm right in two frames, both feet off the ground (4–6 cm) at a3–a4: one-frame wide stance at v25 (az315 `s7`, az90 `s3`) | same left-foot pin f0–f16; only the right foot steps, f3→f6 with a small lifted arc, landing on contact |
| F3 | `dati montage` | v44–v45 (a23.8) | montage ends at a24 but the return to idle is keyed to f28: right hand still 25 cm out and pelvis 12 cm right when it is cut | retime return f16→f24, or extend montage/timeline to 28 |
| F4 | `gioco` | v45–v63, sequence `jab1` | after the montage the body drops into a deep crouch (pelvis −30 cm by v48, head −45 cm) and is not upright again at v62. **Identical in base**: not caused by this change | find what plays after the timeline ends (state field `fl` changes at v45–v48) and blend montage → idle directly |

Not failing, to confirm with the owner: step width of trait 3 (if his O5/O7 have a wider step, the foot IK is eating it; otherwise widen to ~20 cm); hand height at O1 (collarbone vs jaw); hold a9–a16 would feel more alive with a 1–2 frame overshoot/settle.
Capture notes: in new `jab1` the mesh starts 30 cm ahead of the capsule and drains 1.3 cm/frame during v25–v38 (whole body incl. feet slides; absent in `jab1_jab2_faf`, so judged feet on that sequence). New `game` view camera is too far (character ~40 px) to judge gameplay-camera readability; base and new cameras differ in every view.
