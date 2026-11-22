# Gemini (Antigravity) on dropkick k5r3: follow-through, secondary motion, weight (2026-11-21, raw answer)

Asked from the desktop thread runstart-stancewidth on the coordinator's request (owner wants Gemini's view). Model: Gemini 3.8 Flash (High)
via agy-bridge analyze_files, session 5455e291-8c07-40ee-ada3-b4bafa285144. Inputs (branch claude/project-thread-cqad05 82d09c64):
k5r3/README.md, overlord_dropkick_k5_curves.md, k5_dropkick_sweet_hit_az90_slow4x.mp4, k5_dropkick_sweet_whiff_az315_slow4x.mp4,
k5_dropkick_sweet_hit_game_realspeed.mp4, grid_k5high_fall.jpg. Question: follow-through/secondary/weight per body part, arms squeezing
into the crossed pose then relaxing, overshoot after the strike, weight in fall and landing, keeping legs together/straight, arms crossed,
smug fall. NOT reviewed by us: frame refs and bone names are Gemini's (e.g. "knee_l/r" are calf bones on our rig; "whiff ragdoll"
claim not checked). Answer below: sections 1 and 5 verbatim; 2-4 and the table compacted (same numbers).

---

### 1. Follow-Through and Overlapping / Secondary Motion

*Current flaw: `curves.md:17-18` deliberately forced `part_delays = 0` to preserve the snap at f15, which eliminated all natural mass transfer and caused the character to rotate as a single rigid block.*

- **Head & Neck**:
  - **f0–f3 (Takeoff)**: As torso pitches forward and vaults (pelvis pitch 5° → 12°, `curves.md:9`), the head must drag backward (`neck_01` pitch +4°, chin dips down 2 cm) before whipping into the tuck at f4.
  - **f13–f15 (Strike Snap & Follow-Through)**: On the smear frame f14 (`curves.md:12`), hold the head locked on target (`head` pitch -38°). On the f15 hit impact (`curves.md:13`), do not freeze the head flick instantly; let `neck_01` compress forward 1.5 cm on impact, then snap the chin upward (`neck_01` flex 14°, `head` tip back +16°) peaking on f16.
  - **Fall & Slide (f16–landing)**: In the fall glide (`grid_k5high_fall.jpg:a16-a28`), keep active neck flexion (+12° to +14°) fighting gravity so the head stays suspended above the torso line, resting smugly rather than dropping dead center.
- **Torso & Spine**:
  - **f3–f7 (Tuck)**: Pelvis drives the rotation first (`curves.md:10`). Add a 1-frame hierarchical delay: pelvis rotates on f3, lumbar (`spine_01`-`spine_02`) flexes on f4, thoracic (`spine_03`-`spine_05`) flexes on f5, reaching the full curl (-22°) on f6–f7.
  - **f13–f15 (Smear to Strike)**: Reverse the wave. Thighs and pelvis thrust out first on f13.5; lumbar straightens at f14; thoracic hyperextends into the proud arch (`spine_05` -8° to -10°, `curves.md:13`) at f15.
- **Arms**:
  - **f0–f3 (Fold Entry)**: Shoulders lead inward; elbows lag by 1 frame; wrists/hands drag by 2 frames before slapping onto the chest at f3.
  - **f13–f14 (Transition into Lying Fold, `curves.md:25`)**: Forearms must drag against the air stream (+2 cm outward lift) during the 63 cm smear thrust before slamming onto the ribs on f15.
- **Legs**:
  - **f3–f7 (Chamber)**: Thighs pull to chest first; shins and feet trail behind, compressing tightly against the buttocks on f7.
  - **f13–f15 (Extension)**: Thighs extend forward on f13.5; knees whip to 0° lock at f15. Ankle plantarflexion overshoots on f16 (-56° toes) before resting at -50° (`curves.md:13`).

### 2. Arm Squeeze Accent and Settle (Bison Fold)

*Current flaw: Arm spacing changes are either sudden 1-frame snaps or frozen static values (`curves.md:25, 27`). The fold lacks muscle compression and breathing room.*

#### Phase A: Startup Squeeze & Settle (Air Guard → Chest Fold)
- **f0 → f3 (Travel & Contact)**: Arms travel from air guard to chest fold (`curves.md:9`). Easing: `Ease-Out Quad`. Hand contact touches opposite arms at f3.
- **f3 → f4 (The Squeeze Accent / Clench)**: 1 frame, `Linear` / `Hard Snap`. Clavicles depress and pull forward (`clavicle_l/r` pitch -4°, roll -3°). Upper arms clamp against pectorals; forearms press toward spine by **-3.5 cm** (depth to `spine_05` from 11.0 cm to 7.5 cm). Elbow separation narrows by **-4.0 cm** (21 cm to 17 cm off spine). Wrists flex inward +8° gripping the triceps.
- **f4 → f7 (Relax & Settle into Chamber)**: 3 frames, `Ease-Out Cubic`. Clavicles ease back +2°; forearms settle outward **+1.5 cm** (9.0 cm off chest); elbows relax outward **+2.0 cm** (19 cm from spine), into the held chamber at `curves.md:11`.

#### Phase B: Strike Impact Compression & Settle (Lying Fold)
- **f14 → f15 (Impact Squeeze)**: 1 frame (strike arrival, `curves.md:13, 26`). Arm block drops **-2.5 cm** closer to chest (8.8 cm above sternum vs 11.3 cm in `curves.md:25`); elbows tuck inward against ribs **-2.0 cm**.
- **f15 → f18 (Settle to Smug Glide)**: 3 frames (exiting hitstop), `Ease-Out Sine`. Arms expand outward **+1.2 cm** (arm block at 10.0 cm above sternum; elbows at 19 cm off spine), locked for the fall.

### 3. Post-Strike Overshoot Without Breaking Straight-Together Legs

*Current flaw: Overshoot was skipped (`curves.md:28-29`, `README.md:29-30`) because keying it into the 12-frame loop caused a 5 Hz jitter. The strike presently halts with zero dissipation.*

- **Architectural Fix**: Add a non-looping 4-frame montage section (`STRIKE_SETTLE`, f15 → f19) immediately after the 4-frame hitstop (`curves.md:14`) before blending into `FALL_LOOP`.
- **Pelvis & Torso Pitch**: f15 pelvis pitch **84.0°** (+3.5° over the loop's 80.5°, beyond the 82° in `curves.md:13`); f17 rebound **78.5°** (`Ease-Out Quad`); f19 settle **80.5°** (`Ease-In-Out Sine`).
- **Chest & Spine Arch**: f15 `spine_05` **-10.0°**; f17 **-6.0°**; f19 **-8.0°** (`curves.md:26`).
- **Head / Neck Smug Flick**: f15 base strike pose (`neck_01` flex 12°, tilt 25°); f16 chin extra **+6.0°** (18° up), face yaw **30.0°** toward az90; f18–f19 back to 14° chin up, 25° yaw.
- **Legs straight & together**: knees locked at **0.0°** on all frames. Hips hyperextend +3.5° relative to pelvis at f15 (heels up in line with the torso). Ankles: -50° at f15, **-56.0°** at f16, back to **-48.0°** on f18 (`Ease-Out Cubic`). Thigh adduction clamp +2.0° inward on both hips at f15 so knees and ankles stay pressed together.

### 4. Selling Weight in the Fall and Landing (Smug Attitude)

*Current flaws: in flight (`grid_k5high_fall.jpg:a16-a28`) he hangs like an immovable board. At landing on whiff (`README.md:28`, `k5_dropkick_sweet_whiff_az315_slow4x.mp4:00:03`) arms and legs flail open like a limp ragdoll, destroying the smug persona.*

- **Fall posture**: pelvis 78° with spine arched -8°, a shallow confident bow (sacrum 5 cm lower than heels). Subtle float across the 12-frame loop: pelvis ±1.5 cm Z, ±1.0° pitch (`curves.md:15`).
- **Landing anticipation (1 frame before impact)**: no arm flare (`README.md:28`), arms stay crossed; chin tucks +4° (bracing, eyes on the opponent).
- **Impact frame**: shoulder blades hit first. Squash: pelvis **-3.5 cm**, spine flattens -8° → 0°, shoulders hike +2.5 cm. Crossed arms driven into the sternum (**-3.0 cm**), elbows flat on the floor.
- **Rebound & slide (+1 to +5)**: +2 pelvis/thoracic bounce **+2.5 cm**, settle at +5. Head: contact chin to chest (`neck_01` +16°), +2 snaps back, +5 settles tilted 25° to camera. Heels hit 1 frame after the back; knees straight; feet from pointed (-50°) to -5° as heels skid, dust.
- **Whiff persona**: delete the limb splay; he slides on his back, arms crossed, legs together, and stops with an unbothered shrug.

### 5. Stiff, Floaty, and Erroneous Elements

1. **Whiff landing desync** (`README.md:28`, whiff az315 a32 v170): the landing blend opens the forearms and splays the legs, breaking the core identity. The whole landing clip must keep the arm fold and closed legs.
2. **No post-strike recovery** (`curves.md:28-29`): the strike arrives at full speed and freezes; reads like an engine hitstop bug.
3. **Plank flight static hold** (a16-a28): 100% rigid, no sway, breathing or toe drag: looks hung on a wire.
4. **Mechanical arm smear** (`curves.md:25`, f13–f14): 1-frame upright→lying fold with no drag pops the forearms in the side view (az90).
5. **Instant head snap on strike** (`curves.md:13, 26`): head yaw/tilt 20° in one frame (f14 → f15) with no follow-through, strobing at real speed.

### Prioritized keyframe table

| Priority | Frames | Bones | Channel | Delta / target | Easing | Ref |
|:---|:---|:---|:---|:---|:---|:---|
| P1 | f3 → f4 | clavicle, upperarm, lowerarm l/r | trans/rot | depth -3.5 cm, elbows -4.0 cm in, wrists +8° | Linear (snap) | curves.md:9 |
| P1 | f4 → f7 | clavicle, upperarm l/r | trans/sep | depth +1.5 cm, elbows +2.0 cm out | Ease-Out Cubic | curves.md:10-11 |
| P1 | f15 → f16 | pelvis, spine_05 | pitch | pelvis 84.0° (+3.5°), spine -10.0° | Linear → Ease-Out | curves.md:13, 28-29 |
| P1 | f16 → f19 | pelvis, spine_05 | pitch | 78.5° (f17) → 80.5° (f19); spine -8.0° | Ease-InOut Sine | curves.md:15, 27 |
| P2 | f15 → f16 | foot l/r | pitch | -50° → -56.0° (knees 0°) | Ease-Out Quad | curves.md:13 |
| P2 | f16 → f18 | foot l/r | pitch | -56° → -48.0° | Ease-Out Cubic | curves.md:15 |
| P2 | f15 → f16 | neck_01, head | pitch/yaw | chin +6.0° (18°), yaw 30.0° to az90 | Ease-Out Quad | curves.md:26 |
| P2 | f16 → f19 | neck_01, head | pitch/yaw | 14° chin up, 25° yaw | Ease-In-Out Sine | curves.md:26-27 |
| P3 | f14 → f15 | upperarm, lowerarm l/r | trans | arm block -2.5 cm to sternum; elbows -2.0 cm | Linear (impact) | curves.md:25-26 |
| P3 | f15 → f18 | upperarm, lowerarm l/r | trans | arm block +1.2 cm out to rest | Ease-Out Sine | curves.md:27 |
| P4 | contact 0 | pelvis, spine_01-05 | Z / pitch | pelvis -3.5 cm, spine pitch 0.0° | Linear | README.md:28 |
| P4 | contact +2 | pelvis, neck_01 | Z / pitch | pelvis +2.5 cm; chin back +12° | Ease-Out Quad | README.md:28 |
| P5 | loop 0–11 | pelvis, spine_05 | Z / pitch | ±1.5 cm, ±1.0° (24 f period across loop reps) | Sine | curves.md:15, 27 |
| P5 | whiff clip | all limbs | pose | crossed-arms slide instead of splay | hold | README.md:28 |
