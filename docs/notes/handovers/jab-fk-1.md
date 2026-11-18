# Handover: Overlord jab, owner keys copied bone for bone (FK), round 1 built + captured, Fable FAIL x2 (desktop, 2026-11-18)
NOT committed (plumbing commit to a new branch denied; this checkout is another thread's branch). Everything is untracked or modified in
`GASP_Giustino2_ue59`: tool + plans in `Tools/AnimPipeline/cards/overlord/jab/fk/`, game clips `Content/GASNPPTest/Combat/Overlord/AS_OverlordJab{1,2}V3.uasset`
(modified; r1 copies in `/Game/AnimPipeline/Backup/AS_OverlordJab{N}V3_r1_2026-11-18`, not versioned). Next thread: put them on `claude/desktop-jab-fk`.

## Diagnosis (the owner's "retargeting errors")
- The owner's Cascadeur rig IS the UE mannequin skeleton: same bones, local translations equal with Y negated. Raw keys
  (`owner_keys_2026-11-15b.json`) obey G_child = G_parent @ L_child (col vectors, err 0.0015). `casc_to_author.py` reduced them to IK targets and
  dropped pelvis shift (to 33 cm), hand rotations (47-103 deg), clavicles (10-28), foot yaw/roll (to 56), head roll (`scratch loss.py`, numbers above).
- `casc_fk_take.py <plan>`: G_ue = P G_casc S (P swaps Y/Z, S = diag(1,-1,1)); slerps LOCAL rotations between keys (FK arcs, owner principle for
  the slap); `foot_paths` = IK only to pin feet / FK arc + correction for steps; `root_forward` + `keys[].shift`. Key check: hands/head 0.002 cm.
  `ue_write_take.py --keep-root` (copy in fk/) writes the root track. Bone lengths PASS 72/72. Publish: `fk/pub_jab.py` headless (Run-Timed -Kind Test).
- Capture `Saved/AnimR2/jabfk/var_jab` (same frame map as base jdr1: jab1 v21, jab2 v33, re-jab v59); mp4 in `docs/notes/reviews/jab-fk-1/variant/`.

## Fable reviews (`docs/notes/reviews/jab-fk-1/review-jab{1,2}/review.md`) - both FAIL, both clearly better than base, no leg separation now
Jab 1 (round 1 of 3): F1 posa a0-a1 pelvis pops 16 cm + feet slide (owner O0 stance != game idle stance: start f0 from the game idle, see
memory pipeline-root-and-stance); F2 posa a3-a5 left foot flicks, both feet airborne: pin left foot f0-f16, only the right foot steps f3->f6 landing on
contact; F3 dati montage: montage ends a24 but return keyed to f28 (retime return f16->f24); F4 gioco crouch after montage (known, ABP).
Jab 2: trait 1 half (drop + travel yes, but feet slide ~20 cm v36-v45 and the right-foot step is not visible). Most likely ROOT DOUBLE-COUNT:
capsule moves by root motion AND the pose keeps the root translation (unchecked in code: how NetMontageSimulator/ABP consume it). Check first;
if so, author feet as world pin minus root delta. Also: ease root (linear 5 cm/frame), hold contact O19 a9-a10, jab2 f0 must equal jab1 at
cancel a11 (feet jump 17-29 cm at v33), pawn floats 11.85 cm at spawn in the capture (gioco), 2.3 cm drift into the next jab (gioco).
To confirm with owner (coordinator): jab 1 step width, wind-up hand 13 cm below the jaw at O1, settle on the dead hold a9-a16.

Cost: ~140k context (Opus) + 2 Fable review subagents (~270k tokens). Editor lock released.
