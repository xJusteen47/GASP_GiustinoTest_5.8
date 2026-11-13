# M11 slice 16: L7b backpack move slots, memory, jetpack, dome state (laptop, 2026-11-13)
Branch `claude/laptop-m11-l7b` from `96d01f90` (L9), pushed as `claude/project-thread-vxsqrf`. Owner design 2026-11-12 (no backpack button).
**Done**:
1. Memory: `FGSTN_BackpackFragment` (GSTNBackpackCore, feature id `Backpack` = 21 in `GSTNFeatureIds`): 4 slots of charges + one dome, combat
   sync state (rollback/reconcile, 0 bytes when idle). Pure step `FGSTN_BackpackStep` (spend, refill Never/OnLanding/OverTime, dome lifetime).
   New module `GSTNBackpackNP`: tick extension (priority 140) runs it, projects `Backpack.State.Empty.Slot0..3` and `Backpack.State.Dome`.
2. Move slots = the definition's bindings, each with its own trigger. Moves spend through a `Backpack Charge` timeline track and gate on
   the Empty tag (IgnoreTags), like the air spin. `Backpack Dome` track raises the dome (centre = own snapshot pose, WhileHeld or Deploy).
3. Jetpack: glide in the core jump state (`FGSTN_JumpState::bGliding`, falling with the last jump's button held; `GlideGravityScale`,
   `GlideMaxFallSpeed` combat attributes, 0 = off) + `FGSTN_BackpackMovement` (extra air jumps, glide) applied by `UGSTN_GE_BackpackMovement`.
4. Starters as classes (data asset of the class = filled): `GSTN_BackpackDefinition_JetPack` (+1 air jump, glide 0.25, cap 300 cm/s),
   `_ShieldDome` (binding `Command.Backpack.Dome` = Press(layout dodge) + stick neutral, grounded, `UGSTN_GA_BackpackDome`: 400 cm, held,
   max 625 frames, 2 charges, +1 per 1250 frames). `Tools/GAS/create_backpack_assets.py` makes `/Game/GAS/Backpacks/DA_Backpack_*`.
**Gate (laptop)**: build ok (1 test fix, 1 missing module dep, 1 sliced TInstancedStruct::Make fixed). Headless GSTN **1569/0**, 0 ensures
(+7: `GSTNCombat.Core.Jump.Glide`, `GSTNBackpack.Core.{Memory.OverTime,Memory.LandingAndNever,Dome,FragmentWire}`, `GSTNBackpack.GAS.Starters`,
`GSTNBackpack.NP.DomeAbility`; `Saved/M11L7b/headless-b4.log`). Suite `R31_ALL=1` **49/50**, `jump_squat` failed on kit setup (client had no kit),
PASS alone on rerun (known flaky on 5.9 dedicated too); `Saved/R31/report-m11l7b.txt`, `report-m11l7b-squat.txt`.
**Nobody has seen in PIE**: no backpack asset exists yet, no glide, double jump or dome ran in a match. No harness scenario added: the suite
cannot undo a loadout, so a backpack scenario needs its own PIE run with a backpack loadout (next to `scenario_backpack_use`, L10).
**Decisions taken (recommended, flag to owner)**: glide in the core jump state, not in the plugin (reuses the gravity path; backpack off =
stats 0 = no glide); no glide brake (negative gravity flips Mover's up): a fall already faster keeps its speed; jetpack has no move slot
of its own (kit jump + stats); dome lifetime both ways as data, WhileHeld default; dome beats the dodge only with the stick neutral.
**Not built / gaps**: the dome **does not block hits** (next slice: extra hurtbox via the snapshot contributor + a pipeline step, owner to
say what it blocks: ranged only like Helldivers, or everything from outside); no dome visual; memory survives a backpack swap (not reset);
two moves sharing a slot must agree on its capacity (no validator).
**Owner answer 2026-11-13** (dome, next slice): block EVERY hit from outside and push back whoever hits it from outside; make sure a melee
hit cannot land both inside and outside the dome at once (one hit = one side). Jetpack numbers still at playtest.
**Manual steps**: artifact "Cose da fare a mano" item `ed-m11-l7b-backpacks` (create the two assets, put one on a hub pickup, try it).
Cost: ~150k context, Opus, estimated ~$9-12. Wall clock ~2 h (4 builds, 2 headless runs, suite).
