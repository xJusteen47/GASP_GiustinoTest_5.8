# Weak point precision, code slice 3 (desktop, 2026-11-10)

Branch `claude/desktop-weakpoint-3` from 92e9b029. Plan `docs/architecture/plans/precisione-weakpoint-2026-11-09.md` §4-§5. Docs: `Plugins/GSTNCombatNP/README.md`
("Startup steering", "Precision reticle and aim arrow"), `Plugins/GSTNInputNP/README.md` ("Aim stick and the automatic right stick actions"), `Plugins/GSTNInput/README.md`.

**Done (scope A)**
- Predicted turn: each startup frame (before `FGSTN_StartupSteer::GetSteerEndFrame`, the first active frame) `FGSTN_CombatTimelineLogic::ComputeStartupSteer` clamps
  the aim stick yaw to `MaxSteerDegrees` around `Hit.MoveOrigin`'s yaw (no new sync field) and adds a `SetFacing` request (int16 yaw); the ASC pre-tick applies
  `UGSTN_GE_SetFacing` -> Mover instant effect `FGSTN_SetFacingEffect` (yaw only). Centred stick = no request. Hitstop frames and recoil skip it.
- Right stick automatic: `UGSTN_InputConfig::bAutoAimActions` (default on) makes transient `AimStickAction`/`ReticleToggleAction` (Gamepad_Right2D / RightThumbstick,
  `bConsumeInput` false so the camera keeps the stick) in a transient IMC that the player source adds (priority 100). No asset edited. GSTNInputCore now lists InputCore.
- `UGSTN_PrecisionAimComponent` (local, added by the combat component, `bAddPrecisionAim`): reticle toggle + aim arrow, debug-line placeholder look.
  Per mode: `UGSTN_CombatSettings::PrecisionReticleModes` (ini: `Combat.Mode.PvE`, `Combat.Mode.TDM`), `FallbackMatchMode` PvE, `IGSTN_MatchModeSource` on a game state.
- Tests: `GSTNCombat.NP.Weakpoint.StartupSteerTurn`, `SetFacingEffect`; `GSTNCombat.Core.StartupSteer.Goldens` extended.

**Done (scope B, test only)**: `UGSTN_GA_TestStrikeKinds` + `UGSTN_TestStrikeKindsLibrary`, `Tools/GAS/setup_strike_kinds_test.py` (run headless, created
`TL_Test_StrikeKinds` and `KIT_Test_StrikeKinds`; chain check: kit and WPN_Test_Hits fallbacks off), `test_hub.strike_kinds(i, on)`. PIE steps in the TODO artifact.

**Choices (owner can overturn)**: no game mode class exists, so the per-mode flag is a settings list + interface (new modes default to no reticle). The reticle
toggle is local, not a command (nothing in the sim uses it yet). Left stick held during startup: the movement mode still turns toward it (TODO artifact, decision).

**Not done**: `AimPolicy` turn (still data only), gyro, a harness scenario that steers with the aim stick, looking at the placeholders in PIE (I cannot see images).

**Gate**: builds `Saved/WP3/build-b1.out` (failed link: FKey needs InputCore, 67 s), `build-b2.out` OK. Headless `Saved/WP3/headless-h1.log`: 1474 / 0 / 0 ensures (+2).
Harness `Saved/WP3/r1.log`: listen+2, 60/15, 0 %, DefaultLevel_Harness: command_set, camera_aim, combat_main_inputs, ricochet PASS, 0 ensures (no scenario drives the aim stick).

Cost: ~170k context, Opus, estimated ~$11-13. Builds 2 (~1 min + 6 s), content script 1, headless 1, harness 1.
