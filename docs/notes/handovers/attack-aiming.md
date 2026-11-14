# Attack aiming in startup: per-move steering (laptop, UE 5.9, 2026-11-14)
Branch `claude/project-thread-j5gq18` from `867f2b39` (comb59), laptop worktree `GASP_Giustino2_aim` (Content junctions to the main tree).
Spec `handoffs/attack-aiming-research.md`. Owner: steering source per move; left stick by default, right stick (and gyro later) only for moves
that need a reticle. Not answered, recommended applied: signature aerials may turn up to 180 deg in their first frames.
**Done**:
1. Data `FGSTN_CombatTimelineData::Steer` (`FGSTN_SteerWindow`): `Source` (`AimStickWithReticle` / `MoveStick` / `MoveOrAimStick`),
   `StartFrame`, `EndFrame` (-1 = first active frame), `RateScale` curve over the window, `bHoldFacing`, `bShowIndicator`.
   `MaxSteerDegrees` clamp 90 -> 180; `SteerRateDegPerSecond` unchanged (deg/frame = rate / 62.5).
2. One pure decision `FGSTN_StartupSteer::ResolveFrame` (phases None/Before/Steering/Locked) used by the combat tick (SetFacing request),
   the Mover input producer (orientation intent held) and the arrows. Move stick sources hold the facing from frame 0 to the end of the
   last active window, so the left stick cannot pass the cap or turn after the lock; releasing it keeps the aim. Wide cones step in the
   offset from the origin. Ground and air share it (yaw only). Inputs = sticks already in the command, no keys, no project settings.
3. v0 indicators `UGSTN_SteerIndicatorComponent` (auto-added, local pawn only): thin stick arrow, thick facing arrow, allowed arc, white
   flash at the lock. Toggle: move data `Steer.bShowIndicator`, component `bEnabled`, cvar `GSTN.Combat.SteerIndicator` (-1/0/1).
4. ESEMPIO: `GA_Example_Strong` = left stick, 90 deg, 450 deg/s, until first active; `GA_Example_Special` = signature profile, either
   stick, 180 deg, 900 deg/s, frames 0-6. Written by `Tools/GAS/setup_designer_examples.py` (`STEER`, `WriteStartupSteer`), assets committed.
**Gates**: full editor build UE 5.9 OK first try; headless `Automation RunTests GSTN` 1594/0 (1567 + 27 with warnings, as before; +3 new
`GSTNCombat.Core.StartupSteer.Window.*`).
**Not done / not verified**: no PIE, no server + 2 clients run, nobody has seen the arrows. Not checked in game: Mover falling mode
honouring the held intent (the code path is the same). The arrows are debug lines (not decals, absent in Shipping). The spec's
`bReanchorWorldHitboxes` and pitch steering are not done: `WorldAtMoveStart` hitboxes still point at the start yaw (ESEMPIO moves use Root).
**Decision to flag**: the C++ default source stays `AimStickWithReticle`, so every old move is unchanged. Making the left stick the default for
all melee moves is a one-value change per move (or of the default) after the playtest. Light example left as before.
Merge: the coordinator merges and builds on desktop _ue59. `PM_BoostJump`/`PM_ThrowRider` show as modified after checkout: not committed.
Cost: ~180k context, Opus, no subagents; estimated ~$9-11.
