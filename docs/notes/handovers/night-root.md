# NIGHT-ROOT handover: montage root lock tried, measured WRONG, reverted (desktop, 2026-11-16)
Branch `claude/night-knockdown` after 8301cf41. Folder `_ue59`, Opus high. ABP and C++ back to 8301cf41 (ABP byte-identical to the backup
`Saved/Backups/GAS/nightroot/`); editor rebuilt. Kept: per-frame root/feet logging in `pie_capture_move.py`, reader `move_root_rows.py`. No map saved.
## What was done
- Root keys of every pipeline clip (`reviews/night-root/roots.txt`, `measure_roots.py` headless): all 8 reactions and all Overlord moves have
  the root at a constant (-8.6, -13.8, 0) = 16.3 cm, no travel, no yaw.
- Lock built (4 nodes after the DefaultSlot, alpha 1 while a montage weighs, `abp_diff.txt`: nothing else changed), captured in game:
  knockdown root-capsule 16.3 -> 0 (`rows_kd_before/after.txt`, `react_feet_rows.py`), locomotion unchanged (walk_into, jump_only root 0 in
  both, `rows_move_*.txt`, `move_root_rows.py`). BUT feet after the get-up slid MORE: footL 11.6 -> 27.6 cm, footR 13.6 -> 21.8.
## Why (the night-abp diagnosis was wrong)
- Component space (root included) the pipeline poses ARE centred: `cs.txt` (`measure_cs.py`): Jab1V2 pelvis (2.1,-2.5), feet (19.5,12.5)/(-14.1,-20.0)
  = GASP `M_Neutral_Stand_Idle_Break` pelvis (2.1,-2.5), feet (20.0,12.5)/(-14.5,-20.4). The 16 cm root is compensated in the pelvis;
  zeroing it draws every reaction/move 16 cm off. The root offset is harmless (and the dropkick/dair Body Capsule offsets are fine as they are).
- Real cause of the creep: the stance the game settles in (MM idle + Procedural_Feet) is NOT the pipeline reference stance. In game, idle feet
  in mesh space ~(15.1, 1.7)/(-15.5, -8.2) vs the clips' (19.5, 12.6)/(-14.2, -20.0): ~11-12 cm per foot. Every montage pops the feet on its
  first frame and slides them back on blend out: jab1 f21->22 footL x 1.3 -> 12.4, back over f46-90 (`Saved/nightroot/move_before/jab1/frames.jsonl`).
  The get-up's 20-frame hold-blend just makes that slide visible. The pelvis dip (-5.5) is in the same blend, with or without the lock.
## Options for the owner (rule 12: his "fix the ABP" rested on the wrong diagnosis)
- A (recommended): anim data. Measure the game's settled idle pose once (bones, mesh space) and make it the pipeline's reference stance for
  the first/last frames of every move and get-up/roll end. ~3-5 $ measuring + per-move re-export; no ABP risk.
- B: ABP. Plant the feet through montage blend-in/out (foot lock in Procedural_Feet or a two-bone IK to the pre-blend foot positions). Touches
  GASP foot placement: needs its own design thread, ~8-12 $.
- C: make the game idle use the pipeline stance (MM idle choice): GASP data change, risky, not recommended.
## Not done
- dair/dropkick before/after (they need kits `KIT_Test_OverlordDAirV9` / `KIT_Test_OverlordDropkick`; the default kit did not fire them): moot after revert.
- Tool note: Run-Timed.ps1 `-Kind Test` with a -1 exit code throws an Int32 cast error; script paths with spaces break `-run=pythonscript`.
Cost: ~6 $ estimated (no subagents).
