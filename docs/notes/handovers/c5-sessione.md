# C5 pulizia: sessione editor unica + cache capdet, kit TBK5/6, dump dair in gioco (fisso, 2026-11-24) - branch claude/c5-session-2026-11-24
ESITO: PARZIALE | cattura nella sessione viva e base K7 dalla cache funzionano (giro editor K8 191,7 s -> 107,2 s), ma la cattura NON e' deterministica nemmeno a editor nuovo
Riuso: editor_session.py (coda lavori, modo boot), round_runner.py (submit/ping), capture_move._run_key/_cached_dir + manifest (cache capdet), pie_capture_move.py, Save-Assets.ps1.
FATTO (base integ 314b04a7; _ue59 portata qui, file locali dell'owner intatti, copie in `_scratch/c5-backup-2026-11-24/`)
- `session_capture.py`: il comando pie_capture_timed diventa un lavoro "boot" della sessione viva (editor sul 2o monitor via vd2_mover);
  `capture_k8_fast.py --session [--keep]`. Verdetto = `<out>/assert.txt` come Run-Timed.
- 3 bug trovati provando sul fisso (mai provati prima): (1) `Run-Timed.ps1` lanciato da Windows PowerShell 5.1 (`powershell -File`, cosi'
  lo lanciano round_runner, capture_move, session) rifiutava OGNI editor: precheck riceveva le virgolette di -ExecCmds non escapate. Fix
  solo su 5.1, SelfTest 7/7 su pwsh e 5.1. (2) `boot_session` con DETACHED_PROCESS: powershell non fa nulla ed esce 0 -> CREATE_NO_WINDOW
  (round_runner e session_capture). (3) `regress.py` rifiutava ogni build dopo il publish di C4 (le copie Published non esistono ai tag):
  ora si confrontano con lo sha256 di publish.json. test_capdet2: 41/42, il rosso (`loose_file_without_lock`) c'era gia' su integ (fix 2 di C4).
- Build _ue59 OK. Kit: `Tools/GAS/fix_kit_implant_refs.py` via Save-Assets: TBK5, TBK6 e anche TBK7, TBK8, TBR4 avevano la parte
  Implant_Back senza mesh; ri-puntate a /Game/GSTN/Equipment/Placeholders/SM_Implant_Placeholder (commit d935ba90).
MISURA (`Tools/AnimPipeline/round_bench_k8.py before|after`, Saved/C5Bench/*.json; passi editor = dump giunti K7+K8, cattura whiff+hit, 3 viste, fastcap)
- PRIMA (come oggi): sessione 49 s + dump 0,5 + chiusura ~7 + cattura K7+K8 in editor nuovo 127 s = **191,7 s**.
- DOPO: sessione 48 s + dump 0,5 + cattura solo K8 nella sessione 57,6 s (K7 = cache HIT, chiave invariata) = **107,2 s** (-44 %).
  Con la sessione gia' aperta dall'import (--keep) il passo cattura e' 58 s contro 127. K8 da solo in editor nuovo: 91,6 s.
- Non misurato: author/build/check animpipe (le sorgenti Rig K7/K8 sono solo sul portatile; il giro r2 di 25 min aveva 9 min di cattura).
DETERMINISMO (`capture_pair_diff.py`, il punto da guardare)
- Editor nuovo contro editor nuovo, stessa run K8: spawn diverso (y 150 / -450 / 450 nelle 3 catture: PlayerStart non fisso), traiettoria
  fino a 34 cm e mani fino a 32 cm diverse nella finestra f138-160, montage sfasato di ~0,25 frame. Sessione contro editor nuovo: whiff
  nello stesso intervallo (37/30 cm); hit peggio (85 cm a f170) ma la run "solo K8" non resetta lo spawn tra whiff e hit (anche a editor nuovo).
- Quindi una base dalla cache (o da un'altra sessione) NON da' gli stessi fotogrammi della variante: regola 10 non e' garantita da nessuna
  delle due strade. Da fare prima di usare la cache per review: spawn fisso dell'host (teletrasporto a un punto dato, non PlayerStart),
  reset spawn prima di OGNI sequenza, allineamento del montage al frame intero; poi `capture_pair_diff` deve dare < 1 cm.
DAIR v9 IN GIOCO (domanda C3, dalla cattura C1, `ingame_motion_dump.py`, docs/notes/life-calib-run/dumps/ingame_dair_v9_*_c1.json)
- a4-a11: la capsula sale 17-19 cm a frame; la posa rispetto all'attore si muove 1,1-1,9 cm ad a4-a5, 3-5,7 cm ad a6-a8. La a4 "vive"
  solo come blocco che sale: dentro il corpo e' ferma. Giudizio (vivo o statua che sale) al critico, i numeri dicono quale dei due.
SUBAGENTI: 0
COSTO: ~5 $ (stima, oltre il budget di 4 per i 3 bug) | ~35 minuti desktop (build 6, sessioni fallite 20, misure 7) | 0 token review
