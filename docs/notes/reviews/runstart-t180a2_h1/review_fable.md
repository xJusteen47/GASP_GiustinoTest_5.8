# Review (Fable): run start turnaround 180, w2t180L / w2t180R, in-game capture

**Verdict: FAIL** (trait 1, entry v88-v96). Traits 2 and 3 pass at grid sampling; hand-off is clean.

Images used: the 4 grids once each, plus 8 crops (az90 L entry/hand-off, az0 L entry/hand-off, az90 R entry/hand-off,
two 4x zooms of our row v88-v96 and v104-v116). The mp4 was not opened.

## Identity traits
1. **Sprinter weight transfer into the new direction: FAIL.**
   - The drop is there and it is fast: v84 upright idle, v88 already low with deep knees, low held to ~v108, one rise v112-v116, no second dip.
   - But at v88-v92 the chest still faces the OLD direction while the torso is already tilted ~40 deg toward the NEW one.
     So the body tips BACKWARD: head thrown back, hips forward, both feet on the toes (az90 L v88, v92; az0 L v88, v92 show
     the chest to the camera, leaning away). It reads as a slip / limbo fall, not as a sprinter driving forward.
   - The turn only arrives later: profile at v96, back visible at v100, facing the run at ~v104. The "fall forward" exists from v100 on only.
   - R mirror is the same and more exposed to this camera: v92-v100 the torso lies almost sideways over tucked legs.
2. **Frantic limbs from frame 1: PASS.** A foot is off or on the toe in every column v88-v108 (toe pivot v88-v92, knee lift v96,
   swap v100, back kick v104, short cross step v108). No planted pause, no stiff spin on a flat foot. Much busier than stock,
   which stands and shuffles until v100.
3. **No clash idle -> start -> loop: PASS with two watch items.** v112-v140 reads as a continuous run cycle in az90 and az0,
   no hop, no visible double step at 4-frame sampling, knees never bend backward in any frame seen.
   Watch: the idle -> v88 change is very large for 4 game frames (see below), and the long-stride slide numbers.

## Life
- Anticipation: none. The head does not look to the new direction first; at v88 it faces up and back. A head/shoulder lead
  is the cheapest fix and it is what would make the backward tip read as intended.
- Arc: the pelvis path is good (one drop, hold, one rise). The chest path is not: it goes back first, then swings round, then forward.
- Weight: from v100 on, good; low, driving, legs under the body. v88-v92 the centre of mass is behind the toes with no support, off balance.
- Follow-through: arms are alive (raised fist v88-v92, open swing v96-v104). Fine.
- Against stock: ours is livelier and commits sooner, but stock stays readable through the turn (chest always leads the lean). Ours does not for ~8 game frames.
- Style bar: cartoony yes, fluid no, because of the back-tip; at gameplay distance (grid_game) the entry reads as a stumble.

## Items
- **FAIL, dati montage (clip data)** v88-v96, L and R: lean direction is fixed to the final heading while the body yaw is still turning.
  The lean should follow the chest facing (rotate the lean axis with the yaw, or delay the lean until the yaw is past ~90 deg),
  so the body goes: sink upright -> turn -> fall forward. Add a head/shoulder lead in the first 2 clip frames.
- **Watch, gioco (game code)** v84->v88: upright idle to full crouch + 40 deg tilt in 4 game frames. Probably fine once the lean
  is fixed (the approved w2 also falls in 4 clip frames), but check the entry blend again after the fix.
- **Watch, dati montage** long-stride landing slide: L 38.3 cm, R 33.4 then 28.2 cm, against 20 cm on the approved forward w2 and
  29.6 on stock. Not visible as a skate in the stills (v108-v116 look planted), so not a FAIL from the images, but it is nearly
  double the approved value and the R side has two bad contacts in a row. Needs a look in motion.
- **Watch, posa (pose)** v116-v124: ours is already upright while stock still leans into the run. Acceptable (one rise as briefed), slightly early.
- R mirror: mirrors L, no extra defect beyond the items above.
