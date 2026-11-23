# Unreal Assistant: crouch capsule change and mesh pop in stock GASP (2026-11-21, rule 14)

Question: In stock Game Animation Sample (GASP, Mover, UE 5.9): when the capsule shrinks for crouch, the actor moves down ~31 cm and the mesh base offset moves up by the same amount in the same frame. Does GASP's Offset Root Bone node (or foot placement / leg IK) react to a one-frame world move of the mesh component, and how does stock GASP keep the mesh from popping on a crouch capsule change (Mover FStanceModifier, mesh offset, smoothing)? Name exact nodes, settings and cvars.

I couldn't confirm these internals in the official documentation. The [Mover Features and Concepts](https://dev.epicgames.com/documentation/unreal-engine/mover-features-and-concepts-in-unreal-engine) page only says that crouch and stance changes go through movement modifiers. The [Game Animation Sample](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine) page doesn't describe the crouch pipeline. The details below come from my knowledge of the engine source. Check each name against your 5.9 headers before you rely on it. I mark the less certain items.

## 1. Does Offset Root Bone react to a one-frame mesh move?

**Yes, but only if the skeletal mesh component's world transform actually changes.**

`FAnimNode_OffsetRootBone` (AnimationWarping plugin, `BoneControllers/AnimNode_OffsetRootBone.h`) works like this:
- It keeps a simulated root transform in world space.
- Each update, it moves that root by the graph's root motion delta. In **Manual** **Evaluation Mode**, it uses **Translation Delta** / **Rotation Delta** instead.
- It compares that root with the component transform the anim proxy cached at `PreUpdate`.
- The difference becomes the root bone offset. **Translation Mode** decides what happens to it:

| Translation Mode | What happens to a sudden component move |
|---|---|
| `Interpolate` | The root stays in world, then decays back over **Translation Halflife** |
| `Accumulate` | The offset is kept, limited only by **Max Translation Error** |
| `Hold` | The current offset is frozen |
| `Release` | The offset blends out to 0 |

Related settings: **Max Translation Error**, **Clamp To Translation Velocity** / **Translation Speed Ratio**, and the matching Rotation settings.

How this applies to your crouch:
- **Actor -31 cm and mesh relative +31 cm in the same game-thread frame, before the anim proxy caches the transform:** the mesh component doesn't move in world, so Offset Root Bone does nothing.
- **The two changes land in different frames, or the proxy caches the transform between them:** the node sees a 31 cm jump and offsets the root to cancel it. With `Interpolate`, the root then slides back over the half-life. When the other half of the change arrives, the node offsets again. You get a smeared blip instead of a clean pop.
- I believe the node doesn't drop the vertical axis, but I'm not certain. Check it with the debug cvar below.

Cvars (in `AnimNode_OffsetRootBone.cpp`):
- `a.AnimNode.OffsetRootBone.Enable 0/1`
- `a.AnimNode.OffsetRootBone.Debug 1`

## 2. Foot Placement

`FAnimNode_FootPlacement` keeps planted feet locked in world space and puts the pelvis on a spring. If the component world transform changes, it reacts.

- **Pelvis Settings** (`FFootPlacementPelvisSettings`):
  - **Actor Movement Compensation Mode**: `ComponentSpace` / `WorldSpace` / `SuddenMotionOnly`. `SuddenMotionOnly` is meant for exactly this kind of teleport-like jump, such as step-ups.
  - **Linear Stiffness** / **Linear Damping**, **Max Offset**, **Max Offset Horizontal**, **Pelvis Height Mode**, **Enable Interpolation**.
- **Interpolation Settings**: **Floor Linear Stiffness** / **Floor Linear Damping**, **Enable Floor Interpolation**, **Unplant Linear Stiffness** / **Unplant Linear Damping**.
- Cvars (less certain, check `AnimNode_FootPlacement.cpp`): `a.AnimNode.FootPlacement.Enable`, `a.AnimNode.FootPlacement.Debug`.

If the mesh component's net world move is 0, Foot Placement sees nothing either. The crouch pose itself (pelvis drop) comes from the Motion Matching crouch databases, and GASP authors those relative to a root on the floor.

## 3. How stock GASP avoids the pop

GASP has no special anti-pop node for crouch. It relies on a **net-zero world move of the mesh component**:
- **CMC path:** `ACharacter::OnStartCrouch` / `OnEndCrouch` adjusts the mesh's relative location and `BaseTranslationOffset` by `HalfHeightAdjust` in the same call that moves the capsule.
- **Mover path:** `FStanceModifier` (`DefaultMovementSet/Modifiers/StanceModifier.h`, settings in `UStanceSettings`) does the equivalent in `AdjustCapsule` from `OnStart` / `OnEnd`:
  - It resizes the capsule and moves the updated component by the half-height delta.
  - It compensates the visual root through `UMoverComponent::SetBaseVisualComponentTransform`. This matches what you're seeing.
  - Stance changes are broadcast through `UCharacterMoverComponent::OnStanceChanged`. **Handle Stance Changes** (`bHandleStanceChanges`) controls whether the component does this itself.

Offset Root Bone and Foot Placement in the GASP AnimBlueprint are *not* there to absorb this. They're only robust to it.

## 4. Likely cause of the pop in your setup (fixed tick 62.5 Hz)

This is my reasoning about your setup, not something the docs cover. With NPP fixed tick, the visual component position is interpolated between sim frames. Mover writes it in `FinalizeSmoothingFrame` when **Smoothing Mode** on the Mover component is `VisualComponentOffset`.

- The sync-state location drops 31 cm, and that drop is interpolated over about one sim frame (16 ms).
- If the +31 cm base visual offset is applied immediately and not interpolated, the mesh briefly goes up by up to 31 cm, then settles. That's a one-sim-frame pop.
- Anything that moves the actor on the game thread outside the sim can make the mismatch worse, because it bypasses the smoothed state.

## 5. How to isolate it

1. In **Rewind Debugger**, record the actor Z, the mesh component world Z, and the root bone component-space Z around the crouch frame.
   - If the mesh component world Z spikes, the problem is movement or smoothing.
   - If only the root bone or pelvis spikes, the problem is in the anim graph.
2. `a.AnimNode.OffsetRootBone.Enable 0`, then `a.AnimNode.FootPlacement.Enable 0`. If the pop stays, it isn't these nodes.
3. Set **Smoothing Mode** to `None` as a test. If the pop disappears or changes shape, the smoothing and offset ordering is the cause.
4. Fix options:
   - Apply the half-height compensation as a decaying offset: on `OnStanceChanged`, store `-HalfHeightDelta`, spring it to 0, and apply it either to the base visual transform or in the AnimGraph with **Transform (Modify) Bone** on `root` (**Translation Mode** `Add to Existing`, **Component Space**).
   - For Foot Placement, use `SuddenMotionOnly` with a softer pelvis **Linear Stiffness** so any leftover jump is absorbed.

If you paste your `StanceModifier.cpp` `AdjustCapsule` body and your Mover component's smoothing settings, I can check the exact ordering for 5.9.
