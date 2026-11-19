PASS (with one unverified point: first medium hit, see dati 1)

Review of hitstop decision 2A (energy full speed, matter 0.15) and ground-bounce cloud position. Reviewer: Fable, 2026-11-19.
Source: `Saved/LF2/hit_light/{hit_medium,hit_debris,ground_bounce}/game/f###.png` (60 fps, 1920x1080, light backdrop).
Sheets (this folder): `hitstop_sheet_hit_medium.jpg` (f24-63), `hitstop_sheet_hit_debris.jpg` (f34-73), `hitstop_sheet_ground_bounce.jpg` (f98-138, every 2nd).
Hit frames found by frame difference: medium f26 and f50, strong f36, bounce contact f66 and f104-106. No `frames.jsonl` in the capture,
so the hitstop window is read from the frozen poses, not from a flag. Nothing was tested in engine.

## 1. Star / flash
- look, PASS: strong hit f36-38 full star, f39 thinning, f40-41 small core, gone f42. About 5-6 frames, torsos visible again from f40
  while the hitstop still holds (poses frozen to f46). Reads as a flash, not as a lingering star (hd1: the star stayed over the torsos).
- look, PASS: medium hit 2: f50 full, f51 smaller, f52-53 core, gone f54. Visible, on the fast side; do not shorten further.
- dati, NOT VERIFIED: medium hit 1: f26-30 are five identical frames (difference = idle noise), the star does not shrink, then f31-33
  it shrinks as in hit 2. So the star covers the torsos for 8 frames here. Either a first-spawn hitch of the capture or the star age
  not advancing on the first hit. Hit 2 and the strong hit are right, so a hitch is more likely, but this capture cannot tell.

## 2. Streaks / sparks
- look, PASS: strong hit: streaks leave the centre f39-45 and are far out by f44-46 while both bodies are still frozen. Full speed.
- look, PASS: medium hit 2: streaks fan out f52-58, sparks drift to f60, bodies frozen. Hit 1: streaks only from f32 (same doubt as dati above).

## 3. Debris and dust
- look, PASS: red chunks appear f40 and stay almost in place around the torsos to f46 (slowed), then fly out and fall f47-73 with a
  natural arc, on the floor by f66-72. Smoke starts only at f47, after the hitstop, and grows normally.
- look, note (not part of 2A): from f60 the smoke cloud covers the attacker (body washed to grey f62-73) and white 4-point grain
  stars hang at head height f55-70. Same as open item 4 of the hd1 handover (round grain sprite); cloud opacity worth a look.

## 4. Ground-bounce cloud
- gioco, PASS: cloud is born at f106 at the feet/floor line, about 2 character widths, flat lobes on the floor f106-116 (hd1: torso
  height). It then rises to head height by f122-138, which is smoke rising, not a wrong spawn point.
- look, note: after f124 the lobes sit behind the attacker's head against the wall; a lower rise speed would keep it on the floor longer.

## Next step (not a FAIL fix)
Re-capture hit_medium with the per-frame state written (`frames.jsonl`: hitstop flag + star particle age) and a throw-away warm-up
hit before v20. If f26-30 are still identical with the game clock advancing, the star age is not unscaled on the first spawn.
