# Unreal Assistant: stance width in stock GASP (2026-11-21, rule 14)

Question: In stock Game Animation Sample (GASP, Mover variant, UE 5.9): how does it control the lateral stance width of the feet (distance between left and right foot, feet crossing the midline while running)? Is there any stance-width setting, curve, or node (Foot Placement settings, Leg IK, Orientation Warping, Stride Warping, Control Rig) that widens or narrows the feet at runtime without re-authoring the clips? If not, what would be the stock-friendly way: a curve-driven additive, a Control Rig leg offset after Foot Placement, or something else? Name exact nodes and settings.

**Short answer:** stock GASP has no stance-width parameter. No setting, curve or node in the documented setup widens or narrows the feet sideways. Below is what each part does, the approach I'd use, and the exact nodes.

## What stock GASP does with the feet

The documented post-process chain in `ABP_SandboxCharacter` is:
- **Orientation Warping** and steering, inside the Motion Matching node's Blend Stack graph.
- **Simple Additive Lean**, **Simple Aim Offset**, the **Default Slot**.
- **Offset Root Bone**, then **Simple Leg IK Process**.

| Node | Lateral foot control? | Why |
|---|---|---|
| **Orientation Warping** | No | It rotates the IK feet around `ik_foot_root` to match the movement angle, and counter-twists the spine. The distance between the feet is kept, only rotated. |
| **Stride Warping** | Not as a width setting | It scales foot offsets along **Stride Direction** by a stride scale (speed ratio, or **Manual Stride Scale**). There is no separate lateral term. GASP's docs don't describe it in the locomotion chain. |
| **Leg IK** (Simple Leg IK Process) | No | It pins FK feet to `ik_foot_l` / `ik_foot_r`. Width is whatever those IK bones carry. |
| **Offset Root Bone** | No | It moves and rotates the root only. |
| **Foot Placement** | No | It isn't in the documented GASP AnimGraph. If you added it, its plant, pelvis, trace and interpolation settings handle vertical planting and locking, not width. |
| **Control Rig** | Not in the stock graph | You'd write it yourself. |

So in stock GASP, stance width comes 100% from the authored clips plus whatever Motion Matching picks.

**Key detail:** because Leg IK runs late and snaps the FK legs to the IK foot bones, any width change has to move `ik_foot_l` / `ik_foot_r` *before* Leg IK. Otherwise Leg IK overwrites it.

## Recommended: Control Rig that edits the IK foot bones, placed before Leg IK

### AnimGraph order

`... → Offset Root Bone → Control Rig (CR_StanceWidth) → Simple Leg IK → Output Pose`

Check the exact order in your copy. The point is that the Control Rig node goes after anything that warps IK feet and before Leg IK.

### Control Rig, Forward Solve

Do this for each side:
1. **Get Transform** on `root` and on `ik_foot_l` / `ik_foot_r`, both in Global space.
2. **Make Relative** (foot relative to root) to get the foot in root space.
3. Take the lateral component. On the UE mannequin skeleton the character faces +Y in component space, so lateral is X. Check the sign by reading the `ik_foot_l` translation in the skeleton.
4. Enforce a minimum half-width: `Max(lateral_l, MinHalfWidth)` for the left foot, mirrored for the right. Optionally add a constant `WidthOffset`.
5. Smooth the correction with **Spring Interpolate** (or **Accumulate Lerp**) so it doesn't pop.
6. **Set Transform** on `ik_foot_l` / `ik_foot_r` (Global), with **Weight** set to `StanceAlpha`.

**Why measure against the root and not the pelvis:** the pelvis sways side to side during a run. A min-width clamp measured from the pelvis would move a planted foot and cause sliding. Measured from the root, a planted foot keeps a steady lateral offset while moving straight.

**When moving diagonally or strafing:** after Orientation Warping, the "midline" is the movement direction, not the root's forward axis. For best results, pass the movement direction in component space as a vector variable and measure lateral distance perpendicular to it.

### Control Rig node settings in the AnimBP

- Make `MinHalfWidth`, `WidthOffset` and `StanceAlpha` instance-editable in the rig.
- On the node, turn on **Use Pin** for each, or set **Use Curve** if a curve should drive one.
- Use **Alpha Input Type** to fade the whole node in and out.
- Drive alpha to 0 while a montage plays in the **Default Slot** (traversal), and when in air.
- Keep the offsets small, a few cm. Leg IK will stretch to reach if you push past what the legs can reach.

## Lighter alternative: no Control Rig

If you only need a constant widen or narrow, not a fix for midline crossing, use two **Transform (Modify) Bone** nodes in component space before Leg IK:
- **Bone to Modify**: `ik_foot_l`, then `ik_foot_r`.
- **Translation Mode**: `Add to Existing`.
- **Translation Space**: `Component Space`, with ±half-width on the lateral axis.
- **Alpha**: from a variable or curve.

This doesn't fix midline crossing because the offset is the same every frame. It also uses the component axis, so it gets less accurate once Offset Root Bone has rotated the root away from the capsule.

## Why not a curve-driven additive pose

- An additive that spreads the thighs only changes FK bones. Leg IK then snaps the feet back to the IK bones, unless the additive also keys the IK bones or you apply it after Leg IK.
- After Leg IK, rotating the thighs moves planted feet in world space, which causes sliding.
- A curve is still useful as the *alpha source*, through **Use Curve** or **Modify Curve**. It's a poor way to carry the offset itself.

## Experimental: Stride Warping on the lateral axis

In principle, a second **Stride Warping** node set to **Evaluation Mode** `Manual`, with **Stride Direction** on the lateral axis and **Manual Stride Scale** slightly above or below 1, scales foot offsets sideways. In my understanding the projection is per leg, relative to the hip and not the midline, so its effect on crossover isn't intuitive. Its pelvis adjustment also kicks in. Try it with pose watches only if you want to avoid a Control Rig.

## Notes for your setup

- **Mover / Network Prediction at 62.5 Hz:** this is cosmetic animation work. It runs at render rate on each machine and doesn't touch the Mover sync state. Feed any speed or direction inputs from the same smoothed or interpolated values your ABP already uses, so the width doesn't jitter at the fixed-tick rate.
- **GAS / UAF:** no impact. This is all in the classic AnimBP.
- **Debugging:** use **Pose Watch** on the nodes before and after Leg IK, plus `ShowDebug Animation`. The Animation Warping plugin has enable and debug cvars for its warping nodes, such as `a.AnimNode.OrientationWarping.Enable` and `a.AnimNode.StrideWarping.Enable`. These cvar names are from memory, not from the docs, so confirm them with console autocomplete in your build.

Docs: [Game Animation Sample](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine), [Pose Warping](https://dev.epicgames.com/documentation/unreal-engine/pose-warping-in-unreal-engine), [Control Rig in Animation Blueprints](https://dev.epicgames.com/documentation/unreal-engine/control-rig-in-animation-blueprints-in-unreal-engine), [Transform Bone](https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-transform-bone-in-unreal-engine), [Modify Curve](https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-modify-curve-in-unreal-engine).

If your `ABP_SandboxCharacter` has been changed (for example, an added Foot Placement node), paste the AnimGraph node order and I'll pin down exactly where the stance node goes.
