# Desktop handover: integ-cp3 + montage half-rate fix (2026-11-15, _ue59, Sonnet, ~$5 est.)
Branch `claude/integ-cp3`. Merged lc9e27 (53a3bfe2: montage carry, 180 deg snap) and c9tzis (review files), no conflicts. _ue59 built (editor target), Unreal closed.
**Build**: Run-Timed Build OK, 30 s (`Saved/Logs/ci/build_m.log.esito`).
**Tests**: GSTNCombat 579 Success, 0 Fail (`grep -c "Result={Success}" Saved/Logs/ci/test_m.log`); includes MontageSync.StepperCarry and Hits.Facing.InterpolateSnap.
**PIE harness** (L_Test_Combat, listen host+2, lag 60/15/2%): abilities CMP 6/6 OK, forced_rollback CMP 6/6 OK
(`grep "CMP RESULT" docs/notes/reviews/cp5b-pie/abilities_rollback_report.txt`), combat_spike PASS 11 checks (`cp5b-pie/combat_spike_line.txt`).
Notes: abilities/forced_rollback have no verdict in defence_batch (it reports TIMEOUT), so I used a small wrapper (scratchpad) that runs report(); not committed.
BATCH_MAP needs `MSYS_NO_PATHCONV=1` from Git Bash (the first two runs loaded no level: /Game path mangled to C:/Program Files/Git).
**Recaptures** (fixed capture script, L_Test_Hub, Attack on every kit; rows `docs/notes/reviews/cp5b-data/<cap>/`, packs `cp5b-{jab-chain,dropkick,dair-v9,hit-cue}/`):
jab KIT_Test_Overlord jab1_jab2_faf; dropkick KIT_Test_OverlordDropkick dropkick_whiff; KIT_Test_OverlordDAirV9 dair_whiff/land/hit; KIT_Example cue_hit.
**Montage check**: `cd docs/notes/reviews/cp5b-data && python tools/count_repeats.py`: 0 repeated frames in all 6 rows (0/59 jab, 0/49 dropkick, 0/32 x3 dair, 0/85 cue).
Per-frame positions: jab 0,1,2,3,4.04,5.08...11.33 then wrap 0,1 (was 0 2 2 4 4 6); dropkick 0,1..7,7.99..10.99,12.03 (was wrap 15.98 18.97 18.02).
**Not done / not judged**: I did not watch the videos (rule 10: needs a cloud viewer); cue az0 victim-facing on the hit frame and the shake are unchecked.
Not run: full `GSTN` filter (only GSTNCombat).
**State**: AM_OverlordDAirV9.uasset still dirty (not mine; discarding it was blocked, left). PM_*/IC_/KIT_Test_Hits clean. No maps saved. No force-push, feature untouched.
**Cost**: Sonnet, ~1 h machine, 1 build, 1 test batch, 4 PIE batch runs (2 wasted by the map-path issue), 4 captures: ~$5 est.
