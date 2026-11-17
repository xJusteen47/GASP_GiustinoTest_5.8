# Overlord DAir Light Landing — Curve Sheet (`overlord_dair_land_lighttuck_r4`)

- **Take**: `AS_Pilot_OverlordDAirLandLightTuckR4`
- **Frames**: 10 (f0..f9 at 60 fps, ~166 ms total)
- **Window**: Light landing lag triggered when Overlord lands during the initial tuck phase (first 3 frames of DAir).
- **Physical context**: Compact ball tuck contacts floor at f0 (pelvis ~43 cm / loc z -47 cm, head ~93 cm, feet close under pelvis). Feet slide to stance by f2 as pelvis compresses to -50 cm; body rises smoothly to idle readiness at f9.

---

## 1. Curve Specifications Per Part

### Pelvis (Root Translation & Tilt)
- **Keys**:
  - `f0`: Loc Z `-47.0`, Y `-5.8`, Pitch `-12.0°` (touchdown contact, ball compressed)
  - `f1`: Loc Z `-49.0`, Y `-5.8`, Pitch `-11.5°` (downward absorption, no rise)
  - `f2`: Loc Z `-50.0`, Y `-5.8`, Pitch `-11.0°` (lowest squash bottom)
  - `f4`: Loc Z `-40.0`, Y `-4.5`, Pitch `-8.0°` (rise begins)
  - `f6`: Loc Z `-20.0`, Y `-2.5`, Pitch `-4.0°` (maximum vertical velocity)
  - `f8`: Loc Z `-8.0`, Y `-0.8`, Pitch `-1.5°` (deceleration into stance)
  - `f9`: Loc Z `-6.0`, Y `0.0`, Pitch `-1.0°` (settled stance, blends seamlessly to idle)
- **Easing**:
  - *Squash (f0–f2)*: Fast deceleration into the bottom squash at f2. Pelvis stays down, never rising before f2.
  - *Rise (f2–f9)*: Cubic ease-out starting smoothly from f2 through f4, steep velocity slope f4–f6, smooth ease-in into f8–f9 without snapping or bounce.
- **Overshoot**:
  - None on rise: Pelvis targets `-6.0` cm at f9 directly, avoiding upward overshoot to blend into standing idle.
- **Overlap**:
  - Pelvis vertical drive initiates at f3–f4, lagging 1 frame behind the head initiating the rise, and pushing upward ahead of the chest/spine unwinding.

---

### Head (Cervical Pitch & Gaze)
- **Keys**:
  - `f0`: Pitch `+22.0°` (level horizon gaze counter-rotating against `-30°` tucked spine; head height ~93 cm)
  - `f1`: Pitch `+20.0°` (maintaining gaze during ground impact)
  - `f2`: Pitch `+18.0°` (lowest head dip during squash)
  - `f4`: Pitch `+8.0°` (actively righting upward, leading the extension)
  - `f6`: Pitch `0.0°` (fully righted to neutral)
  - `f8`: Pitch `0.0°` (held neutral)
  - `f9`: Pitch `0.0°` (held neutral, ready for idle blend)
- **Easing**:
  - Rapid ease-out from f2 into upward pitch recovery across f3–f6; smooth soft landing into 0° at f6.
- **Overshoot**:
  - 0° neutral reached at f6 with zero head recoil/bounce.
- **Overlap**:
  - **Head leads the rise by 1 frame**: Head begins righting upward at f3 (reaching `+8°` by f4), leading both the pelvis extension and spine unfold.

---

### Chest / Spine (Thoracic & Lumbar Pitch)
- **Keys**:
  - `f0`: Pitch `-30.0°` (tight forward tuck at contact)
  - `f1`: Pitch `-28.0°` (absorbing impact)
  - `f2`: Pitch `-26.0°` (bracing at maximum squash depth)
  - `f4`: Pitch `-20.0°` (beginning to unfold)
  - `f6`: Pitch `-10.0°` (mid-rise unwinding)
  - `f8`: Pitch `-3.0°` (near-neutral)
  - `f9`: Pitch `-2.0°` (neutral stance forward pitch)
- **Easing**:
  - Heavy, viscous ease-out out of squash. Lagged acceleration behind head and pelvis; gentle ease-in into final `-2.0°` at f8–f9.
- **Overshoot**:
  - None. Spine does not hyper-extend past upright neutral.
- **Overlap**:
  - **Chest is last**: Spine remains coiled while the head rights and pelvis drives up. Head reaches neutral `0°` at f6, while chest continues extending through f6 (`-10°`), f8 (`-3°`), and settles last at f9 (`-2°`).

---

### Feet & Legs (IK Targets & Knee Poles)
- **Keys**:
  - `f0`: `foot_w` `[±12.0, 12.0, 14.0]` (tucked tight under pelvis, knees high at chest)
  - `f1`: `foot_w` `[±18.0, 8.0, 11.0]` (rapid slide downward and outward)
  - `f2`: `foot_w` `[±24.0, 4.0, 11.0]` (planted at full stance: width 48 cm, depth 4 cm)
  - `f4`: `foot_w` `[±24.0, 4.0, 12.0]` (planted stance, ankle geometry adapting)
  - `f6`: `foot_w` `[±24.0, 4.0, 13.0]` (planted stance)
  - `f8`: `foot_w` `[±24.0, 4.0, 14.0]` (planted stance)
  - `f9`: `foot_w` `[±24.0, 4.0, 14.0]` (planted stance)
  - `pole_w`: Constant `[±30.0, 70.0, 50.0]` style throughout.
- **Easing**:
  - High-velocity linear/quartic slide out across f0–f2 (2 frames); hard friction stop into full planting at f2.
- **Overshoot**:
  - Zero lateral foot sliding once planted at f2.
- **Overlap**:
  - Feet establish base of support early (f0–f2) before any vertical upward body rise is permitted.

---

### Arms & Clavicles (Crossed Hold Identity)
- **Keys**:
  - Constant identity across all frames (f0–f9):
    - `arm_l.hand`: `[-10.4, 9.7, 1.7]` | `pole`: `[60.0, 40.0, -6.0]` | `hand_rot`: `[0.0, -0.2, 0.0]`
    - `arm_r.hand`: `[4.8, 14.4, -1.4]` | `pole`: `[-60.0, 40.0, -6.0]` | `hand_rot`: `[1.6, 4.2, -1.1]`
    - `clavicle_l`: `[-10.1, -7.5, 2.4]` | `clavicle_r`: `[-13.8, -9.1, -2.5]`
- **Easing**:
  - Rigid hold. Arms stay firmly crossed at sternum with elbows flared out.
- **Overshoot**:
  - None. Never uncrosses or separates from sternum.
- **Overlap**:
  - Rigidly parented to chest coordinate space; smoothly inherits chest motion without secondary arm whip.

---

## 2. Timing & Overlap Summary

| Frame | Pelvis Z | Spine Pitch | Head Pitch | Feet [X, Y, Z] | Event / Hierarchy Phase |
|---|---|---|---|---|---|
| **f0** | `-47.0` | `-30.0°` | `+22.0°` | `[±12, 12, 14]` | Contact: ball touches down; head level |
| **f1** | `-49.0` | `-28.0°` | `+20.0°` | `[±18, 8, 11]` | Compression: feet slide down & out |
| **f2** | `-50.0` | `-26.0°` | `+18.0°` | `[±24, 4, 11]` | Bottom squash: feet planted at stance |
| **f4** | `-40.0` | `-20.0°` | `+8.0°` | `[±24, 4, 12]` | Rise begins: **head leading by 1f** |
| **f6** | `-20.0` | `-10.0°` | `0.0°` | `[±24, 4, 13]` | Mid-rise: head arrives at 0°; pelvis passing through |
| **f8** | `-8.0` | `-3.0°` | `0.0°` | `[±24, 4, 14]` | Settle: pelvis eases in |
| **f9** | `-6.0` | `-2.0°` | `0.0°` | `[±24, 4, 14]` | Stance settled: **chest last to settle**, ready to blend |
