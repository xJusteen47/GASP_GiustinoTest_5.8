# PUSHBOX-P2 handover: pushbox per state tag + dair/dropkick captures (desktop, 2026-11-15). DONE (code), knockdown not seen in game
Branch `claude/pushbox-p2` from `claude/pushbox-p1` d50bfbd4, folder `GASP_Giustino2_ue59`. Opus, no subagents. Not pushed (needs owner approval naming branch + commit).
`AM_OverlordDAirV9` dirty and jab `director/` untracked: not ours, untouched. No asset, map or project setting saved.
## Code (commit ae3b4039)
- Core `GSTN_Pushbox.h`: `FGSTN_PushboxStateRule` (StateTag by name, bEnabled, Radius/HalfHeight) and `FGSTN_PushboxSettings::StateRules`, default
  Combat.State.Knockdown off + Combat.State.Tumble off (owner 2A, plan 4). Order: move Pushbox track > first matching rule > movement capsule x0.8.
- Tags by name (as `FGSTN_GrabRules::IgnoreTagNames`): no code names the state it turns off; the rules live on the shared CombatRules (P1 choice).
- NP: new contributor hook `IGSTN_SnapshotContributor::AppendStateTags` + `Registry::CollectStateTags`; the snapshot adds the core action state tag
  (Tumble, Splat) and resolves the pushbox after the contributors. Defence contributor answers Combat.State.Knockdown from `IsKnockedDown(Frame)`.
- Capture script: rows carry the dummy's place (`dummy.loc`); new sequence `dropkick_hit` (dummy 120 cm). `Tools/GASNPPTest/pushbox_rows.py` = dist/step table.
## Applied choices (rule 6)
- Pushbox only: the lying low ECB of plan 4 (hh ~25) is NOT done (touches world collision of a downed body; own small checkpoint if wanted).
- Splat keeps the standing pushbox (no default rule; plan gave none).
## Proof
- Build `Saved/Logs/ci/pushbox_p2_build1.log.esito` OK. Tests `Run-Timed -Preset Test -Tests "GSTNCombat+GSTNLink+GSTNMinion+GSTNDefence"`:
  886 Success, 0 Fail (count `Result=\{Success\}` / `Result=\{Fail` in `pushbox_p2_tests1.log.ue.log`); new Core.Pushbox.StateRules passes.
- Captures (vd2, PIE_CAPTURE_COLLISION=1) `pushbox_p2_cap_dair.log.esito`, `pushbox_p2_cap_dropkick.log.esito` OK. `docs/notes/reviews/pushbox-p2/`:
  dair_hit, dropkick_hit, dropkick_whiff mp4 normal + quarter; az90 sheets. Numbers: `python Tools/GASNPPTest/pushbox_rows.py <cap> <seq> <out>`
  on `Saved/pushbox_p2/cap_*` (tables `dair_rows.txt`, `dropkick_rows.txt` in the review folder).
- dair_hit: hit connects (attacker pausedTotal 4), min distance 45.8 cm v113, largest step 16.2 cm v121 (Body Capsule back to standing, as P1). Looked
  at az90 v104-v127 once: no slide back; arms reach the chest v110-v120, knees cross v121+ (as P1). The dummy at 0% only flinches: NO knockdown.
- dropkick_hit: NO hit (pausedTotal 0): the kick passes ~230 cm over the dummy, lands lying at 64-71 cm, head against the dummy's feet v106-v130
  (visible overlap, pushboxes 24+24 do not touch: torso capsule centre is far). No shove, no slide. Whiff: landing + end with collision shown.
## Open
- Knockdown-no-pushbox not seen in game: the harness cannot raise the dummy's percent, so no tumble/knockdown. Needs a test-only percent setter
  (C++ BlueprintCallable on the test component, ~1 build) or an NP test with a downed fighter. Proven only by the core test today.
- Rule 10 cloud review of `reviews/pushbox-p2` mp4s/sheets. Dropkick dummy distance to retune (~250-300 cm) for a real landing hit.
- Option, not changed: `CombatRules.Pushbox.CapsuleRadiusScale` 0.8 -> 1.0 (more room: arms on chest in dair, head on feet in dropkick).
**Cost**: estimated $6-7 (one Opus thread ~90k context, 1 build, 1 test run, 2 captures).
