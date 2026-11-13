# M11 slice L5: kit mods (moveset and stats mods, special unlocks, registry cap) (laptop, 2026-11-12)
Branch `claude/laptop-m11-l5` from `c4207dbb` (L6). Plan §2 L5, card gate G4, spec `combat-loadout-mods.md` §8 (*Built (M11 L5)* note).
L9 skipped: aimed ranged moves need owner move design (which button, projectile or instant, vertical aim). L5 taken instead.
**Done**: new optional plugin `GSTNKitMods` (Core + GAS, enabled in the .uproject, `EnabledByDefault` false; README in the plugin):
1. Core: `FGSTN_MoveModifier`, kit extension `FGSTN_KitModSlots` (slots, `Min/MaxTimeScale`, `MaxReachScale`, `MaxDerivedTimelines` 64),
   loadout extension `FGSTN_LoadoutModsExt` (wire id `KitMods`; configs per kit identity tag + wildcard), native tags `Mod`, `Mod.Slot`.
   Pure `FGSTN_KitModBaking`: combine + clamp, time in integer thousandths (windows >= 1 frame, keys ordered, baked sockets resampled,
   montage rate / scale), hit multipliers via `ApplyVariant`, hitbox extent, reach (L4), AddedTags track, stable name CityHash64.
2. GAS: `UGSTN_KitModDefinition` (asset type `GSTNKitMod`, IsDataValid), extension `GSTN.KitMods` prio 100: removed/added verbs,
   passives, derived transient timelines set as `TimelineOverride`, stats effect on grant/clear, unlocked alternatives in the rules view.
3. GSTNCombatGAS: new `FGSTN_LoadoutExtension::ContributeRulesView`, run at the end of `BuildRulesView` (additive).
**Decisions taken (recommended)**: a mod that does not fit is a warning and is skipped, never an error (a kit swap is never refused);
M11.4 kept open as data (kit-specific config vs wildcard; kit swap code untouched, extensions already carry over); M11.13 = one mod per
slot entry; no `FGSTN_ModVisual` type (mods carry an L6 `FGSTN_EquipmentVisual`, data only); base inline timeline read by reflection so
the plugin never depends on GSTNCombatNP; no `bFixedTime` per key.
**GAP FOUND (needs slice L5b, Opus high, GSTNCombatNP, not parallel with Core/NP slices)**: `UGSTN_CombatTableSet` keys the registry by
ability class and the sim reads the class default's timeline, so `TimelineOverride` (and every derived timeline) has NO in-game effect.
L5b: per-registry-entry timeline data in the table set, sim + ability (montage, movement) read it; customised specials get modifiers.
Verbs, passives, stats and unlocked alternatives work now.
**Gate (laptop)**: build ok; headless GSTN **1537/0**, 0 ensures (+7; one fix: `TInstancedStruct<Base>::Make` sliced a derived track,
`Saved/M11L5/headless-t2.log`). Suite `R31_ALL=1` **50/50**, 0 ensures (`Saved/R31/report-m11l5-all.txt`).
**Not covered / nobody has seen in PIE**: no mod content exists; no mod has run in a match. Desktop: build, enable check, author one
test mod (e.g. long blade on a `KIT_Test_*`) only after L5b. Game/Shipping not built. No manual step for the owner; artifact unchanged.
**Owner questions**: none new. Still open: M11.2/M11.3/M11.4, M11.6, special swap input, L9 move design.
**Next in order**: L5b (runtime of derived timelines), then L7 (backpacks), L8, L10; L9 when the owner gives its design.
Cost: ~120k context, Opus, estimated ~$8-11. Wall clock ~1 h 15 (2 builds, 2 headless runs, suite).
