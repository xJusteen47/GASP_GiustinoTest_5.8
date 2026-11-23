# turnaround-180l-c2 (2026-11-23): Turnaround 180L da base 27 (w2t180Lp) + fisica 29 w3 (3 round, Regola 10)
Branch: `claude/deepseek-turn4` (worktree `deepseek_turn4`). Pipeline `gstn-anim-reviewer` + `gstn-anim-executor` (0-token UE/catture + review indipendente `gemini-4-neon-high` su `az180`, `az90`, `game`, `grid_pivot_v84_116`).

## Cosa è stato fatto (`Tools/AnimPipeline/cards/runstart/turnaround_180l_ue.py`)
- Ripartito dalla base pulita `AS_GSTN_RunStart_Generic_w2t180Lp` (27) con assi locali ancorati al bacino/petto (mai ai piedi) e fisica `29 w3` (falling-forward COM +11 cm, drop -5 cm, knee drop 0.28, lag/catch + 2-bone leg IK, banking torso 14°, lean pivot 14° + run 5.5° fino a f34, head lift +6.5°).
- 3 round di posa (r1 -> r2 -> r3): risolti `GATES: OK` (`gap min 13.6 cm`, `bone worst 0.5%`, `jitter max 6.0`), `ASSERT: OK` (`27_p` e `28_s`), conservation mani entro `+3%`.

## Esito Review indipendente (`gemini-4-neon-high`, r3)
- **VERDETTO:** `FAIL` (Regola 10: stop a 3 round, decisione owner con video in `docs/notes/reviews/turnaround-180l-c2/fine/`).
- Miglioramento netto su peso/caduta in `v96–v116` (`az90` e `game`), ma 3 gap residui:
  1. Gomito sinistro ancora aperto a ~90° a `v92` (ereditato dalla posa `out_s` di `27` in `runstart_pivot_compose.py`) e residuo `v100–v104`.
  2. Raddrizzamento brusco `v122 -> v137` all'aggancio con il loop standard.
  3. Braccia/spalle simmetriche a `v137` in `az180`.
- Prossimo passo consigliato se owner vuole proseguire: integrare `w3` + braccio L pulito direttamente in `runstart_pivot_compose.py` / Cascadeur invece di correggere `27` in post via UE.

COSTO: 0 $ (3 round UE/cattura deterministici + 3 review agy_student neon-high) | provider=agy_student | quota_pro=0%
