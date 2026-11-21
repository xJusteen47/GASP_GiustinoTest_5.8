# Generic run start w1: curves sheet (rule 13c), shared by author and reviewer

Source of truth: env recipe of runstart_generic_compose.py (below) and the pose files Saved/AnimPipeline/runstart/generic_{s2,w1}_pose.npz.
Clip frames from 0, 60 fps. Measured with a one-off (lean = pelvis_MainPoint -> neck_01_MainPoint from vertical, pelvis = pelvis_MainPoint
height in cm) on the composer output, before Cascadeur (Cascadeur re-solves knees/elbows, pelvis/feet/head MainPoints stay < 3 cm).

## The sprinter's weight-transfer principle (what the owner liked in s2, measured, not the recipe)
1. Fall first, step second. In 4 frames (0.07 s) the pelvis drops 9 cm (89.5 -> 80.5) AND the torso tips 0 -> 45 deg together:
   the mass goes down and forward before the first drive foot lands (s2 R plant f8-10). g3 tips as far (36-41 deg) but its pelvis
   only sinks 3-5 cm and the hips stay over the feet: it leans, it does not fall.
2. The push foot stays planted while the body falls over it (s2 L planted f0-5), the free leg swings from frame 1.
3. Low hold: pelvis 79-83 cm and torso 42-47 deg through BOTH drive steps (f4-f15). Vertical bounce damped to 0.3: no hop, the energy
   of each push goes forward. Elbows/hands 14 cm back: the arms drive, they do not reach.
4. Late, single rise: the body comes up ONLY on the push-off of the second drive step (s2 L plant f14-16 -> pelvis 79 -> 90 over
   f16-21, torso 32 -> 16 deg), i.e. the "TUM" is the stride where the weight rises and the pelvis gets ahead of the feet
   (pelvis ahead of the feet midpoint: 3-5 cm in the drive, 12 cm at f16, 20 cm at f20). Then it is the stock loop.
5. Rate: sprinter s2 loads at 0.8-1x for 3 frames, then ~1.9x held flat to f12 (pushes, not spins).

## w1 = that principle for the average character, with the frantic rev-up of g3 from frame 1
Recipe: TAG=w1 R0=2.3 N_REV=19 HOLD=12 RATE_SAG=0.15 LEAN_OUT=20 ENTRY=4 ENTRY_LEAD=1 BOUNCE=0.3 LEAN_DEG=24 LEAN_IN=4 ARM=1.7
ARM_BACK=10 DROP_CM=12 DROP_IN=4 DROP_HOLD=12 DROP_OUT=19 (LEG_IK=1). 64 frames, steps_in_rev 3.39 (s2 2.76).

| Channel | s2 (sprinter) | w1 | Why |
|---|---|---|---|
| Play rate | 0.96 f0, 1.8-1.96 f2-12, 1.15 f16 | 2.29 f0, 2.2 f2-8, 2.07 f12, 1.33 f16, 1 from f19 | owner: frantic limbs from frame 1 (no slow load) |
| Pelvis height | 89.5 -> 80.5 by f4, 79-83 to f16, rise to 90 by f21 | 89.5 -> 79.8 by f3, 80.7-83.6 to f14, rise to 86.8 at f17, loop compress 81.6 at f20-21 (R contact), 90.5 f26 | same fall and hold; rise on the second push-off (L plant f12-13) |
| Torso lean | 0 -> 45 by f4, 42-47 to f13, 32 f16, 16 f20 | 0 -> 41 by f4, 38-43 to f13, 28 f16, 16 f19 | average build: ~4 deg less than the sprinter |
| Drive steps | R plant f8-10, L plant f14-16, long stride f16-25 | R plant f7-8, L plant f12-13, long stride f13-20 | 2-3 frames sooner: higher rate |
| Arms | ARM 1.6, back 14 cm | ARM 1.7, back 10 cm | pumping like g3, elbows back but less strict than a sprinter |
| Bounce | 0.3 | 0.3 | no hop |
| Knees | forward (2-bone IK) | forward, 0 backward frames in Cascadeur, min 6.3 cm ahead | owner rule |

Foley (stock notifies, runstart_generic_g_ue.py add_foley): L f3/12/31/54, R f8/20/... (AS_GSTN_RunStart_Generic_w1_build.json).
