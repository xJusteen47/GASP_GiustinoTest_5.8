# turnaround-180l-c3 (2026-11-23): Turnaround 180L agile platform / Link-OoT + 29 w3 -> PASS
Branch: `claude/deepseek-turn4` (worktree `deepseek_turn4`). Pipeline `gstn-anim-reviewer` + `gstn-anim-executor`.

## Interventi (`Tools/AnimPipeline/cards/runstart/turnaround_180l_ue.py`)
- Base pulita `27` (`w2t180Lp`) + fisica **`29 w3`** (`PELVIS_FWD=12 cm`, `PELVIS_DROP=-1.5 cm` no crouch-in-place, `KNEE_DROP=0.35`, `SWING_LAG=4 cm`, `CATCH_FWD=5.5 cm`, `SPINE_LEAN_PIVOT=10°`, `SPINE_LEAN_RUN=5°` fino a `f34`).
- Spalle pari/alte (`TORSO_BANK=5°`, `CLAV_L_ELEV=7°`, `CLAV_R_ELEV=3°` — eliminata spalla L cadente).
- Piedi reattivi (`STEP_NARROW=2.5 cm`, `PIVOT_HEEL=14°` sull'avampiede R già in `f2-f6` — eliminata "A" larga e piatta).
- Braccia scattanti (`ARM_L_TUCK_IN=14°`, `ARM_L_BACK=24°`, `ARM_L_ELBOW=16°`, `ARM_R_FWD=22°`, `ARM_R_ELBOW=24°`).

## Verifica e Review finale
- **`GATES: OK`** (`gap_min=11.9 cm`, `bone_worst=0.5%`, `jitter_max=9.5`); **`ASSERT: OK`** (`27_p` e `28_s`, `mv_turn_180a` `88..132`).
- **Review indipendente `gemini-4-neon-high` (`az180`, `az90`, `game`, `grid_pivot_v84_116`):** **`PASS`** (vita/peso/continuità coerenti con Link OoT / `29 w3`; 2 rifiniture minori non bloccanti su `v92` gomito L e `v122->137`).

COSTO: 0 $ (UE/catture deterministici + review agy_student neon-high) | provider=agy_student | quota_pro=0%
