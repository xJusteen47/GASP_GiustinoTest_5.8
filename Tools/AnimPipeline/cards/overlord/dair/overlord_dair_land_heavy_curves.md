# Overlord DAir Heavy Landing Curves & Timing Sheet

Clip: `overlord_dair_land_heavy` (16 frames @ 60 fps, frames 0..15).
Landing response for stomp landing during frames 4..31 of DAir.

## Timing & Curve Summary Table

| Body Part | Key Frames & Values | Easing (In / Out) | Overshoot (Amount & Frame) | Follow-Through / Overlap |
| :--- | :--- | :--- | :--- | :--- |
| **Pelvis** | f0 (z=-6), f1 (z=-16), f2 (z=-27), f3 (z=-34), f5 (z=-36.5), f6 (z=-34.5), f9 (z=-26), f12 (z=-14), f15 (z=-6) | Ease-out drop (f0->f2), linear drive (f2->f3), ease-in rise (f7->f9), ease-out stop (f12->f15) | -2.5 cm drop overshoot at f5; settles +2.0 cm to f6 | Leads kinetic chain on impact (0f lag); initiates rise at f7 |
| **Spine / Chest** | f0 (4°), f1 (0°), f2 (-10°), f3 (-17°), f5 (-20°), f6 (-17.5°), f9 (-12°), f12 (-6°), f15 (-2°) | Ease-in forward sink (f1->f3), cushion settle (f5->f6), gradual upright return (f7->f15) | -2.5° forward lean overshoot at f5; settles +2.5° to f6 | Torso sinks 1 frame after pelvis (1f overlap); begins rise at f8 |
| **Head** | f0 (-2°), f1 (0°), f2 (+6°), f3 (+12°), f5 (+15°), f6 (+13°), f9 (+8°), f12 (+4°), f15 (+1°) | Smooth counter-pitch tracking spine (ease-in/out across all keys) | +2.0° pitch compensation overshoot at f5; settles to f6 | Lags chest by 1 frame (2f total after pelvis); steady, level gaze throughout |
| **Arms** | All keys (f0..f15): Chest-frame locked Bison crossed arms (`author.poses["26"]`) | Zero relative motion to chest frame (tangents match spine) | None (rigid fold on chest, no rebound) | Rigid follower: travels 1:1 with spine_05 and chest coordinate frame |
| **Legs** | All keys (f0..f15): `foot_w` planted `[±18, 4, 9]`. Knees track squat depth: `pole_w` y=30->56.5->30, z=48->29->48 | Ease-in knee compression (f0->f3), ease-out knee extension (f7->f15) | +2.5 cm forward knee push overshoot at f5 (`pole_w` y=56.5, z=29.0) | Compress synchronously with pelvis drop; zero foot slide or drift |

## Mechanics Notes
- **Impact & Squash (f0–f3):** Pelvis drops 28 cm from f0 (-6 cm) to f3 (-34 cm). Torso sinks 1 frame later, cushioning the impact into a deep, dignified Overlord squat.
- **Bottom Settle (f4–f6):** Heavy compression overshoots by 2.5 cm at f5 (pelvis z=-36.5 cm, spine -20°) and rebounds into a grounded settle at f6 (pelvis z=-34.5 cm).
- **Slow Heavy Rise (f7–f15):** Pelvis and legs push upward smoothly from f7, easing to rest at f15 (pelvis z=-6 cm, upright heavy posture) with arms unbroken and still crossed on the chest.
