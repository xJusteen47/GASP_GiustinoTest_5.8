# Momentum slide on ground attacks (cloud, 2026-11-13/14)
Branch `claude/project-thread-77ctnl` from `79454ebc` (5.9 line). Owner spec: ground attacks started while running slide on the feet along the
momentum; started against it (dot too negative) they may behave differently; both per move. **NOT COMPILED** (no UE in the cloud).
**Done**:
1. Data (Core): `FGSTN_MomentumSlide` on `FGSTN_CombatTimelineData::MomentumSlide` (details: Timeline > Movement), off by default = no change for
   existing moves. Knobs: `MinSpeed` 150, `OppositeDotThreshold` -0.3, `bStopAtLedge`, `bAddRootMotion`, and two responses `Along` / `Against`,
   each `Mode` Slide/Stop, `SpeedScale`, `MaxStartSpeed`, `Friction` Fixed (`Deceleration` cm/s^2, Along 1500, Against 5000 = hard brake) or
   Character (Mover legacy braking: Deceleration + BrakingFriction*Factor*v, times `FrictionScale` 0.25), `MaxFrames`. Pure math
   `FGSTN_MomentumSlideMath` + Core tests `GSTNCombat.Core.MomentumSlide.*`.
2. Sim (NP): `FGSTN_LayeredMove_MomentumSlide` (OverrideVelocity, prio 0) queued through `UGSTN_GE_MomentumSlide` by
   `UGSTN_GA_BaseCombat::BeginTimeline` BEFORE the montage plays. The move decides on its first frame inside Mover's tick (previous frame's
   velocity, facing after the same frame's snap turn, data via new `UGSTN_CombatSimComponent::ReadRunningSlide`). It adds this frame's montage
   root motion (copied from the queued `FLayeredMove_SyncMontageRootMotion`), ends with the move / hit / cancel / recoil / leaving the ground /
   ledge, pauses in hitstop (freeze wins at prio 1, frozen frames not counted). All latched state is serialized in the move.
3. `Tools/GAS/setup_momentum_slide_examples.py` (headless, idempotent, after setup_designer_examples): enables it on ESEMPIO
   `GA_Example_Light` (against = brake) and `GA_Example_Strong` (character friction, against = Stop), so both behaviours can be felt.
4. combat.md 11.3 gained the paragraph.
**Desktop gates (in order)**: build _ue59; headless GSTN tests (new Core tests + all old ones: a timeline field was added); run the script;
PIE in L_Test_Hub with ESEMPIO: run + X, run + RT, run then flick back + attack; dedicated server + 2 clients (`gasnpp_test.py`) for corrections.
**Risks to check first**: (a) frame 0 order vs the root motion move (if the run stops for 1 frame, the slide was queued after it);
(b) `FindRootMotion` relies on Mover's start-state copy still holding this frame's queued moves (MovementModeStateMachine.cpp ~260):
if root-motion travel vanishes while sliding, that is why; (c) ground check uses `GroundMovementModeName` of the shared settings.
**Not touched**: knockback/launch/knockdown files (other thread), ABP, movement tracks (jab "no magnet" stays separate), minions, air moves.
**Decisions (recommended, flag)**: off by default; slide follows the run direction, not the facing; the facing used is the one of the move's
first frame (a rate-limited steer that turns later does not change the pick); against default = hard brake rather than stop.
**Next**: designer guide artifact (Mosse di esempio GSTN) should list the new fields; owner picks defaults after feeling them.
Cost: ~140k context, Opus 5.5, no subagents; estimated ~$10-14. Wall clock ~1 h.
