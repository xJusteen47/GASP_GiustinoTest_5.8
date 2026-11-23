FAIL - run stop V2 + dust (capture mv_run_stop, R clip, v180-229; reviewer Fable, rule 10)

Identity traits
- Trait 1 (front foot planted, facing forward, no turn): OK. Plant v189-190, no yaw, front foot stays down until v222.
- Trait 2 (torso rises only after the feet stop): mostly OK. Feet stop v216; the big rise starts v217 (head back 20 cm, up 8 cm
  by v229). Small early drift: head +5.5 cm, pelvis +3 cm during the brake v196-216 (authored: trunk -37 -> -28, pelvis -10.5 -> -7.5).
- Trait 3 (arms/head later): arms hold to v216 and close from v217; cannot be judged to the end (see capture cut).

posa
1. v197-205: the back foot steps up beside the body DURING the brake (clip f14-20), so the wide braking pose lives only v190-196
   (7 frames). From v205 to v222 he stands narrow (feet ~20 cm apart), both legs almost straight, only leaning forward: no weight,
   reads as a mannequin tipping, not a medium-heavy brake. V1 base had the same narrow stance; V2 did not fix it.
2. v225-229: front foot steps back and both knees bend while the trunk comes up: a hunched half squat, weak silhouette.
3. Plant v188-190 is good: long line, heel first, clear in side and az45.

dati montage
4. Stick released v180, the stop starts v185-186 (5 frames late); v185 is a repeated frame (actor step 0).
5. v196: actor jumps +5.6 cm and the mesh shifts -5.6 cm the same frame (world pose continuous, not visible).
6. Front foot slides 95 cm on the floor v190-216 (brake slide, wanted, covered by dust); last 14 cm v205-216 on two flat feet.
7. Capture ends v229 = clip f44 of 60: the rise (ends about v243) and the exit to the idle are cut. It matters: the V2 change
   (rise delayed and spread) and the exit pop are NOT verified.

gioco (dust)
8. Timing good: first puff v186-188 on the plant, trail follows the slide.
9. Too big and too long: thigh high and ~1.5 m long at v200-210, still dense around the feet at v229 (40+ frames). In the game
   view (camera behind) it covers feet and shins from v194 to v229, so the brake legs are not readable.
10. Flat beige blob, little light shaping (v3 sheet wants lit smoke); white star glints at the foot v196-200 read as hit sparks.

Next fix (one): in run_stop_gen.py keep the back foot dragging on its toes until STOP (move RF_POS/RF_ROT step from f14-20 to
STOP+2..STOP+10) and hold the pelvis at -11/-12 to STOP, so the wide low stance lasts the whole slide. Then recapture 260 frames
with the dust at about half size and half life, and no stars.
