# GASP_Giustino2

Unreal Engine **5.8** project built on the Game Animation Sample (GASP). One C++ module, `GASP_Giustino2` (prefix `GSTN_`).
Focus: wall movement for **Mover**, driven by **Gameplay Abilities** simulated by **Network Prediction** (the "GAS-NPP fork").

Read the linked docs only when the task touches that area. Do not read the whole `Plugins/` tree: it is engine-level code.

## Where things are

| Area | Path | Doc |
|---|---|---|
| Project C++ | `Source/GASP_Giustino2/{Public,Private}/` | this file |
| Abilities, tags, ability input | `.../GAS/` | [gas-npp.md](docs/architecture/gas-npp.md) |
| Wall movement modes, transitions, math/query libs | `.../WallMovement/` | [wall-movement.md](docs/architecture/wall-movement.md) |
| Input plugin (commands, buffer, priority, bindings; embeddable NP input simulation) and the Mover sim-state tracker (`UGSTN_MoverSimStateTracker`, `GSTNInputNP/Mover/`) | `Plugins/GSTNInput/` (Core), `Plugins/GSTNInputNP/` (NP adapter) | [input.md](docs/architecture/input.md) |
| Combat plugin (default command set, combat tags and settings; pure, no fork), its GAS layer (ability tags, no NP) and its NP adapter (own NP model, combat abilities, hits; needs the fork) | `Plugins/GSTNCombat/` (Core), `Plugins/GSTNCombatGAS/` (GAS), `Plugins/GSTNCombatNP/` (NP adapter) | [combat.md](docs/architecture/combat.md), [gas-npp.md](docs/architecture/gas-npp.md) |
| Destructible parts (pure math, data, validators; extends the combat kit) | `Plugins/GSTNParts/` (Core) | [combat-parts.md](docs/architecture/combat-parts.md), [m4b-doc-deltas.md](docs/notes/m4b-doc-deltas.md) |
| Combat editor tooling (timeline editor, validators; editor only) and combat debug adapter (not in Shipping); the input display itself is `GSTNInputDebug` in `Plugins/GSTNInput/` | `Plugins/GSTNCombatEditor/`, `Plugins/GSTNCombatDebug/` | [combat-m5-editor.md](docs/architecture/combat-m5-editor.md), [m5-doc-deltas.md](docs/notes/m5-doc-deltas.md) |
| Traversal surface data contract (what a collider offers: wall slide, climb, launch, stand; `Surface.*` tags; resolution from a hit) | `Plugins/GSTNSurface/` (Core) | [traversal-surfaces.md](docs/architecture/traversal-surfaces.md) |
| Network test harness (C++ side) | `.../GASNPPTest/` | [testing.md](docs/architecture/testing.md) |
| Editor Python tooling | `Tools/GAS/`, `Tools/GASNPPTest/` | [testing.md](docs/architecture/testing.md) |
| GAS-NPP plugin forks (GameplayAbilities, NetworkPrediction, AbilitySystemSimulation...) | `Plugins/GASNPP/` | [gas-npp.md](docs/architecture/gas-npp.md) |
| Mover / ChaosMover / SmartObjects overrides of engine plugins | `Plugins/EngineOverrides/` | |
| Working notes, plans, spike reports and the current handover (`takeover-state.md`, last entries) | `docs/notes/` | [README](docs/notes/README.md) |

Headers carry long doc comments that explain intent and invariants: read the header of a class before its `.cpp`.

## Conventions

- Every project type is prefixed `GSTN_` (`UGSTN_`, `FGSTN_`, `EGSTN_`); export macro is `GASP_GIUSTINO2_API`.
- Public headers in `Public/<Area>/`, implementations in `Private/<Area>/`; includes are module-relative (`"GAS/GSTN_GameplayAbility.h"`).
- Comments explain *why* and the simulation constraints, in English, at the density of the existing headers. No restating the code.
- Gameplay tags are native (`UE_DECLARE_GAMEPLAY_TAG_EXTERN` in `GAS/GSTN_AbilityTags.h`), not ini-defined. One exception: designer-made input tags (buttons, commands and their lock tags) live in `Config/Tags/GSTNCommands.ini` (input.md §3.1).
- Tuning values are `UPROPERTY(EditAnywhere/EditDefaultsOnly)` grouped by `Category = "Area|Section"` with `meta = (ClampMin, ForceUnits)`.
- Tunable data goes in small `USTRUCT`s (see `GSTN_WallMovementTypes.h`), not as loose floats on the mode.
- Stateless math/queries go in `UBlueprintFunctionLibrary` classes (`*Utils`), not in the mode classes.

## Rules that are easy to break (details in the docs)

1. **Simulation code must be deterministic and rollback-safe.** No `IsServer()` / "is resimulating" branches, no state outside the sync/aux state or `FSync*` variables, no reading Mover component queries from abilities (use `GetMoverSimState`). See gas-npp.md.
2. Abilities **never set character state directly**: they move the character through Mover queued moves (gameplay effect with `UApplyMoveGameplayEffectComponent`).
3. Only `UNpGameplayAbility` children work in the fork: derive from `UGSTN_GameplayAbility`.
4. Wall mode behaviour is added by **subclassing and overriding the extension points**, never by editing the base core. See wall-movement.md.
5. The ability system ticks **before** Mover in the fixed tick: tags/effects granted in a frame are seen by Mover in the same frame, but Mover's new mode is visible to abilities one frame later.

## Content and versioning

- Only `Content/{Blueprints,GAS,GASNPPTest,Input,Levels}` are versioned, through Git LFS (`*.uasset`, `*.umap`), **without locks**. The rest of `Content/` is sample content, not in git.
- Blueprints are binary and cannot be merged: **tell the user before editing a shared Blueprint**, and never edit the same one from two sessions.
- `Tools/GAS/setup_gas_assets.py` is idempotent and backs Blueprints up to `Saved/Backups/GAS` before changing them. Prefer it (or the editor MCP) over hand-editing uassets.
- Ability input mapping contexts must be listed in `Config/DefaultGame.ini` under `AbilitySystemMappingContexts`.

## Build and run

- Engine association `5.8`; Rider is the IDE (Rider MCP tools are available: prefer `search_symbol`, `get_class_hierarchy`, `get_symbol_info` over reading files).
- C++ changes: Live Coding in the open editor, or a full build from Rider for header/reflection changes.
- Editor automation: the `unreal-mcp` server (`.mcp.json`) and the `unreal-mcp` skill, when the editor is running.
- Network behaviour is verified with the PIE harness in `Tools/GASNPPTest/gasnpp_test.py` (dedicated server + 2 clients). Anything touching abilities or movement simulation should be checked with it; see testing.md.

## Git

Branch `feature/gas-npp-5.8`, base `main`. Commit messages: short imperative subject, a body that explains the reason.
