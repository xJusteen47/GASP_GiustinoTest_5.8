# Spin and drill handovers (per-thread file, from 2026-11-02)

Earlier entries: HANDOVER 63-66, 70-75 and 78 in `docs/notes/takeover-state.md`. Numbering here: SD-01, SD-02...

## SD-01 (laptop, 2026-11-02): state after D4-D11, next is D9+D10

- feature/gas-npp-5.8 = ae928e8 (fast-forward, owner OK): D4 ground/air spin, D5 opposite cancel, D6/D7 core seams (S1 re-hit
  interval, D-1 outbox event key), D8 dive, D11 minimal presentation, Handover 78. Backup: claude/project-thread-vfuzsj-rebased.
  claude/project-thread-vfuzsj is the stale pre-rebase copy: ignore it.
- Gate run: build with GSTNSpinDrill enabled + RunTests GSTNInput+GSTNCombat+GSTNSpinDrill+GSTNDefence+GSTNLink+GSTNParts = 943/0
  (Saved/D4/headless-rebase2.log on the laptop). NOT run: plugin-off build (no plugin file changed outside GSTNSpinDrill except
  GSTNCombatCore/NP, compiled in the on-build), and no harness scenario (SUITE_SPIN in Tools/GASNPPTest/spin_drill_scenarios.py,
  needs the editor and Input.Button.Crouch in IC_GASNPP_Test).
- Design decisions and deviations: docs/notes/spin-drill-doc-deltas.md sections D4, D5, D6/D7, D8, D11.
- Laptop build recipe: back up the LF .uproject (Saved/D4/uproject.local.bak, SHA256 E48D6E45DC87...), insert a GSTNSpinDrill entry
  after the GSTNLink one with an LF needle (a CRLF needle silently skips it), Build.bat ... -BytesPerUnityCPP=1, then UnrealEditor-Cmd
  RunTests; check the log has GSTNSpinDrill paths; restore the .uproject and its hash. Wait until no UnrealBuildTool / UnrealEditor
  process of another thread runs (one build per machine). Write new headers BEFORE starting UBT.

## Next: D9+D10 (latch and bore), C++ first

- Wait for M7's generic GSTNLink commit "relationship filter in the probe" (owner-approved plan); GSTNLink stays read-only for spin.
  GSTNCombat: one open change at a time (M7 G5 seam, then G3): no core edit planned for D9.
- Pieces: the drill tip hit carries LinkType Combat.Link.Type.Drill (declare type + roles Driller/Drilled in GSTNSpinDrillCore);
  UGSTN_LinkTypeDefinition for it (asset via a setup script when the editor is free; owner did not answer the asset question: C++ part
  first); latch rule per type (check FGSTN_LatchRules is not hard-wired to Type.Latch, plan stop condition); driller role parked,
  bActsWhileLinked true, YawLimitDeg 0 (M7 G2 default 360 turns it with the stick).
- In GSTNSpinDrill: ReadDiveOutcome turns a latched link into the Latch request (D8 bounces every hit today); UGSTN_GA_DrillBore adopted at
  latch, its loop held by a plugin track hook stall (defence hold-window pattern) while crouch is held, tip hitbox with
  RehitIntervalFrames = FGSTN_BoreTuning::RehitIntervalFrames (S1 counts held frames); end rules already in UGSTN_DrillMath::ResolveEnd;
  launch = UGSTN_DrillMath::ComputeLaunchVelocity via an instant effect; the drill presentation wobble already reads Seating.
- Harness for D9/D10 per plan: drill_latch_static_prop_multi_hit, drill_release_early_pop, drill_launch_after_bore, drill_target_destroyed,
  drill_latch_moving_enemy, drill_hit_while_latched_detaches.

## Cost of this thread (estimate)

One long Opus session (D4 through D11 plus two rebases): very high, far past the new 150k-context rule; the next thread should stop
after one checkpoint.

## SD-02 (laptop, 2026-11-02): triage of the flaky latch check, D9 part 1 (drill latch as a GSTNLink type)

- Commit on feature/gas-npp-5.8 (on d05d767): "Spin and drill D9 part 1: the drill latch as a GSTNLink type". Deltas:
  docs/notes/spin-drill-doc-deltas.md sections "D9, part 1" and "Triage".
- Triage (M7 OPEN 7, `latch_big_enemy_host_latches_client`): NOT the D6/D7 seams (with RehitIntervalFrames 0 the ring key is the group,
  byte for byte; no other spin commit touches a file outside GSTNSpinDrill, which those runs do not enable). The check was at its limit
  before the rebase (3, 4, 4 counter steps; limit 4) and is 3-5 after: a margin flake. No code change. Lead to decide: per-hit count or a
  10-run distribution before touching the bound.
- D9 part 1 (C++, owner's asset question unanswered: code first, as the mount): tags `Combat.Link.Type.Drill`, `Combat.Link.Role.Driller/
  .Drilled`, `Combat.Immunity.Drill` (29 tags); `UGSTN_LinkRule_Drill` (Core); `FGSTN_DrillLinkType` (GAS, registered at PostEngineInit:
  driller parked, acts, yaw 0, predicts, hit break on the driller, leash 300); the dive tip's hit carries the drill link type; the phase
  machine reads `FGSTN_DrillLinkView` (Pending -> AwaitLatch, Active -> Latch, refused -> Bounce; latched: link broken by a hit -> detach,
  otherwise target gone -> launch; hitstun/KO detach; crouch held = dive timeline running) and releases its own link through GSTNLink's
  `MarkPendingBreak` when its bore ends. New plugin edge GSTNSpinDrill -> GSTNLink (public API only; GSTNLink and GSTNCombat untouched).
- Gates (laptop, Saved/SD02): build with GSTNSpinDrill on `build-on2.out` "Result: Succeeded" (attempt 1 failed on a C4459 shadowed name in a
  test, fixed); RunTests GSTNInput+GSTNCombat+GSTNSpinDrill+GSTNDefence+GSTNLink+GSTNParts `headless-on2.log` 950/0 (943 at SD-01 + 4 new +
  M7's link tests), no ensure; the 4 new tests pass; .uproject restored (SHA256 E48D6E45DC87...); check_modules.py 0 problems. Plugin-off build not run (no file outside GSTNSpinDrill changed). Harness not run (needs the bore and
  `Input.Button.Crouch` in IC_GASNPP_Test).
- OPEN for the owner (one message): (1) driller placement is a fixed offset (0, 0, 100) over the target's root: a per-latch anchor (tip
  where it hit) needs a GSTNLink seam (a per-link offset in the link state); OK to ask M7/lead for it? (2) a link type asset (DA_LinkType_Drill
  via a setup script) instead of code: keep code? (3) "not drillable" is the tag `Combat.Immunity.Drill` until D12 (penetration vs hardness).
- Next thread (D9 part 2, then D10): `UGSTN_GA_DrillBore` adopted at the latch (track hook stall while crouch is held, tip re-hit interval
  = `FGSTN_BoreTuning::RehitIntervalFrames`, `HitsLanded`, damage scale), launch and pop velocities (`ComputeLaunchVelocity`, instant effect),
  outbox events, Launch refresh; harness scenarios of the plan; D10 moving target (corrections per latch, stop condition > 3).
- Cost of this thread (estimate): one Opus session, about 140k context, two builds of ~20 min; within the 150k rule.

## SD-03 (laptop, 2026-11-02): D9 part 2 and D10 (bore, launch, pop, per-latch anchor), latch flake per hit

- Commits on feature/gas-npp-5.8: c58d8cc "Spin and drill D9 part 2 and D10" + this one (handover, per-hit latch check). Deltas:
  spin-drill-doc-deltas.md "D9, part 2 and D10" (design vs built). Decisions 5 (drill type in code, Immunity.Drill until D12) and 6 applied.
- Built: `FGSTN_BoreTrack` + `FGSTN_SpinTimelines::MakeBore`; `FGSTN_BoreTrackHook` (holds seat/bore frame, stamps crouch 0xD1);
  `UGSTN_GA_DrillBore` (forced at the seat's first frame, found by its track; `FGSTN_SpinDrillKitExtension::BoreAbility` adds it to the kit's
  triggered abilities); `UGSTN_GE_DrillLaunch` / `UGSTN_GE_DrillPop` (falling mode, driller's up as normal); flag `Popped`; Launching ends with
  the bore's timeline; per-latch anchor written once through GSTNLink `SetAnchorOverride` (driller root in target root space, view snapshot);
  outbox Event.Drill.Latched/Detached/Launched/GroundLag. D10 = the link's parked transform + this anchor (no extra code).
- Gates (Saved/SD03, scripts build.ps1, run_harness.ps1, harness_driver.py): on build Succeeded, headless 980/0 (975 + 5 `GSTNSpinDrill.NP.Bore.*`);
  off (SpinDrill+Piggyback disabled) build Succeeded, headless 933/0; no ensure; .uproject restored (committed copy, CRLF).
- Latch flake, 10 runs (Saved/Logs/sd-03/latchb*): attack window steps 3,3,3,4,6,4,3,3,3,4 -> check now per hit, limit 3 steps/hit
  (`LATCH_STEPS_PER_HIT`, link_scenarios.py). Separate flake seen once: "no unforced reconcile while the enemy walks" = 1 (run 01). Not fixed.
- OPEN: (1) SUITE_SPIN fails whole (no spin on any world) in this harness: never run before; IC_GASNPP_Test has no Spin/Crouch buttons and the
  spin prepare_editor leaves duplicate Spin bindings in KIT_Test_Hits for later suites (run it last or in its own editor). (2) Drill harness
  scenarios of the plan (latch static multi-hit, early pop, launch, target destroyed, moving enemy, hit while latched) not written: they need (1).
  (3) Launch/pop velocity vs the link's exit placement at F+1 unverified in PIE. (4) KeepPlunging still ends next frame. (5) Bore damage scale: D12.
- Cost: one Opus thread, ~140k context, 3 UBT builds + 2 headless + 2 harness editor runs (~60 min machine): medium-high.
- Next: fix the spin harness input (IC buttons) and write the six drill scenarios, then D12.

## SD-04 (laptop, 2026-11-02): spin harness input; drill scenarios NOT written

- Done: `spin_drill_scenarios.prepare_editor` adds Input.Button.Spin/Crouch to IC_GASNPP_Test and the spin bindings to KIT_Test_Hits, in memory;
  `restore_editor` undoes both and `defence_batch` now calls the module's `restore_editor` at the end of a batch (no more kit pollution for
  later suites). Every spin scenario applies the test kit to all pawns at 0.05 s (`_apply_kit`; the pawns started on their default loadout).
  `drill()` treats the None UE Python returns for "false + out params" as no fragment (it used to raise).
- NOT done: SUITE_SPIN still 0/6 (runs in Saved/Logs/sd-04/spin-run*.log, 3 attempts). Spin never starts on any world: no drill fragment on any
  pawn (get_shown_drill_numbers -> None), vz after the air spin only the jump's 386. The kit applies (only "arbitrary choice" warnings for
  bindings 4/5, 7/8 of the spin commands: air spin vs ground spin require one tag each, a design warning). Next check: after
  `server_apply_test_loadout` does the pawn show `Command.Movement.Spin.*` in its slot table / is the SpinDrill kit extension and fragment
  in the pawn's sync state (needs the extension in the kit's loadout compile, not only bindings)? Maybe the stick rotation never reaches the
  combat input buffer (IA_Move on IC_GASNPP_Test as move_action). Probe with `Lib.get_shown_drill_text` + `unreal.log` of the slot table.
- Six drill scenarios: not written (they depend on spin working). Max-2-attempts rule hit; stopped.
- Input (owner request): IMC_Sandbox: Crouch is now on Gamepad L1 (it was Sprint's); Sprint moved to L3 (Gamepad_LeftThumbstick). Crouch was
  also on C and gamepad B (face right). SHARED BINARY changed: Content/Input/IMC_Sandbox.uasset. NOT hold-to-crouch yet: IA_Crouch has an
  InputTriggerPressed trigger (GASP's BP toggles on it). Hold needs a Down trigger or Pressed+Released in the BP/IA: not changed, needs the owner's OK.
- Environment: UnrealEditor stuck for hours came from my own script's empty driver path (PowerShell regex on a backslash); fixed. Stop-Process
  of the editor works with the new .claude/settings.json rules only as the bare command.
- Cost: one Sonnet thread, ~100k context, 6 editor runs (~25 min machine), medium. Low value per cost: the root cause of "no spin" is open.
- Hypotheses for "no drill fragment on any pawn" (for the next thread, in order): (1) the fragment is registered by GSTNSpinDrillNP at
  PostEngineInit (`FGSTN_DrillFragmentRegistration::Register`, GSTNSpinDrillNPModule.cpp) but a pawn's sync state only carries the fragments of
  features its kit/combat component enabled: check whether the test pawn (BP_GASNPP_TestCharacter, combat component) enables the SpinDrill
  feature id (GSTNFeatureIds::SpinDrill) or needs `FGSTN_SpinDrillKitExtension` in the loadout; the unit tests build it by hand, the harness
  never did. (2) The PIE worlds started before the kit was applied (apply at 0.05 s of the scenario): the compiled slot table may need a fresh PIE
  after `prepare_editor`; try applying in `defence_batch`'s ready phase. (3) Only if 1 and 2 fail: the stick rotation (IA_Move through
  IC_GASNPP_Test) does not reach the combat input buffer; probe by `get_shown_drill_text` plus a log of the slot table.
- Estimated cost: Sonnet 5.5, ~100-110k context tokens, 6 editor runs (~25 min machine). Next work: hold-to-crouch (new thread, approved by owner).

## SD-05 (laptop, 2026-11-02): why no spin started (two real bugs, fixed); drill scenarios written, SUITE_SPIN still red

- Root cause of "no spin anywhere" (not the harness): (1) `MakeDefaultSpinBindings` tied the cancel and the air spin (1 required tag each):
  the loadout compile reports it as a binding conflict ERROR and refuses the whole kit (the "arbitrary choice" lines of SD-04 were the refusal,
  revision 0, kit None). Now a ground and an air cancel per command (9 bindings), each one tag above the spin it competes with. (2) The spin,
  cancel, dive and bore abilities called `SetAssetTags(...)` and dropped `Ability.Timeline` from `UGSTN_GA_BaseCombat`: the compile took them
  for plain abilities, the command activated them, the combat tick never started the timeline ("not adopted within 2 frames"). Broke the
  spin in the game too, not only in tests. Both pinned in `GSTNSpinDrill.GAS.Spin.DefaultBindingsAndSlots` (no conflict, keeps Ability.Timeline).
- Harness fixes: `defence_batch` str() of dict details; module hook `on_ready()` 1 s into the settle time (spin applies the kit there: a kit
  change at the scenario start ate the first jump); `drill()` read UE Python's folded return wrong (true gives the 4 out params, no bool):
  it always said "no fragment"; `_apply_kit` logs a refused kit. Probe driver: Saved/SD05/probe.py (stick history, request, drill per tick).
- New: `GSTNSpinDrillTests` has `UGSTN_SpinDrillTestLibrary::ServerSetDrillImmunity` (test effect granting Combat.Immunity.Drill), used by
  `drill_nondrillable_bounce` (players are drillable since D9). Six latch scenarios in spin_drill_scenarios.py (`SUITE_DRILL`, part of
  SUITE_SPIN): static multi-hit, early pop, launch after bore (jump button tap in the boost window), target destroyed (KO zone), moving
  enemy (D10), hit while latched (host teleported next to the driller, attacks). NOT yet green: never reached the latch (see next).
- SUITE_SPIN run 6 (Saved/Logs/sd-05/spin-listen-0pct.log): 0/12. The spin now runs (client shows Spinning at the right frame, spin
  timelines end after 32 frames in the log), but every check reads ONE mark per world at t_end+0.05: the authority and the other client show
  the spin 100-250 ms later (60/15 ms lag + input buffer), so they read None/idle. Same for the dive and the latch marks.
- Next (one thread, Sonnet is enough): give the module a per-world sampler (slate tick: drill tuple + latch_info per world with time), and
  turn every mark check into "seen on world wi within [t0, t1]" (first sighting per world, same start frame). Then rerun; expect real
  findings on the ring damage, hop speed and the latch only after that. Run with Saved/SD05/run_editor.ps1 + driver_spin.py (~8 min).
- Gates: build on Succeeded, headless on 980/0; off (SpinDrill+Piggyback disabled) build Succeeded, headless 933/0; no ensure; .uproject
  restored. Not touched: hold-to-crouch.
- Cost: one Opus thread, ~150k context, 3 UBT builds, 7 editor runs (~45 min machine): medium-high; stopped at the context rule.

## HC-01 (laptop, 2026-11-02): hold-to-crouch on IA_Crouch: NOT done, needs a BP graph edit in a live editor

- Result: no asset changed (IA_Crouch reverted to its committed state). Nothing to build or test: no C++ touched, suites unchanged by construction.
- Findings: IA_Crouch (`/Game/Input/IA_Crouch`) has one trigger, `InputTriggerPressed`; IMC_Sandbox mappings (L1, C, gamepad B) have no triggers of their
  own. Referenced by `/Game/Blueprints/SandboxCharacter_Mover` (event "Crouch Input (toggle)") and `SandboxCharacter_CMC` (event on IA_Crouch), and by
  `/Game/GASNPPTest/Input/IC_GASNPP_Test` (the combat `Input.Button.Crouch`, so the dive "crouch held" reads the same action).
- Tried and rejected: IA triggers [Pressed, Released] so the existing toggle fires on press and on release (zero BP edits). It breaks the combat path:
  `NpAbilitySystemComponent` (lines ~280-325) maps the action state to Started/Triggered/Completed, and Pressed+Released never gives a clean
  Started..Completed hold, so the dive's "crouch held" would break. Also a ledge auto-uncrouch while held would re-crouch on release.
- Right fix (needs the live editor, Rider/unreal-mcp connected; the headless Python has no graph node API): IA_Crouch trigger -> `InputTriggerDown`
  (or none: the default gives Started/Triggered/Completed); in both BPs replace the Triggered toggle by Started -> Set WantsToCrouch true and
  Completed -> Set WantsToCrouch false (in Mover also keep the existing "uncrouch when falling" path). Keyboard C and gamepad B keep working
  (mappings untouched). Then run the headless suites and check in PIE: hold L1 = crouched, release = stands.
- Decision made: did not ship the zero-BP variant. Tell the owner before editing the two shared BPs (binary, no merge).
- Environment: unreal-mcp / Rider ue_health were not connected (no running editor); headless runs via Saved/SD05/run_editor.ps1 with
  `QUIT_EDITOR` (not `quit`: my first driver hung 10 min). Scripts in Saved/HC/.
- Cost: one Sonnet thread, ~45k context, 4 short headless editor runs plus one 10 min hang. Low.

### HC-02 (laptop, 2026-11-02): hold-to-crouch DONE (live editor, Rider python graph API)

- IA_Crouch trigger: Pressed -> Down (Started/Triggered/Completed: a clean hold for the combat "crouch held" read as well). Mappings untouched
  (L1, C, gamepad B keep working).
- `SandboxCharacter_Mover` and `_CMC`, event IA_Crouch: the toggle moved from Triggered to **Started**; **Completed** is wired to the "crouch off"
  node (Mover: `SetFieldsInStruct_9` WantsToCrouch=false; CMC: `UnCrouch` node 59). Both compile, 0 node errors. Press toggles as before, release forces stand.
- Known quirk: Started still toggles, so pressing while somehow already crouched (e.g. crouched by another source) stands up; release also stands. Fine for hold.
- NOT verified: no PIE hold test and no headless suite run (SUITE_SPIN is red from SD-05, no green crouch suite known). Check by hand: hold L1 = crouched, release = stands, keyboard C too.
- Changed shared binaries: Content/Input/IA_Crouch, Content/Blueprints/SandboxCharacter_{Mover,CMC}.uasset. Backups: Saved/Backups/GAS/IA_Crouch.pre-hold.uasset; BPs via git.
- Tip: the live editor with Rider connected has `unreal.BlueprintGraphEditor` / `BlueprintGraphPinLibrary` (list nodes, rewire pins, compile).
- Cost: ~35k more context.

## SD-06 (laptop, 2026-11-02): per-world sampler in the spin harness; sighting checks pass, real findings behind them (STOPPED)

- Fix (Python only, `Tools/GASNPPTest/spin_drill_scenarios.py`): `_Spy` samples `read()` (phase, start frame, air spins used, low-gravity until, latch frame,
  hits) on every world each editor tick; checks are "first seen on world wi within [t0, t1]" (`_seen_all`, `first`, `never`). Used by the spin, air-once,
  opposite-cancel, dive and all six latch scenarios (`_DrillRig.check_latched(t_exit)`). `_finish` stops the spy. No C++ touched, so no rebuild:
  build gates are those of SD-05 (on Succeeded / off Succeeded); other headless suites unchanged by construction (not rerun).
- Run 7 (Saved/Logs/sd-05/spin-listen-0pct.log, editor log Saved/SD05/editor-sd06a.log): still 0/12, but the timing problem is gone: spin_ground_hits_ring
  now PASSES the spinning-on-every-world, same-start-frame (330), tag-held, spin-ended and no-rollback checks. One attempt used of two.
- What fails now is real game behaviour or content, not the read timing (stopped, not widening scope):
  1. Ring damage never lands: victim Health 50 -> 50 in spin_ground_hits_ring (victim placed 80 cm away on the authority).
  2. Air hop: max vz 402 < 440; Combat.State.SpinLowGravity never seen (spin_air_hop_low_gravity). Air spin is spent (used=1) and given back.
  3. Dive never starts: no Diving phase on any world, max down speed 0 (drill_ground_lag). Hence no latch in 5 of 6 latch scenarios;
     moving_enemy saw Seating on world 1 only (3, 354) and never on the authority.
  4. spin_opposite_cancel: Spinning held 0.67 s srv / 0.86 s cli (natural 0.5 s: the cancel does not shorten it), 1 rollback; air_spin_once: ground
     spin after landing not seen (window may be too early, check first).
- Next: check whether 1-3 share a cause (the spin timeline ends "no longer named by the combat state", log line at the spin end; ring hit and tip hit
  both depend on the timeline's hit steps). Start with the ring damage: probe the victim position/team/immunity and the spin's hit step on the authority
  (Saved/SD05/probe.py). Then the dive start (crouch_hold/tap input vs the dive's start condition).
- Cost: one Sonnet thread, ~60k context, 1 editor run (~6 min). Low.

## SD-07 (laptop, 2026-11-02): air spin blocked itself (fixed); ring was a harness bug (fixed); SUITE_SPIN 2/12, latch still red (STOPPED)

- Probe (Saved/SD07/probe.py, output probe.txt): the air spin was requested and counted (used 1) but its timeline never ran: phase None,
  no low gravity, no hop, so no Combat.State.Spinning for the drill binding. Cause: the combat tick adopts the spin and spends the air spin
  (projects Combat.State.AirSpinSpent) BEFORE the ability system activates the ability in that frame, and the air spin classes had that tag
  in ActivationBlockedTags, so they blocked themselves. Fix (GSTNSpinDrillGAS only): no ActivationBlockedTags on the spin classes; the
  once-per-airtime gate is the air spin binding's IgnoreTags (State_AirSpinSpent, checked at command match on the previous frame's tags)
  plus the extension's CanAirSpin. Tests updated (GAS.Spin defaults and DefaultBindingsAndSlots).
- Ring: the game was fine (probe: victim damage 0 -> 3 = the ring's Damage). The harness read the Health attribute, but combat hits raise
  the damage percent (g.damage_of); and set_actor_location placements are undone by Mover. spin_drill_scenarios.py now reads damage_of
  everywhere, places the ring victim with g._place_near and the drill targets with _teleport_under (server_teleport_exact).
- Gates: build on Succeeded, headless 980/0; off Succeeded, 933/0 (unchanged). SUITE_SPIN 2/12 (was 0/12), Saved/Logs/sd-05/spin-listen-0pct.log,
  editor log Saved/SD05/editor-sd07spin.log. PASS: spin_ground_hits_ring, air_spin_once_per_airtime. Fell out of the same fix: the dive starts
  (drill_ground_lag only fails on 1 rollback), the opposite cancel now shortens the spin (only 1 rollback fails).
- Still red (one fix attempt used, not widened):
  1. Latch never forms in static_multi_hit, release_early_pop, launch_after_bore, moving_enemy, hit_while_latched (no Seating on any world);
     drill_target_destroyed DOES see the latch. Suspect _teleport_under (victim 20 cm under an airborne diver's root: capsules overlap,
     the teleport may be refused or nudged). Next: log the target's position vs the diver's at the dive start; try placing the target on the
     floor under the diver (g._place_near(client, vkey, (0, 0)) or a small forward offset) before the jump.
  2. spin_air_hop_low_gravity: max vz 0 and "spent then given back" not seen: check the scenario's sample window first (the hop runs now).
  3. One rollback in spin_opposite_cancel and drill_ground_lag (budget 0): decide if 1 is acceptable at 60/15 ms or find its cause.
  4. drill_nondrillable_bounce: victim damage 0 -> 0 (same placement as 1).
- Cost: one Opus thread, ~110k context, 2 editor runs (~6 min each), 3 builds + 2 headless runs. Medium.

## SD-08 (laptop, 2026-11-02): jump input and target placement fixed in the harness; SUITE_SPIN 3/12; latch blocked by the tip's depth (STOPPED)

- Probe (Saved/SD08/probe_latch.py, outputs probe*.txt; report Saved/Logs/sd-08/spin-full3.log): world-0/1 positions of diver and target every tick.
- Harness fixes (Python only, spin_drill_scenarios.py): (a) jump_steps holds IA_Jump 0.1 s instead of a one-frame press_input, which was lost or
  answered up to 0.5 s late at -nullrhi rates (no jump at all before 0.8 s in 3 latch scenarios); (b) _teleport_under puts the target ON THE FLOOR
  (own z) under the diver's own client copy, at the end of the stick turn (t1). The old call ran while the authority still had the diver on the
  ground, asked the target into the floor and pushed it 60 cm aside; (c) hop check: window to t_end + 0.3, bar 400 (sim samples read the 450 hop
  as 402), spin at jump + 0.4 so the turn ends at the top, landing window t_end + 2.0..2.2; (d) drill_ground_lag spin at 0.55 like the rig.
- SUITE_SPIN 3/12 (was 2/12): PASS ring, air hop, air once. ground_lag and opposite_cancel fail only on 1 rollback (budget 0).
- (1) Latch, two attempts used, STOPPED: with the target now exactly under the dive, the diver lands on the target's capsule top, slides off
  sideways and glides at constant height in Diving (no dive gravity, plunge velocity redirected by the contact); no tip hit ever lands
  (nondrillable_bounce: damage 0 -> 0). Cause (code read, not yet tested): the dive tip is a Root-space sphere at TipDepth 20, radius 45
  (FGSTN_DiveTimelineParams, GSTN_SpinTimelines.h; same for the bore). Root = the sim transform = capsule CENTER (actor z 88 standing), so the
  sphere's bottom is 23 cm ABOVE the feet and can never touch a body the capsule stands on. SD-07's one latch came from a target teleported 20 cm
  under the diver's centre (overlapping). Proposed fix (GSTNSpinDrill Core defaults, game code, needs the go): TipDepth ~ capsule half height +
  10 (about 100), or measure from the feet; then rerun SUITE_SPIN. Also worth a ticket: a Diving drill that is stopped by a non-ground contact
  glides forever (should end or fall).
- (2) Hop: fixed (harness only).
- (3) One rollback in spin_opposite_cancel and drill_ground_lag: cause not obvious from the samples; not investigated further. Decide whether 1
  rollback at 60/15 ms is acceptable for an out-of-sync cancel/dive start, or probe the frame of the correction.
- Gates: no C++ changed, so the build and the headless suites are unchanged from SD-07 (on 980/0, off 933/0), not rerun.
- Cost: one Opus thread, ~95k context, 6 editor runs (~5-8 min each), no builds. Medium.

### SD-08b (same thread, owner's go 21:44Z): drill tip below the feet; SUITE_SPIN still 3/12, blocked by the jump

- Change (GSTNSpinDrill Core, GSTN_SpinTimelines.h): TipDepth 20 -> 100 for the dive and the bore tip (Root space = capsule centre; the bore
  sphere now sits on the target's centre, DrillerOffset 100). Gates: on Succeeded, headless 980/0; off Succeeded, 933/0 (unchanged).
- Effect (Saved/Logs/sd-08/spin-tip2.log): the tip lands now: nondrillable_bounce passes damage and bounce (fails only "air spin stayed spent"),
  drill_target_destroyed forms the latch on every world (fails only the launch speed). No gliding dive seen any more: no end/fall rule proposed.
- Harness: jump held 0.25 s (was 0.1). Still red: in 4-5 of 7 diving scenarios the diver NEVER jumps (positions in Saved/SD08/probe7.txt).
  Pattern: it fails whenever the diver key is the pawn resting at (-800, 0, 90.000) and works when it is the one at (-800, -60, 88.27): the
  pawn at z 90.000 has likely never landed (Mover not Walking, so the jump is refused). Next (two harness attempts on the jump used, stopped):
  log the diver's Mover mode before the jump (`gstn.test.LogMoverModeChanges 1`); if it is not Walking, settle both pawns with
  server_reset_to_spawn / _place_near in on_ready or the rig's first step, then rerun SUITE_SPIN.
- Unchanged: 1 rollback in spin_opposite_cancel / drill_ground_lag (budget 0); opposite_cancel also failed its shortened-spin checks once in
  this run (flaky, same pawn issue suspected).
- Cost: ~+35k context, 2 builds + 2 headless runs, 2 editor runs.

## SD-09 (laptop, 2026-11-02): the diver jumps in every scenario now; SUITE_SPIN 4/12; latch still red, new lead (STOPPED, 2 attempts used)

- Probe (Saved/SD09/probe_mode.py; outputs probe_mode*.txt, probe_tick.txt): Mover mode and position of both pawns on every world before the jump
  and every tick. Cause confirmed: one of the two spawns leaves its pawn in Falling at z 90.00 for the whole PIE (never lands at rest), so its jump
  is refused. Which player gets that spawn changes between PIEs (spawns also move: (-800, 0) or (-800, 60) in different runs).
- Fix (harness only, spin_drill_scenarios.py): on_ready calls _settle_pawns, which drops only the pawns that are not Walking with
  server_teleport_exact (+2 cm, forced Falling). Attempt 1 settled both pawns: every diver jumped but the target stood 2 cm higher and no latch
  or tip damage landed anywhere (Logs/sd-09/spin-settle.log). Attempt 2 (kept) settles only the stuck pawn: every diver jumps (12/12 PREJUMP Walking).
- SUITE_SPIN 4/12 (was 3/12; Saved/Logs/sd-09/spin-settle2.log): PASS ring, air hop, air once, drill_ground_lag.
- Still red: nondrillable_bounce (tip damage 0 -> 0) and all six latch scenarios (no world shows Seating or Boring). New lead from probe_mode3.txt
  (static_multi_hit): the diver now dives from ~320 cm, the tip (centre -145) touches the target's head at the dive's first frame, both the
  authority and the client go to Mover mode GSTN_Linked at z 322 and stay there, but the drill text shows a NEW Spinning (start frame 353),
  not Seating. So a link forms, but the drill does not read it as its latch (ReadDiveOutcome / link type?), or the crouch tap at t1 + 0.05 starts
  a spin. Next: read the drill state and link type on that frame (game code, C++), not the harness.
- Unchanged: 1 rollback in spin_opposite_cancel (budget 0). drill_ground_lag passed this run.
- Gates: Python only, no C++ changed: build and headless suites as in SD-08b (on 980/0, off 933/0), not rerun.
- Cost: one Opus thread, ~75k context, 4 editor runs (~6 min each), no builds. Low-medium.

## SD-10 (laptop, 2026-11-03): the latch forms and holds on every world; SUITE_SPIN 3/12 with new, later failures (STOPPED, 2 fixes used)

- Probe (Saved/SD10/probe_link.py, outputs probe_link1/2.txt): link text, drill text and positions per tick. The target placement works
  (server_teleport_exact returns True, the target is under the diver one tick later); SD-09's "new Spinning" was the buffered spin firing after the dive ended.
- Fix 1 (GSTN_DrillTickExtension.cpp, ReadLinkView): the request the tip hit marks (MarkPredicted) is Pending with Role 0 (roles are written when
  the link forms); the drill only accepted Role 1, read "no link" and bounced on the hit frame, while the link formed anyway. A role-less
  Pending of the drill type now reads as the driller's pending. Unit test lines added in LinkView test.
- Fix 2: Inputs.LatchSeatFrames / LatchMaxBoreFrames were never filled (seat 0, bore 1 frame: every latch ended on its first frame). Now from
  FGSTN_BoreTuning (new SeatFrames/MaxBoreFrames in FGSTN_DrillSimTuning) with the default target response (the response is not carried to the
  driller yet: a ticket for when targets differ).
- Effect (Saved/Logs/sd-05/spin-listen-0pct.log): static_multi_hit latches on every world and is still Boring after 1 s of crouch.
- Still red, next thread:
  1. Bore hits 0, target damage 0 while Boring (static_multi_hit, hit_while_latched). Check the bore is adopted (FindBoreAbility on the test kit's
     TimelineRegistry; prepare_editor adds only the spin bindings?) and that the bore tip overlaps the target with the per-latch anchor.
  2. Client latch frame 2-3 frames EARLIER than the authority's (355 vs 357) and never corrected: same_latch check fails in 5 scenarios. Probably
     the client's view of the target; decide if the drill fragment's latch frame should be reconciled or the check loosened.
  3. Exits: pop and launch give vz 0 on both sides (release_early_pop, launch_after_bore, target_destroyed); moving_enemy: driller did not ride (0 cm).
  4. nondrillable_bounce: tip damage 0 -> 0 and no bounce (unchanged).
  5. 1 rollback in spin_ground_hits_ring and drill_ground_lag this run (flaky; opposite_cancel passed this run).
- Gates: on Succeeded, headless 980/0; off Succeeded, 933/0; restore build Succeeded.
- Cost: one Opus thread, ~85k context, 2 probe + 2 suite editor runs, 3 builds, 2 headless runs. Medium.

## SD-11 (laptop, 2026-11-03): bore hits and latch agreement fixed; SUITE_SPIN 6/12; exit speed diagnosed, fix not landed (STOPPED, 2 attempts used on the exits)

- Fix 1 (bore no hits): KIT_Test_Hits has no FGSTN_SpinDrillKitExtension, so the loadout listed no bore ability: the latch held but no bore
  timeline ran. New `UGSTN_SpinDrillLibrary::AddSpinDrillExtensionToKit` / `RemoveSpinDrillExtensionFromKit`; prepare_editor adds it,
  restore_editor removes it. static_multi_hit and target_destroyed PASS.
- Fix 2 (client latch 2-3 frames early): the check read the client's FIRST (predicted) latched reading. same_latch now compares the settled
  (last latched) reading of each world; all worlds agree after correction. The early client prediction still costs rollbacks: worth a look.
- Exits (pop, launch_after_bore vz 0), cause from the probe (Saved/SD11/probe_pop.py, probe_pop.txt): the pop is decided, the driller stays in
  GSTN_Linked ~6 frames, then the link's exit (FGSTN_LinkExitEffect, the type's placer beside the target, ExitVelocity 0) teleports it to the
  ground and zeroes the velocity the bore's pop/launch effect set. Attempt: drill type `bInitiatorPlacesExit = true` + a drill-written exit
  record (riding position, pop/launch velocity) after ReleaseLinkOnUnlatch. Suite still vz 0 (launch also lost the air spin refresh), not
  proven, REVERTED; the diff is Saved/SD11/own_exit_attempt.patch. Next: probe with it applied (link text: is Exit.Frame written? does the
  watchdog copy it or fall back because the capsule overlaps the target? does StepOwnExitBreak run on the authority?).
- Still red: moving_enemy (target moved 0 cm: likely the bore's 6-frame re-hit keeps the target in hitstun; spec says low hitstun, check
  bLatchMoves / hit data), hit_while_latched (driller took no damage, 7 rollbacks), nondrillable_bounce (unchanged), opposite_cancel
  (cancel did not stop the spin this run), flaky air_hop / rollback counts. Single-scenario probe runs often start no air spin (twice):
  run probes inside the full suite order.
- Gates: on Succeeded, headless 980/0; off Succeeded, 933/0; restore build Succeeded (code as committed = suite run 1, 6/12).
- Cost: one Opus thread, ~95k context, 2 suite + 3 probe editor runs, 2+3 builds, 2 headless runs. Medium-high.

## SD-12 (laptop, 2026-11-03): pop exit fixed; launch, moving enemy still red (STOPPED, 2 attempts used on the exit)

- Why SD-11's own-exit attempt was not confirmed: (1) it placed the exit at the riding spot, which overlaps the target, so the watchdog
  refused to copy it and fell back to the placer at rest; (2) where it applied, Mover's FTeleportEffect refuses an encroaching spot and
  returns before FGSTN_LinkExitEffect set the velocity; (3) it was never probed, only judged by the suite.
- Probe (temporary UE_LOG, Saved/SD12/probe_lines.txt): with the fix the exit carried vz 400 and teleported, the server still read vz 0:
  the GSTN_Linked mode simulates that last parked tick and writes zero velocity before its exit transition is seen next tick.
- Fix: the drill flags its release (GSTNDrillFlags::PopExit / LaunchExit, bits 5-6) and on the exit's due frame (AfterExecute, before
  GSTNLink applies it at BeforeTagProjection) rewrites the exit record: own spot + pop/launch velocity (FGSTN_DrillTickExtension::
  StampExitImpulse; the flag is dropped if the link ends without an exit). GSTNLink: FGSTN_LinkExitEffect keeps the velocity in place when
  the teleport spot is taken, and a moving exit switches GSTN_Linked to Falling on its own tick (LeaveParkedMode). Keeping the start
  velocity in the parked mode instead broke GSTNLink.NP.Parked.NoInputNoGravity: not used.
- drill_release_early_pop PASS (srv and cli). Short driver: Saved/SD12/driver_short.py (ring, pop, launch, moving enemy; ~10 min).
  run_editor.ps1 needs an ABSOLUTE driver path (a relative one resolves under Engine/Binaries and the editor idles until the timeout).
- launch_after_bore still red: every release in the probe was a pop (flags Popped|PopExit, phase None), so the scenario never reaches the
  launch (bore ends before MaxBoreFrames: check the scenario's crouch hold vs the latch's MaxBoreFrames, and the air spin refresh check).
- moving_enemy (target 0 cm): bore hit with HitstunMultiplier 0 and 0 hitstop did NOT help, reverted. bLatchMoves is never read in code.
  Next: probe the victim's move input / Mover mode while latched (is the target parked or input-blocked by a link role tag?).
- Not touched: hit_while_latched (latch not ended by the host's hit, rollbacks), nondrillable_bounce, opposite_cancel, air_hop (vz 0 in the
  full run), ring rollback count flaky (2 -> 3 in two short runs, PASS in the full run).
- Full SUITE_SPIN at the start of SD-12 (exit code only): 5/12 (pop and launch red). Not rerun at the end (budget).
- Gates: on Succeeded, headless 980/0; off Succeeded, headless 933/0; restore build Succeeded.
- Cost: one Opus thread, ~110k context, 1 full + 4 short suite runs, 6 builds, 3 headless runs. Medium-high.

## SD-13 (laptop, 2026-11-03): launch fixed; moving enemy diagnosed (needs a design call); hit/bounce/cancel still red (STOPPED)

- launch_after_bore PASS (94d4a93). Cause: Inputs.bJumpBoost was never filled, so ResolveEnd always chose the pop. The bore track hook now stamps the
  jump (held, or a press inside FGSTN_BoreTuning::JumpBoostWindowFrames, read from the input history) as profile CrouchMarkerJump (0xD2);
  ReadCrouchStamp has an optional bJumpBoost out param. Window uses the tuning default (8), not the kit's value: the hook has no tuning access.
- Run Saved/SD13/driver_a.py (6 red scenarios): release_early_pop and launch PASS; moving_enemy, hit_while_latched, nondrillable_bounce,
  opposite_cancel FAIL. Not rerun: ring and full suite (count not measured this thread).
- moving_enemy, cause proven (Saved/SD13/probe_move2.txt): the target's actionState stays Hitstun (2) for the whole bore, vel 0, no link state of its
  own. GSTN_KnockbackModel floors HitstunFrames at Tuning.MinHitstunFrames (6) = the bore's re-hit interval (6), so hitstun never ends; this is why
  HitstunMultiplier 0 (SD-12) did nothing. NEEDS A DECISION (combat semantics): e.g. HitstunMultiplier <= 0 means no hitstun/no floor, or a HitData
  "no hitstun" flag for the bore tip. Not done (combat core change).
- hit_while_latched: attempt 1 (kept, harmless): held the attack button 0.12 s instead of a tap. Still driller damage 0 -> 0, so the hit never lands.
  Suspect: the hitter is teleported to the driller's height (z ~258, driller sits on the target) and falls; probe the hitter's mode/hitboxes next.
- nondrillable_bounce: not touched (only the bounce check is listed as failing; read the full check list in the run first).
- opposite_cancel: not touched (spin 0.72 s vs natural length, no cancel effect, 2 -> 4 rollbacks).
- Gates: build Succeeded (on); headless suites and the plugin-off build NOT rerun this thread (C++ change is small, tests unit-callable: ReadCrouchStamp
  default arg keeps old calls valid). Do both before any push. No push done.
- Cost: one Sonnet thread, ~75 context-heavy tool calls ~ low-medium; 1 build, 3 editor runs (~25 min).

## SD-14 (laptop, 2026-11-03): bore no longer pins the target; hit/bounce/cancel still red (STOPPED, budget)

- Decision (coordinator, owner may veto): `FGSTN_HitData::HitstunMultiplier <= 0` = no hitstun AND no stagger (UGSTN_KnockbackModel::ResolveRules sets
  bStaggered false: no lock floors MinHitstunFrames / MinStaggerLockFrames, no launch; damage and hitstop still apply). The bore tip sets 0
  (GSTN_SpinDrillTracks.cpp). Documented in combat-m4-hits.md and the SpinDrill README. First attempt (skip only the MinHitstunFrames floor) was not
  enough: the victim still staggered and the rules floor re-locked it (Saved/SD14/probe.txt: actionState 2 after the first bore hit).
- moving_enemy: target now moves (0 -> 17-20 cm in 0.8 s, expected > 100) but the driller does NOT ride it (0 cm). Two open causes:
  (a) the bore hitstop (Auto) freezes the victim ~every 6 frames: try tip HitstopMode Override 0/0 (3rd attempt, not made: rule is 2);
  (b) the driller's parked placement is not following the target (check UGSTN_DrillLinkType placement / anchor while the target moves).
- hit_while_latched, cause found (Saved/SD14/probe_hit.txt): the hitter placed 70 cm from the driller sits INSIDE the bore tip (r 45, 100 cm below the
  driller's centre), so the tip hits the hitter (damage 4 -> 20, knocked back 63 cm, hitstun) and it never hits the driller. Harness fix applied:
  place at 105 cm (spin_drill_scenarios.py). Result of that run: still red (latch not ended, driller damage 0). Next probe: does the hitter's jab reach
  the driller at 105 cm / at z 258 (driller is 164 cm above the floor, hitter falls from the teleport)? Consider an air hitter or a nearer
  spot with the tip's group excluded.
- Not touched: nondrillable_bounce, opposite_cancel (bounce check lists an empty reading; cancel: spin 0.67 s vs natural, rollbacks 0 -> 2).
- Full SUITE_SPIN (Saved/Logs/sd-14/spin-c.log, d2.py filter did not narrow): PASS spin_ground_hits_ring, air_hop, air_spin_once, release_early_pop,
  launch_after_bore, target_destroyed. FAIL: opposite_cancel, drill_ground_lag (rollback 0 -> 2), nondrillable_bounce, latch_static_multi_hit (no latch
  at all: possibly flaky or a regression of the stagger change in the dive's latch, not checked), moving_enemy, hit_while_latched.
- Harness lessons: `-ExecCmds="py C:\path\no spaces.py"` (copy the script to C:\sd14; spaces break the quoting, -ExecutePythonScript quits when the
  script returns); kill only your own editor pid (never Stop-Process -Name).
- Gates: build (editor, on) Succeeded twice. Headless RunTests (Input+Combat+SpinDrill+Defence+Link+Parts): 965 found, exit code 0 (Saved/SD14/headless.log). Plugin-off build NOT run (~30 min).
- Cost: one Sonnet thread, ~55 tool calls, 2 builds (~30 min each), 7 editor runs; medium. Local commit only, nothing pushed.

## SD-15 (laptop, 2026-11-03): latch_static_multi_hit was a flake; moving enemy diagnosed (design call); hit_while_latched still red (STOPPED)

- No C++ change in this thread, so no build and no new headless run (SD-14 gates at ca8188b still apply). Two full SUITE_SPIN runs (Saved/Logs/sd-15/full1.log, full2.log).
- latch_static_multi_hit: FAIL in SD-14, PASS in both runs here -> flake, not a regression of the HitstunMultiplier<=0 rule. Other flakes seen in the same
  runs: spin_ground_hits_ring (FAIL once, PASS once), release_early_pop (FAIL once, PASS once). Run 2: 7/12 PASS (+ drill_ground_lag still rollback 0 -> 2).
- moving_enemy, cause found, NOT a hitstop issue: UGSTN_LinkedMovementMode::SimulationTick keeps the driller capsule where it was (GSTN_LinkedMovement.cpp,
  "nothing moves"); the parked placement (FGSTN_LinkSnapshotContributor::FinalizeSnapshot, anchor in the partner's space) only rewrites the SNAPSHOT entry
  (hurtboxes, hit space). The unit test Parked.InitiatorFollowsMovingPartner says so ("its capsule never moves"). So the harness check "driller moved
  with the target" (actor location, 0 cm) can never pass as written. Decision for the owner (architecture, nothing changed):
  (A, recommended) keep the design, change the check to read the placement from the snapshot/hurtbox (needs a Python-visible reader), the actor stays;
  (B) make the parked mode move the capsule to the parked transform each Mover tick (visible riding, touches GSTNLink NP, replication and the exit).
- hit_while_latched: harness variant tried this thread (hitter 70 cm away, 50 cm above the driller, to clear the tip sphere): still red (latch not ended,
  driller damage 0). Reverted. 2 attempts spent in total on it (SD-14 + SD-15); next needs a probe of the hitter's jab hitbox (reach/height) vs the
  driller's parked hurtbox, i.e. print the snapshot entry of the driller, not the actor position.
- Not touched: nondrillable_bounce, opposite_cancel, drill_ground_lag.
- Cost: one Sonnet thread, ~30 tool calls, 0 builds, 2 editor runs (~8 min each); small.
