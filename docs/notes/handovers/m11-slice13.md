# M11 slice L8: mod editor (laptop, 2026-11-12)
Branch `claude/laptop-m11-l8` from `73682872` (L7). Plan `m11-kit-piano-a-fette.md` L8 (gate G7), spec `combat-loadout-mods.md` §15 (*Built (M11 L8)*).
**Done**:
1. M5 seam in GSTNCombatEditor: `FGSTN_ValidationExtensions` (public): other plugins register rules (AppliesTo, Rule, extra asset classes);
   they run in `BuildReportForAsset` (panel, Validate all, Validate button), Validate all collects their classes, and one new Data Validation
   validator `UGSTN_ExtensionAssetValidator` runs only them (no double reports). `ValidateTimelineData` runs the M5 timeline rules on any data.
2. GSTNKitModsGAS: `FGSTN_KitModLoadout::BuildPreview` + `FGSTN_ModMovePreview`/`FGSTN_KitModPreview`. The compile contribution was split
   into `ApplyModVerbs` + `PlanModdedMove`, shared by compile and preview (the editor never shows a move the game would not run). No behaviour change.
3. New bridge plugin `GSTNKitModsEditor` (editor only, on in the .uproject with an Editor allow list; neither GSTNKitMods nor GSTNCombatEditor
   names the other). Rules `Mod.*` on mods (own data, fit, NoMove, Clamped info, Derived = M5 rules the derived timeline breaks and the base
   passes, ReachMesh vs the mod's mesh bounds, ReachNoMesh info), on kits with mod slots (`Mod.Slots.Data/Cap/NoMod`) and on loadout holders
   (`Mod.Loadout`). Preview: mod right-click > Preview on kit > kit > move opens the derived timeline (copy in `/Temp/GSTNModPreview`) in the
   M5 timeline editor; console `GSTN.KitMods.Preview`; `UGSTN_KitModsEditorUtils` for scripts. Per-user pref `ReachMeshTolerance` (0.25, Editor Preferences, not a project setting).
**Gate (laptop)**: build ok; headless GSTN **1554/0**, 0 ensures (+5 `GSTNKitModsEditor.{Registered,Preview,ModRules,KitRules,Loadout}`,
`Saved/M11L8/headless-t4.log`). Suite `R31_ALL=1` **50/50**, 0 ensures (`Saved/R31/report-m11l8-all.txt`). 2 build fixes (test asserts), 1 test fix (sliced track).
**Nobody has seen in the editor**: the context menu, the preview tab on a `/Temp` timeline (opening, save prompt on close), the mod findings
in the validation panel. No mod asset exists yet, so on real content the panel only shows kit-slot findings once a kit has the extension.
**Decisions taken (recommended)**: separate bridge plugin instead of a module inside GSTNKitMods (keeps GSTNKitMods free of the NP fork the
M5 editor needs); preview as a separate editor tab rather than a mod toggle inside an open timeline editor; mod findings never errors
except broken data (a kit swap never fails because of a mod, L5); reach check is an estimate from bounds, warning only.
**Owner questions**: none new (L8 is tooling). Still open: M11.2-M11.4, M11.6, M11.10, M11.11, special swap input, L9 design, jet-pack feel.
**Not built**: in-place mod toggle in the timeline editor (overlay of base vs derived); `bFixedTime` per key (spec §8) still missing.
**Manual steps for the owner**: none (artifact unchanged). Desktop re-verify: build, open a kit with Kit mod slots, run Validate all, author one
test mod and try Preview on kit (L10 can do it with the hub mod pad).
**Next**: L9 (aimed ranged moves) or L10 sweep. Cost: ~120k context, Opus, estimated ~$7-9. Wall clock ~1 h 15 (4 builds, 4 headless runs, suite).
