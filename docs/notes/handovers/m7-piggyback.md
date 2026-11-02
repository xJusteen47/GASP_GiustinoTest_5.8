# M7 piggyback handovers (per-thread file, from 2026-11-02)

Earlier entries: HANDOVER 67-69, 76 and 77 in `docs/notes/takeover-state.md`. Numbering here: M7-01, M7-02...

## M7-01 (laptop, 2026-11-02): G5, mount, dismount and drop (GSTNPiggybackRuntime part 1) + spike S3

- Branch feature/gas-npp-5.8, pushed from the laptop worktree `GASP_Giustino2_m7` (local branch claude/project-thread-ks98dh), rebased onto
  2c3ff00 (spin D4-D11 + SD-01): no conflict, `GSTN_CombatTick_Attacker.cpp` holds both D6/D7 and the seam. Commits:
  a3307e8 "GSTNCombat: per-relationship hittable override on hitbox variants" (seam, lead OK, option A),
  65f784d "GSTNLink test: ReasonTableAppend relative to reasons other plugins registered" (lead OK; the drill needs it too),
  d1ecd6a "GSTNLink test types: an enemies-only team rule on the grab and the latch" (lead OK, content via setup_defence_link_content.py),
  a946354 "M7 G5: mount, dismount and drop", then this docs commit. Deltas: docs/notes/m7-doc-deltas.md section G5; S3:
  docs/notes/m7-spike-report.md section S3.
- Seam: `FGSTN_HitVariant::bForceHittable` lets one hitbox hit a relationship the match rules refuse (the mount on an ally); the attacker step
  skips the rules' gate for it, the inbox entry carries it in bit 0x80 of the relationship byte (`GSTNHitRelationship::ForcedBit`, `IsForced`;
  `FromIndex` masks it) to `FGSTN_ResolvedHit::bRelationshipForced`, read by TeamFilter. Off by default, existing entries byte-identical.
- Piggyback: the mount type made by code (`FGSTN_MountLinkType`, registered at PostEngineInit; rider = role 1 parked on the carrier's root +
  (-20, 0, 95), acts, Release reason Dismount; carrier = role 2, acts, Release reason Drop; `bInitiatorPredicts`, PendingFrames 2; rules Team
  (AlliesOnly) + `UGSTN_LinkRule_Mount`; hit breaks from `FGSTN_MountRules`; leash 300 cm; `UGSTN_Placer_Mount` BehindCarrier 90 cm; offer
  `Interact.Target.Mount` priority 50, reach 150). The rule runs `EvaluateEligibility` on the request context (kits' `CarryCapacity` / `Weight`,
  links, tags, link cooldown) and writes the carry ratio as Payload. `UGSTN_GA_Mount` (one-frame ally-only hitbox, one target), `_Dismount`,
  `_Drop` (Release keys), Blueprintable. `FGSTN_MountLoadout`: bindings Mount (slot -1), Dismount (101), Drop (102, Crouch held);
  DisallowedCommands -> the carrier role ignored. Test kits `/GSTNPiggyback/Test/KIT_Test_Mount` (100/150) and `KIT_Test_MountHeavy` (200),
  copies of KIT_Test_Link; the plugin's Content is versioned now (.gitignore exception). Exits are SERVER-GIVEN; `bPredictRiderExits` (false) is
  the switch for the owner's predicted rider exit.
- Tests added: `GSTNCombat.NP.Hits.Attacker.ForceHittableVariant`, `GSTNCombat.NP.Hits.TeamFilter.ForcedRelationship`;
  `GSTNPiggyback.Runtime.{Rule.AcceptsAllyWithMountData, Rule.EveryRefusal, Rule.TypeRegisteredAsSpecified, Rule.MountHitboxTargetsAlliesOnly,
  Bindings.RiderHasNoGroundContext, Bindings.DisallowedCommands, Break.EveryReasonClearsStates, Disabled.UnboundCommandIsLegal}`.
- Counts (laptop worktree, `Saved/m7-g5`, `Select-String 'Test Completed. Result=\{Success\}'` / `\{Fail`), after the rebase onto 2c3ff00:
  `headless-final-on.log` 1232/0 (15 GSTNPiggyback), `headless-final-off.log` 1217/0 (the log never names the plugin); no ensure. Before the
  rebase: on 1229/0, off 1214/0 (= 1212 + 2 seam tests), GSTNLink off `headless-g5-linkoff.log` 1109/0 (= 1107 + 2). Builds
  `build-final-on.out`, `build-final-off.out`, `build-g5-linkoff.out`: "Result: Succeeded". Variants as G4 (`uproject.pb` = off + GSTNPiggyback);
  the committed .uproject restored byte-identical (SHA256 9C4F29DE...) and does not list the plugin. Grep gate `Tools/Modules/check_modules.py`:
  0 problems on the committed tree.
- Harness (listen host + 2 clients, one editor process, 60/15 ms; reports in `Saved/Logs/m7-g5/`, editor logs in `Saved/m7-g5/editor-*.log`):
  SUITE_MOUNT (`mount_basic`, `mount_refused`, `mount_heavy_refused`, `dismount_drop`, `mount_interact_priority`) 5/5 at 0 % after the rebase
  (`mount-listen-0pct-final.log`) and 5/5 at 2 % before it (`mount-listen-2pct.log`); two earlier 0 % runs had a flaky check each (fixed: the
  third pawn now leaves every reach, the heavy check asserts no optimistic attach). SUITE_LINK at 0 %: 19/19 before the rebase
  (`link-listen-0pct.log`, with the grab team rule), after the rebase 18/19 (`link-listen-0pct-final.log`): `latch_big_enemy_host_latches_client` failed one check (client 1, "at most one correction per hit in the attack window": 5 counter steps, limit 2 per hit); rerun twice (`link-listen-0pct-final-rerun.log`): FAIL (same check, 5) then PASS. It passed in every pre-rebase run with the G5 code, and touches no mount path: OPEN for the lead/spin thread (flaky, or the merged D6/D7 combat seams).
- S3: accepted mount 0 rollbacks on the rider's client, 1 correction on the carrier's; an unseen refusal costs 3 rollbacks and a 117 cm snap for
  3-4 ticks (not 1 as G1 assumed: the rider's client re-predicts against its stale copy of the carrier until it catches up); a refusal by data
  (heavy kit) makes no request; the server-given dismount costs 3-4 rollbacks and ends 0.0 cm from the authority. Same at 0 % and 2 %.
- How to run the harness without the editor MCP: an editor process with `-nullrhi -unattended -ExecCmds="py <driver>"` whose driver arms a
  slate tick, runs `g.setup_editor(True, 'listen', 2)`, the module's `prepare_editor()`, `defence_batch(...)` and quits; the worktree's
  `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` needs `PlayNetMode=PIE_ListenServer`, `PlayNumberOfClients=3` for the run
  (patch, then restore; Standalone/1 makes every scenario time out).
- OPEN: (1) predicted rider exits with the server rewinding the carrier (owner decision 2026-11-01): not built, needs a GSTNLink seam; first
  G6 item. (2) An unseen refusal costs 3 rollbacks (a refusal memory on the rider, or no attach until the carrier's copy is newer than the
  request). (3) No remount cooldown. (4) Per-carrier data the request context and exit query lack (socket, exit mode, combat attributes, the
  match's `FGSTN_MountRules`). (5) The committed .uproject does not list GSTNPiggyback (owner's call). (6) The G0-G4 "exactly one rollback"
  wording of `scenario_mount_refused` is now a recorded count bounded 1-4. (7) The intermittent latch check above.
- Next: M7-02 (generic GSTNLink commit: a relationship filter in the interact probe), then G6 after the predicted rider exit decision.
