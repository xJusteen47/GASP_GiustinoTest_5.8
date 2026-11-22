# Calibrazione Cieca 2C — Gemini 4 Neon High (2026-11-22)

- **Stato:** **IN CORSO (`1/4` completato — pausa voluta per budget finestra 5h)**
- **Soglia Regola 2C:** `>= 3/4` verdetti concordi con Fable E **`0` falsi PASS** per abilitare `PASS` autonomo di `gemini-4-neon-high` su `M3` (sempre con spot-check Claude su `1 PASS su 3`). Finché incompleta: Neon resta **FAIL-only / pre-screening**.

| Caso | Build SHA | Ground Truth Fable | Neon Cieco (`gemini-4-neon-high`) | Match | Falso PASS | Note / Durata / Costo |
|---|---|---|---|---|---|---|
| `K6 r1` (dropkick) | `5a821c53` | `FAIL (posa)` (`2bd9b2ef`) | **`FAIL (posa)`** | **SI (1/1)** | **NO (0)** | `412.4 s`, `0 $` (`agy_student`) |
| `K6 r2` (dropkick) | `84b7c354` | `FAIL (posa)` (`f6adaef8`) | *PENDING* | — | — | Cartella cieca in `%TEMP%\cal2c\case_2_k6_r2` (**solo `.jpg`**) |
| `v27 r2` (turn 180L) | `22300411` | `PASS` (`a42d8c8a`) | *PENDING* | — | — | Cartella cieca in `%TEMP%\cal2c\case_3_v27_r2` (**solo `.jpg`**) |
| `v28 r3` (turn 180L style) | `3c779b08` | `FAIL (posa)` (`52802e1e`) | *PENDING* | — | — | Cartella cieca in `%TEMP%\cal2c\case_4_v28_r3` (**solo `.jpg`**) |

## Dettaglio `K6 r1` (`case_1_k6_r1`, blind session `8048451a-931c-4e4d-afef-92dba905883a`)
- **Allineamento con Fable:** pieno. Neon cieco ha individuato esattamente tutti e 3 i gap principali di Fable:
  1. `a15-a24` (`grid_high_settle.jpg`, `az90`/`az315`): corpo congelato da statua senza overshoot leggibile, senza flick testa (`a20`) e senza rilascio/risollevamento avambracci.
  2. `a4-a12` (`tiles_*`, `az90`/`az315`): braccia parcheggiate sul petto, press (`a4`) e relax (`a8`) indistinguibili da `K5` a velocità reale.
  3. `cattura`: vista `az180` da dietro assente; framing `az90`/`az315` taglia testa (`a3-a6`) e piedi colpenti (`a15-a24` e `high`).
- Ha anche confermato `ASSERT OK`, lo snap `a13-a15` e il miglioramento della linea corpo Big Band `j.HK`.

## Nota operativa anti-quota per i 3 casi rimanenti
- `read_file` campiona i video `.mp4` a **1 fps** (`5` frame totali su `slow4x`), quindi passare i `10 .mp4` ha allungato `K6 r1` a 412 s e consumato molta quota 5h a fronte di quasi nessun dato aggiuntivo rispetto ai grid.
- Sui 3 casi restanti (`case_2_k6_r2`, `case_3_v27_r2`, `case_4_v28_r3`), istruire `gemini-4-neon-high` a leggere **solo le griglie `.jpg` + `brief.md` + `assert*` (+ `facing_*.csv`/`gates_*.json` solo dopo le immagini)**, senza `.mp4` multipli. Le fonti binarie sono già fetchate nei commit `84b7c354`, `22300411`, `3c779b08`.
