# GSTNInputNP plugin split: verify after the build

Done on disk only (no build, no commit). The module `GSTNInputNP` moved with `git mv` (staged) from
`Plugins/GSTNInput/Source/GSTNInputNP` to `Plugins/GSTNInputNP/Source/GSTNInputNP`.

Before building
- [ ] Editor closed. Delete stale NP module artifacts in the old plugin: `Plugins/GSTNInput/Binaries/Win64/*GSTNInputNP*`
      (dll, pdb, modules json: UnrealEditor.modules in that folder lists it) and `Plugins/GSTNInput/Intermediate/Build/**/GSTNInputNP`.
      Safest: delete `Plugins/GSTNInput/Binaries` and `Plugins/GSTNInput/Intermediate`, so GSTNInputCore is rebuilt too.
- [ ] Regenerate project files (new plugin dir) and do a full build from Rider (reflection/descriptor changes, not Live Coding).

After the build
- [ ] Build succeeds for the editor and game targets; `GSTNInputNP` is compiled out of `Plugins/GSTNInputNP/Intermediate`.
- [ ] Editor loads without "plugin missing/module not found" for GSTNInput, GSTNInputNP, GSTNCombat; Plugins window shows GSTN Input NP enabled.
- [ ] Headless: `Automation RunTests GSTNInput+GSTNCombat`: 94 tests pass + the new exclusive-context-groups tests (count them from the log).
      Names `GSTNInput.NP.*` still appear.
- [ ] Redirect: `Plugins/GSTNInputNP/Config/DefaultGSTNInputNP.ini` is read ([CoreRedirects] of the tracker). Check that the old
      `[CoreRedirects]` block no longer exists in `Plugins/GSTNInput/Config/DefaultGSTNInput.ini` and that a load log has no
      "class not found" for `/Script/GASP_Giustino2.GSTN_MoverSimStateTracker` or `/Script/GSTNCombatNP.*`.
- [ ] Assets that reference `/Script/GSTNInputNP.*` types (binding sets, combat Blueprints in Content/Blueprints, GAS, Input) load without errors.
- [ ] Class defaults / settings: Project Settings > Plugins > GSTN Input still shows the settings; DefaultGSTNInput.ini values intact
      (the other agent's ExclusiveContextGroups line is kept).
- [ ] PIE harness (Tools/GASNPPTest/gasnpp_test.py) still passes the combat scenarios.
- [ ] Sanity of the split: temporarily disabling GSTNInputNP (and GSTNCombat) in the .uproject leaves GSTNInput building with no fork modules.
- [ ] Source/GASP_Giustino2/GASP_Giustino2.Build.cs references GSTNInputNP as module dep: fine (module name unchanged), but the project
      .uproject now lists the plugin GSTNInputNP explicitly.

Known caveat
- GSTNCombat.uplugin now lists GSTNInputNP as a non-optional dependency, so enabling GSTNCombat (even only its Core) pulls in the fork.
