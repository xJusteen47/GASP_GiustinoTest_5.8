# Launch, ground bounce, wall splat, knockdown (laptop, UE 5.9, 2026-11-14)
Branch `claude/project-thread-ebigxw` from `79454ebc`. Worktree `GASP_Giustino2_launch` (laptop), Content junctions as `_padmenu`.
**Dummies not leaving the ground**: NOT Mover (Falling only floor-checks when vertical speed is ~0 and the move goes down; a launch sets
Falling on its first frame). Cause: the standard WeightDriven model counted % only through the health weight curve (max x1.67 at MaxHealth)
and below LaunchThreshold (1080) a grounded hit is a horizontal PushBack by design. Hub strikes are 300-900 with growth 0: never a launch.
Fix: `UGSTN_KnockbackModel_WeightDriven::PercentGrowth` (data, default 0; standard rules 1.0 in FillStandard and the asset): x(1 + %/100).
Medium jab 550 launches from ~60 %, tumbles at 100 %. Owner can tune it in `GSTN_CombatRules_Standard` > Knockback Model.
**Wall splat / ground bounce**: the standard rules had NO surface impact table, so nothing ever splatted. New `GSTN_SurfaceImpacts_Standard`
(Surface.Type.Wall, no default outcome, min 300 cm/s, restitution 0.6, splat 40 frames). A floor contact now bounces when the hit's
`Impact.Force = Bounce` (spent after the first floor bounce); Force Splat never splats on a floor. Test moves (owner: "cloni di una mossa"):
`KIT_Example_Impacts` (pad IMPATTI at 1200,-1400): X = GA_Example_GroundBounce (spike -45, 1900), RT = GA_Example_WallSplat (15, 2000).
**Knockdown (owner spec)** = Defence F8, already GAs: tumble landing -> mini bounce (GA_GroundBounce) -> lying; stick held -> GA_GetupRoll
toward it; jump (A) -> GA_GetupNeutral; no get-up attack, no shield tech. It only works on kits with a defence get-up set: the script adds
it to KIT_Test_Hits/Light/Medium/Heavy/Ranger/Example/Example_Impacts (no shield, no tech set).
**Self knockdown (dropkick)**: a Tags track with `Combat.Move.KnockdownOnLanding` on the move: landing inside it after being airborne in
it cancels the move and downs the user (same path). Strike spec has `Tags`/`TagsFromFrame` for test moves.
**States and where an animation plugs in (for the anim thread)**: today there is NO victim-side montage hook in code. Hitstun
(`Combat.State.Hitstun`), launch/tumble (`Combat.State.Tumble`), splat (`ActionState Splat`), lying (`Combat.State.Knockdown`) have no
anim field: need a reaction anim set (state -> montage) = new thread. GAs with a timeline (TL Montage field): GA_GroundBounce_Test
(landing bounce), GA_GetupRoll_Test (roll), GA_GetupNeutral_Test (stand up), tech GAs (unused by the owner). Face up/down: not modelled.
**Gates**: build ok; headless GSTN **1587/0**, 0 ensures (3 new tests). Content written headless by `Tools/GAS/setup_launch_impacts.py`. PIE not run.
**Cost**: ~200k context, Opus, no subagents; estimated ~$20-25.
**Not done / next**: PIE check on the desktop (hit a dummy at 100 % with X, watch it fly; IMPATTI kit near a wall); reaction anim hook
and the shrinking capsule (ECB) are queued to other threads; harness suites not run (KIT_Test_Hits now downs on tumble landing).
