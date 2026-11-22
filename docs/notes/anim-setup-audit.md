# Locomotion animation setup: how it works, what is clip-specific, refactor plan (anim-setup-1, 2026-11-19)

Owner principles (scope: the whole animation setup): a designer must understand it and swap in another animation set; no per-clip
hacks; if a fix needs editing 40 clips, it belongs in code/systems.

## 1. How GASP is meant to work (the part a designer should keep)
- **Motion matching** picks the pose for ordinary movement (idle, walk, run, starts, stops, pivots) from **Pose Search databases**
  (PSD_*), using the character's trajectory. The clips carry the data it needs: curves (contact_l/r, movedata_speed) and notify
  states (PoseSearchBlockTransition, BranchIn). Add a clip = add it to a database; no code.
- **Choosers** (CHT_*) decide *which* database or clip set fits the situation (stance, gait, state) from columns a designer edits.
- The **Blend Stack** plays what was picked and blends between picks; **inertialization** hides the joins.
- **Warping** (Orientation/Stride warping, Offset Root Bone) and the **leg IK** (Procedural_Feet) bend the chosen pose to the real
  capsule speed, direction and floor: one clip serves many speeds and angles.
So in GASP the clip is data, the systems adapt it. That is what makes another animation set "just data".

## 2. What we added on top (as of claude/locomotion-finish-4)
| Piece | Where | What it does |
|---|---|---|
| Own states | ABP_GSTN_Locomotion state machine: Skid, Skid Exit Run, Run Stop, Run Stop Blend, Run Start, Dash, Crouch Enter/Idle/Exit | Each state, on entry, calls `SetBlendStackFromStateName`; the clip is the row of **CHT_GSTN_OwnStates** with that state name. States hold no pose themselves |
| Run presentation | `UGSTN_RunPresentationComponent` + `DA_AnimSet_Locomotion_Default` (RunStop, RunStart, Skid sections, lean) | Reads the sim (skid phase, run stage, stick) and exposes getters the ABP transitions use (IsAnimOwnerSkidding, ...RunStopClipTime...) |
| Run skid (sim) | `FGSTN_LayeredMove_RunSkid`, tuning `FGSTN_RunSkidTuning` in the locomotion profile | Brake along the old direction, then turn and push off |
| Override layers | `GSTNAnimation` (override-layer seam, anim sets) | Feature layers (skid layer, wall, crouch) that replace the main pose |
| Post-process nodes | BlendOutFootLock, GroundContactFix, StanceOffset, CoveredLanding, DirectionalHitAdditive | Generic fixes after the locomotion pose |

## 3. Audit: what is clip-specific or opaque
Rating: **G** generic (works for any set), **C** tied to one clip, **O** works but a designer cannot see why.
1. **C - Run stop timing measured from one clip.** The stop clip is speed-matched to the sim brake from its root track
   (BrakeStart/StopIndex) and then "settled" with SettleRate/SettleClipSeconds tuned for V1-V6. A new clip needs new numbers.
2. **C - Run stop end stance had to match one idle.** The game blends to whatever idle the chooser picks; the clip ended on
   another idle's stance -> 4 cm drift (runstop-v6/rest-idle-cause.md). Rounds of re-posing were spent on this. **Fixed generically
   here (5.1).**
3. **C/O - Skid turn side came from float noise.** An exact reverse has no shorter side, so the body turned either way while the
   clip turns one fixed way. **Fixed generically here (5.2).**
4. **C - Skid exit/hand-back constants.** Skid state exit at 0.42 s state time, Skid Exit Run entering the run loop at t=2.117
   (phase-matched by hand), hold 0.75 s, skid layer MaxTailSeconds, the 3-section skid clip. All are numbers read off one clip.
5. **O - Two ways to play an own clip.** Some features go through ABP states + CHT_GSTN_OwnStates, others through override
   layers. A designer cannot tell which path a clip takes without reading C++.
6. **O - States that output nothing.** The State Controller machine has no pose; the visible pose is the Blend Stack written on
   state entry. Anything put inside a state is silently never seen (abp-state-routing memory).
7. **C - Our clips carry no GASP data.** Pose-built clips (run stop, crouch) have no contact curves, movedata_speed, Foley
   notifies or PoseSearchBlockTransition, so GASP's systems (dynamic play rate, foot placement, MM transitions) cannot use them.
8. **O - Getter-driven transitions with magic windows.** Run Start alias "from Locomotion Transition/Loop only while t < 0.25 s",
   one-frame-late getters, re-entry guards. Correct, but only explainable by the history.
9. **G - Things that are already right:** profile/anim-set data assets, the override-layer seam, the post-process nodes (foot lock,
   ground contact), the run skid sim tuning, surface friction/ledge rules.

## 4. Refactor plan (order = value per cost)
1. **Hand-offs are a system, not a clip property** (done for run stop): any clip that owns the pose and ends at rest declares
   itself a pose driver; the feet hold through the blend and step to the next pose. Next users: skid release (ends at rest),
   landings (air crouch landing rise), dash end.
2. **Turn handling from data** (done for skid side): the clip's turn side lives in the tuning. Next: bake it from the clip's root
   yaw with the existing RootTrack bake button so the designer never types it.
3. **Hand-backs to motion matching by GASP's own rules:** replace the hand-tuned exit times (item 4) with "leave the state when
   the clip's PoseSearchBlockTransition window opens / the clip ends, then let MM pick with inertialization". Needs item 4.
4. **Every clip we write gets GASP's data automatically:** extend curves_author.py into a post-step of every clip builder: contact
   curves from foot height/speed, movedata_speed from the root, a PoseSearchBlockTransition state on the brake/turn part.
   Then our clips can live in Pose Search databases like Epic's (the long-term goal: fewer own states).
5. **One documented path for own clips:** ABP state + one CHT_GSTN_OwnStates row (name = state name) is the path for gameplay
   clips; override layers only for whole-body features (wall, crouch layer). A table in anim-layers.md says which feature uses which.
6. **Name the magic numbers:** every remaining window/threshold becomes a named field in DA_AnimSet_Locomotion_Default with a
   tooltip ("why"), not a literal in a transition rule.
7. **Speed-match from data, not from one clip:** the run stop already measures its clip's root at load; make the settle generic
   (end when the clip's root stops + the hand-off of item 1), drop SettleClipSeconds.

## 5. Applied in this thread (claude/anim-setup-1)
### 5.1 Run stop: feet held through the hand-off (any stop clip, any idle)
- `IGSTN_PoseDriverSource` (GSTNAnimationCore): a component says "my clip owns the pose" + its hand-off seconds.
  `UGSTN_CharacterAnimInstance` treats it like a montage: the blend-out foot lock holds the feet, waits for the hand-off weight to
  reach 0, then steps each foot to the next pose. `UGSTN_RunPresentationComponent` implements it while the stop clip plays
  (`RunStop.bHoldFeetThroughHandOff`, `RunStop.HandOffSeconds` = 0.75 s in the anim set).
- Result (Saved/AnimSetup1/cap1/step.txt, from frames.jsonl): feet constant v255-299 during the blend (before: 4.6 cm drift over
  45 frames), right foot steps v300-308 (lift 2.6-4 cm), left foot 3.4 cm blends in 4 frames.
### 5.2 Skid: turn side from data
- `FGSTN_RunSkidTuning::TurnSide` (Right = +yaw, the side of SS_180_Turn_Root180) and `ReverseConeDeg` (45): a new direction
  within 45 deg of straight behind turns on that side, latched on the skid's first frame; other turns take the shorter way.
  Test cvar `gstn.Skid.TurnSide` (-1 data, 0 shortest, 1 right, 2 left).
- Result: both runs now turn +yaw identically (release v133-139, hold v131-138, 22.5 deg/frame); before, hold turned -yaw
  (Saved/Skid/cap6 vs Saved/AnimSetup1/cap1, yaw column of frames.jsonl).
- Seen, not fixed: after reaching 180 the actor overshoots ~17 deg (to ~-163) and comes back by ~v150, in both old and new runs.
