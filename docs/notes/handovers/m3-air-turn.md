# M3 air turn fix (cloud, Opus, NOT compiled). Branch `claude/project-thread-1d70rm` from `claude/qa-movement-1117` 180164f3
## Cause (qa-movement-1117 defect 3: air turn 300 deg/s instead of the profile's 3600)
GASP's `Content/Blueprints/MovementModes/BP_MovementMode_Falling` (child of UFallingMode) overrides GenerateMove: it calls the parent,
then recomputes `ProposedMove.AngularVelocityDegrees` with `MovementUtils::ComputeAngularVelocityDegrees` whose TurningRateLimit is a
literal 300 typed on the node. Evidence: `strings` on the uasset shows GenerateMove + ComputeAngularVelocityDegrees + TurningRateLimit and
the only literal `300.000000` (pin default). 300 deg/s = 4.8 deg/frame: 180 deg in 38 frames, matching air_reverse_az90.png (~30 to within 30 deg).
Shared TurningRate (attempt 1 of qa-movement) never reaches that node, so it had no effect. Kept anyway (Mover's own modes use it).
## Fix (code only, the Blueprint is untouched)
- `FGSTN_AirTurnInputs` (GSTNCombatNP Movement/GSTN_AirTurn.h): Mover input entry, rate deg/s (uint16 on the wire). The combat
  component adds it in ProduceInput on the frames its air turn writes the orientation intent (same conditions as before: stick active,
  not on ground, not in a move unless bAirTurnDuringMoves). Predicted and replayed with the command; the server reads the same rate.
- `FGSTN_AirTurn::Apply`: after the mode (Mover's ProcessGeneratedMovement hook, bound in UGSTN_CombatSimComponent::BeginPlay/BindAirTurn)
  sets the yaw rate toward the intent at the entry's rate, about the local up. Skipped: no entry, a non-falling mode, a layered move with
  an override mix (knockback, slide, freeze own the rotation). Pitch/roll dropped (falling keeps upright).
- Pure maths `FGSTN_AnalogWalkLogic::AirTurnYawRate` (Core), same side rule as StepAirYaw (exact 180 turns positive).
- Known limits: GASP's RotationOffset (strafe aim) is ignored on air-turn frames; if anything else binds ProcessGeneratedMovement
  (single delegate) the air turn falls back to 300 with a warning in the log (`LogGSTNCombatSim`).
## Tests added (not run: cloud)
`GSTNCombat.Core.AnalogWalk.AirTurnRate` (rates, 180 in 4 frames at 3600, 38 at 300), `GSTNCombat.NP.AirTurn.Inputs` (wire round trip,
quantize), `GSTNCombat.NP.AirTurn.Apply` (rate, short way across 180, cap, left-alone cases, 180 loop in 4 frames).
## Next (desktop, Haiku low for build/test)
1. Build: `Tools/CI/Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\m3air-build1.log` (UHT: new USTRUCT + UFUNCTION with FProposedMove& out param).
2. Tests: `Tools/CI/Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\m3air-test1.log -Tests "GSTNCombat.Core.AnalogWalk+GSTNCombat.NP.AirTurn+GSTNCombat.NP.RunStages+GSTNCombat.NP.StartupSteer"`.
3. In game: `Tools/GASNPPTest/run_movement_qa.ps1 -Names move_air_control`: bar = facing the new way within 8 frames (expected ~4), on
   authority AND owner. If it still reads ~300: check the log for the "ProcessGeneratedMovement is bound by" warning first.
4. Then the QA jump/air pass (brief: project files handoffs/qa-jump-air-brief.md).
Cost estimate ~6 $ (Opus medium, no builds).
