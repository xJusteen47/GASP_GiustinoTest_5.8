# Dash r3 in-game dossier (2026-11-19, desktop _ue59, branch claude/locomotion-finish-4)

## Why the boost never fired (cap1-cap4)
- Run stages read the boost as `Command.Movement.RunBoost` (WasCommandEnteredWithin). The command table keeps only commands an
  ability is bound to; nothing binds the boost, so FindCommandIndex = -1 on every kit (Saved/Dash/cap4/mv_dash/frames.jsonl,
  combat.run.boostCmd = -1). The L3 press itself arrived (history `+J`). Not a capture bug: the boost could not fire in play either.
- Fix (data-driven, the key stays in the input data): query commands. FGSTN_KitCompileInput::QueryCommands -> the compiler gives
  them their slot from the kit's command set, no row; the NP table adds them as commands with 0 bindings (never candidates). The
  locomotion loadout extension adds the profile's BoostCommand. Test GSTNCombat.Core.Table.QueryCommands.
- Capture: boost pressed 3 frames after the stick (pie_capture_move.py mv_dash, 'boost': 23).

## Capture Saved/Dash/cap5 (KIT_Test_Playtest, pie_capture_move.py mv_dash + mv_dash_base)
- mv_dash: stick v40, L3 v43; sim boost on v44: run stage 0 -> 1 (target), dash counter starts (combat.run.stage/dash).
  Travel x (cm): v44 -46, v50 -22, v54 26, v58 65, v64 124, v70 182, v80 290 (170 cm in 20 frames).
- mv_dash_base: stick only, run start stamp: v44-64 680 -> 687 (7 cm in 20 frames), v80 766.
- ABP state is still not logged (no state machine API from Python): the clip has to be read from the frames.

## Files
dash_r3_mv_dash{,_base}.mp4 (7 views), _side/_behind/_front34/_game.jpg grids (f14-139, step 4).
Identity traits: docs/notes/reference-dossiers/dash-ganon.md; curves: docs/notes/reviews/dash-ganon/curves-sheet.md.
