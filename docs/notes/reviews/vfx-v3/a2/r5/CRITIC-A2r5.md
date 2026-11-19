# Blind critic, A2 r5b (landing dust)

**At real speed:** a quick flat donut of separate cauliflower clumps snaps out around the feet, then sits there and erodes away. It reads as "a ring of puffs placed around the character", not as "the ground got hit and dust was pushed out". On the light floor it reads as grey soot or smoke, not dust.

**One burst or separate puffs:** separate puffs. From f162 the ring has a hollow, empty centre (the shadow disc shows through) and clear gaps between clumps (side dark f164-f170, light f166: a left cluster and a right cluster with nothing between them). The reference is one mass: dense at the body at f80-f83, then a single merged haze by f86-f90.

**Shrink:** yes, slightly. Side dark/light f172-f178: the outer clumps get smaller and sparser as they fade (alpha erosion eats the silhouette), and they don't grow. In the reference the puffs keep growing while they fade.

| Aspect | r5b | Reference |
|---|---|---|
| Origin | at the feet, the right place; a dark contact disc at f157-f158 | under the body; the cloud starts dense in the centre |
| Travel/brake | out to full radius by ~f162 (~80 ms), then a dead stop | pushes out over f79-f84 (~170 ms), then keeps drifting through f90 |
| Growth | almost none after f164 | keeps growing until it merges into haze |
| Fade | erosion, the clumps break into bits | opacity drops while it spreads; soft dissolve |
| Value vs floor | dark: blue-grey, fine but a bit bright and cold; light: dark grey, well below the floor (f158 zoom: near-black blob) | the floor colour itself, slightly darker only in shadow |
| Edge/texture | high-frequency cauliflower, crisp edges (zoom f166) | soft, low-detail, no inner noise |
| Merging | none, the clumps stay separate | full merge by f86 |
| Duration | ~f157-f178, ~0.35 s | f78-~f97 at 30 fps, ~0.6 s plus residue |

Extra: the white pebble specks at the feet (f162-f166) are fine, but they are the only thing that sells "impact".

**Verdict: NOT YET**

1. **One mass, not a donut** (f166 side/zoom, f164 light): fill the centre (spawn on a disc rather than a ring, with some low-speed particles), use fewer, bigger, softer sprites that overlap so the clumps merge into one cloud by ~f168.
2. **Value matches the floor** (f158 and f162, light zoom): tint the dust from the floor colour, close to its value (a little darker in shadow, never grey soot). On the dark floor, bring it down a bit and warm it slightly.
3. **Brake and grow, don't stop and erode** (f162 dead stop, f172-f178 shrinking): a longer drag tail with slow drift after the brake, size that keeps growing ~1.5-2x across the life, a fade on opacity rather than erosion, and a life of ~0.5-0.6 s. Soften the cauliflower noise as it grows.
