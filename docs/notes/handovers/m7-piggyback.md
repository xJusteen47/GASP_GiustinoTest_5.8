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

## M7-02 (laptop, 2026-11-02): generic GSTNLink commit, relationship filter on interact offers (option B)

- Commits on feature/gas-npp-5.8: 4031ea5 "GSTNLink: relationship filter on interact offers", 6301009 "M7: the mount offer applies to allies
  only", then this docs commit. Deltas: docs/notes/m7-doc-deltas.md (G5, GSTNLink changes, option B built).
- `FGSTN_LinkInteractOfferDef::Relationships` (empty = every relationship: existing offers unchanged; `AppliesTo`); `FGSTN_LinkInteractOffer`
  carries it; `FGSTN_LinkInteractProvider::FindAcceptedOffer` computes the candidate's relationship with `UGSTN_RelationshipResolver::DefaultRule`
  on the two teams and skips offers for other relationships before their rules (refusal `Team`); the code-registered grab offer lists Enemy and
  Neutral; alternatives keep the offer's relationships; the mount lists Ally. Not from the match rules: the probe has no rules asset (a custom
  resolver of the match is not consulted). The grab/latch team rule of d1ecd6a stays (belt and braces, and it covers the hit path).
- Test: `GSTNLink.GAS.Interact.RelationshipFilter`. Counts (`Saved/m7-g5`): `headless-m702-on.log` 1233/0, `headless-m702-off.log` 1218/0, no
  ensure; builds `build-m702-on.out`, `build-m702-off.out` "Result: Succeeded". Harness 0 %: `mount-listen-0pct-m702.log` 5/5;
  `link-listen-0pct-m702.log` 18/19 (`latch_big_enemy_client_latches_client`: one unforced reconcile during the walk), rerun
  `link-listen-0pct-m702-rerun.log`: client_latches_client PASS, host_latches_client FAIL ("at most one correction per hit in the attack window":
  5 counter steps).
- OPEN for the lead and the spin thread: `latch_big_enemy_host_latches_client` fails that attack-window check in 3 of 5 runs since the rebase
  onto the spin work (2c3ff00), with or without this commit, and passed in every run before it (G2, G5 pre-rebase). The latched enemy is linked,
  so the probe never runs for it; suspect the merged D6/D7 combat seams (re-hit interval). GSTNLink is now frozen for D9/D10.

## M7-03 (laptop, 2026-11-02): G3 partner credit; G6 item 1 seam designed, not built

- Commit on feature/gas-npp-5.8: 1ce94f2 "M7 G3: partner credit on hits" (GSTNCombatCore + GSTNCombatNP), then this docs commit. Deltas:
  docs/notes/m7-doc-deltas.md sections "G3" and "G6 item 1". GSTNCombat has no open change now.
- G3: `FGSTN_HitCredit { PartnerEntityId, PartnerShare 0..255 }` in `FGSTN_HitOutcome::Credit` / `FGSTN_StepResult::Credit` (later set
  wins); MeterGain step splits (attacker part as before, partner part as `EGSTN_HitRequestTarget::Partner`); credited HitLanded / Health KO
  add a partner copy tagged `Combat.Event.Hit.CreditPartner` (`FindCreditPartnerActor`). Inbox entry and wire unchanged; no core step
  writes the credit (G7's mount hit step will, from the attacker's link extension). Nobody applies Partner meter requests yet (G7 OPEN).
- Tests `GSTNCombat.Hits.Credit.{EmptyByDefault, MeterSplit, EventCarriesPartner}`.
- Gates (laptop, `Saved/M703`, script `build.ps1`): on = local .uproject + GSTNSpinDrill + GSTNPiggyback, `build-on2.out` Succeeded,
  `headless-on2.log` 968/0 (950 + 15 piggyback + 3); off = local .uproject (Defence, Link on), `build-off1.out` Succeeded,
  `headless-off1.log` 926/0; no ensure; .uproject restored (hash of Saved/D4/uproject.local.bak); check_modules.py 0 problems. Attempt 1 failed
  one new test (it set the outcome's damage, the event reads the entry's), fixed. Harness NOT run: no step sets a credit, so no simulated
  behaviour changes (SUITE_MOUNT/SUITE_LINK to run with G6/G7).
- G6 item 1 (predicted rider exits, owner decision S1): design in the deltas, confirmed by the owner (2026-11-02) to also carry the drill's
  per-latch anchor. Two commits for the next thread: (1) GSTNCombatNP `IGSTN_CombatTickServices::GetViewSnapshot()` = lazy
  `ResolveAttackerSnapshot` for tick extensions; (2) GSTNLink `bInitiatorPlacesExit` (rider places its exit in its link tick from the view,
  watchdog copies it into the Break) + `FGSTN_LinkState` anchor override (flag `AnchorOverride`, int16 cm x3, fragment version bump) for
  D9 part 2. Then harness `dismount_predicted`. I own the GSTNLink seams until that lands: tell the spin thread before editing.
- Owner decisions (2026-11-02): GSTNPiggyback stays OFF in the committed .uproject; LAST STEP OF G6: enable GSTNPiggyback together with
  GSTNLink, GSTNDefence and GSTNSpinDrill in the .uproject.
- OPEN: (1) who applies Partner meter requests (G7); (2) the M7-01 OPEN items 2-6 unchanged; (3) latch margin flake as per SD-02.
- Cost: one Opus thread, about 130k context, two builds pairs (~25 min UBT + tests): medium.
- Next: M7-04 = G6 item 1 (the two commits above), new thread.

## M7-04 (laptop, 2026-11-02): G6 item 1 built (S1 seam + predicted rider exits), plugins enabled

- Commits on feature/gas-npp-5.8 (rebased onto 988a6cd, anim pipeline only): 93e9e15 "GSTNCombatNP: the entity's lag-compensated view for
  tick extensions", ad6665c "GSTNLink: initiator-placed exits and a per-link parked anchor", 53319fb "M7 G6: predicted rider exits",
  c2aab63 "Enable GSTNDefence, GSTNLink, GSTNSpinDrill and GSTNPiggyback" (owner decision 1), then this docs commit. Deltas: m7-doc-deltas.md
  "G6 item 1 / Built in M7-04" (design vs built table).
- GSTNCombatNP: `IGSTN_CombatTickServices::GetViewSnapshot()` (virtual, default null = use Context.Snapshot), built once per tick and shared
  with the attacker step (`ResolveViewOnce`). `FGSTN_SnapshotExtensionBlob::MaxBytes` 32 -> 40 (link extension now 38 B with the anchor).
- GSTNLink: `bInitiatorPlacesExit`; link tick `StepInitiatorExit` (PendingBreak edge -> exit from the view, flag `GSTNLinkExitFlags::InitiatorPlaced`,
  due F+1) and `StepOwnExitBreak` (at F+1 the initiator ends the link via the same `ApplyCommand`); watchdog copies the exit if the world is free
  (else own placement), and breaks the staying side at once if the initiator ended a link the watchdog missed. `ShouldReconcile` tolerance
  `UGSTN_LinkSettings::PredictedPlacementToleranceCm` (5 cm, yaw 5 deg). Anchor seam for D9 part 2: `FGSTN_LinkState::SetAnchorOverride`
  (flag `AnchorOverride`, int16 cm x3 in partner socket space, on the wire only with the flag; snapshot, leash, presentation use it). Nothing
  writes it yet. GSTNLink seams are free again for the spin thread.
- Piggyback: `bPredictRiderExits` default true -> `bInitiatorPlacesExit`. Mount leash removed (LeashCm 0): GSTNLink's leash is measured from the
  parked capsule, which never moves, so a walking carrier broke the mount after 3 m (found by the new scenario).
- Tests: `GSTNLink.NP.InitiatorExit.{PlacedFromView, OwnBreakEqualsCommand, BlockedPlaceFallsBack, OnlyWhenAsked, MissedFrameBreaksPartner,
  ReconcileTolerance}`, `GSTNLink.NP.Anchor.Override`; piggyback Bindings test now checks the predicted default.
- Gates (`Saved/M704`, scripts `build.ps1`, `run_harness.ps1`, driver `harness_driver.py`): on (local + SpinDrill + Piggyback = the committed set)
  `headless-on1.log` 975/0, off (Defence+Link only) `headless-off1.log` 933/0, no ensure, both builds Succeeded; check_modules 0 problems.
  Harness 0 % (60/15 ms, listen + 2 clients), `Saved/Logs/m7-04/`: SUITE_LINK 19/19 (`link-listen-0pct-a.log`, latch flake did not show);
  SUITE_MOUNT 5/6 then, after the leash fix, 6/6 (`mount-listen-0pct-b.log`). dismount_predicted: carrier moved 396 cm in 0.9 s, rider client
  rollbacks 0, client-server 0.27 cm, carrier client 1 correction. Headless not rerun after the leash constant (piggyback only, no test reads it).
- OPEN: (1) the parked capsule stays where it mounted (an invisible blocker for third pawns? not checked); a carrier teleport check
  (`MaxCarrierTeleportCm`) is not built. (2) A client re-simulation re-places the exit from the current proxies (within tolerance so far).
  (3) M7-01 OPEN 2-4, 6 and M7-03 OPEN 1 (Partner meter requests, G7) unchanged. (4) The Saved/D4 uproject backup is stale now that the
  committed .uproject lists the plugins; build scripts that compare against it will abort.
- Cost: one Opus thread, ~160k context, 3 UBT builds + 2 headless runs + 2 harness runs (~70 min machine): medium-high.
- Next: G6 remaining items (or G7 mount hit step) in a new thread; spin D9 part 2 can use the anchor seam.
