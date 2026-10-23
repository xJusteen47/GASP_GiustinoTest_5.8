# M5 spike report (E0)

Scratch notes of the M5 editor work, not design docs. Paths are relative to the project root. Date 2026-10-23.

## Summary

| # | Spike | Verdict |
|---|---|---|
| S1 | Montage sampling | not done (needs an editor session) |
| S2 | Tick rate source | PASS by source read, confirmed by an editor test: readable from the settings CDO with no world, 62.5 Hz |
| S3 | Insights / Rewind Debugger | not done (needs PIE with the sim running) |
| S4 | Gizmo in a custom viewport | not done (needs an editor session) |

## S2 Tick rate source (source read, then confirmed by `GSTNCombat.Editor.Settings.TickRateFromNetworkPrediction`)

### Where the fixed tick rate lives

- The property is `FixedTickFrameRate` (float) in the struct `FNetworkPredictionSettings`, declared in
  `Plugins/GASNPP/NetworkPrediction/Source/NetworkPrediction/Public/NetworkPredictionSettings.h:29`, default `62.5`. Its comment
  says `Engine::FixedFrameRate` takes precedence when `bUseFixedFrameRate` is on; **nothing in the plugin implements that**
  (grep for `bUseFixedFrameRate` finds only the comment), so the engine setting is irrelevant here.
- The struct is held as the property `Settings` of `UNetworkPredictionSettingsObject`
  (`NetworkPredictionSettings.h:135-143`, `UCLASS(config=NetworkPrediction, defaultconfig)`), a plain `UObject` whose config
  lives in `DefaultNetworkPrediction.ini`, section `[/Script/NetworkPrediction.NetworkPredictionSettingsObject]`.
- The project sets it in `Config/DefaultNetworkPrediction.ini:2`: `FixedTickFrameRate=62.5`. The plugin's own
  `Plugins/GASNPP/NetworkPrediction/Config/DefaultNetworkPrediction.ini:2` says `60`; the project value wins, and the
  struct default is 62.5.
- At run time each `UNetworkPredictionWorldManager` copies the CDO into its own `Settings`
  (`NetworkPredictionWorldManager.cpp:50` and `:74-76`, `SyncNetworkPredictionSettings(GetDefault<UNetworkPredictionSettingsObject>())`)
  and every world pre-tick feeds `Settings.FixedTickFrameRate` to `OnWorldPreTick_Internal` (`:195`), which sets
  `FixedStepRealTimeMS = 1000 / rate` (float) and `FixedStepMS = (int32)FixedStepRealTimeMS` (`:208-209`). In the editor,
  `PostEditChangeProperty` re-syncs live managers (`NetworkPredictionSettings.cpp:15`). So the CDO is the source of truth
  for every world, and reading it needs no world.

### Readable with no world? Yes, by reflection only

`UNetworkPredictionSettingsObject` has **no export macro** (`class UNetworkPredictionSettingsObject : public UObject`), so another
module cannot name it (`GetDefault<>` would not link). The editor therefore finds the class by path and reads the property by
reflection: `FindObject<UClass>(nullptr, "/Script/NetworkPrediction.NetworkPredictionSettingsObject")`, its CDO, the struct
property `Settings`, the numeric property `FixedTickFrameRate`. This needs neither a world nor a link to the fork.

Implemented as `UGSTN_CombatEditorSettings::GetSimulationTickRate()` (`Plugins/GSTNCombatEditor/Source/GSTNCombatEditor/Private/Settings/GSTN_CombatEditorSettings.cpp`),
fallback `FallbackTickRateHz` (default 62.5) when the class is not loaded or the value is not a positive finite number.
The editor test reads the same class and compares with the value parsed from the config file it was loaded from: 62.5.

### Things to know for E4/E8

- **The simulation step is an integer number of milliseconds**: `FixedStepMS = (int32)(1000 / rate)`. At 62.5 Hz that is exactly
  16 ms, so frame time `f / 62.5` equals `f * 16 ms` and the ruler's millisecond line is exact. At 60 Hz the step is 16.67 ms
  real but **16 ms in the sim clock** (`FixedStepMS` truncated), so a project at a rate whose period is not a whole number
  of milliseconds drifts between "frame / rate" and the NP sim clock. The editor and the baker should keep using
  `frame / rate` (the real-time meaning, also what `FGSTN_TimelineValidation` assumes); the 62.5 Hz choice is what makes both agree.
  Worth a validator warning in E11 when `1000 / rate` is not an integer.
- The baker stores the rate it used (`FGSTN_BakeInfo::TickRateHz`, E8), so a change of the project rate marks bakes stale.
- Spike S2's "equal to the runtime value logged in PIE" half is not run (no PIE in this session): the runtime copies the CDO
  (`:50`, `:74-76`), so equality is by construction.
