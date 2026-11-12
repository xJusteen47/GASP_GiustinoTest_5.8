# M11 slice 15: L9 aimed ranged moves (laptop, 2026-11-12)
Branch `claude/laptop-m11-l9` from `287992d0` (slice 14). Owner answers 2026-11-12: buttons change with the reticle, hitscan or projectile
per move, vertical aim only with the reticle, plus "ergonomics" (Helldivers 2) tied to the moves' rotation limit.
**Done** (all data, no assets touched, no Project Settings):
1. Buttons by reticle: `GSTN_InputSimulator` adds `Input.Context.PrecisionAim` to the binding context when the frame's command has
   `bPrecisionAim`. A binding that requires it wins over the plain one (which ignores it) while the reticle shows. No host adds it.
2. `UGSTN_GA_FireProjectile`: `Delivery` = Projectile / **Hitscan** (`FGSTN_HitscanSpec` range 5000, radius 5). Hitscan = non-moving
   projectile with a capsule muzzle-to-range, 1 frame of life, `bHitNearestFirst` (new param; subsystem stable-sorts candidates by
   distance from the carrier). Same host pipeline, MaxHits > 1 pierces.
3. New aim `EGSTN_ProjectileAim::Reticle`: reticle on (fire frame's `IsPrecisionAimFrame`) = command camera aim with pitch clamped to
   `Min/MaxReticlePitchDeg` (-60/60); off = `ReticleOffAim` (Facing/Stick) flat + `LaunchPitchDeg`. Pure math in `FGSTN_ShotAim` (Core).
4. Ergonomics: the rotation limit in the specs is combat-m4-hits §3 (HV.3, "Control" track, never built); what exists is `MaxTurnDegrees`
   and the reticle startup steer `MaxSteerDegrees`. New `FGSTN_CombatTimelineData::SteerRateDegPerSecond` (0 = instant, all old moves):
   the steered facing follows the aim stick at that rate inside the `MaxSteerDegrees` cone (`FGSTN_StartupSteer::RateLimitYaw`, steps from
   the Mover pose = rollback-safe). Both plausible behaviours ship as data: cone (MaxSteer) and drag (rate). ProduceInput's hold intent
   uses the same rate.
**Gate (laptop)**: build ok. Headless GSTN **1562/0**, 0 ensures (+3: `GSTNProjectile.Core.HitscanAndReticleAim`,
`GSTNInput.NP.Simulator.PrecisionAimBinding`, `GSTNCombat.NP.Weakpoint.SteerAimHandling`; `Saved/M11L9/headless-t1.log`).
Suite `R31_ALL=1`: see the thread reply (`Saved/R31/report-m11l9-all.txt`).
**Nobody has seen in PIE**: no aimed move exists as content (no Blueprint child, no binding); no harness scenario. Game/Shipping not built.
**Known gaps**: hitscan still spawns a 1-frame projectile actor (spawn bytes) and ignores walls like every projectile; ergonomics is yaw
only (pitch follows the reticle at once); a timeline move that fires (track key) still does not exist, the shot ability uses FireDelayFrames.
**Owner questions (asked in the thread)**: weapon/kit multiplier on the rate; ergonomics on pitch too; hitscan wall collision now or later;
default pitch limits and range.
**Next**: L10 sweep could add a hub aimed-shot move (BP child, Aim=Reticle, Hitscan) bound to RT with `Input.Context.PrecisionAim`, and a
`aimed_shot` harness scenario.
Cost: ~120k context, Opus, estimated ~$7-9. Wall clock ~1 h (1 build, 1 headless run, suite).
