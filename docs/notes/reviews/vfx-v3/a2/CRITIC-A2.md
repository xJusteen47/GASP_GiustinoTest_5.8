# CRITIC-A2: landing dust A2 (blind adversarial pass)

Looked at: both A1b|A2 grids, zooms dark f160/f168 and light f168, owner_size_target, dk_sheet_c, one own composite of raw A2 crops
(dark f157-f183, light f158-f183), plus pixel measures on raw frames (touchdown = f157; 1 px = raw 1920x1080). § = vfx-principles.
DK caveat: in the TV-filmed sheet the landing puffs are too small to resolve, so the DK answer rests on §7 and the owner's Astro/DK notes.

**Would it sit in DK Bananza? No.** **Does it look like real dust? Not yet: it reads as a thin steam/fog patch.** Reasons = flaws 1-3.

**Owner size target: met, slightly under.** A2 is ~160 px wide at +0 and peaks at ~235-240 px at +15/+20 (dark x 753-992, light
x 773-1004), about 2x shoulder width; top stays at mid-shin, reaching the crouched knee only at +20. The owner frame (A1b dark
+0/+1) measures ~265-300 px. So A2 max = 0.8-0.9x the target, never above it. Note: the frame itself is ~2.3x shoulders, not 1x.

| # | Weight | Frame, view | What is seen | Principle | Label | Smallest fix |
|---|---|---|---|---|---|---|
| 1 | major | light f165-f172 (+8..+15) | The dust is far denser than it looks on the floor: floor haze is only +15 over floor 158 (9 %), but the dark shins go from L~40 to L~130 (floor 146), i.e. core alpha ~0.65. The lower legs and feet melt into the floor for ~10 frames; the eye reads "legs dissolve", not "dust on the ground". Readability is bought with opacity over the character. | §5.4, §5.6-bis, owner "no inflated opacity" | hierarchy | Cap core alpha at ~0.3-0.35, or emit on a ring just outside the stance so the mass sits beside/behind the feet, not in front of them. |
| 2 | major | dark f157, f160 (+0, +3), zoom f160; all frames both views | Single-tone unlit haze: every dust pixel is lighter than the floor, no lit top, no shaded underside, no core. In the first frames the lobes are brightest on their lower outline with a hard flat base and an emptier inside: a rim-lit shell, not a mass densest at the centre. | §8.6, §4.4, §7.1, §7.7 ("unlit gray translucency"), §8.7 | material | Fake two-tone light (§8.9b): top = floor colour x1.15, underside = x0.8; remove the edge brightening so density peaks in the core; depth-fade at the floor. |
| 3 | medium | light f157-f187 (numbers), dark f172-f183 | Growth and fade are in sequence, not together: width 149 -> 231 px by +15 while contrast stays flat (15.5 -> 13.2); then width is frozen (231 -> 234) while contrast drops 12.5 (+20) -> 7.8 (+26) -> 0 (+30). It reads as a still patch held ~20 frames and then dimmed. | owner rule "grows while fading", §3.13, §8.5, §8.2 | timing | Start the alpha ease-out at ~+5, reach 0 at +30; move ~20 % of the growth into the second half, keeping the cap <= 265 px. |
| 4 | medium | dark f157-f177, zooms f160/f168 | Round crisp white pellets (~12-14 px, ~5 cm at character scale) between the legs, brighter than the dust, floating up to knee height at +6..+11 instead of falling. They read as bubbles or popcorn. Also present in A1b. | §8.1 (dust falls back), §7.7 (small dots), owner "real material behaviour" | material | <= 4 px, floor colour x0.9, gravity so they land by +8; or drop the layer. |
| 5 | medium | dark f168, f172, f177 | Two mirrored lobes, left and right of the character, same size and height, nothing readable in front or behind: a pair of puffs, not a skirt around the feet. No big/medium/small. | §2.4, §2.5, §8.8 | shape | 3 unequal masses at random azimuth (1 big, 1 medium, 1 small), biased by the landing velocity. |
| 6 | minor | dark vs light, f160-f168 | Dust/floor value ratio is 1.24 on dark (194 vs 157) and 1.09 on light (171 vs 158): on dark it is white steam, not floor dust. | owner "dust takes the floor colour", §7.1, §8.6 | colour-value | Tint multiplier <= 1.12-1.15 on both floors; get the read from shading (fix 2), not from whiteness. |
| 7 | minor | dark f177, f183 (+20, +26) | Fine curly noise gets sharper as the cloud fades (smooth at +3..+11, wispy/cellular at the end): internal contrast rises in dissipation. | §6.5, §7.7 (procedural noise) | material | Lower contrast / blur the late flipbook frames. |

Evidence gap (for the judge, not a flaw of the effect): both floors expose to the same screen value (L ~146-165), so the "light vs dark
background" test (§6.7) is not really exercised; only the character colour changes. A truly dark or saturated floor is still unseen.

What holds (looked for and not found): no opaque shrink, alpha does reach 0 by +30 with no lingering veil; life 30 frames is near the
dust range (§3); the dust is born on the ground plane (§8.10); it keeps the floor's cool tint (B-R +5 vs +6); no silhouette inflation vs A1b.
