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

## M7-05 (desktop, 2026-11-03): G7 hits while mounted (step + tests); harness and rules asset not run

- Worktree `GASP_Giustino2_m7` (desktop), branch claude/project-thread-7autp9 from feature 60da6fa. Commit c254dca "M7 G7: hits while
  mounted", then this docs commit. NOT pushed to feature (needs harness + owner's "ok merge"). Deltas: m7-doc-deltas.md "G7".
- Note: G6 is NOT complete. Carry load (`UGSTN_GE_CarryLoad` + MMC, `scenario_mount_carry`) is not built; only G6 item 1 was.
- `UGSTN_HitStep_MountHit` (Runtime): pure `Evaluate(FGSTN_MountHitInput, FGSTN_MountRules)` + `Resolve`. Rider below threshold: knockback x
  RiderKnockbackScale (0 = StaggerInPlace), hitstun x RiderHitstunScale; above: break RiderHit. Carrier above: break CarrierHit. Partner hits
  dropped unless LinkedPartnerOnly. Front cover arc (no redirect). Credit per DefaultCredit. `AddMountStepToRules` / `RemoveMountStepFromRules`.
- Tests `GSTNPiggyback.Hits.MountHit.{RiderBelowThreshold, RiderAboveThreshold, CarrierThreshold, Cover, PartnerExclusion, Credit, AddToRules}`.
- Gates (`Saved/M705`, scripts `build.ps1`, `headless.ps1`, `diagnosis.txt`): on (committed .uproject) `build-on1.out` Succeeded,
  `headless-on1.log` 1282/0, 0 ensure; off (GSTNPiggyback false) `build-off1.out` Succeeded, `headless-off1.log` 1260/0 (= on minus 22
  piggyback tests); .uproject restored from `Saved/M705/uproject.bak`. Binaries in the worktree are the OFF build: rebuild on first.
- NOT done (next thread, needs the editor on the m7 worktree): (1) run `setup_defence_link_content.py` so `GSTN_CombatRules_Standard` gets
  MountHit after ComputeKnockback, commit the asset; until then the step is not in any pipeline. (2) Harness `scenario_mounted_hit`,
  `scenario_rider_attack` (mount_scenarios.py; needs a third-pawn attack above 1800 cm/s for the break case, check the test kit's moves),
  SUITE_MOUNT and SUITE_LINK at 0 % and 2 %. (3) Then the push to feature with the owner's "ok merge".
- OPEN: bCarrierShieldCoversRider unread; carrier-break drop velocity (G9); Partner meter requests (M7-03 OPEN 1); G6 carry load; teleport
  break check (M7-04 OPEN 1) unchanged.
- Cost: one Opus thread, ~110k context, 3 UBT builds (base, on, off) + 2 headless runs (~45 min machine): medium.
- Next: M7-06 = rules asset + G7 harness, then G6 carry load (or G8 pair-move data, pure data, S), new thread.

## M7-06 (desktop, 2026-11-03): G7 closed in the pipeline and on the network (mounted_hit); rider attack waits for a Rider binding

- Worktree `GASP_Giustino2_m7`, branch claude/project-thread-7autp9. NOT pushed to feature (needs the owner's "ok merge").
- Rules asset: `setup_defence_link_content.py` run headless; `GSTN_CombatRules_Standard` now ... ComputeKnockback, **MountHit**, ApplyDamage,
  Hitstop, MeterGain, LinkBreakOnHit (once). Committed with this entry.
- `mount_scenarios.py`: `scenario_mounted_hit` (third pawn hits the rider from behind: below threshold the hit lands on the rider's server inbox,
  mount held on every world, rider parked; then the rider threshold lowered IN MEMORY on the code-made `DA_LinkType_Mount` (`set_mount_break`,
  the test move is far below 3600 cm/s) and the same hit breaks it once, plugin reason index 20). Helpers `_place_at`, `_along`, `_inbox_since`.
- `scenario_rider_attack` written, NOT in SUITE_MOUNT: the rider makes no hit (empty hit log) because riding drops Grounded and no kit has the
  spec 9.2 `Command.Attack.*` + `Role.Rider` binding (GSTN_MountLoadout adds only Mount/Dismount/Drop). Needs that binding (content or loadout
  extension): owner decision, not built.
- Harness (driver `Saved/M706/driver.py` + `run.ps1`: editor -nullrhi, map DefaultLevel_Harness, ini patched to listen/3 and restored),
  60/15 ms, `Saved/Logs/m7-06/`: SUITE_MOUNT 7/7 at 2 %, 5/7 at 0 %; SUITE_LINK 18/19 at 0 % and at 2 %. The 4 failures (mount_refused 5
  rollbacks vs <=4, dismount_predicted / latch_big_enemy_client_latches_host / link_break_distance_client_grabs_host with a missing-pawn
  exception) all passed on one rerun (`*-rerun.log`): flakes, different each run. mounted_hit at 0 %: rider client 2 rollbacks for the hit,
  13 for the break.
- Headless (`Saved/M706`): on `headless-on1.log` 1282/0; off (GSTNPiggyback false) `build-off1.out` Succeeded, `headless-off1.log` 1260/0; 0 ensure.
  .uproject restored; binaries in the worktree are the OFF build again: rebuild on first.
- Launcher lesson: `-ExecCmds` takes no quoted path with spaces and `;` does not separate a `py` command: copy the script to TEMP with forward
  slashes and let it quit by itself (two failed launches before that).
- OPEN: rider attack binding (above); G6 carry load not built; bCarrierShieldCoversRider unread; mount_refused can cost 5 rollbacks at 0 %;
  M7-04 OPEN 1 (teleport break) and M7-03 OPEN 1 unchanged.
- Cost: one Opus thread, ~95k context, 2 UBT builds + 3 headless editors + 3 harness editors (~1.5 h machine): medium.
- Next: owner "ok merge" for G7; then G8 pair-move data or the rider attack binding (owner's pick), new thread.

## M7-07 (desktop, 2026-11-03): G8 pair-move data; G7 feature push blocked by the permission classifier

- Worktree `GASP_Giustino2_m7`, branch claude/project-thread-7autp9. NOT on feature: the owner delegated the G7 merge to the coordinator ("gestisci tu",
  15:24Z), the FF push `8b3922d -> feature/gas-npp-5.8` (FF from 60da6fa, checked) was DENIED by the desktop's auto-mode classifier: owner runs it or allows it.
- Built (GSTNPiggybackRuntime): `UGSTN_PairMoveDefinition` (PrimaryDataAsset `GSTN_PairMove`, spec 4 fields + authored `PairMoveId`, `ExitMode`/
  `ExitDistanceCm`, `Validate`/`ValidateWithTimelines`/`IsDataValid`, `MakeLinkActionInfo`), `UGSTN_PairMoveSet` (IsDataValid: id clashes),
  `UGSTN_GA_PairMove` (abstract, `PairMove` back-reference checked), `UGSTN_PairMoveRegistry` (IGSTN_LinkActionProvider of `Combat.Link.Type.Mount`,
  module-owned, registered at post-engine-init after the mount type; ordered by id, same-id clash refused, one `UGSTN_Placer_Mount` per definition).
  Core: `FGSTN_MountKitExtension::PairMoves` (soft path to the set); the mount loadout extension adds the kit's set to the registry at compile.
- Deviations (m7-doc-deltas.md "G8"): authored wire id instead of compiler `PairIdx` (G2 provider is per type, the key names the id); no timeline
  overrides; INDEX_NONE outcome = none (Break needs a frame); offset counted from the PartnerMove key; Interruptible forces no command lock.
- Tests `GSTNPiggyback.Runtime.PairMove.*` (3): validation cases of spec 9 (missing abilities, empty partner, no/other key, offset past the
  initiator incl. offset 0 = 1, OutcomeFrame outside/before the key, Break without frame, SwapRoles, roles, id range), registry order from two add
  orders and from a set, idempotence, clash, resolve (offset clamp, placer, reason, lock).
- Gates (`Saved/M707`, diagnosis.txt): build on Succeeded; `headless-on1.log` 1283/2 (test-only bug: `TInstancedStruct::Make(Track)` slices to the
  base: use `Make<FGSTN_LinkOutcomeTrack>`), fixed, `headless-on2.log` 1285/0; off (GSTNPiggyback false) `build-off1.out` Succeeded,
  `headless-off1.log` 1260/0 (= M7-06); 0 ensure. Harness not run: G8 touches no NP/simulation code and the plan names no scenario for G8.
  .uproject restored (= HEAD); binaries in the worktree are the OFF build: rebuild on first.
- Deferred by the owner (no abilities designed yet, 15:24Z): rider mounted-attack binding (spec 9.2). Still OPEN: G6 carry load, carrier shield
  over rider, partner requirements on provider-only tags (FGSTN_LinkRequestTags, matters for G9 content), M7-04/M7-03 OPEN items.
- G9 not started (size M + harness + S4; budget): next thread. It needs the PM_ThrowRider / PM_BoostJump content (definitions + abilities with a
  PartnerMove key + the kit set), `scenario_throw_rider`/`scenario_boost_jump`, LaunchLinked for the exit velocity.
- Cost: one Opus thread, ~100k context, 3 UBT builds + 3 headless editors (~45 min machine): low-medium.

## M7-08 (desktop, 2026-11-03): G9 part A, the exit launch of a pair move

- Worktree `GASP_Giustino2_m7`, branch claude/project-thread-7autp9, commit aed53de on 2062e15 (G8). Not on feature (desktop permission, owner).
- Found: GSTNLink G2 already plays pair moves generically (PartnerMove key, partner inbox, Outcome Break with the action's placer), and the exit
  already carries a velocity to the parked mode. Missing for the reference moves was only the exit velocity: the mount placer returned zero.
- Decision (rule 6): G9 split. Part A here, part B next thread (content + scenarios + spike 4). Deltas: m7-doc-deltas.md "G9 part A".
- Built: Core `FGSTN_PairExitLaunch` (Speed, PitchDeg, UpSpeed, InheritPartnerVelocity, bInheritHorizontalOnly) + `UGSTN_MountUtils::ComputeExitLaunch`;
  `UGSTN_Placer_Mount::Launch` (candidate and fallback), `UGSTN_PairMoveDefinition::ExitLaunch` copied by the registry. GSTNLink (additive
  fields only, tell the spin-drill thread): `FGSTN_ExitQuery::PartnerVelocity`, `FGSTN_LinkWatchdogEntity::Velocity` (pawn velocity on the
  authority; snapshot `Velocity` in `StepInitiatorExit`). Zero launch = every existing exit byte-identical.
- Test `GSTNPiggyback.Runtime.PairMove.ExitLaunch`: zero, throw (flatten + pitch + full inherit), aim along up, boost (horizontal inherit,
  tilted up), placer, watchdog `PlaceExit` aims along the carrier and inherits, registry copies the definition's launch.
- Gates (`Saved/M708`, diagnosis.txt): build on Succeeded, `headless-on1.log` 1286/0 (+1 vs M7-07); off `build-off1.out` Succeeded,
  `headless-off1.log` 1260/0 (= M7-07); 0 ensure. Harness NOT run: no existing content has a non-zero launch, so no scenario can change;
  the throw/boost scenarios belong to part B. .uproject restored (= HEAD); worktree binaries are the OFF build: rebuild on first.
- Part B to do: PM_ThrowRider / PM_BoostJump (4 test abilities with timelines: carrier throw 12+2+18 f with PartnerMove key, rider thrown
  with sweeping hitbox + ThrownActionableFrame, rider boost 6 f, carrier braced with Lock.Movement 10 f), the 2 definitions with ExitLaunch,
  the test kit's PairMoveSet, Command bindings for the initiators (test-only, not designed abilities), RefundAirJumps, scenario_throw_rider /
  scenario_boost_jump in mount_scenarios.py, spike 4 (partner delay, 150 ms threshold) into m7-spike-report.md.
- OPEN unchanged: rider attack binding (owner deferred), G6 carry load, carrier shield over rider, provider-only partner tags, M7-04/M7-03.
- Cost: one Opus thread, ~70k context, ~45 calls, 2 UBT builds + 2 headless editors (~30 min machine): low.

## M7-09 (desktop, 2026-11-03): G9 part B, the reference pair moves as test content; spike 4

- Worktree `GASP_Giustino2_m7`, branch claude/project-thread-7autp9, on abab24b. Not on feature (owner pushes).
- Built (deltas: m7-doc-deltas.md "G9 part B"): TEST abilities `UGSTN_GA_Test_CarrierThrowRider` / `_RiderThrown` / `_RiderBoostJump` /
  `_CarrierBraced` (`GSTN_PairMoveTestContent.h`, constructor timelines, not designed abilities); `UGSTN_PairMoveDefinition::TriggerCommand`
  (optional): `FGSTN_MountLoadout::ContributePairMoves` binds the initiator with its role and makes each partner a triggered ability (the
  partner's registry needs it); content helpers `FillReferencePairMove`, `WritePairMoveSet`, `WriteMountKitPairMoves`; `WriteMountKitData`
  keeps the rest of the entry. Assets `PM_ThrowRider`, `PM_BoostJump`, `PMS_Test_Mount` in the plugin's Content/Test, both test kits name the set
  (made by `mount_scenarios.prepare_editor()`). Throw on `Command.Special.Neutral` (no Directional slot in the test kits), boost on Jump.
- Test `GSTNPiggyback.Runtime.PairMove.ReferenceContent`. Scenarios `throw_rider`, `boost_jump` (in SUITE_MOUNT) with the spike 4 probe.
- Gates (`Saved/M709`, diagnosis.txt): on build Succeeded, `headless-on1.log` 1287/0; off `build-off1.out` Succeeded, `headless-off1.log`
  1260/0; 0 ensures. .uproject restored (= HEAD); worktree binaries are the OFF build: rebuild on first.
- Harness (driver `Saved/M709/driver.py`, `run.ps1`; the PIE ini must be listen/3, patched and restored: attempt 1 failed on it), 60/15 ms,
  `Saved/Logs/m7-09/*-2.log`: SUITE_MOUNT at 0 %: 6/9 pass. mount_interact_priority = the known missing-pawn exception flake (M7-06).
  throw_rider: everything passes except "flew >= 250 cm", which is checked 0.65 s after the tap while the release lands about 0.5 s after it
  (235 / 194 cm): the check is too early, so move it to t+1.4 (not rerun: 2 attempts used). Rider client = server after the flight (0.0 cm).
  boost_jump: the link breaks everywhere and the rider rises 604-617 cm, but the "braced" probe never saw `Combat.Lock.Movement` through
  `has_tag` (ASC): it probably isn't an ASC tag (unverified), so the probe needs fixing. Rider client rollbacks 2-4 (predicted, expected 0):
  OPEN, check which correction.
- Spike 4 (m7-spike-report.md S4): about 280 ms (throw, server-given release, both screens together) and about 300 ms (boost, rider to
  carrier screen), at 0 and 2 %. Over the 150 ms gate: by spec 2.4, a forward-prediction milestone for pair partners. Owner/coordinator decision.
- OPEN: RefundAirJumps not built; thrown land-cancel not built; the 3 probe/timing fixes above; rider attack binding (deferred), G6 carry load,
  carrier shield over rider, provider-only partner tags, M7-04/M7-03.
- Cost: one Opus thread, about 110k context, about 75 calls, 2 UBT builds, 2 headless test editors, 2 harness editors (about 1 h of machine time): medium.

## M7-10 (desktop, 2026-11-03): G9 closed, the two probes fixed

- Worktree `GASP_Giustino2_m7`, branch claude/project-thread-7autp9. Not on feature (owner pushes). Test/tool changes only, no gameplay change.
- throw_rider: the flight check moved to t+1.4 s (release lands ~0.5 s after the tap). Passes.
- boost_jump: finding, not only a probe bug: `FGSTN_TimelineTrack_Tags` has no consumer in GSTNCombatNP (only `UGSTN_TimelinePlayer` fills
  `TagWindows`), so `Combat.Lock.Movement` of `UGSTN_GA_Test_CarrierBraced` never reaches the ASC: the carrier is not really rooted. The
  probe now reads the carrier's `combat.inTimeline` (snapshot). Server and rider client see the brace; the carrier's OWN client never does
  (it gets the forced partner move by correction): the gate is the server, the carrier client is a note and the spike 4 braced measure has `gate=False`.
- Also fixed: the ON build failed in unity mode (anonymous `SetFrames` in both GSTN_MountAbilities.cpp and GSTN_PairMoveTestContent.cpp): renamed to `SetPairMoveFrames`.
- Gates (`Saved/M709`, `Saved/Logs/m7-10`): harness SUITE_MOUNT 0 %, listen/3: 8/9 (boost_jump the only one), boost_jump rerun PASS = 9/9,
  mount_interact_priority passed this time. Headless on `headless-on2.log` 1287/0, off `headless-off2.log` 1260/0, 0 ensures. .uproject = HEAD; worktree binaries are the OFF build.
- Still open: the Tags-track consumer (carrier root / command locks from a timeline: decide with the combat owner), forward prediction for pair partners
  (spike 4 ~280-300 ms, pending owner), rider client rollbacks 2 on boost (carrier client 9), RefundAirJumps, thrown land-cancel, rider attack binding, G6 carry load.
- Cost: one Sonnet thread, ~65k context, ~45 calls, 3 UBT builds, 3 harness editors, 2 headless editors: medium-low.

## M7-11 (desktop, 2026-11-03): the Tags track's consumer; G10-G12 not started

- Worktree `GASP_Giustino2_m7`, branch claude/project-thread-7autp9. Not on feature (owner pushes). Deltas: m7-doc-deltas.md "M7-11".
- Built: `UGSTN_CombatSimComponent::ProjectTimelineTags` / `CollectTimelineTags` (GSTNCombatNP, generic): at the end of the combat tick the
  tags of the open `FGSTN_TimelineTrack_Tags` windows go to the ASC as loose tags, added/removed as the difference between the input state
  (F-1) and the output state (F). No member cache: the fork's ASC tag counts are in its rolled-back sync state and it ticks after combat.
  Difference, not Set, so effect-granted `Combat.Lock.Movement` survives.
- boost_jump probe: 'braced' reads `Combat.Lock.Movement` on the carrier's ASC again, new check "the brace lock is gone on every world".
  Result: server (and the rider's client) see the lock, gone ~170 ms later, no leak; carrier's own client still never sees it (correction).
- Gates (`Saved/M711`, diagnosis.txt; reports `Saved/Logs/m7-11`): build ON Succeeded, `headless-on1.log` 1287/0; OFF (`GSTNPiggyback` off)
  Succeeded, `headless-off1.log` 1260/0; 0 ensures. Harness listen/3, 60/15 ms: boost_jump alone 10/10; SUITE_MOUNT 0 % 9/9; throw_rider +
  boost_jump 2 % 2/2; SUITE_LINK 0 % 15/19, the 4 failures were the known missing-pawn exception flake, rerun 4/4 (= 19/19).
  Not run: SUITE_DEFENCE (driver has no defence module), GSTNLink-off config, dedicated server.
- Boost rollbacks now: rider client 2, carrier client 4 (was 9).
- .uproject = HEAD, PIE ini restored; worktree binaries are the OFF build: rebuild ON first.
- Owner (2026-11-03): option (a) confirmed; forward prediction for pair partners = a separate milestone after M8 ("fix prediction once and for all").
- NOT started: G10 (presentation/anim/camera, M), G11 (GSTNPiggybackEditor is an empty module: factory, validators,
  `gstn.Combat.Debug.Anchor`, link log line), G12 (sweep, byte table, docs). Next thread: G11 first (S), then G10, G12.
- Still open: RefundAirJumps (no gate needed it), thrown land-cancel, rider attack binding, G6 carry load, the expired-table-set edge (deltas).
- Cost: one Opus thread, ~75k context, ~70 calls, 2 UBT builds, 2 headless editors, 3 harness editors (~40 min machine): medium.
