# Overlord dair V10, review r1 (Fable, cloud, on a78cec35): FAIL; round 2 answers (desktop)
PASS: charge (head level v38-v52, pop gone, frame 1 still changes pose). Kept as is (same AS/AM_OverlordDAirV10 in round 2).
FAIL heavy landing (labelled posa): at v92 kneeling/seiza, pelvis at the ground, shins flat, wrists crossed in the lap, head bowed; held ~10 f, then
pops to standing in 1-2 f (v99 -> v101). Fix asked: impact key f2-3 pelvis UP 30-35 cm into a deep wide squat, feet flat, knees over toes; arms
crossed at the sternum, elbows out; contact f0, lowest f2-3, +3-5 cm settle by f6, rise f8-16 head leading, no 10 f hold + 2 f pop.
FAIL transition (gioco/dati montage): dair_land contact v84, landing clip v88 (air ball on the floor 3-4 f); dair_hit 1 f. Max 2 attempts.
NOT VERIFIED: the light clip; the feet rebound f19-20 / recovery. Add a higher dair (lands late, light window) and a landing in f1-3.

## Round 2 (desktop)
- Seiza = GAME, not pose: Saved/DairV10/variant/dair_land/frames.jsonl: during the lag actor z 35.8 (h -52.4) with the standing capsule (hh 86):
  the mesh was 52 cm under the floor; clip pelvis 55 cm over the feet at its lowest (a squat). Same bug in BASE V9 (27 cm, actor 60.5 for good).
  Cause: the standing capsule came back with the Body anchor (mesh root kept, below the tucked feet). Fix (C++, generic): FGSTN_BodyCapsule::
  GroundedRestore, on the floor the capsule grows from the floor; test GSTNCombat.Core.BodyCapsule.GroundedRestore. The pop v99->101 was the clip
  drawn under the floor blending out to the air pose: same cause.
- NOT applied: "pelvis up 30-35 cm" (it would bend the pose to hide the game bug, rule 13e).
- Applied (AS_OverlordDAirLandHeavyR2, card overlord_dair_land_heavy_r2): arms crossed at the sternum, elbows out; stance 48 cm; pelvis -22/-33/-35
  at f1-f3 (lowest f3), -31 f6 (+4 settle), -30 f8, rise f10-f15 with the head back first (f10) and the trunk last (f14-15).
- Transition: real contact = the frame the actor stops falling (v86 in dair_land, not v84: the earlier figure was h <= 2 with the small capsule);
  the clip starts v88 = contact + 2: one frame is the documented ground-fact latency (Mover's new mode is seen by the combat tick a frame later,
  CLAUDE.md rule 5), see frames.md r2 for the new numbers. Not changed in code (attempts 0/2).
- New captures: dair_land_early (pressed falling at 30 cm) and dair_land_late (pressed rising at 60 cm), base V9 and V10R2.
