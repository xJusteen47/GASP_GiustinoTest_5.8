# Limb contact IK (hands and feet on the wall)

Status (2026-10-26, branch `claude/project-thread-69tz7u`, on top of `claude/project-thread-v1plhd` with the wall layer): C++ and the asset
script written, **not compiled or run yet** (section 5).

Owner, 2026-10-25: during climbing and wall slide, the limbs that the animation keeps next to the wall must **attach to the wall in front**
with a Control Rig, instead of floating in front of it or sinking into it.

## 1. Rules

1. **Visual only.** It changes the evaluated pose and nothing else. Mover, abilities, timelines, hitboxes, hurtboxes and baked sockets never read
   the live pose (they use reference montages and `BakedSocket` frames, anim-layers.md section 5), so gameplay is the same with it on or off.
2. **The clip decides which limbs touch.** A limb is pulled onto the wall only if its animated effector is already near it (`ReleaseDistance`),
   reaches towards it (`bRequireReach`) and the target is within reach (`MaxReachRatio`). A hand that the clip raises away from the wall stays where it is.
3. **Only motion along the wall normal is added.** The clip still decides where on the wall the hand or the foot lands; the IK only closes the gap
   (or pushes a limb out of the wall), and turns the palm or sole towards it by `RotationAlpha`.
4. **Decoupled from the main graph.** It lives inside the wall layer (`ABP_GSTN_WallLayer`), so it comes and goes with the layer, and a skin
   variant of the layer inherits it. The main ABP is not touched.

## 2. Pieces

| Piece | Kind | Where |
|---|---|---|
| `FGSTN_LimbSurface`, `FGSTN_LimbContactSettings`, `FGSTN_LimbContactResult`, `FGSTN_LimbProbeSettings` | data (generic, any surface) | plugin `GSTNAnimation`, module `GSTNAnimationCore`, `GSTN_LimbContactTypes.h` |
| `UGSTN_LimbContactUtils` | function library: `ComputeContact`, `StepContactAlpha`, `AlignAxisToSurface`, `InverseTransformSurface`, `ProbeSurface` | same, tests `GSTNAnimation.Core.LimbContact.*` |
| `FGSTN_RigUnit_LimbContactIK` | Control Rig node "GSTN Limb Contact IK" (two-bone IK onto a surface, one per limb) | plugin `GSTNAnimation`, new module `GSTNAnimationRig` (depends on ControlRig, RigVM, AnimationCore) |
| `FGSTN_WallLimbContactSettings`, `FGSTN_WallLimbContactState` | wall data: probe bones, per-mode weights; per-limb surfaces + weights for the rig | project, `WallMovement/Animation/GSTN_WallAnimTypes.h` |
| `UGSTN_WallAnimUtils::ComputeLimbWeights`, `GatherLimbContact` | wall side: weights from the wall phase and mode, probes | project, test `GSTN.Wall.Anim.LimbContactWeights` |
| `UGSTN_WallAnimInstance::LimbContactSettings`, `WallLimbContact` | what the layer exposes to its Control Rig node | project |
| `CR_GSTN_WallLimbContact` | Control Rig asset (4 nodes, variable `WallLimbContact`) | `Content/Blueprints/Animation/`, made by `Tools/GAS/setup_wall_limb_contact.py` |

The generic part (rule, node, probe) knows no wall: a ledge hang or a ladder can reuse it with its own surfaces.

## 3. Data flow, one frame

```
Game thread, UGSTN_WallAnimInstance::NativeUpdateAnimation (wall layer)
  WallAnim = StepWallAnimState(...)                               (wall-anim.md)
  HandWeight, FootWeight = ComputeLimbWeights(WallAnim, Settings)  slide or climb weights in Enter/Active, 0 otherwise (Exit optional)
  if a weight > 0 and the wall sync state has a wall:
      plane = (WallContactPoint, WallNormal)                       the movement's filtered wall
      for each limb: origin = its chain root bone (upperarm_l/r, thigh_l/r) in last frame's pose
                     surface = sphere sweep origin -> -WallNormal (ProbeDistance 120, radius 4)
                               hit with a normal within MaxNormalAngle (40 deg) of the plane's, else the plane
                     -> component space
  WallLimbContact = { HandLeft, HandRight, FootLeft, FootRight, HandWeight, FootWeight }

Worker thread, ABP_GSTN_WallLayer anim graph
  wall state machine -> Control Rig node (CR_GSTN_WallLimbContact, input mapping WallLimbContact -> WallLimbContact) -> Output Pose

CR_GSTN_WallLimbContact, Forwards Solve
  GSTN Limb Contact IK (upperarm_l, lowerarm_l, hand_l, HandLeft,  Weight = HandWeight, Hand settings)
  GSTN Limb Contact IK (upperarm_r, lowerarm_r, hand_r, HandRight, Weight = HandWeight, Hand settings)
  GSTN Limb Contact IK (thigh_l,    calf_l,     foot_l, FootLeft,  Weight = FootWeight, Foot settings)
  GSTN Limb Contact IK (thigh_r,    calf_r,     foot_r, FootRight, Weight = FootWeight, Foot settings)
```

Inside each node:

```
Gap    = dot(effector - Surface.Point, Surface.Normal) - ContactOffset
Target = effector - Surface.Normal * Gap
TargetAlpha = 1 if Gap <= AttachDistance, smooth fade to 0 at ReleaseDistance; 0 if not reaching or out of reach
Alpha  = StepContactAlpha(Alpha, TargetAlpha * Weight)            per-limb smoothing (BlendInSpeed 12/s, BlendOutSpeed 8/s), kept in the node
goal   = lerp(effector, Target, Alpha)
two-bone IK of root/joint to goal, pole = animated joint (the clip keeps the elbow / knee direction), no stretch
effector rotation: ContactAxis turned towards -Normal by RotationAlpha * Alpha
```

Why the probe is on the game thread: world queries are safe there, and the probe origin (shoulder, hip) is not moved by this IK, so last frame's
pose is the animated one. The rig itself does no world query.

Why per-limb probes and not only the movement's plane: the plane is one point at the capsule, so on a curved wall, a corner or a bump the
hands would touch an imaginary flat wall. The probe follows the real surface; the plane is the fallback when the probe misses or hits
something else (a ledge top, a pipe).

Networking: simulated proxies run the same code on their smoothed presentation state. Nothing is replicated.

## 4. Tuning (defaults are starting points)

| Value | Default | Notes |
|---|---|---|
| `ContactOffset` | 3 (hand), suggest 6-8 (foot) | half the thickness at the bone; the foot bone is at the ankle |
| `AttachDistance` / `ReleaseDistance` | 8 / 25 cm | raise `ReleaseDistance` if the clips keep the hands far from the wall |
| `bRequireReach`, `MaxReachRatio` | on, 1.0 | |
| `BlendInSpeed` / `BlendOutSpeed` | 12 / 8 per s | |
| `RotationAlpha` | 0.6 | 0 for feet on the slide if the sole looks wrong |
| `ContactAxis` | (0, 1, 0) | **check on the mannequin**: hand palm and foot sole axes in bone space. Show the bone axes in the skeleton editor; the axis that points out of the palm (or the sole) goes here |
| `SlideHandWeight`, `SlideFootWeight`, `ClimbHandWeight`, `ClimbFootWeight` | 1 | on the layer's class defaults |
| `bKeepDuringExit` | off | on if the exit clip pushes off the wall with the hands |
| Probe: channel / distance / radius / max angle | Visibility / 120 / 4 / 40 deg | the walls must block the channel |

## 5. Editor steps (scripted: `Tools/GAS/setup_wall_limb_contact.py`)

The wall layer (`ABP_GSTN_WallLayer`) is made by `setup_wall_anim_layer.py`; this script runs after it.

1. Build (new modules `GSTNAnimationRig` and the authoring helper `FindOrInsertPoseNodeBeforeOutput` in `GSTNAnimationEditor`; the plugin
   now depends on ControlRig, enabled by default in the engine). Run `GSTNAnimation.Core.LimbContact` and `GSTN.Wall.Anim` tests.
2. Run the script. It makes `Content/Blueprints/Animation/CR_GSTN_WallLimbContact` (bones of the character's skeleton) with one public
   variable `WallLimbContact` (`GSTN_WallLimbContactState`) and the four `GSTN Limb Contact IK` nodes of section 3 in the Forwards Solve, their
   `Surface` and `Weight` pins linked to the variable's members.
3. It puts a `Control Rig` node with that rig at the end of the `WallLayer` graph (state machine -> Control Rig -> Output Pose), with the input
   mapping `WallLimbContact` -> `WallLimbContact`: the node copies the layer instance's property into the rig variable each update. One struct
   instead of six pins keeps the layer graph to one node and the binding by name.
4. Check in PIE: slide and climb on a flat wall, a curved wall and a corner; toggle `LimbContactSettings.bEnabled` on the layer's class defaults
   to compare; the nodes' `Alpha` and `Target` outputs can be watched in the rig editor's debug view.
5. Find the right `ContactAxis` for hand and foot (section 4) and set it in the script's `HAND` / `FOOT`, then run it again.

The node is inside the wall layer, not in the main ABP between its Inertialization and Pose History nodes: the data lives on the layer
instance, the main graph keeps knowing nothing about walls, and a variant layer (child anim Blueprint) inherits the node. The layer output
already carries the IK, so the Pose History records the pose that was shown.

## 6. Possible next steps (not done)

- **Planting** while climbing: keep a hand fixed in world space while the clip says it holds (a curve `GSTN.LimbContact.Plant.<Limb>`), like foot
  locking, so hands do not slide along the wall when the body moves. Today the contact follows the clip along the wall.
- Per-limb curves from the clips as a weight multiplier, when the final clips exist.
- Spine / pelvis offset so the chest keeps a distance from the wall on very tight surfaces.
