# Camera anchor: a jitter-free pivot for any camera

**For designers.** If you build your own camera (Blueprint camera, camera modifier, Sequencer rig, a new GSTN camera mode), do NOT follow
`GetActorLocation` of the character. The actor location is the centre of the movement capsule, and it jumps:

- when the capsule changes size (crouch, dair tuck, dropkick, knockdown, getting up): tens of cm in one frame while the body does not move;
- at every simulation step (the game logic runs at 62.5 Hz, the screen at 144 Hz): only the drawn mesh is smoothed between steps.

Use instead (Blueprint category **GSTN | Camera**, C++ `UGSTN_CameraAnchorLibrary`):

| Node | Returns | Use it for |
|---|---|---|
| **Get Camera Pivot** (Character) | Where the actor root would be with the standing capsule: horizontal from the drawn, smoothed mesh, height from the capsule's bottom + the standing half height, so no capsule change on the ground (crouch, landing, knockdown) moves it. The height steps at the 62.5 Hz logic rate on slopes and jumps: damp it. Bool: false = no mesh found, actor location returned | Your own damping, aim, lock-on, framing |
| **Get Damped Camera Pivot** (Character) | The same pivot with the project camera's damping and Mario-style air hold (tuning in `DA_CameraAnchor_Default`). `Damped` false = the anchor is not running on that character, you get the undamped pivot | Matching the built-in camera's feel |
| `GSTN_CameraAnchor` component: **Get Damped Anchor / Get Standing Root** | Same values, read straight from the component | When you already hold the component |

**When to call it.** Inside the camera's own update: Player Camera Manager (`BlueprintUpdateCamera`), a camera modifier (`ModifyCamera`), a
camera component's view, or any tick in **TG_PostPhysics or earlier**. Never move a camera from a tick in **TG_PostUpdateWork**: Unreal
builds the view of the frame before that group, so the camera shows one frame late and shakes (the 2026-11-20 jitter bug).

Console: `gstn.Camera.Anchor.Vertical 1/2` (height from capsule bottom = default / from the drawn mesh, first version), `gstn.Camera.Anchor 0/1` (anchor off/on), `gstn.Camera.Anchor.Early 1/0` (new/old update order, A/B only),
`gstn.Camera.Anchor.Debug 1` (red = pivot, green = damped pivot, yellow = raw actor location).
Code: `Source/GASP_Giustino2/{Public,Private}/Camera/GSTN_CameraAnchor*`.
