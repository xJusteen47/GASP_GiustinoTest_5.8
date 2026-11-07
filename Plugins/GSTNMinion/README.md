# GSTNMinion

Minions for the GSTN combat: 40+ awake enemies on one host, simulated as data in a store instead of one actor each
([combat-m8-minions.md](../../docs/architecture/combat-m8-minions.md), [plan](../../docs/architecture/plans/m8-minion.md)).

Status: **experimental (0.1)**, `EnabledByDefault: false`. Built by M8 checkpoint **G2** (skeleton and pure Core). Enabling it
changes nothing until a level spawns minions; nothing in the other plugins knows it (`Tools/Modules/check_modules.py` enforces it).

## Modules

| Module | Type | State | What it is |
|---|---|---|---|
| `GSTNMinionCore` | Runtime | **G2** | Pure: no world, no actors, no GAS, no Network Prediction. Tags, store and handles, intent, LOD logic, phase scheduler, attack tokens, net item codec, definition asset. |
| `GSTNMinionRuntime` | Runtime | empty shell | Batch subsystem, actors, presentation, spawner, brain hook (G3 and later). |
| `GSTNMinionEditor` | Editor | empty shell | Definition and registry validators, wave details (G12). |
| `GSTNMinionTests` | Developer | empty shell | Tests that need a world and harness helpers (`GSTNMinion.Runtime.*`, G3 and later). |

## What G2 contains (`GSTNMinionCore`)

- `GSTN_MinionTags.h`: native tags `Combat.Entity.Minion`, `Combat.Event.MinionSpawned/Despawned/Promoted`, `Minion.State.*` (the 12
  action states), `Minion.LOD.*` (the 4 tiers), `Minion.Type` (root only); `GetDeclaredTags()`. Defined in a Runtime module so a cooked game has them.
- `Store/GSTN_MinionStore.h`: `FGSTN_MinionStore` (struct of arrays, lowest-free-index allocation, 16-bit generation per slot,
  capacity from `gstn.Minion.Max`, default 128, clamped to the index range), `FGSTN_MinionHandle`, conversions to the pipeline id
  (`GSTNEntityKinds::MakeMinionEntityId`) and to `FGSTN_EntityRef{Minion, Index, Generation & 0xFF}`, `EGSTN_MinionActionState` (the 4-bit wire enum).
- `Intent/GSTN_MinionIntent.h`: what a brain asks (move, face, command, target); stale after 30 frames.
- `LOD/GSTN_MinionLOD.h`: `EGSTN_MinionLODTier`, `FGSTN_MinionLODParams` (borders, hysteresis 10 %, dwell 30 frames, intervals),
  `FGSTN_MinionLODMath` (pure tier decision), `UGSTN_MinionLODSettings` data asset.
- `Schedule/GSTN_MinionSchedule.h`: a slot of interval P runs when `(Frame + Index) % P == 0`.
- `Tokens/GSTN_MinionAttackTokens.h`: per-target cap (default 4), request, `Resolve()` grants by ascending slot index, release.
- `Net/GSTN_MinionNetCodec.h`: the 8.2 item layout (position 3 x 19 bits at 1 cm, yaw 8 bits, state 4, move id 6, move start 10,
  frame stamp 8, hit counter 3, health 5, speed 6, flags 3, optional type, global id and extension byte), `Serialize` to a bit
  archive, frame-stamp reconstruction for 8 and 10 bits. Quantize helpers live here until a second plugin needs them in `GSTNCore`.
- `Data/GSTN_MinionDefinition.h`: `UGSTN_MinionDefinition` (primary asset type `GSTN_MinionDefinition`) and its `IsDataValid`.

## Gameplay tags

Native: see `GSTN_MinionTags.h`. Designer vocabulary (ini only) is in `Config/Tags/GSTNMinion.ini` (`Minion.Type.Grunt`,
`Minion.Type.Heavy`, `Command.Minion.Attack.Swing`). The plugin's Core module registers that folder with the tag manager at startup
(the engine reads `Config/Tags` for ordinary plugins only through this call), the same way `GSTNDefence` does. Add your own types in your project's tag ini.

## Dependencies

- Plugins: `GSTNCombat` (`GSTNCombatCore`: entity ids, hurtbox set and timeline assets), `GSTNCombatNP`, `GSTNCore`.
- `GSTNMinionCore` lists engine modules, `GSTNCombatCore` and `GSTNCore` only; never GAS, NP, Mover, an Editor module or a sibling minion module.
- Nothing requires `GSTNMinion`. No `/Game/` path and no `ConstructorHelpers` anywhere in `Source`.

## Tests

`GSTNMinion.Core.*`, no world: `Store.*`, `Schedule.*`, `Tokens.*`, `LOD.*`, `Net.*`, `Tags.*`, `Definition.*`. Run headless:
`UnrealEditor-Cmd GASP_Giustino2.uproject -ExecCmds="Automation RunTests GSTNMinion.Core; Quit" -abslog=<file>` and read the counts from the log.
