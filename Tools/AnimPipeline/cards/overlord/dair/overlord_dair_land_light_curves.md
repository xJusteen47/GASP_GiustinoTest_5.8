# Overlord DAir Light Landing Curves & Timing Sheet

Clip: `overlord_dair_land_light` (4 frames @ 60 fps, frames 0..3).
Landing response for reduced landing-lag windows of DAir (lands in first 3 frames or final recovery frames).

## Timing & Curve Summary Table

| Body Part | Key Frames & Values | Easing (In / Out) | Overshoot (Amount & Frame) | Follow-Through / Overlap |
| :--- | :--- | :--- | :--- | :--- |
| **Pelvis** | f0 (z=-6), f1 (z=-14), f2 (z=-16), f3 (z=-8) | Ease-out drop into bottom (f0->f2), ease-in rise into idle blend (f2->f3) | -1.0 cm drop overshoot at f2 (bottom at -16 cm, settles to -8 cm at f3) | Leads compression impulse; snaps directly into rising transition |
| **Spine / Chest** | f0 (4°), f1 (-6°), f2 (-7.5°), f3 (-2°) | Smooth ease-out flexion to bottom, ease-in return toward neutral | -1.5° forward lean overshoot at f2; smoothly recovers toward idle posture | Cushions downward kinetic force; aligns smoothly with pelvis damping |
| **Head** | f0 (-2°), f1 (+3°), f2 (+4°), f3 (+1°) | Dynamic counter-pitch tracking spine curvature across keys | +1.0° counter-pitch compensation at f2; settles smoothly to +1° at f3 | Follows thoracic compression to keep line of sight locked forward |
| **Arms** | All keys (f0..f3): Chest-frame locked Bison crossed arms (`author.poses["26"]`) | Zero relative motion to chest frame (tangents match spine) | None (rigid crossed fold on chest, no rebound) | Rigid follower: moves 1:1 with spine_05 and chest coordinate frame |
| **Legs** | All keys (f0..f3): `foot_w` planted `[±18, 4, 15]`. Knees track: `pole_w` y=30->38->32, z=48->41.5->46.5 | Ease-out compression (f0->f2), ease-in knee extension (f2->f3) | +1.0 cm knee push forward overshoot at f2 (`pole_w` y=38.0, z=41.5) | Compress synchronously with pelvis drop; zero foot slide or drift |

## Mechanics Notes
- **Impact & Quick Dip (f0–f1):** Pelvis drops from contact (-6 cm) to -14 cm with spine cushioning forward to -6° and head compensating (+3°).
- **Bottom Cushion & Overshoot (f2):** Reaches bottom at -16 cm with 1 cm overshoot and spine dip (-7.5°).
- **Smooth Recovery (f3):** Eases up into pelvis -8 cm and spine -2°, cleanly prepared for blend-to-idle without snapping.
