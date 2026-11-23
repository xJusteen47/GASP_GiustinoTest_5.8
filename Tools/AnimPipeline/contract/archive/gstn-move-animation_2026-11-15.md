> ARCHIVED 2026-11-15. This was the account skill `gstn-move-animation`. It is NOT a procedure to follow any more: it pushed the model to build a move on its own and to write dense keys. Replaced by `gstn-anim-reviewer`, `gstn-anim-executor` and `Tools/AnimPipeline/contract/`. Kept only to look things up (Cascadeur scripting and physics tools, cameras, thresholds). Where it disagrees with the contract, the contract wins.


# Animating GSTN combat moves (Cascadeur -> Unreal)

Use this when a move animation for the GASP_Giustino2 project must be created, polished,
reviewed or improved. The owner blocks key poses in Cascadeur (Step interpolation) and gives a
short text description and/or the move sheet. You turn that into a clip that reads like a
3D Smash Bros move and lands in Unreal.

Write reasoning, logs and messages to the owner in **Italian**. The owner judges the result
on video and spots everything, so look at every view yourself first. Your job is to reach
that point with the technical problems already removed.

## Two commands

- **"Rivedi la mia animazione"** (review only). Run every check below on the owner's clip
  and render the sheets. Write a short report: what breaks which principle, at which frame,
  and a suggested fix. Change nothing.
- **"Migliora questa animazione"** (improve). Input: the owner's Step `.casc`, a text
  description, and optionally a reference (CSV curve, frame data, a GIF split into frames).
  Keep the owner's key poses and intent. Retime them to the reference windows, add the
  in-betweens and polish. Fix pose problems only when a check fails or the owner allows
  pushing poses, and log every change.
- **From scratch** (no owner blockout, for example the Ganondorf forward tilt pilot): build the
  poses yourself from the UFD references, adapted to 3D, with a stance that fits the move
  (the owner's punch guard is in References).

**Start and end pose (owner rule):** every move (except crouched and aerial moves) is entered
**from an upright standing idle**, the pose before frame 0. Frame 0/1 must already show a big,
readable change into the move's own stance (the jab starts directly in its guard), not a slow
transition. During recovery the body rises back toward the upright idle by the FAF (Mario's
jab rises from about frame 15). Each move needs a stance that fits it: Ganondorf v1 reused
the low two-footed jab guard for a one-legged kick and sank into a squat.

**Big pose changes (owner rule):** if a fix needs a relatively big pose change beyond what
the owner asked for, stop, describe the change and why, offer the solution, and apply it
only after his approval. Widening the guard or rotating the body toward profile is the
right tool for silhouette problems. The owner may waive the rule for specific moves (he did
for jab v11 and the Ganondorf forward tilt); still log every change.

## Effort and delegation

- Technical steps (export, Live Link, metrics, renders, scripting fixes): **medium** effort
  or a cheaper model.
- Pose and timing decisions, judging an animation, reading silhouettes: **high** effort,
  or xhigh when the owner asks. Never Fable.
- Keep each pass a closed loop: edit -> checks -> look at the sheets -> log -> next pass.
- **Cost rules (owner, 2026-10-31):**
  - Intermediate passes: numbers and checks first, then images only of flagged frames and
    key poses, 4 views. The full six views, game cameras and videos only on the final version.
  - Mechanical steps (export, Live Link, renders, metric runs, page publishing) go to a
    Sonnet or Haiku subagent at low effort.
  - One fresh session per move. When a move is delivered, write `Saved/AnimPipeline/handoff.md`
    (state, files, open checks, next steps) for the next session.
  - The cloud thread reviews images only of the final version.
  - Keep reports short: numbers and decisions.
  - Log wall-clock time per step in `<move>_log.md`.

## Where things are (desktop, `C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2`)

- Scratch work: `Saved/AnimPipeline/`. It holds:
  - `<move>_vN.casc` and `<move>_log.md`;
  - `metrics.py`, `clearance.py`, `visual_check.py`, `capture_ue.py`, `ue.py`,
    `bridge_call.py`;
  - checks: `legibility.py`, `legibility_shots.py`, `legibility_page.py` (review page),
    `motion_check.py` (includes `joints.py`: joint naturalness, elbows, forearm/hand roll),
    `traj_overlay.py` (bone paths), `easing.py` (`strike()`, `strong_strike()`),
    `balance_check.py`, `pose_rules.py`, `arm_check.py` (all run inside `motion_check.py`),
    `view_sheets.py <take> "<poses>" elbows=f,f` (elbow close-ups);
  - idle: `idle_points.py` -> `idle_points.json` (rig points of `M_Neutral_Stand_Idle_Loop`);
  - curves: `sparse_apply.py` (sparse plans, same key frames on all layers),
    `ue_tangent_test.py` (per-key Sequencer tangents/weights from Python);
  - runner: `ganon_iter.sh` (env `TAKE CASC ASSET MODEL DENSE=1 SMOOTH=12-36 CHECKS=1`),
    timings in `sheets/<take>_timings.tsv`; review page template `ganon_page.py`;
  - `README.md` (pipeline, every script, where each check and threshold lives);
  - `handoff.md` (state for the next session), `tangent_test.casc` (curve experiments);
  - references: `ufd_fetch.py`, `ufd_measure.py`, `refs/ufd/` (`ufd_framedata.csv`,
    `calibration_metrics.csv`, `*_timing.csv`, GIFs), `refs/klk_*`;
  - data: `move_sheets.json` (per-move timing, FAF, hold frames),
    `legibility_thresholds.json`, `motion_thresholds.json`;
  - `cascadeur_api_verified.md` and `sheets/`.
- Move sheets (CSV) are in project files `anim-pipeline/pilota/` (`mosse.csv`, `pose.csv`,
  `hitbox.csv`, `cancel.csv`, `hurtbox.csv`). Frames in the sheet are **game frames**.
- Reference curves are in project files `anim-pipeline/pilota/riferimento/`, for example
  `mario_jab1_timing.csv`.
- Unreal assets go only under `/Game/AnimPipeline/Pilot/` (for example `AS_Pilot_JabV11`).
- Never overwrite the owner's `.casc` or an earlier version. Always save `<move>_v(N+1).casc`.
- Don't touch git, `DefaultLevel.umap` or stray `NewBlueprint` assets unless asked.
- Don't use the desktop while the owner is using it.

## Timing rules

- The simulation ticks at **62.5 Hz**, and clips are authored at **60 fps**.
  `FrameAnim60 = round(gameFrame * 0.96)`. The clip's last frame is
  `round(TotalFrames * 0.96)`. This is a settled decision: never map 1:1.
- Stay at **60 fps**. The owner rejected 120 fps scenes as excessive.
- Slow motion (time dilation): Unreal interpolates each bone's rotation between baked frames,
  so a 1-frame snap can leave the strike line between frames (jab v11: 7 cm at 0.25x). Build
  the snap poses so that per-bone interpolation stays close to the line: avoid big rotations
  of the upper arm/forearm or thigh/shin inside one frame where possible.
- Smash frame data maps 1:1 to game frames (`gameFrame = SmashFrame - 1`).
- **FAF convention (owner, settled)**: count game frames from 0. A move with UFD total N
  cancels at game frame N (jab: total 19 -> cancel at game frame 19 -> anim frame
  round(19 x 0.96) = 18). Record it in `move_sheets.json`.
- Phases: startup -> active (hitbox) -> recovery -> **FAF** (any input cancels).
  The gameplay animation ends at the FAF; all of the move's character must be in there.
- After the FAF an optional **tail** returns to idle. It starts AT the FAF and ends on the
  idle of the skin's trait (Anim.Trait.*), so Visual variants end on their own idle.
- Hitstop is a montage pause in code. Never bake it into the clip.

### Replicating a reference

1. Split the reference into frame windows. Measure per game frame the extension of the
   attacking limb (0 = guard, 100 = contact) and head/torso offsets, then save a CSV next to
   `mario_jab1_timing.csv`. The jab-1 example windows are: startup G0; active G1-2 at 100;
   snap retract G3-5 (46, 24, 17); **spring recoil** G6-8 (back forward to 29); hold G9; slow
   return G10-13; rise to guard G14-18 (FAF at G19).
2. Drive the limb with keys shaped to that curve. Accept a max error of a few %.
3. Note any deviation forced by proportions (for example, torso lunge kept at 62%).

### References

- **First step for every new move**: run `ufd_fetch.py <character> "<move>"` on the desktop
  for 2-4 similar SSBU moves (similar frame data or similar animation). It appends the frame
  data to `ufd_framedata.csv`, downloads the hitbox GIF and measures it with
  `ufd_measure.py` (extension per frame, impact spacing, hold, recoil). Take style ideas
  from them, then animate.
- **ultimateframedata.com** has every SSBU move: frame data and hitbox GIFs. The owner allows
  downloading any GIFs needed. The cloud proxy blocks it, so fetch them from the desktop
  and keep them in `Saved/AnimPipeline/refs/ufd/`. Split each GIF into frames and measure:
  - extension %;
  - impact spacing;
  - hold and recoil;
  - torso tilt.
  These are SIDE views: use them for timing, spacing, hold and weight, never as rear-view
  legibility targets.
- Rear/far-camera readability: Kill la Kill IF (owner's clip). Attacks travel laterally or
  diagonally outside the silhouette, the torso turns near-profile on the strike, the stance
  is wide and low, the anticipation is long and visible, and contact lasts 1 frame plus a
  trail VFX.
- Owner's examples: Sakurai's Terry forward tilt (weight, impact spacing) and Ryu's forward
  smash (hold + recoil). Owner's guard pose for punches:
  `anim-pipeline/pilota/riferimento/jab_guardia_proprietario_2026-10-31.png` (wide stance,
  torso toward profile, wider lower guard, elbows down); moves are still entered from an
  upright idle.

## Posing fundamentals (check every key pose)

Researched after Ganondorf v1 looked "completely unbalanced and weightless". Sources: Animator
Island "Principles of Animation Physics" part 4, Thinking Animation "Balance".

Automatic (in `motion_check.py` / `joints.py`):
1. **Static balance**: the centre of mass projected on the floor lies inside the support base;
   on one foot, over that foot (ball to heel). Off-base only when momentum carries it there
   (a lunge, a fall) and a following frame catches it; declare those frames.
2. **Weight shift before lift-off**: the CoM moves over the support foot before the other foot
   leaves the ground.
3. **Counterbalance**: when a limb extends one way, torso, arms and head move the other way so
   the CoM stays over the base (kick forward -> torso back, arms back/out).
4. **Contrapposto and support leg**: on the weight-bearing side the hip rises and the shoulder
   drops. A standing support leg is only mildly bent (target about 15-40 deg; the automatic
   check accepts 10-45 deg on one foot unless the sheet declares a crouch),
   knee over the toes, foot flat, pivoting on the ball with the hips; pelvis not sunk. No
   near-kneel unless the move is a declared crouch.
5. **Kinetic chain**: velocity peaks travel from the ground up: support foot/pivot -> hips ->
   torso -> shoulder/thigh -> limb end.
6. **No twinning**: left and right limbs are not mirrored in angle or height at key poses.
7. **Head and gaze**: the head stays stable and looks toward the target during the attack.
8. **Feet**: planted feet flat, no sliding, no floor penetration, correct heel/toe contact.

Thresholds as implemented:
- `balance_check.py`: CoM from volume-weighted capsules; support base = flat foot, or toes
  only when the heel is up; 2 cm tolerance. Declared lunges, falls and airborne frames go
  in the move sheet as `balance_exempt_frames` (jab: contact lunge frames 1-3).
- Weight: punches = CoM shift into the strike; kicks = hip drive from the wind-up of at
  least 8 cm, with the whole-body CoM over the support foot. Kicks: upper body leans back
  relative to the hips by at least 2 cm from the anticipation key to contact.
- `pose_rules.py`: weight shifted before lift-off; kick chain pelvis -> thigh -> shin -> foot
  over contact-2..contact+1; no twinning at contact and mid-hold (left/right elbow, knee
  and upper-arm angles all within 6 deg = fail); head within 35 deg yaw / 30 deg pitch of
  the attack through strike and hold; planted-foot slide at most 0.5 cm (ball pivot allowed).
- Calibration: Ganondorf v1 fails balance, lift-off, both support-knee rules and foot slide;
  jab v11 passes everything.

By eye (one line per key pose in the log):
9. **Line of action**: one clear C or S curve through the body, not a stiff vertical.
10. **Character mass**: a heavy character (Ganondorf) loads bigger and longer, pushes with
    commitment and settles longer; nobody ever looks weightless.
11. **Phase story**: idle -> weight shift -> load -> strike -> hold -> recovery -> idle reads at
    a glance.

## How the owner judges (check in this order)

1. **Weight and force depiction** (Terry's forward tilt = the move's name, not a spine lean):
   weight comes from impact spacing (item 2), the hold (item 3) and a readable weight
   transfer (anticipation the opposite way, centre of mass moving with the strike). Terry's
   torso actually counterbalances backward, so spine tilt is info only, never a rule.
2. **Impact spacing**: a long interpolation can be good or bad. A strike needs a long
   ease-in build, then the striking limb reaches its target in only **1-2 frames**. Most of
   the path is covered in the last 1-2 frames, and the speed peaks on the frame into contact.
   Even (contiguous) spacing kills the impact. This is why tangents/easing matter. In
   Cascadeur, see "Curves" below.
3. **Hold + recoil**: most attacks hold the pose for a while (Sakurai). Strong moves keep
   the limb extended on the held pose for several frames, with a small decaying
   back-and-forth recoil of a few cm, instead of retracting at once. The number of hold
   frames is set per move in the move sheet (jab 1-2, smash 6+). A perfectly frozen hold
   on a strong move is a defect.
4. **Secondary motion / overlap**: speed peaks cascade from shoulder to elbow to wrist to
   hand by 1-2 frames, and from chest to head, with a settle after the stop.
5. **Smooth trajectories**: draw the main bones' motion paths. Jitter shows up mostly on
   **arcs and circular motions** (swings, spins, a hand circling back to guard): a path
   leaving the smooth arc, or speed along the arc wobbling. Intentional recoil inside a hold
   window is not jitter.
6. **Legibility**: limbs and **elbows stick out of the body** and are exaggerated, from every
   direction.
7. **Joint naturalness (bones, not mesh)**: if a pose is wrong the limb will look broken
   anyway, so judge the bones. Wrists in line with the forearm; elbows pointing down/back,
   never outward or reverse-bent; knees over the toes, no inward collapse or hyperextension;
   ankles, foot roll and ball of the foot natural. An out-of-range angle is accepted only as
   an **exaggeration**: in the direction of the motion or force at that moment, and only in
   the frames of that action. Otherwise it's a defect (v8's inward wrists exaggerated
   nothing). The hand/foot must rotate with its limb, never stay frozen in world space while
   the forearm/shin turns.

Kick mode (move sheet `strike_limb: leg_l`, effector `ball_l`; `legibility.configure(take)`):
the shin is the "forearm"; rule F judges leg reach and the knee outside the outline; the
rear-elbow flare rule is off (arms counterbalance on purpose); the arms-hide-the-foot
window ends 1 frame after the hold (owner may override). `joints.py`: an airborne kicking
foot is not judged for roll or floor; knee-over-toes only where both directions are
horizontal; a planted foot pivoting with the hips is natural. `motion_check`: the wind-up
reversal (anticipation key +-2) is a beat, not jitter.
Kicks: the attacking part is the foot, and either arm may cover it; weight is measured from
the load; the load reversal counts as a beat; a hanging arm may point its elbow back; the
"arm over the attacking limb" rule applies until 1 frame after the hold.

Default strike profile for generated in-betweens: long build -> 1-2 frame snap -> hold with
decaying recoil (strong moves) -> recovery led by the limb.

## Cameras

- Most levels: a far, high camera on rails, like Mario 3D World. PvP 1v1: a lateral camera.
  The designed Follow camera in GSTNCamera has a 650 cm arm, pitch -25 and FOV 65; check the
  plugin docs for the current presets.
- Camera rings used by `legibility.py`: follow `f000-f315` (elevation 25, 650 cm), rail
  `h000-h315` (elevation 40, 1000 cm), PvP `p090/p270` (elevation 12, 450 cm). GSTNCamera is
  not wired to the character yet (the GASP camera runs today), so re-check when it is.
- **Every review shows six views per key pose, side by side: front, right, back, left, top
  and bottom** (plus 3/4 and the game cameras). Top and bottom count in the legibility check
  for aerials; for ground moves they are shown only. Look at all of them yourself, including
  elbow and wrist direction on both arms.
- The character turns freely and attacks in any direction relative to the camera. So judge
  legibility over the **full ring of yaws** at the far/high elevation plus the side view, and
  report the worst yaw. Never judge from a single "behind" view.
- Weapon swings must be predictable and centred on the attack direction (for example, a
  vertical light-penetration slash on a big enemy has a vertical arc plane containing the
  attack direction). PvE penetration areas depend on it.

## Style principles (what "good" means here)

Sakurai / Smash:
1. Startup readable but short. Contact is a **snap**: 0-1 frames between chamber and hit.
2. Hold the contact pose through the active frames, then snap back with a small spring.
3. Push poses past "realistic": deeper pelvis, lean, twist. If a pose isn't exaggerated
   enough and the owner allows it, push it yourself and log why.
4. Recovery and the FAF end in a compact guard that reads as "ready", with **fists closed**.
5. The silhouette must read from every side, because this is a 3D game:
   - From side and 3/4 views, the attacking limb must leave the torso outline at contact.
   - From **3/4 and rear views** the attacking fist must stay visible. The off arm must
     never be raised over it or cover it.
   - The **rear elbow stays tucked inside the torso outline**. It must never swing out
     toward the back.
   - Frontal views will collapse, so add lean, head offset and off-hand position to
     compensate.

Mechanics (Cascadeur separation/compensation; owner feedback on jab v5-v7):
6. **Separation**: the body builds energy and almost stops, then the limb takes the fast
   part. The centre of mass path stays smooth even when the fist is sharp.
7. **Compensation**: while the limb goes out fast, the body and the off arm counter-move.
8. **Recovery is muscle-driven and starts from the limb.** The fist retracts first (elbow
   bends, arc slightly down then in), the shoulder follows, and the torso unwinds 1-2 frames
   later. If the torso moves while the arms stay rigid, it reads as "a doll pushed by an
   external force". Arms must never be passive cargo.
9. Overlap: the wrist settles 1-2 frames after the forearm, and the off hand has its own
   timing offset (1-2 frames) and a small settle. The head stabilises before the torso.
10. Overshoot and settle in small amounts (a few degrees or cm), never floaty.
11. Feet stay planted unless the sheet says otherwise (drift < 0.5 cm).
12. No self-penetration. Hands must stay clear of the head and torso.

## Cascadeur scripting (verified on 2026.2.3; full list in `cascadeur_api_verified.md`)

- The MCP server is `cascadeur`. Start it from Commands > MCP > Start script server. It stops
  when Cascadeur restarts. Start it again yourself with computer use (owner's OK), or ask
  the owner. There is one tool,
  `run_script(code)`, also reachable as `POST http://127.0.0.1:8765/run`. `scene`, `csc` and
  `app` are in scope. PySide6 6.5.3 is available, and the UI is QML.
- Start from the sample `UEFN_Mannequin.casc` (88 bones = SKM_UEFN_Mannequin). Y is up, the
  character faces +Z, and its right side is -X.
- FPS can't be set from the API: new scenes start at 30. Set 60 in the UI with computer use
  (owner's OK on Cascadeur's tools) or ask the owner.
- Every edit goes inside `scene.modify_update(name, fn(model, update, su))`. It can fail
  while returning ok, so check `%LOCALAPPDATA%\Nekki Limited\Cascadeur\logs\cascadeur_log.log`.
- Pose via IK points: call
  `update.get_object_by_id(id).root_group().node_deep('Position').set_value(v, frame)`, then
  `su.run_update({node.data_id()}, frame)`. Also key the hand/head **direction points**, or the
  forearm twists between keys.
- Interpolation per interval:
  `le.change_section(pos, layer_id, lambda s: setattr(s.interval, 'interpolation', csc.layers.layer.Interpolation.X))`.
  The types are STEP, BEZIER, LINEAR, CLAMPED_BEZIER, LOW_AMPLITUDE_BEZIER, FIXED and AI.
- Per key (`scene.layers_viewer().layer(id).key(f)`), you can set:
  - the tangent type, Continuous or UserDefined;
  - `left_weight_velocity` / `right_weight_velocity` (default 0.333), which are the ease
    weights;
  - `common.ik_fk` and `common.fixation`.
  Free handle directions and Trajectory Tangents have **no API**.
- **Curves.** The owner wants editable curves. What was learned on Ganondorf v1:
  - Plain BEZIER overshoots on strikes (2.2 cm past contact, speed peak mid-path): don't use
    it for snaps.
  - CLAMPED_BEZIER with a long incoming handle on the contact key (`left_weight_velocity`
    about 0.9) and a short outgoing one on the anticipation (0.05) gives about 72% of the path
    in the last 2 frames, no overshoot. Add a "pre-snap" key one frame before contact and make
    the last interval LINEAR to put the speed peak into contact.
  - **Per-controller interpolation**: keys and interpolation are per LAYER (the rig has 13
    body-part layers). Create a new layer by script and move a single controller into it
    (kicking-foot IK, knee direction, hands, head). It then has its own keys, interpolation
    and handle weights, so overlap comes from offset keys. Moving it clears its animation:
    re-key it by script.
  - With sparse keys Cascadeur interpolates IK points in world space, so check arcs on the
    return and add breakdown keys where they go off.
  - **Limit on this rig**: shoulder/elbow/hand and hip/knee/ankle are IK chains whose points
    sit on different layers. If points of one chain have keys on different frames, the joint
    in the middle zig-zags. So a controller on its own layer must keep its keys on the same
    frames as the rest of its chain; only interpolation and weights may differ. Ganondorf v1
    ended with keys on every frame and the overlap in the curves.
  - The exact per-key tangent control is in Unreal (Sequencer: tangents and weights per key
    settable from Python). Plan: poses and keys in Cascadeur, numeric curve polish in Unreal.
  - Trajectory tangent handles and Graph Editor curves have **no API** (not scene objects).
    Dragging them with computer use is imprecise; leave that polish to the owner.
  - **Default keying**: key every frame on every layer from a smooth model, with overlap
    as per-part time offsets in the curves. Sparse plans (`sparse_apply.py`) only with the
    same key frames on all layers.
  - Animation **unbaking** (tested): 44 -> 13 keys per layer within 3 cm, but it erases the
    snap (foot 35 cm early). Usable as an editable base for the owner only after re-keying
    the snap frames by script.
- To set the length, call `set_fixed_interpolation_or_key_if_need` on each layer, then
  `model.fit_animation_size_by_layers()`.
- Built-in commands: `app.get_action_manager().call_action("<id>")`, using ids from
  cascadeur.com/help/category/301 (format `Category.Action`).
- Never close a scene from script by answering "Don't Save" through PySide: Cascadeur exits.
  Leave test copies open and tell the owner to close them.

### AI interpolation

Cascadeur's AI in-betweening (`View.Inbetweening_RunMotionGeneration`; style is the per-scene
`RootMotionStyles` setting, 7 = Combat, set by hand) often makes a mess. Use it only as a
first proposal. Run the checks, and replace failing spans with keys from the smooth model.

## Cascadeur physics tools (verified)

AutoPhysics loses control (owner: it's for "ordinary mortals"). The main path is precise
control of keys, layers and tangents, with weight, overlap and settle placed by hand and
verified by the checks. Run AutoPhysics only as a side comparison on a **copy**
(`<move>_vN_autophys.casc`), after the key poses and timing are right, and report honestly
which is better. Always protect the contact frames with priority frames.

Result on Ganondorf v1 (fulcrum on the support foot, kicking foot free, secondary on hands,
priority frames on contact): **worse**. The forearm went 7 cm into the torso, the back load
dropped from 24 to 16 cm and the push into the kick vanished: physics balances the body by
removing exactly the exaggerated weight transfer the move needs.

- **Per-point and per-frame values** are written with
  `model.data_editor().set_data_value(data_id, frames, value)`:
  - Muscle stiffness (Compensation and Separation);
  - Secondary motion local/global blending, damping and air friction;
  - `fulcrum_state` (0 auto, 1 Enforce, 2 NotFulcrum);
  - priority frame, collision radius and keep-global-rotation/translation.
  The rig default for Secondary local_blending is 10 on most points. Before a snap with
  Secondary on, reset it to 100 everywhere except wrists and hands.
- **AutoPhysics**: `AutoPhysicsTool.Switch Auto Physics`. `AutoPhysicsTool.Set priority frame`
  works only while AutoPhysics is on.
- **Snap to AutoPhysics**: `AutoPhysicsTool.Snap to Auto Physics` opens a non-blocking QML
  window, "Apply AutoPhysics". To answer it:
  1. wrap each `QGuiApplication.topLevelWindows()` as `QQuickWindow` (shiboken6);
  2. walk `contentItem()` to the item with text "All Frames";
  3. call `QMetaObject.invokeMethod(item, "clicked")`.
  Every unanswered snap leaves a dialog open.
- The **global toggles** (Secondary, Compensation, Separation, Smooth rotation/trajectory) are
  per-scene settings with no setter. Switch them on in the UI with computer use (or ask the
  owner) once per scene; they stay saved in the `.casc`.
- `TweenMachine.Attract to inertial position.Selected frames` carries momentum on. It needs
  objects and an interval selected, with `set_full_selection_by_parts` called outside
  modify.
- `Timeline.Bezier viscous.Bezier viscous on selected interval` and
  `View.FixFoot_KeepKeyFrames` (foot sliding) also work.

## Calibrated thresholds (from UFD measurements, 2026-10-31)

The live values are in `motion_thresholds.json` and `legibility_thresholds.json`; these are
the values at calibration time.

- Impact spacing: >= 40% of the limb path in the last 2 frames before contact, speed peak
  on the frame into contact (Ryu fsmash: 78%).
- Hold: after 2 settle frames, >= 60% extended and moving < 6% of reach per frame.
  `hold_frames_min`: jab 1, tilt 3, smash 6. Recoil required on smashes (Ryu: 4 reversals,
  about 9% of reach).
- Overlap cascade: arm spread >= 1 frame, 2-joint lag >= 0.5 frame.
- Clearance between limbs and body: >= 2 cm.
- Arc jitter: 7-frame windows, radius >= 8 cm, radial error >= max(1 cm, 6% R), off-plane
  >= 1 cm, >= 2 angular-speed flips.
- Reference timings (startup / active / total): Mario jab 2/2-3/19; Terry ftilt 8/8-13/27
  (1-frame snap, 5 frozen hold frames); Ryu fsmash 15/15-17/45; Little Mac fsmash
  14/14-15/43 (settles at 72%, frozen 12 frames); Ganondorf ftilt 10/10-12/39 (6-frame build,
  90% of the path in the last 2, about 9 frozen hold frames at 69%, no recoil).

## Checks after every pass

1. `metrics.py` on the baked clip flags:
   - a flip: more than 90 deg/frame on a non-key frame;
   - a dip: the hand path reverses by more than 0.2 cm/frame;
   - a slide: the feet drift more than 0.5 cm without a planted contact.
   It also reports reach, pelvis height, head offset at contact, and the error against the
   reference curve.
2. `clearance.py` measures self-penetration with the physics-asset capsules. The target is
   at least 2 cm everywhere.
3. `visual_check.py` uses Unreal SceneCapture in `L_PilotCapture` to render:
   - the six views per key pose (front, right, back, left, top, bottom), plus 3/4 and the
     game cameras;
   - the arm-outside-torso %;
   - a lit 3/4 strip;
   - an mp4, at normal speed and slowed.
   Intermediate passes: 4 views, key and flagged frames only.
4. **Pose checks** from the owner's v7-v9 feedback. The v7 passed a binary "is the fist
   hidden?" test while the raised off arm half-covered it, so never use a binary test.
   - Measure **how much of the attacking fist stays visible** (%) from 3/4, rear and game
     views on every frame, and how much the off arm adds to the silhouette around it.
     Flag any frame where the off arm covers part of the fist.
   - The rear elbow stays inside the torso outline. Build the chest frame so it can't
     flip when the torso turns past 90 deg (a v8 false positive came from that).
   - The fists are closed in guard and at the FAF, measured by finger curl.
   - `joints.py`: elbow pole pointing down (arm raised at most about 50 deg outside the
     strike), no reverse bend, forearm/hand roll vs guard and roll jumps between frames,
     wrist flexion/deviation (about 20 deg), knees, ankles, feet; plus the frozen-hand check.
     Exceptions only as exaggerations (see "How the owner judges", item 7). Mesh vertices
     aren't readable from UE Python; the bones are the source of truth.
   - `arm_check.py`: elbow direction (from 40 deg of upper-arm elevation), flare, chicken
     wing (raised more than 50 deg while flexed), reverse bend, forearm/hand roll (120 deg),
     roll jumps (60 deg, or 100 right after contact). The corkscrew at contact is accepted.
   - Balance, weight shift, counterbalance, support knee, kinetic chain, twinning, gaze and
     feet from "Posing fundamentals".
   - A fix for one check (for example pulling a hand away from the head to clear a
     penetration) must be re-checked against all the others.
   - Optional second opinion: `animationsight` (MIT, BVH input, isolated venv
     `C:\Users\jacop\Tools\animationsight-venv`, owner-approved after code review): CoM vs
     support base, foot slide/penetration, single-joint pops. The owner wants all the
     AISight Claude Code plugins installed (animationsight, solidsight, texturesight, shadersight,
     pcbsight; skill-only, code reviewed). Run the CLI from the venv with `USERPROFILE`/`HOME`
     set to a scratch folder (its first run otherwise edits `~/.claude/CLAUDE.md`) and
     never with `--show` (opens a browser).
5. **Legibility and motion checks** (`legibility.py`, `motion_check.py`, `traj_overlay.py`).
   Render one large image per key pose per view: the real mesh beside the same pose with
   one flat colour per part (capsules), with the attacking fist outlined even when hidden.
   Flat black silhouettes hide overlaps, so never judge from them alone. Also run the
   impact-spacing, hold/recoil, overlap-cascade, arc-jitter, elbow-out and weapon-arc checks
   from "How the owner judges", and draw the bone paths.
6. **Look at the large images yourself** (high effort) before the numbers, and write down
   everything that looks off, on every limb and on balance, not only what the owner last
   flagged. Never explain a defect away ("the head covers it, unavoidable"). The review page
   (`legibility_page.py`) is a private Artifact so the thread can review it from the cloud.
   Then show the owner. Compare with the previous version side by side
   (`sheets/<move>_before_after.png`). For reference work, also compare with the reference
   (`sheets/<ref>_vs_vN.png`).

## Desktop access

- When the owner says he is using the PC: no Unreal, no mouse/keyboard, no Cascadeur relaunch
  until he says so; only file work (docs, scripts that don't need the editors).
- The owner allows computer use on the desktop: clicking dialogs, using Cascadeur's tools
  (FPS, global physics toggles, trajectory mode), relaunching Cascadeur and starting its
  script server, and Unreal editor preferences.
- Approval cards for a Remote Control session appear only in that session inside the Claude
  desktop app, and RC project sessions are hidden from the sidebar. The owner opens one with
  Win+R `claude://claude.ai/epitaxy/<local session id>`. Grants last about 30 minutes.
- Cascadeur MCP Plus (`C:\Users\jacop\Tools\cascadeur-mcp-plus`, MCP name `cascadeur-plus`)
  works in RC sessions (its tools load in a session started after it was registered). It
  adds scene, layers, keys, timeline, actions, screenshot and UI-input tools, interpolation
  type per interval, IK/FK, reset tangents and reset easing weights. It has no
  handle-direction or Trajectory Tangent control. Don't use `QQuickWindow.grabWindow`: it
  crashes Cascadeur.

## Unreal

- **Live Link** (plugin CascLiveLink 1.0.2, in `UE_5.8/Engine/Plugins/Marketplace/`):
  - It is one-way, Cascadeur -> Unreal, and writes into a chosen AnimSequence.
  - It streams only when a key frame changes. It takes about 1 s, versus about 30 s for FBX,
    and matches the FBX to 0.01 cm.
  - The asset arrives baked, with no tangents, and must be saved by hand or by script.
  - Tangents edited in Sequencer don't go back to Cascadeur by Live Link. If the final
    polish happens in Unreal (hybrid below), log it, and use FBX + unbaking to bring it back
    to Cascadeur when needed.
- **FBX fallback**:
  1. `FbxSceneLoader` with `csc.fbx.FbxSettings(bake_animation=True)`;
  2. `loader.export_joints(path)` (cm, Y up);
  3. `UE_MCP_Bridge` `import_animation` over `ws://127.0.0.1:55976` (`bridge_call.py`).
  The bridge never saves. UE 5.8's own MCP is on port 8000.
- **Hybrid (owner-approved):** poses, keys and layers in Cascadeur; final tangent/spacing
  polish in Unreal Sequencer + Control Rig (per-key tangents and weights are settable from
  Python, verified), then bake to the AnimSequence. Pick the simplest working path and log why.
- The skeleton is `/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin`.
- Hitboxes come from the Reference clip only. Skins' Visual clips never change hitboxes.
- UE 5.8's AnimationAssistantToolset can only edit (Sequencer, Control Rig, keys). It never
  reviews a clip, so the review is always ours.

## Log and handoff

- Append to `<move>_log.md` (Italian): per version and per joint, what changed, at which
  frames, **why** (which principle), the metrics before and after, and the time per step.
- The message to the owner covers what changed, the video, and any open choices. Keep it short.
- When a move is delivered, write `handoff.md` for the next session.

## Speed

Measured on Ganondorf: a Cascadeur -> Unreal -> checks loop takes about 16 s, the
legibility computation about 130 s (Python), six views of one pose about 5 s. The real cost is
reasoning turns (about 40 on Ganondorf v1, mostly on the failed sparse-key attempt): decide
the approach early and avoid long trial loops. Older estimate: a pass took 3-5 minutes,
mostly the Unreal render. "Use Less CPU when in Background" can't be set from Python and
renders are already fast, so leave it. Cascadeur edits take about 3 s, Live Link about 1 s,
and metrics about 10 s. Desktop RAM is tight, so close test scenes you no longer need (ask
the owner).