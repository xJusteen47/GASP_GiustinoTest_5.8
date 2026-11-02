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
