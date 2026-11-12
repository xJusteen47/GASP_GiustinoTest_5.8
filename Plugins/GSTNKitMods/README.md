# GSTNKitMods

Kit mods (M11 slice L5, card gate G4, spec [combat-loadout-mods.md](../../docs/architecture/combat-loadout-mods.md) §8).
Optional plugin. Disabled = kits without mods; a saved loadout's mod entry is dropped with a warning (loadout extension rule).

## Modules

| Module | What | Depends on |
|---|---|---|
| `GSTNKitModsCore` | Data types, the loadout extension `FGSTN_LoadoutModsExt` (wire id `KitMods`), the pure baking `FGSTN_KitModBaking` | `GSTNCombatCore` |
| `GSTNKitModsGAS` | The mod asset `UGSTN_KitModDefinition`, the loadout extension `GSTN.KitMods` (`FGSTN_KitModLoadout`) | `GSTNCombatCore`, `GSTNCombatGAS` |

No NP module: the base timeline of an ability with an inline timeline is read by reflection (`TimelineAsset`, `Timeline`).

## Designer flow (no Blueprint wiring)

1. On a kit asset, add the **Kit mod slots** extension (`FGSTN_KitModSlots`): one entry per slot (`Mod.Slot.*`, ini tags), the
   limits (`MinTimeScale`, `MaxTimeScale`, `MaxReachScale`) and `MaxDerivedTimelines` (registry cap, default 64).
2. Make a `UGSTN_KitModDefinition` (primary asset type `GSTNKitMod`): identity tag under `Mod`, its `Slot`, `CompatibleKits`
   (query on kit + primary weapon tags), `MoveModifiers`, `StatsEffect` + SetByCaller magnitudes, `UnlockedAlternatives`,
   `AddedBindings`, `RemovedCommands`, `Passives`, `Visual`.
3. The player's choice is `FGSTN_LoadoutModsExt` in `FGSTN_LoadoutData::Extensions`: a list of configs, each for one kit identity
   tag or for any kit (empty `Kit`). The kit-specific one wins.

## What happens at compile (outside the simulation)

- Mods are resolved in kit slot order (one mod per slot, first free slot of its type). A missing, incompatible or slotless mod
  is **skipped with a warning**, never an error: a kit swap is never refused because a mod does not fit.
- `RemovedCommands` remove bindings from the kit and every weapon; `AddedBindings` and `Passives` go on the kit.
- Every kit or weapon binding a `MoveFilter` selects (ability asset tags + command tag; empty filter = every move) gets a
  **derived timeline**: base data baked with the combined modifier (scales multiply, kit limits clamp). It is a transient
  `UGSTN_CombatTimeline` named `GSTNModTL_<CityHash64(base path | mod ids in slot order)>`, same path on every machine; the
  binding's `TimelineOverride` points to it.
- Baking: hit data multipliers (damage, knockback, hitstop, stagger) and hitbox extent, reach on `ReachAxis` (L4), time in
  integer thousandths (windows keep >= 1 frame, keys stay ordered, baked sockets resampled, montage rate / scale), a Tags track
  for `AddedTags`. Not scaled: recoil frames, landing lag, movement curves.
- Rules view: the mods' warnings, `UnlockedAlternatives` added to the primary weapon's allowed list.
- On grant (authority): each mod's `StatsEffect`, removed on clear.

## Known gaps

- **The simulation does not read `TimelineOverride` yet**: `UGSTN_CombatTableSet` keys the registry by ability class and reads the
  class default's timeline. So move modifiers are baked and registered but have **no in-game effect until slice L5b** makes the
  table set and the ability read the per-entry timeline. Verbs, passives, stats and unlocked alternatives work now.
- A special chosen through `SlotCustomisations` is swapped in by the compiler after the extensions run, so it keeps its base
  timeline (L5b).
- Mod visuals are data only; the attachment provider that shows them is not written (L7 with backpacks, or L5b).
- No `bFixedTime` per key (spec §8): every key scales.

## Tests

`GSTNKitMods.Core.{Combine,BakeTime,BakeHits,NamesAndConfig}`, `GSTNKitMods.GAS.{Resolve,Compile,RulesView}`.
