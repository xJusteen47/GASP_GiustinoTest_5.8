# Handover test-cubes-1: fixed cubes of different hardness to attack (2026-11-25)

Branch `claude/project-thread-k3aapg`, base `main` 152d25e6. Cloud thread, NOT compiled (desktop offline, owner away).
Owner request 2026-11-25: "Programma qualche superficie tipo cubi a penetrazione diverse. Fissi che posso attaccare."
Reading taken: "penetrazione diversa" = the Helldivers rule already in GSTNParts (hit penetration against part hardness: full, 65 %, ricochet).

## What
- `gstn.TestCubes.Spawn [0,1,3,6,10,15] [Distance=350] [Spacing=200]`, `gstn.TestCubes.Clear`, `gstn.TestCubes.Report` (not in Shipping).
- `UGSTN_TestCubesSubsystem` + `AGSTN_TestCubePuppet` in GSTNPartsMinion (`Debug/GSTN_TestCubes.{h,cpp}`): each cube is a minion of a transient
  type (one per hardness, reused) with one unbreakable part on one box hurtbox; fixed by data (speeds 0, body collision None, reaction capped
  to StaggerInPlace, KnockbackScale 0, health 1e6). Puppet = engine cube coloured by hardness + text label (hardness, band of the last hit, damage).
- Bridge observer `UGSTN_PartsMinionSubsystem::OnPartOutcomeApplied` (multicast, raised at the end of ApplyOutcome). Inert when unbound.
- Reuse: minion store and hit flow (GSTNMinion), part math and PartResolveMinion (GSTNParts/GSTNPartsMinion), spawn pattern of `gstn.SpinScrew.Spawn`.
  No new plugin, no .uproject change (GSTNPartsMinion, GSTNParts, GSTNMinion already enabled).

## Desktop steps (one thread, _ue59 lock)
1. Editor closed. Merge `claude/project-thread-k3aapg` into the day's integration branch.
2. `Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log` (new UCLASSes: full build).
3. PIE standalone on the playtest map, console `gstn.TestCubes.Spawn`. Expected: 6 cubes in a row, green to red, label "Durezza N".
4. Hit each one: a move with penetration 0 does 65 % on hardness 0 and "RIMBALZO" on the others (log `TestCubes hit ...`). Set a hitbox's
   Part Hit penetration in the timeline editor to see the full band.
Max 2 fix attempts (rule 2). Not checked: compile, that the puppet draws on a listen host, label orientation, that a cube never slides.

## Not done / to confirm
- Remote clients do not draw the cubes (transient types). Fine for standalone/host playtest.
- The owner's moves probably all have penetration 0 (default): to confirm in the editor; the per-move values are a design choice of the owner.

ESITO: PARZIALE codice scritto, non compilato ne provato (macchine offline)
SUBAGENTI: 0
COSTO: ~4 $ | 0 min desktop | 0
