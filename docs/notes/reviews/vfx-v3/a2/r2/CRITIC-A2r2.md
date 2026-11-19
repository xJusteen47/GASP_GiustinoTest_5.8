# Blind critic: A2r2 landing dust

Sources: game_dark, game_light, side_dark, side_light strips; zoom_A2r2_{dark,light}_f161 and f169.
Target: DK Bananza landing dust, small, low, soft, floor colour, readable on any floor.

What works: no quad lines or hard sprite edges anywhere; height stays below the knee; legs and feet
stay readable in every frame; gone by f182 (about 0.4 s) on both floors; much smaller than A1b.

## Flaws

1. **major** - dark floor, game f161-f170 and zoom_dark f161/f169: the puff is light grey/near white
   on a dark blue-grey floor. It reads as white steam or smoke, not the floor's own dust.
   Fix: lower dust albedo/emissive toward the floor tint (about 1.3-1.6x floor value, not white),
   so it stays readable but clearly belongs to the floor.
2. **major** - light floor, game f161-f170, side f162-f170, zoom_light f161/f169: the dust is darker
   than the floor and soft, so it reads as a shadow smudge or motion blur next to the real cast shadow,
   not as dust. At f169 it is almost invisible.
   Fix: on light floors lift the top/core value slightly above the floor and keep the darker tint only
   on the underside rim; raise opacity of the first 4-5 frames a little.
3. **medium** - dark floor, zoom_dark f169 and game f167: the two lobes sit outside both feet and
   together span about 1.5x stance width. Over the "about stance wide" cap.
   Fix: cut horizontal spawn speed / end size about 20-25 %, or pull spawn offset inward toward the feet.
4. **medium** - dark floor, zoom_dark f161/f169, side_dark f160-f167: the inside of the puff is grainy,
   crinkled, cauliflower-like noise with fibrous wisps. Contrast comes from high-frequency texture,
   not from soft light/shade volumes.
   Fix: blur or lower the tiling/detail of the flipbook (bigger noise scale, fewer frames of crisp detail).
5. **medium** - both floors, all zooms: no readable value structure (darker brownish underside,
   whiter top). Dark floor is uniformly pale, light floor uniformly darker.
   Fix: vertical gradient in the material (particle-local Z or UV.y): bottom -15 % value warm tint,
   top +10 % value.
6. **minor** - dark floor, game f160-f170 and zoom_dark f169: dust splits into two separate jets either
   side of the feet with an empty gap under the body; it reads as two side sprays, not one low ring.
   Fix: add 1-2 small low particles in the centre / behind the heels.
7. **minor** - side_dark and side_light f160-f164, zoom_light f161: small round pebble dots near the feet
   are crisp and bright on dark (white beads), visible as dots on light.
   Fix: shrink them, tint to floor colour, or fade them in 3-4 frames.
8. **minor** - side_dark f164-f175: in side view the dust trails backward about one shin length behind
   the rear foot and lingers there longest, so the motion looks one-sided.
   Fix: lower backward velocity bias or fade trailing particles faster.

Player notices the landing: dark floor yes (for the wrong reason, white smoke); light floor no (reads as shadow).
