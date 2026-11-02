# Move cards, one folder per kit

A **kit** is a moveset, the proxy of a character (in game any character can use any kit). Cards live in
`cards/<kit>/<kit>_<move>_v<N>.json` and the card's `move` field carries the same name, so every output
(`sheets/`, `Saved/AnimPipeline/animpipe/`, in-game previews) is prefixed by the kit.

| Kit | Character idea | Moves |
|---|---|---|
| `overlord` | "Elegant violence": composed boss, controlled power, arms crossed or behind the back (M. Bison, Rugal) | `ftilt` (from Ganondorf ftilt v3.1), `dair` (Ganondorf dair timing + Bison head stomp) |
| `pilot` | pipeline test moves | `jab` (Mario jab 1) |

The kit name is provisional (owner may rename). Unreal assets built before the kits keep their paths
(`/Game/AnimPipeline/Rig/AS_GanonFTiltV31_Build`); new ones keep the build folder `/Game/AnimPipeline/Rig/` with the kit in the name (`AS_OverlordDAirV1_Build`).

## Owner rules for authored moves (2026-11-02, from the overlord dair reviews)
Every card and every review pass applies these; the checks that enforce one are named.
- **Timing comes from the owner** with each reference (key frames, hitbox frames via `anim_active`); never derive it from a GIF.
- **"Too much is just right"** (Sakurai): push key poses to the extreme, never soften the owner's poses.
- **Frame 0 breaks clearly from the air/ground idle** but is not yet the charge; the charge is an extreme of an already loaded pose.
- **Body synergy**: arms, trunk and legs travel together on every segment; no limb glued while the body moves (`co_motion`).
- **Charge**: knees stay in front, tucked toward the chest, never swinging back behind the pelvis; a long charge stays alive.
- **Slow release** over a few frames, the largest step still ON the first hitbox frame; nothing (head included) snaps
  after an active frame (`hitstop_snap`, head turn counted). Hold the strike pose long.
- **Villain hand** (overlord): open, fingers straight and together (`fingers`), bent outward at the wrist off the arm
  line (`wrist_out`); hands in the owner's Cascadeur files are default, never copy them.
- **No interpenetration** accepted (`clearance`): fix forearms/head/crossed arms rather than accept them.
- **Feet**: side by side unless the pose says otherwise; check the back view for crossed ankles and frog knees.
