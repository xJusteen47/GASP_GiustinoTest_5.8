# Handover: hit-react captures + Mario run P1 integration (desktop, _ue59, 2026-11-16, Sonnet)
Branch `claude/mario-run-int` = `claude/hit-react-int` (be77bf31) + `claude/project-thread-dru892` (1a5c7cb5) + test fix + data. Pushed to origin, not to feature.
## 1. Directional hit-react captures (rule 10, PLACEHOLDER poses: system check only, poses NOT judged)
- Files: `docs/notes/reviews/hit-react-directional/` (README, mp4 on/off for hitstand, back_hitstand, side_hitstand, side_hitstand_l; grids; cmp_* = ON top / OFF bottom).
- Capture script got side sequences + env PIE_CAPTURE_PLACE_SIDE / PLACE_BEHIND. Logs `Saved/Logs/ci/hitint_cap_{on,on2,on3,off}.log` (ESITO: OK). Seen by me: front, back, side(right) ON/OFF grids.
- Result: all 4 hits land (Hitstun segment in frames.jsonl), nothing breaks, ON adds a torso bend over the same HitStand base, OFF = base only. Side picked is NOT logged (no readout), so "right side picked" is inferred from geometry only. First runs missed on back and left (victim walked out of reach): re-placed, see README.
- Cameras per ON/OFF run differ slightly, so a pixel diff is meaningless; compare by eye.
## 2. Mario run P1
- Merge conflict only in GSTN_CombatSimComponent.cpp (stats modifier queue): kept our body-capsule version, added `FGSTN_RunStageMover::RegisterPersistentState(*Mover)`.
- Build OK (`Saved/Logs/ci/mario_build1.log`, 24 s; build2 11 s). Tests: `GSTNCombat.Core.RunStages`+`NP.RunStages`+`NP.Stats` 9/9 after a test fix (`grep -c "Result={Success}" Saved/Logs/ci/mario_test_new2.log.ue.log`). Fix attempt 1 of 2: SyncState test used copy construction of FMoverSyncState, which shares elements (only operator= is deep); test now assigns. Game code unchanged.
- Whole `GSTNCombat`: 615 Success, 0 Fail (`mario_test_all.log.ue.log`; before merge 609 + 6 new).
- Data, headless (logs `mario_setup_*.ue.log`): setup_player_input_contexts (IA_GASNPP_RunBoost, IMC_GASNPP_Test), setup_run_stages (DA_Locomotion_Default + entry in 28 kits), setup_gas_assets needed `run(combat_input=True)` (the commandlet does not pass `--combat-input`; wrapper in the scratchpad) -> IC_GASNPP_Test and SS_GASNPP_Standard carry RunBoost. Verified by `grep -ac RunBoost` on the uassets.
- NOT done: pad test (owner). Stage speeds are placeholders (450/650/850) in DA_Locomotion_Default.
## State left
- _ue59 on `claude/mario-run-int`, editor closed, nothing running. Lock `_ue59/IN-USO.txt` removed. AM_OverlordDAirV9 still modified and uncommitted (not ours); untracked `Apri editor 5.9.bat`, jab/director left alone.
- Reverted startup dirt: IMC_GSTN_Locomotion. PM_* / maps untouched (only commandlets ran, no full editor except captures; no map saved).
## Da confermare (coordinator to owner)
- Back-hit turn makes every staggered hit read Front (see hit-react-directional.md); side only shows on unstaggered hits.
- Stage values 450/650/850, boost mode/window.
Cost: roughly $3-4 for both checkpoints (estimate, no subagents).
