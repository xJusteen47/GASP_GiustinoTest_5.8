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
