# Overlord jab V3, in-game capture numbers (Saved/AnimJab5/capture, 60 fps video frames "v", move frames "f" = timeline frames)
Source: frames.jsonl of each sequence. Setup: montage rate 62.5/60, blend-out 4/60 s, root motion on jab 2 only (root lock REF_POSE, force lock off).

## Starts and sync (a = animation frame the montage shows, 60 fps from 0)
| sequence | jab 1 starts | jab 2 starts | a at f6 (jab 1) | a at f9 (jab 2) |
|---|---|---|---|---|
| jab1 | v21 (a2.04 shown on f0) | - | 7.25 (v26) | - |
| chain (whiff) | v21 | v33 (press v32) | 7.61 (v27) | 10.37 (v41) |
| chain_hit | v21 | v33 | 6.46 (hit, v27-30) | 9.33 (hit, v41) |
The montage still runs about 1.0-1.4 anim frames ahead of the timeline (a2.04 on f0): known code issue, unchanged by this setup.

## Hitstop and the old "hand goes back" defect
- Jab 1 hit (f6, v27): a6.46 held flat v27-v30 (H on v28-v30), then jumps to a8.67 at v31 (+2.2 frames, the known resume skip). No return.
- Jab 2 hit (f9, v41): a9.33 held flat v41-v45 (H on v42-v44, 3 hitstop frames), resumes at a11.29 on v46 (+1.96). The hand never goes back: old defect NOT present.

## Jab 2 root motion (actor x forward, cm; start x=0 in chain, x=30 in chain_hit because the pawn is left where the chain ended)
chain, per f: f0-2 0.0 | f3 0.6 | f4 1.6 | f5 (skipped by the sim, v37-38) | f6 8.0 | f7 17.0 | f8 29.9 | f9-52 30.0 (constant to the end, no slide back, no drift).
chain_hit: f3 30.6, f4 31.6, f5 33.0, f6 38.0, f8-9 59.9 (hitstop frozen, so the root is frozen too), f10 64.0, then constant to the end.
- Advance in the whiff = 30.0 cm = authored value: capsule is neither double-moved (would be ~60) nor still (0). Y stays -450 (no side drift).
  I did not judge the mesh against the capsule visually (the grids are for the reviewer).
- UNEXPECTED 1: on a hit the advance is 34.1 cm, not 30: the last +4.1 cm lands on f10, the first frame after the hitstop (root motion paused during hitstop appears to be caught up, or the hit pushes the attacker). Not seen in the whiff.
- UNEXPECTED 2: actor z drops 100 -> 88.1 at f3 of jab 2 and stays 88.1 after the move (also at the start of the next capture). Probably a Z component of the animation's root translation (or ground snap); worth a look.
- Jab 1 (root motion off): x constant 0, z constant 100, as intended.

## Blend-out
Montage state turns to "none" after a22.86 (jab 1, v41) and a46.8 (jab 2, v76): the blend-out starts about 4 frames before the end of the 28 and 52 frame animations (a ~24 and ~48), as set, instead of ~13 frames before as with the 0.25 s default.
