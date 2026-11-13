# M11 slice 18: L7c the shield-backpack dome blocks hits (laptop, 2026-11-13)
Branch `claude/laptop-m11-l7c` from `56112788` (L10, carries L7b), pushed as `claude/project-thread-360h11`. Plan: `docs/notes/plans/m11-l7c-dome-plan.md`.
Owner rule 2026-11-13: block EVERY hit from outside, push back whoever hits it from outside, one hit = one side.
**Done** (code `46e927a8`, assets in the handover commit):
1. Side of a hit = the attacker's ROOT in the snapshot (end of F-1), never the weapon: attacker prediction, victim and lag comp give one verdict.
2. `FGSTN_BackpackDomeSnapshotContributor` (GSTNBackpackNP, id 21): 40 B blob (dome centre, radius, `InsideDomes[4]`), the dome as an extra
   sphere hurtbox (priority 255, nothing ignored: grabs too, `bOutsideOnly`), `FinalizeSnapshot` writes the inside lists (early out with no dome).
3. Core: `FGSTN_ExtraHurtbox::bOutsideOnly` + `IsSkippedFrom` (attacker tick: root; minion attacker: root; projectile: previous position), so
   an attacker inside never touches the shell. Snapshot blob cap 3 -> 4 (`MaxBlobs`, `MaxSnapshotExtensions`). Contributor context has `Transform`.
4. Step `UGSTN_HitStep_BackpackDome` after TeamFilter: blocks a hit on the shell, or a victim inside a dome the attacker is not inside. No damage,
   no knockback, no flinch, 0 hitstop; outcome `FGSTN_BackpackDomeBlock` -> event `Backpack.Event.DomeBlocked` (applier). Attacker outcome
   `Backpack.Dome` -> Ricochet: a melee attacker recoils (move's ricochet response, else the combat default shove). Projectiles just stop.
5. `UGSTN_BackpackContentLibrary::AddDomeStepToRules`; `create_backpack_assets.py` ran (full -nullrhi editor; the plain commandlet cannot see
   plugin content): step in `GSTN_CombatRules_Standard`, `DA_Backpack_JetPack` / `_ShieldDome` created (`/Game/GAS/Backpacks`).
**Gate (laptop)**: build ok first try. Headless GSTN **1578/0**, 0 ensures (+8: `GSTNCombat.Core.Hits.ExtraOutsideOnly`,
`GSTNCombat.NP.Hits.Attacker.OutsideOnlyShell`, `GSTNCombat.NP.SnapshotExt.FourContributors`, `GSTNBackpack.NP.Dome.*` x5; `Saved/M11L7c/headless-b1.log`).
Suite `R31_ALL=1` 48/50, `di` and `hit_on_wall_hit` red, both PASS alone on rerun; 0 ensures (`Saved/R31/report-m11l7c-all.txt`, `-rerun.txt`).
They are NOT on the known-flaky list: watch them next run (the new step is a no-op without a dome, so likely timing).
**Decisions (recommended, flag)**: side = attacker's body; push = ricochet recoil for now; the dome blocks grabs too; inside->outside hits pass.
**Not built**: push DISTANCE per dome data (needs the outcome extension to hand its own `FGSTN_RicochetResponse` to the attacker tick: core
interface change); dome visual; nobody saw it in PIE; harness scenario `backpack_dome`; a carrier parked by Link keeps its shell at its own
transform (inside lists use the moved one).
**Manual steps**: artifact NOT updated (context limit): coordinator, in item `ed-m11-l7b-backpacks` drop step 1 (assets now exist) and "non blocca ancora i colpi"; owner to try the dome from a pickup in PIE.
Cost: ~140k context main thread + subagents ~410k (Opus impl, Sonnet suite), estimated ~$18-24. Wall clock ~1.5 h.
