# Overlord Dropkick Landing Tech Bounce Curves & Timing Sheet

Clip: `overlord_dropkick_techbounce` (20 frames @ 60 fps, frames 0..19).  
Take: `AS_Pilot_OverlordDropkickTechBounce`  
Rig: `/Game/Characters/UE5_Mannequins/Rigs/CR_Mannequin_Body`  
Move: `overlord_dropkick_techbounce` (aerial / tech bounce landing)

---

## Timing & Curve Summary Table

| Body Part | Key Frames & Values | Easing (In / Out) | Overshoot (Amount & Frame) | Follow-Through / Overlap |
| :--- | :--- | :--- | :--- | :--- |
| **Pelvis** | f0 (z=-80.0, pitch 80°), f1 (z=-83.0), f2 (z=-85.5), f3 (z=-86.0, pitch 90°), f5 (z=-78.0), f7 (z=-72.0), f9 (z=-68.5), f10 (z=-68.0, pitch 96°), f13 (z=-72.0), f16 (z=-74.0), f18 (z=-74.5), f19 (z=-74.6, pitch 84°) | Fast impact squash (f0->f3, -6.0 cm into ground), explosive rebound rise (f3->f7), ease-out apex peak (f7->f10), parabolic gravity fall (f10->f16), smooth ease-in settle into groundhit (f16->f19) | Peak rise overshoot of +18.0 cm over max squash (+12.0 cm over contact) at f9–f10 (apex z=-68.0 cm); settles down 6.6 cm to f19 | Impact driver (0f lag). Rebound launches entire body as a consequence of squash; zero forward pop on transition. |
| **Spine / Chest** | f0 (-12°), f1 (-4°), f2 (+2°), f3 (+6°), f5 (+10°), f7 (+14°), f9 (+16°), f10 (+15°), f13 (+6°), f16 (-10°), f18 (-13.8°), f19 (-14.0°) | Compression flatten (f0->f2), inertial curl into rise (f3->f7), apex hold (f9->f10), uncurl ease into groundhit layback (f10->f19) | +2.0° pitch curl overshoot at f9 (+16.0°); settles back to -14.0° at f19 | Torso follows pelvis rebound with 1-frame lag; provides stable platform for arms. |
| **Head** | f0 (-26°), f1 (-15°), f2 (+4°), f3 (+10°), f5 (-4°), f7 (-12°), f9 (-15°), f10 (-14°), f13 (-8°), f16 (-11°), f18 (-12°), f19 (-12.0°) | Chin tucked at f0, whip-squash to floor f1->f3 (+10° arrive last), trailing lag through rise (f5->f10), settling to -12° at f18-f19 | Head arrives last on squash (+10° at f3); lags rise peak by 1 frame (-15° at f9) | Lags torso by 1-2 frames throughout (heavy villain helmet mass and neck inertia). |
| **Arms (Left)** | f0–f3 (crossed on chest: `hand_w` [-10, -28, 26]), f5 (begins uncross: [5, -32, 30]), f7 (arc outward: [24, -36, 37]), f9 ([40, -40, 43]), f10 (peak balance bar: [45, -42, 45]), f13 ([48, -44, 46]), f16 ([49.5, -44.8, 46]), f18 ([49.8, -45, 46]), f19 ([50, -45, 46]) | Locked on chest f0->f3; ease-in uncross initiation at f5; sweeping circular outward arc f5->f10; ease-out settle f13->f19 | Sweeps wide outward to balance bar position (+50 cm X-spread) matching Reaction GroundHit f0 | **Starts uncrossing at f5** (2 frames before Right Arm). Follow-through during rise, never a 1-frame snap. |
| **Arms (Right)** | f0–f5 (locked crossed on chest: `hand_w` [8, -30, 28]), f7 (begins uncross: [-4, -35, 33]), f9 (arc outward: [-24, -42, 42]), f10 (wide arc: [-32, -46, 46]), f13 ([-39, -50, 49.5]), f16 ([-42.5, -53, 51.2]), f18 ([-43.8, -53.8, 51.8]), f19 ([-44, -54, 52]) | Locked on chest f0->f5; ease-in uncross initiation at f7; outward balance bar arc f7->f10; ease-out blend to groundhit f13->f19 | Reaches balance bar extension (-44 cm X-spread, +52 cm Z) matching Reaction GroundHit f0 | **Starts uncrossing at f7** (exactly 2 frames of overlap after Left Arm starts at f5). |
| **Legs** | f0 (`foot_w` [±5, 68, 48]), f1 ([±5, 66, 46]), f2–f3 ([±5, 63.5, 43.5]), f5 ([±5, 65, 52]), f7 ([5/-7, 66/65, 57]), f9–f10 ([7/-9, 65.5/63.5, 60.5]), f13 ([10/-13, 62/58, 56/61]), f16 ([14/-17.5, 57.5/50, 51/63]), f18 ([17/-20, 54.8/46.2, 48.2/63.8]), f19 ([17/-20, 55/46, 48/64]) | Legs straight & together f0->f10; slight knee give f1->f3; lift with rebound f4->f9; apex float f9->f10; asymmetric blend into GroundHit f10->f19 | Vertical leg lift apex at f9–f10 (+12.5 cm over contact); cushions down to GroundHit pose at f18–f19 | Straight together through impact and rise; separates smoothly into GroundHit reaction frame 0. |

---

## Identity Traits & Mechanics Verification

1. **Legs Together & Straight Through Impact and Rise:**  
   - Maintained with close horizontal stance (X spacing ~10 cm, centered on trunk) and straight knee extension from contact f0 through peak f9–f10.  
   - Knees give slightly together during squash f1–f3 (feet drop ~4.5 cm in Z, poles stay parallel), preserving the missile silhouette of the dropkick loop.
   - Legs blend outward into the asymmetric landing stance only during the downward fall (f10–f17).

2. **Arms Crossed at Impact, Asymmetric Follow-Through Uncross During Rise:**  
   - Arms remain locked crossed across the chest through contact (f0) and squash (f1–f3).  
   - Follow-through uncross begins only after rebound initiates: **Left arm uncrosses at f5**, **Right arm uncrosses at f7** (2 frames of overlap / stagger).  
   - Both arms open in a wide outward arc like a tightrope balance bar, avoiding any sudden 1-frame snaps, reaching their full open balance posture at peak (f10) and settling into GroundHit frame 0.

3. **Rebound Consequence of Impact (Not a Jump):**  
   - f0 = rigid contact on back (pelvis bone ~12.8 cm, lowest surface ~1.8 cm at floor).  
   - f1–f3 = heavy squash compression into ground (pelvis drops 6.0 cm to -86.0 cm, head whips back to floor f2–f3).  
   - f4–f9 = natural elastic rebound rising +18.0 cm over max squash (+12.0 cm over initial contact) peaking at f9–f10.  
   - f10–f17 = freefall descent blending toward the ground hit pose.  
   - f18–f19 = EXACT match with `cards/reaction/reaction_groundhit.plan.json` keys["0"] (zero velocity / zero pop tangent with `hold: true` on f19).


## r1 retime (2026-11-17, desktop thread)
The game's hop (25 cm track, DA_DefenceRules_Test gravity scale) measured in `Saved/DropkickBounce/variant2/dropkick_whiff/frames.jsonl`: actor peak at contact +6/+7, ground at +12. Keys moved 0,1,2,3,5,7,9,10,13,16,18,19 -> 0,1,2,3,4,6,7,8,10,12,13,14 (clip 15 frames, ability TotalFrames 15). Impact/squash f0-f3 unchanged; arms: left starts f4, right f6 (2 frames overlap kept); peak f7-f8; lying pose (GroundHit f0) at f13-f14.
