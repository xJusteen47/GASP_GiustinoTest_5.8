# Camera anchor: a jitter-free pivot for any camera

**For designers.** If you build your own camera (Blueprint camera, camera modifier, Sequencer rig, a new GSTN camera mode), do NOT follow
`GetActorLocation` of the character. The actor location is the centre of the movement capsule, and it jumps:

- when the capsule changes size (crouch, dair tuck, dropkick, knockdown, getting up): tens of cm in one frame while the body does not move;
- at every simulation step (the game logic runs at 62.5 Hz, the screen at 144 Hz): only the drawn mesh is smoothed between steps.

Use instead (Blueprint category **GSTN | Camera**, C++ `UGSTN_CameraAnchorLibrary`):

| Node | Returns | Use it for |
|---|---|---|
| **Get Camera Pivot** (Character) | Where the actor root would be with the standing capsule: horizontal from the drawn, smoothed mesh, height from the FLOOR under the character + the standing half height (since 2026-11-23; in the air the drawn body, never below that floor height), so no capsule change, on the ground or in the air, moves it. The height steps at the 62.5 Hz logic rate on slopes and jumps: damp it. Bool: false = no mesh found, actor location returned | Your own damping, aim, lock-on, framing |
| **Get Damped Camera Pivot** (Character) | The same pivot with the project camera's damping and Mario-style air hold (tuning in `DA_CameraAnchor_Default`). `Damped` false = the anchor is not running on that character, you get the undamped pivot | Matching the built-in camera's feel |
| `GSTN_CameraAnchor` component: **Get Damped Anchor / Get Standing Root** | Same values, read straight from the component | When you already hold the component |

**Capsule observer.** `GSTN_CameraAnchor` component › **On Capsule Changed** (Old Half Height, New Half Height, Bottom Shift): fires once per
rendered frame when the drawn capsule changes size (crouch, aerials, landing, knockdown). Not needed to ignore those changes (the pivot already
does); bind it only to react on purpose. Presentation event: never fired by rollback re-simulations.

**ABP warping is safe.** Stride/orientation warping and GASP's offset root move BONES inside the mesh; the pivot reads the mesh COMPONENT and
the capsule, never a bone, so they do not move it. A camera that follows a bone (pelvis, head) would see them.

**When to call it.** Inside the camera's own update: Player Camera Manager (`BlueprintUpdateCamera`), a camera modifier (`ModifyCamera`), a
camera component's view, or any tick in **TG_PostPhysics or earlier**. Never move a camera from a tick in **TG_PostUpdateWork**: Unreal
builds the view of the frame before that group, so the camera shows one frame late and shakes (the 2026-11-20 jitter bug).

Console: `gstn.Camera.Anchor.Vertical 3/1/2` (height from the floor = default / from the capsule bottom, 11-20 version / from the drawn mesh, first version), `gstn.Camera.Anchor 0/1` (anchor off/on), `gstn.Camera.Anchor.Early 1/0` (new/old update order, A/B only),
`gstn.Camera.Anchor.Debug 1` (red = pivot, green = damped pivot, yellow = raw actor location).
Code: `Source/GASP_Giustino2/{Public,Private}/Camera/GSTN_CameraAnchor*`.

**Why the floor (2026-11-23, dropkick K6).** The 11-20 source (capsule bottom on the ground, drawn mesh root in the air) still leaked the capsule
at the switch between the two: the dropkick loop's clip root sits ~55 cm under the body, so in the air the pivot followed that root down, and
at touchdown it jumped to the small torso capsule's bottom + 86 cm (+57 cm in one frame). A capsule lifted onto the torso while Mover still
reported ground moved it too. The floor (a line from the capsule centre down, on the capsule's own collision responses, `FloorProbeDistance`)
is touched by no capsule; in the air the body is followed upward only (`bFloorClampInAir`). The P1a camera also receives its pivot from the
anchor's own update now (`OnAnchorUpdated`), the same frame, and every camera of the pawn ticks after the anchor.
