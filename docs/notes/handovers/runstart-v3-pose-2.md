# runstart-v3-pose-2: Overlord run start, pose round 3 of 3 (desktop, 2026-11-20)
Branch claude/runstart-v3-pose-2 (from claude/runstart-v3-pose-1 52d88705). _ue59 lock taken and freed. Re-check (rule 11): v4r2
assert_base1_a1.txt read, ASSERT: OK f100-180 as stated.

## Owner feedback v4r2 -> result
1. Arms into the body (posa): runstart_restyle_casc_v4r3.py, fist passes the hip OUTSIDE the thigh (+14 cm out at the bottom, -9 cm at
   the chin instead of -12), elbow pole wider; arc, radius, timing unchanged (13e). runstart_arm_clearance.py (rig-point capsules, approx):
   v4r2 min -10.1 cm fist_r-thigh_r f20, 18/69 frames < 0; v4r3 min +1.8 cm (f46), stomp f0-11 drift 0.001 cm.
2. Faster first steps (posa): compose v4 with RATE_OVERRIDE (8,.6)(11,.6)(14,1.95)(24,1.95)(36,1.3)(46,1)(48,1) and K_OVERRIDE
   (8,.72)(11,.72)(14,.54)(24,.54)(36,.8)(46,1)(48,1) on a fresh copy of RunStart_v0_gaspZ (restyle gets the same RATE): contacts at 30 fps 8 / 5 5 5 / 7 8 9 10 (v4r2 9 / 7 6 7 / 8 10 9), stride shortened in proportion. Root travel
   998.7 cm (v4r2 994.4; AS_GSTN_RunStart_v4r3_build.json), in game f80-200 794 cm (v4r2 785; metrics_v4r*.json, actor_x f200-f80).
3. Torso pops when rising: capture chain checked first. Every base-1 run start capture (stock clip too: Saved/RunStart1/base1) has
   frozen frames f109/f112: sim frame (historyFrame), anim time and image identical to the previous frame (az90 mean diff 0.5 vs 7-10),
   then a double jump. It sits on the rise after the stomp. v4r3 capture: f109 only. Not the clip (torso pitch d2 <= 1.7 deg/f^2).
   Not in other sequences (jab, dair, dropkick captures: none). Label GIOCO, code queue: find what stalls the sim ~29 frames after the
   stick on base 1 (NP fixed tick 62.5 Hz vs t.OverrideFPS 60 normally gives double ticks every 24 f, never zero ticks).
   Second candidate (Fable): torso rising 40 -> 20 deg in the blend to the loop f176-192 (same as v4r2, d1 <= 1.7 deg/f, smooth).

## Unreal
AS_GSTN_RunStart_v4r3 (Save-Assets runstart_v4_ue.py), rows: v4r2 off, v4r3 on (rows/runstart_v4r2off.txt, runstart_v4r3_new.txt).
Capture Saved/RunStartV4R3/hires (EDITOR_WIN=full): ASSERT: OK f100-180 min 0.94. Evidence docs/notes/reviews/runstart-v4r3/.
Review Fable r4: PASS (review_fable_r4.md). Owner videos owner/runstart_v4r3.mp4, _025x.mp4.

## Open
1. gioco: frozen frames f109/f112 above (code thread, then recheck "scatta quando sale" with the owner).
2. verify: planted-foot slide f80-240 R 73.6 cm vs 52.1 (heuristic; per window f130-180 L+R 22 vs 19, f80-130 34 vs 29): not seen by eye.
3. Not verified: GASP curve list vs ABP, clip in isolation.
4. Posa: round 3 of 3 used; next pose change needs the owner.

COSTO: ~16 $ | ~8 min desktop | review ~0.10M token
