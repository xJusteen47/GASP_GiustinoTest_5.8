# Calibrazione Cieca 2C — Gemini 4 Neon High (2026-11-22)

- **Stato:** **COMPLETATA — SUPERATA (`3/4` concordi con Fable, `0` falsi PASS)**
- **Soglia Regola 2C:** `>= 3/4` verdetti concordi con Fable E **`0` falsi PASS** per abilitare `PASS` autonomo di `gemini-4-neon-high` su `M3` (sempre con spot-check Claude su `1 PASS su 3`). **Esito:** `gemini-4-neon-high` abilitato su `M3` **con solo griglie `.jpg`** (niente `.mp4` multipli).

| Caso | Build SHA | Ground Truth Fable | Neon Cieco (`gemini-4-neon-high`) | Match | Falso PASS | Note / Durata / Costo |
|---|---|---|---|---|---|---|
| `K6 r1` (dropkick) | `5a821c53` | `FAIL (posa)` (`2bd9b2ef`) | **`FAIL (posa)`** | **SI (1/1)** | **NO (0)** | `412.4 s` (`404.9 s` agy), `0 $` (`agy_student`), `.jpg`+`.mp4` |
| `K6 r2` (dropkick) | `84b7c354` | `FAIL (posa)` (`f6adaef8`) | **`FAIL (posa)`** | **SI (2/2)** | **NO (0)** | `418.8 s` (`405.6 s` agy), `0 $` (`agy_student`), **solo `.jpg`** |
| `v27 r2` (turn 180L) | `22300411` | `PASS` (`a42d8c8a`) | **`FAIL (posa)`** | **NO (falso FAIL)** | **NO (0)** | `377.9 s` (`373.1 s` agy), `0 $` (`agy_student`), **solo `.jpg`** |
| `v28 r3` (turn 180L style) | `3c779b08` | `FAIL (posa)` (`52802e1e`) | **`FAIL (posa)`** | **SI (3/4)** | **NO (0)** | `354.8 s` (`352.9 s` agy), `0 $` (`agy_student`), **solo `.jpg`** |

## Dettaglio dei 4 casi ciechi
1. **`K6 r1` (`case_1_k6_r1`, session `8048451a-931c-4e4d-afef-92dba905883a`):** pieno allineamento con Fable (`a15-a24` congelato in `grid_high_settle.jpg`, `a4-a12` braccia immobili in `tiles_*`, framing `az90`/`az315` troncato su testa/piedi + vista `az180` assente; confermati `ASSERT OK`, snap `a13-a15`, linea Big Band `j.HK`).
2. **`K6 r2` (`case_2_k6_r2`, session `b2866026-ff4c-4521-adc7-a29b7f070178`, solo `.jpg`):** pieno allineamento con Fable (`FAIL (posa)` solo braccia: flicker/snap `a0-a3`, pugno dx davanti al mento a `a3`, silhouette collassata `az180` `a3-a5`, tuck rigido `a4-a13`; confermati `settle overshoot` `a16-a24`, identità e linea corpo `j.HK`).
3. **`v27 r2` (`case_3_v27_r2`, session `cdd7d469-e4ca-4426-bf93-cd1f816ff248`, solo `.jpg`):** falso FAIL (più severo di Fable). Ha comunque confermato `ASSERT OK`, `v84-v94` torso più eretto rispetto a `26`, binari paralleli `v98-v110` e uscita run `v125-v140`; ha bocciato su `assert_pivot_weights.json` (`ok: false` `[82,104]`), stallo/unwind in `facing_v82-111.csv` (`v93-v102`), piede `R` a `0°` (`v88-v92`), skating `v98-v104` e pop braccio `L` a `v106` (anche per assenza `clip-check/metrics.json`).
4. **`v28 r3` (`case_4_v28_r3`, session `c0af673f-d3d7-497e-8d99-7191204273f6`, solo `.jpg`):** pieno allineamento con Fable (`FAIL (posa)`: *chicken-wing* `L` + chiusura `R` su `v102-v107`, hand-on-hip `L` e `R` lungo coscia su `v94-v102`, braccia passive `v86-v92`; confermati pivot `v86-v94` 8 frame, perno `R` e `gates_*.json` verdi).

## Nota operativa finale
- La modalità **solo `.jpg`** (`grid_*.jpg` + `tiles_*.jpg`, senza `.mp4` multipli campionati a 1 fps) ha confermato piena capacità di diagnosi su pose e difetti frame-by-frame.
- `gemini-4-neon-high` risulta calibrato con profilo **strict / conservativo** (`0` falsi PASS, `3/4` match): abilitato a `PASS` autonomo su `M3` (con spot-check Claude su `1 PASS su 3`). In review di locomozione, includere `clip-check/metrics.json` se disponibile per evitare FAIL automatici su `ROOT` per dato mancante.
