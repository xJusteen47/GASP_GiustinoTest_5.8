# NIGHT-KD-1 handover: the dummy stays down after a knockdown (desktop, 2026-11-16). DONE
Branch `claude/night-knockdown` (pushed, owner approval 01:40Z): 03f708c0 + merge of `claude/integ-night-1116` 194ad7e3 + 84088011. Folder `_ue59`, Opus medium.
`AM_OverlordDAirV9` dirty and jab `director/` untracked: not ours, untouched. No map, no asset, no setting saved. `_ue59/IN-USO.txt` set, then freed.
## Two causes (both fixed)
1. `FGSTN_CombatVictim::TumbleProbeFrames` was 240: the knockback move that probes the tumble contact expired in the air (180% flight ~250 frames),
   so the landing was never recorded. Now 1800; past 240 frames the move ends itself the frame after its floor contact was read
   (`FGSTN_LayeredMove_Knockback::LandingEndsAfterFrames`), so before 240 nothing changes and after it the move never outlives the landing.
2. The hub dummies had NO kit, so no defence data: `dummy defence: none` in `Saved/nightkd/cap2/capture.log` (contact found, `k2` floor, resolved
   r1606, no knockdown). `test_hub.dummy_setup()` gives them `KIT_Test_DefenceLanding` 1 s after the play setup (spawn-tick loadout crashed on 11-06)
   and puts the defence steps back in the hit rules if a suite removed them.
## Proof
- Build `Saved\Logs\ci\nightkd-build.log.esito` OK. Tests `nightkd-test.log.ue.log`: 1665 `Result={Success}`, 0 Fail/Error (`Select-String` counts).
- Capture cap3 (`python Saved/pushbox_p3/st.py` style rows, `Saved/nightkd/cap3/dair_knockdown/frames.jsonl`): Tumble to v343 (lands z 88), InTimeline
  (bounce) v345-~v378 rising to z 106 (~18 cm) and landing v360, then Actionable + downed: `down=2147483647` (never ends) in `capture.log`, x fixed
  at 2292 from v343 to v535 (no slide).
- Capture cap4 `dair_knockdown_near` (new sequence): looked at az90 v428-v500 (`docs/notes/reviews/night-kd/lying_v428-v500_az90.png`): dummy lying face
  down, attacker walks over its legs (no pushbox while down, as designed). mp4 `docs/notes/reviews/night-kd/dair_knockdown_180_az90.mp4` (v80-v519).
## Choices applied (rule 6)
- Dummy kit = the saved `KIT_Test_DefenceLanding` (stick hold = roll, jump = neutral get-up, no automatic get-up) instead of editing `KIT_Test_Hits`.
- Not checked: whether that kit binds a get-up ATTACK (owner wants none) and whether the owner's "A" is the jump button. Da confermare (rule 13d).
## Not done
- ABP foot creep (~12 cm, react-hook-6 note 2) and the exit crouch: probe only (`Saved/nightkd/abp_probe.py`, 2 runs failed on Python property
  names, no graph dump). Needs its own thread: dump `SandboxCharacter_Mover_ABP` with `GSTN_AnimGraphAuthoringLibrary.describe_graphs` (drop the
  `parent_class` read), then look at slot vs foot placement / offset root order. Est. Opus medium 6-10 $.
- Coordinator item (hurtbox copy + leg check + rollback check): not started; next thread per rule 2. RISK (rule 12): "Bake all" re-saves every
  timeline asset (binary, the owner's animation threads touch them): bake only the timelines of the test kit, or decide first.
**Cost**: estimated 12-15 $ (Opus ~120k context, 1 build, 1 test run, 4 captures, 2 probes).
