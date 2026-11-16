# GSTNKitModsEditor

Editor tooling of kit mods (M11 slice L8, card gate G7, spec [combat-loadout-mods.md](../../docs/architecture/combat-loadout-mods.md) §15).
Optional, editor only. A bridge plugin: the only module that names both `GSTNKitMods` and `GSTNCombatEditor`, so the mods plugin stays
free of the M5 editor (and of the NP fork it needs) and the M5 editor knows nothing about mods.

## What it adds

| Piece | What | Where |
|---|---|---|
| Mod rules | Findings in the M5 validation panel, Validate all and Data Validation (save, Validate Assets) | `FGSTN_KitModsEditorRules`, registered with `FGSTN_ValidationExtensions` (GSTNCombatEditor) |
| Mod preview | The modded timeline of one move, built as the loadout compile bakes it, opened in the M5 timeline editor | `UGSTN_KitModsEditorUtils`, mod context menu **Preview on kit**, console `GSTN.KitMods.Preview` |
| Preference | `ReachMeshTolerance` (Editor Preferences > Plugins > GSTN Kit Mods Editor), per user, tooling only | `UGSTN_KitModsEditorSettings` |

## Rules (codes)

- **Mod asset**: `Mod.Data` (its own IsDataValid: identity tag, slot tag, empty modifier); `Mod.NoKit` (no kit has a fitting slot that
  CompatibleKits accepts); per fitting kit and modded move: `Mod.NoMove` (modifiers select no move), `Mod.Clamped` (info: kit limits cut
  it back), `Mod.Derived` (the derived timeline breaks an M5 timeline rule the base passes: only what the mod adds), `Mod.ReachMesh` (the
  scaled hitboxes end too far from the mod mesh's reach, estimate from bounds), `Mod.ReachNoMesh` (info: reach changes, no mesh shows it).
- **Kit with Kit mod slots**: `Mod.Slots.Data` (slot tag outside `Mod.Slot`, limits that exclude 1, no slot), `Mod.Slots.Cap`
  (MaxDerivedTimelines below the kit's moves), `Mod.Slots.NoMod` (info: no mod fits a slot yet).
- **Any asset holding an `FGSTN_LoadoutData` with mods**: `Mod.Loadout` (a choice the game skips, with ResolveMods' reason).

Only broken data is an error; everything a match would skip is a warning (a kit swap never fails because of a mod, L5).

## Preview

Right click a mod (or several) > **Preview on kit** > a kit they fit > a modded move: the timeline editor opens a copy in
`/Temp/GSTNModPreview/` (named like the compiled derived timeline, `_Kit` or `_W<n>` for the source). Open the base timeline next to it
to compare. Edits to the copy are saved nowhere: the mod and the base are the sources; previewing again replaces it.
Console: `GSTN.KitMods.Preview <KitPath> <ModPath>[+<ModPath>...] <CommandTag>`.

The preview and the compile share one code path (`FGSTN_KitModLoadout::BuildPreview`, `PlanModdedMove` in GSTNKitModsGAS), so the
editor never shows a move the game would not run. It ignores `MaxDerivedTimelines` (the cap rule reports it instead).

## Known gaps

- The reach check is an estimate: the hitbox centre plus its largest half extent against the farthest bounds point of the longest mesh
  of the mod's visual, as if the blade were attached at its pivot. It warns, never fails.
- No in-place mod toggle inside an open timeline editor: the preview is a separate editor tab.
- Nobody has opened the preview or the menu in a real editor yet (headless tests only).

## Tests

`GSTNKitModsEditor.{Registered,Preview,ModRules,KitRules,Loadout}`.
