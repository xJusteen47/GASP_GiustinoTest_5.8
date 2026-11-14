# Overlord dropkick (Fair), offline pose pass r1 (2026-11-14, no editor)
Route: `<move>.plan.json` -> pose_plan.compile_plan -> card with a dense `author` block (every frame posed) + `_stick.png`
(rows side / behind = game camera / top). Originals kept as `<move>.r0.backup.json`; old `_design` kept as `_design_r0`.
**From the Antigravity worktree** (read only): the three cards and `_v1` there are byte-identical to ours; nothing else
usable. Its poses had arms crossed on the chest, a land that never reaches the floor (pelvis +10 above idle) and a v1
legibility FAIL; `renders/dropkick_suite_strip.png` shows only idle. Reused: section lengths, key frames and pose names,
take/anim paths, exemptions, sheet_extra, the 80-deg lay-back idea. Arms crossed was replaced (brief: arms counter, wide).
**Startup 0-15** (keys 0,1,2,3,4,8,11,12,13,14,15; names kept at 0/4/8/11/15). f0 breaks from the fall (knees up, arms
lifting out); f1 carries 55% of the tuck (biggest early step, 24.5 cm); f4 hard tuck, knees 38 cm above the hip, arms
thrown forward and wide (left higher); f4-f11 the coil stays alive and rocks back (pitch 38->52); release 6/10/26/58% on
f12-f15, so f14->f15 = 45.8 cm is the largest step of the section, ON the first active frame (CSV: f15 active).
f15 strike: pelvis pitched 70 deg + legs 25 deg hip flexion (slight pike), barrel roll 28 deg (left hip up), yaw 12 deg;
spine -12, head -42 (looks over the feet); left arm flung overhead-back-wide, right arm out-back-low; villain hands
(fingers 1, wrist_out 18). Body line head->left ball 4.7 deg from horizontal.
Reach f15: left ball 84.6 cm ahead of the pelvis, 72 cm ahead of the root, at z 87 (right ball 21 cm lower, bent 0.95).
From behind (offsets from the pelvis, x/z cm): balls -33/+21 and -38/0, left hand +48/+53, right hand -37/-18, head
+8/+32: legs and both arms leave the trunk outline in an X; no limb hides behind the pelvis.
**Loop 0-11** (every frame keyed, period 12): startup f15 == loop f11 (0.0 cm), so loop f0 follows the startup exactly as
it follows its own f11. Legs recoil 3-4% on f0-f3, re-extend slowly f3-f11; pelvis breathes +-1.5 cm; trunk, head and
arms trail 1-2 frames. Steps 0.9-3.4 cm per frame, seam f11->f0 1.63 cm (same size as the others), never frozen.
**Land 0-31** (keys 0,1,2,4,6,8,10,12,16 hold,31 hold). f0 pelvis and low back touch, shoulders curled, legs still up;
f1-f2 slam/squash (heels, back and palms flat, head whips back); f6 bounce: pelvis z 24.1 (= +13.5 over the squash,
+12 over lying); f10 second touch; f16 = `reaction_lieup` exactly (0.0 cm on 14 bones), held still to f31.
Lowest surface per key (bone z - radius, cm): f0 pelvis 1.6, f1 pelvis 0.4, f2 spine_03 -1.4, f6 head 2.6, f10
spine_03 -0.5, f16/f31 spine_03 0.5. Lying: pelvis 1.1, back 0.5/1.5, heels 1.0, left hand 1.0 (all within 2 cm).
Fingers go 1.0 -> 0.8 and wrist_out -> 0 by f16 to match lieup (so the get-up clips start clean).
**Checks offline**: hands >= 10 cm off the body in startup/loop; knees to chest 1.4 cm at f11 (rough radii, intended
contact); land -1.2 cm at f16-31 = lieup's own right hand on the belly. Air poses: lowest point 44 cm above the root.
**Quaternius UAL Standard** (43 clips, names only): no dropkick, flying kick, lying or get-up. Possible bases: `Jump_Loop`
(air pose before f0), `Jump_Land`, `Death01` (fall, maybe onto the back), `Roll`, `Hit_Chest`.
**Open risks / owner**
- Loop -> land is a cut: pelvis drops 53 cm (66 -> 13 above the root) and the body rotates 14 deg (worst relative
  limb change: left ball 29.5 cm). Root = standing capsule bottom; needs the ECB/shrinking capsule or an earlier land
  trigger (game side). The air pelvis already sits 25 cm low in the capsule to reduce it.
- Arms: brief (thrown, asymmetric) overrides the old `_design` (arms crossed, character sheet). Owner to confirm.
- part_delays set to 0 (dense keys; delays would shift the head after the active frame). Timing CSVs are in `timing/`
  (loop: all frames active) and must be copied to `Saved/AnimPipeline/refs/ufd/` before author/build.
- Re-running `pose_plan.py` on the loop plan rewrites its CSV as "last frame active": re-copy it. `assemble_dropkick_montage.py` plays the loop 3 times (no section loop): the hold-until-landing needs a montage
  section that loops, or the gameplay GA.
- Not verified offline: real mesh interpenetration, hitbox placement, the in-game look, `check` gates.
