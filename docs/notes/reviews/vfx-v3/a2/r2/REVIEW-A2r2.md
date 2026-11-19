# VFX review: landing dust A2 r2 — FAIL — 2026-11-19

Capture chain: **incomplete, enough for a FAIL, not for a PASS**. Present: game and side camera, light and dark floor, 60 fps frames with labels, VFX-off pass, A1b under the same GASP hub light (pairs are fair now). Missing: real-speed and x0.25 video, blind critic on r2, low preset. Assets under review = r2 pass 3 (`Saved/VfxV3/a2r2d`: SmokeReal `DarkSnow` 0.4, `DarkBrightness` 1.15, `Brightness` 0.65, `OpacityMax` 0.9, `FadeStart` 0.2), not the pass 2 described in the handover.
Touchdown = f158; 1 px = raw 1920x1080; game view unless stated. Numbers come from an on/off pixel difference with the character and its shadow masked out. The OFF pass pose drifts a little from the ON pass (more with each frame), so read them as +/- 5 px and +/- 3 L. Knee line in game view = 90 px above the foot line (r1 value, standing character 336 px tall).

Real-speed impression: **not available** (no video). Not verified: whether the 8 readable frames on the light floor register at all at 60 fps, whether the dark-floor puff reads as a white pop, flipbook flicker and noise crawl during the fade, any pop at death, timing against the foot plant.

| question | yes/no | frame and view | what is seen |
|---|---|---|---|
| 1 message | yes (dark), weak (light) | light f158-f166, f169-f178 | light: dust composites at 0.93x the floor (median), darkest tenth 0.80-0.87x for +0..+8: two small grey smudges read; from +11 it is <= 6 % and nothing reads |
| 2 character readable (D1) | yes | dark f161, f169 zoom; OFF rows | feet and shins clear on both floors, dust never covers the body (top <= 76 px) |
| 5 orientation / clipping | yes | dark f161, f169 zoom | no rectangle and no straight line any more. The black oval under the feet is the character's own shadow (present in the OFF pass, 450-2970 px at L < 14, vs 180-1190 px with VFX on) |
| 7 life curve (D7) | yes | dark f158-f178 | width 202 px (+0) -> 229 (+3) -> 259 (+8) -> 268 (+11), then holds and fades; growth slows (ease-out), median value 99 -> 59 over the same frames |
| 8 clean death (D6, owner rule: fade to alpha 0) | yes | dark f175-f182 | 40 px wisp at +20, nothing at +22 (r1: +38); no veil left |
| 10 value / background (D14) | NO | dark f158-f166, light f161 | dark: median L 84-99 on floor L 32 = 2.7-3.1x, lit tops L 120-135 = 3.8-4.2x. light: 0.93x. r1 by the same method (`a2/dark6`): median 78-103, tops 127-140. Dark floor unchanged |
| 11 matter and light (D8, D12) | NO | dark f161 zoom, side dark f160-f170 | neutral light grey (B/R 1.19-1.21) on a navy floor (B/R 1.43-1.50): white smoke again. Side view: crunchy "cauliflower" detail, near-black creases inside light grey, not soft |
| 12 quality bar (D11) | NO | both floors | vs A1b and DK Bananza: holds = small, low, short, disperses; does not hold = not the floor's colour on dark, shadow-like blot on light |
| D3 countable | yes | dark f161 zoom | two lobes, a front band, 3-5 pellets |
| D4 frame 1 | yes | dark f157-f158, light f158 | present at +0 on both floors (light: faint, darkest tenth 0.87x) |
| D2, D10 | n/a | | realistic dust by owner decision |
| D5, D9, D13, D15-D21 | n/a | | hit, hierarchy, hitbox, far camera, hitstop, kits, bounces: nothing of that in a landing-only capture |

Size target (A1b early frame, same light, same method: 302 px at +0, 328 px at +1, top 48-53 px in the columns free of the legs): **width met**, A2r2 202 -> 268 px = 0.82-0.89x, never above. **Height: under the knee line, over the A1b early frame.** Top 45 px (+0), 59 (+3), 68 (+6), 72 (+8), 75-76 (+11..+14), 70 (+17). r1 was 100-103 px at +17..+24, so the late rise is cut, but the top is 1.5x the A1b early frame and above the 60 px cap asked in r1 from +6 to +17. Side view: top 62 px (+0) -> 166-168 px (+11..+14) in the leg-free columns, with the knee pads at about 160-205 px in f158 (approximate reference): at the lower knee pad.

Previous gaps:
1. colour-value / material: **not closed on dark, closed in part on light.** Dark: value as in r1 for +0..+8, hue a little cooler (B/R 1.10 -> 1.20, target >= 1.3). A2r2 is also whiter than A1b at the same frame (median L 99 vs 71 at +0). Light: from "nothing reads" to the r1 target in the darkest tenth (L 171-182) for +3..+8.
2. hard rectangle / straight edge: **closed.** Minor leftovers: at f161 a thin lighter arc of dust follows the lower-left edge of the shadow oval (reads a little like a rim); at f169 4-5 white pellets sit in a row like a dashed line.
3. late rise to the knee: **closed in part** (76 px max instead of 100, gone by +20), see gap 3.

Owner idea (brown-dark edges, whiter highlights): not visible. On the light floor the dust is cooler than the floor (B/R 1.047 vs 1.030), not browner, so it looks like a second soft shadow and the left lobe merges with the character's shadow (light f161 zoom). `EdgeTint` acts only where the texture alpha is low, which is where the puff is nearly transparent. On dark the highlights are far more than "slightly" whiter.

Gaps (max 3, gravest first):
1. **colour-value / material, dark floor** — dark f158-f166 game, side dark f160-f170. Dust is 2.7-3.1x the floor (lit tops ~4x) and neutral grey. Pass 1 (`DarkSnow` 0.15, `DarkBrightness` 0.85) vanished, pass 3 (0.4, 1.15) is as bright as r1: the target lies between. Fix, dark-floor parameters only: capture two values in one session, `DarkSnow` 0.22 and 0.30 with `DarkBrightness` 1.0, and keep the one where the median is L 55-65 (1.7-2.0x floor), the lit tops L <= 90 and B/R >= 1.3. If the creases still look crunchy in the side view after that, `OccStrength` 0.8 -> 0.6.
2. **colour / read, light floor** — light f161, f169 zoom. Reads for +0..+8 only, as a cool grey blot that merges with the shadow. Per-pixel contrast already equals A1b (median 0.93x, darkest 0.82-0.84x); the read is smaller because the area is 4-5x smaller (2600-5400 px vs 10000-21800 px), so more read must come from hue and value, not size. Fix: warm `ShadeColor` on bright floors (about 0.42, 0.39, 0.36 instead of 0.40, 0.40, 0.45) and `EdgeWidth` 0.35 -> 0.6 so the brown tint reaches the body; aim for the median at 0.88-0.90x for +3..+11 with dust B/R below the floor's 1.03. Keep `OpacityMax` 0.9.
3. **shape / size, height** — dark f164-f175 game, side dark f164-f172. Top 68-76 px from +6 to +17 (A1b early frame 48-53 px, r1 cap 60 px); height grows 1.7x while width grows 1.3x. The measured top matches the geometry (origin +6 cm, half of 65 cm, rise 3). Fix: Skirt max size 65 -> 50 cm with ring 30 -> 36 cm to hold the width, or origin +6 -> 0 cm. Expected top about 60 px.

Game side: the dust still spawns at the sim landing frame (f156-f157), 1-2 frames before the foot is flat. Inherited, not a round.

Decisions for the approver:
- Light floor (exposes at L 218 whatever the albedo): accept a faint read of about 8 frames there, or allow a darker and warmer underside (gap 2 fix), which will look like a visible grey-brown smudge on white?
- Height: is "below the knee line" enough (met: 76 px vs 90), or does the A1b early-frame height (50-60 px) stand? Gap 3 assumes the second.
- Next capture (r3): real-speed and x0.25 video, blind critic, low preset, and the OFF pass taken in the same run as the ON pass so the poses match and `vfx_dust_measure.py` can be used.
