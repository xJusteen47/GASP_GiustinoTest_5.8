# capdet-1: cattura deterministica, parte 1 (punti 1-4 delle "7 aggiunte")
Branch claude/project-thread-uvohng (base claude/runstart-generic-desktop). Macchina: laptop, nessun editor avviato. Non aggiorna _ue59.
## Cosa c'e' (tutto riuso, 3 file nuovi + 2 modifiche piccole)
- `Tools/AnimPipeline/capdet/capdet.py <spec.json> [--stage gates|capture|post|all] [--dry]`: un comando, ordine gate -> cattura -> ASSERT/video.
  Un gate che fallisce o manca (GATES: MANCANTE) = niente editor, niente cattura, exit 1. Stampa max 5 righe, il resto in `Saved/Capdet/<nome>/capdet.json`.
- Gate pre-cattura (punto 4): `runstart_pivot_gates.py` (invariato: caviglie >= 10 cm, punte nel range stock, ossa +-5 %, jitter gomiti/ginocchia/polsi,
  compenetrazioni) + `capdet/basecmp.py` NUOVO: ampiezza, velocita' di picco (mani/piedi relativi al bacino, mediana su 3 frame) e root motion contro la clip
  base, limite 15 % (regola 10). Solo perdite; sotto soglia (10 cm, 30 cm/s, 5 cm) nella base = riportato, non giudicato.
- Cattura (1, 2): `pie_capture_move.py` ora ha `PIE_CAPTURE_VIEWSET=det` = 4 viste fisse in UNA sessione: behind (az180), side (az90), q34 (az315), close
  (az90, 170 cm, mira ai piedi, fov 40) con geometria per vista (VIEW_GEOM); `PIE_CAPTURE_SEED` (RNG python, VFX spenti: la sim non ha RNG). Resta
  1/60 s per frame (t.OverrideFPS 60), sim 62.5 Hz, luci di riempimento fisse, input scriptati. Timeout: `pie_capture_timed.ps1` -> Run-Timed (15 min,
  chiude i processi, `run.log.esito` = ESITO: TIMEOUT, copiato in `esito.txt`). `-Monitor N`: la finestra editor sul secondo schermo (best effort).
- ASSERT (3): `capdet/post.py`: peso clip >= 0.8 nella finestra (capture_assert.py), righe = PNG = `frames` (durata), nessun frame nero (luma < 3),
  nessun frame uguale al precedente nella finestra (`hold_ok` per le pose tenute). Prima riga "ASSERT: OK" o "ASSERT: FAIL <motivo>", max 5 righe.
- Video: 2x2 a 60 fps (`split.mp4` se c'e' ffmpeg, altrimenti `split.webp` animata con Pillow). Fingerprint `fingerprint.json` per vista;
  `capdet.py --verify <capA> <capB> <seq>` dice se due run danno gli stessi frame (DETERMINISM: OK/FAIL).
## Verifica
`python Tools/AnimPipeline/tests/test_capdet.py` -> 18 test OK su dati sintetici (basecmp, ASSERT, ordine gate/cattura). Esempio spec: `capdet/specs/runstart_generic_g1.json`.
## NON verificato (serve l'editor, non l'ho avviato)
1. Una cattura reale con VIEWSET=det (camere close/q34, `-Monitor 2`, che i PNG escano uguali in 2 run): prima cosa da fare sul fisso, poi `--verify`.
2. Il portatile ha 1 solo schermo e niente ffmpeg: video in webp; "tempo reale a schermo" = i frame vengono da scene capture a passo fisso, non da registrazione schermo.
3. `runstart_pivot_gates.py` vuole `Saved/AnimPipeline/runloop/loop_relaxed_dump.npz` e i joints json della clip (export dall'editor, non c'e' qui).
4. Le viste det assumono l'ABP/kit di runstart; altre mosse: stesse viste, cambiare solo `expect` e `gates`.
Punti 5-7 (montaggio A/B ffmpeg, cache per hash, regressione pose bloccate) -> thread nuovo.
COSTO: 1.5 $ stimati | 0 min desktop | 0 token review
