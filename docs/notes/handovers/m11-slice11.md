# M11 slice L5b: modded moves run their own timeline in the simulation (laptop, 2026-11-12)
Branch `claude/laptop-m11-l5b` from `d9c1abec` (L5). Brief: `m11-slice10.md` (gap found by L5). Spec `combat-loadout-mods.md` §8 (*Built (M11 L5b)*).
**Done** (GSTNCombatNP, plus small reads in GSTNDefenceNP, GSTNCombatDebug, GSTNKitModsGAS):
1. `UGSTN_CombatTableSet`: per registry entry `TimelineOverrides` (null = class default) and `GetTimelineData(Index)`, the one read path.
   `AddTimelineClass(..., Override)` shares only (class, override) pairs and loads the override's montage and recoils. Checksum adds the
   override path and the entry's real TotalFrames. Converter step 4 loads `Compiled.Registry[i].TimelineOverride` through the new
   `FGSTN_CombatTableConvertInput::LoadTimeline` (default: object in memory first, the derived timelines are transient, then TryLoad);
   one that does not load keeps the entry on the class default's data, with a report error.
2. Sim: `GetRegistryTimeline` -> `Set.GetTimelineData`, so the clock, commit, gate, hits, steer, recoil, snapshot all read the entry's data.
3. Ability `UGSTN_GA_BaseCombat`: sync var `RunningEntry` (set at BeginTimeline from the combat state), `GetRunningTimelineData()`
   (entry data, checked against the class, else class default), montage played/stopped from `TimelineMontagePlayed` (fallback: running
   data). `IsStillTheRunningTimeline` also checks the entry. `UGSTN_GA_MoveTimeline` movement track, defence parry track and bake check
   read the entry's data. `UGSTN_CombatSimComponent::IsTimelineRunning(..., OutEntry)`, `GetActiveTableSet()`.
4. Kit mods: a weapon row changed by a slot customisation is baked from the alternative (tags and timeline), as the compiler swaps it later.
**Decisions taken (recommended)**: adoption by trigger keeps "first registry entry of the class" (with base + modded of one class: base);
supers are not modded (no owner ask); Link partner presentation still reads the partner class default (presentation only, no mods there).
**Gate (laptop)**: build ok; headless GSTN **1542/0**, 0 ensures (+5: `GSTNCombat.NP.ModTimeline.*` x4, `GSTNKitMods.GAS.CustomisedRowBakesAlternative`;
`Saved/M11L5b/headless-t1.log`). Suite `R31_ALL=1` **50/50**, 0 ensures (`Saved/R31/report-m11l5b-all.txt`). First try, no fixes needed.
**Not covered / nobody has seen in PIE**: still no mod content, so no modded move has run in a match; the harness has no mod scenario.
Desktop: build, then author one test mod (e.g. long blade x1.5 time on a `KIT_Test_*` attack) and watch it in PIE listen+2: longer move,
slower montage, same frames on both clients (table checksum equal in the logs). Game/Shipping not built. No content scripts run here.
**Owner questions**: none new. Still open: M11.2/M11.3/M11.4, M11.6, special swap input, L9 move design. No manual step; artifact unchanged.
**Next in order**: L7 (backpacks), L8, L10; L9 when the owner gives its design. Worth a harness scenario `kit_mod_slower` once content exists.
Cost: ~95k context, Opus, estimated ~$6-9. Wall clock ~1 h 30 (1 build, 1 headless run, suite).
