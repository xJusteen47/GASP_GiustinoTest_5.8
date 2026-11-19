# Blind critic, landing dust A2 r6 (r6b2 frames)

Verdict: **NOT YET**. It still reads as a "system" effect: a ring of identical cotton puffs parked around the feet, not ground material thrown out by an impact.

## Problems, most important first

1. **The cloud does not evolve. It freezes in place (f160-176, every view).** At f160 the puffs are already at their final radius. From f160 to f176 they keep the same size, spacing, height and opacity, so the character crouches inside a static wreath. Real impact dust moves fast and then decays: an outward shove in 3-4 frames, a strong slowdown, a slow lift and curl, then thinning.
   Fix: spawn the material at the contact point at f157. Push it out at high speed and use strong drag, so it covers about 80% of its final radius by f161. From f162, let it grow about 1.5-2x while opacity falls toward 0, ending around f190-200. Add a slight upward drift with curl noise, so the top edge breaks up. Nothing should keep a constant size and alpha for more than 4-5 frames.

2. **The puffs are uniform, discrete and laid out on a ring (zoom f159-166, side v160-177).** Every clump is the same size and the same "cauliflower" sprite, evenly spaced, with clean gaps between them. That reads as N emitters on a circle. Real dust is one continuous low sheet with lumps in it.
   Fix: put a soft, low, continuous ground skirt under the clumps (flat, wide, alpha around 0.3). Vary clump scale 0.4-1.6x, distribute them unevenly (denser on the side of travel and impact), and give each a random lifetime. Drop clump count, raise overlap. Clumps also climb to knee height in the side view (v163-170). Keep most of the mass below the ankle and let only a few wisps rise.

3. **The dust value and colour do not belong to the floor.**
   - Dark scene: the dust is saturated blue-cyan and brighter than everything around the feet (f159-176, side v160-177). It looks emissive, like a magic cloud rather than dust.
   - Light scene: the dust is mid-grey, clearly darker than the white floor (f157-166). It reads as soot or exhaust.

   Dust should be a slightly lighter, desaturated version of the floor albedo and take the scene light. The DK reference cloud is warm and dim, tinted by the cave.
   Fix: drive the tint from the floor colour. Cap the dark-scene brightness at about 1.2x the lit floor value, with saturation near 0. In the light scene the dust should sit at about 0.9-1.0x the floor value, with only a soft self-shadow on the underside. Add a little backlight or rim light only where the key light hits.

4. **The dark contact disc is too hard and shows up too early (dark f156, zoom-dark f156 and f161-166, zoom-light f165-166).** At f156 a black, sharp-edged ellipse appears under the airborne foot, before contact, and reads as a hole. Later it stays as a crisp dark oval.
   Fix: start it at contact (f157), not before. Use a soft falloff with no visible edge and at most about 25-30% darkening. Fade it out by about f170.

5. **The small dark debris reads as floating pepper (zoom-light f157-166, zoom-dark f159-166).** The pellets are the same size and very dark. They hang in a cluster beside the shin for about 8 frames with no arc. They look like insects, not grit.
   Fix: launch them ballistically from the contact point (up 30-60 deg, short arc), using real gravity, so they land within about 10-14 frames. Use scale variance 0.3-1.5x, and colour them from the floor but darker (about 0.5x), not black. Use roughly half as many, and have some bounce or skid once and stop.

## Do the r6 additions help?

- **Light flash: yes, a little.** The warm glow on the dark floor at f157-158 (side v157-158) sells the "hit". Its colour fights the blue dust, though, and in the light scene (f156, side v156) it is a white bloom that starts before the foot touches. Lock it to the contact frame, keep it at 2-3 frames, and tint it to match the scene.
- **Small dark debris: no, not as made.** See problem 5. With a ballistic arc it would help a lot.
- **Dust lighting: no.** The dust is still lit more like an unlit sprite than by the scene: glowing blue in the dark, flat dark grey in the light (problem 3). It is the main reason it reads fake, after the frozen timing.

## Priority order for the next round

Timing and decay first (1), then making the clumps one sheet (2), then value and tint (3). The disc and debris fixes are cheap polish.
