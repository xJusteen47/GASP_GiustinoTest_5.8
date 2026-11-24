# C3 pulizia: registro clip, publish/approve, stop_sync a eventi (cloud, Opus medium, budget 5 $) - branch claude/project-thread-q8ph8z
Base 49998c49 (claude/dair-v9-stops-review = PR #38). Riuso: animpipe verbs.py (contratto una riga json), life.py, life_calibrate.py.
ESITO: OK (1-3 fatti; stop_sync BLOCCANTE dopo la scelta owner A, life_gate.json scritto con CALIB OK 12/12)
FATTO
- `Tools/AnimPipeline/registro-clip.csv` (colonne del piano §4 + `note`): 9 righe da anim-approvate.md + tag approved/* + branch
  approved-*. 8 approvate (run start v4r5 v4r7 w1 w2 w3, jab v12, ftilt v31, dair v9), K7 `bloccata` (niente tag, owner 11-24).
- `docs/notes/anim-approvate.md` = vista generata (`python -m animpipe registro view`); la sezione Esenzioni resta a mano
  (parole owner) e i nomi AS_/.casc restano nelle righe: guard, anim_layer_ops e capture_move la leggono come prima.
- `animpipe/registry.py` + verbi: `publish <mossa> <ver> --from=<uasset,..> --review=<dir>` rifiuta senza assert.txt tutto
  `ASSERT: OK`, `final/review_pack.json` complete e `final/verdict.json` {level 2, verdict PASS, blind_critic PASS, clips}
  che nomina l'asset; copia byte per byte in `Content/AnimPipeline/Published/<mossa>/<ver>/` + publish.json (sha256); mai
  sovrascrive. Riga gia' `approvata` (jab v12, ftilt v31, dair v9) = solo archivio, stato invariato.
  `approve <mossa> <ver> --owner "<parole>"`: riga PASS, file committati, `git lfs lock`, tag annotato su HEAD (mai spostato),
  vista rigenerata. `registro check|view`. `.gitattributes`: Published lockable (unica eccezione a "senza lock"); `.gitignore` la apre.
- stop_sync: EVENTI per secondo (greedy: fermata di tante parti sullo stesso frame = 1; due fermate vicine = 2), soglia
  2,5/s (quota per parte resta come secondo numero); `shown` (card, voce di taratura o --shown=a-b:motivo, motivo obbligatorio) toglie solo i frame mai a schermo.
PROVA (python Tools/AnimPipeline/life_calibrate.py [--write-gate] -> Saved/AnimPipeline/life_calibration_result.json): 12/12.
  PASS stock 0,95/0,48/0,43/1,91, w2 w3 ftilt 0; FAIL jab v12 3,24, K7 4,0, jabfk3 6,49, K8 r2 8,0. Unico errore: dair v9
  (attesa PASS) 5,29/s con a4, a17, a25; a35 tolto (montage finisce ~a34). a17 e a25 sono fermate VERE per il critico cieco.
  Owner 02:19Z "Deve fallire." (scelta A): dair v9 attesa FAIL in taratura, resta approvata e bloccata come base. Gate: still_runs, stop_sync.
- a4: viva in gioco per il critico (capsula sale), ma sul dump dell'asset bacino 0,2 cm (nucleo < 1): serve un dump in gioco (C4/C5).
Test: 10 nuovi; fix card_path (approved/<mossa>.json) = 5 test verbi verdi; suite 290 ok, 3 rossi + 1 errore gia' presenti.
PER OWNER / COORDINATORE
- Ora una variante che eredita le fermate a17/a25 della dair v9 o del jab v12 e' bloccata da stop_sync (senza esenzioni owner).
- C4: publish legacy di jab v12/ftilt v31/dair v9 poi `approve` (lock; il tag c'e' gia'). La copia in Published e' un archivio:
  i montage/kit puntano ancora al percorso di lavoro finche' un editor non li reindirizza. C2: CLAUDE.md "senza lock" -> eccezione Published.
- INDICE-PROGETTO §5.x riga pipeline: "Stato clip: Tools/AnimPipeline/registro-clip.csv (animpipe publish/approve/registro); anim-approvate.md = vista generata".
SUBAGENTI: 0
COSTO: ~3,5 $ (stima: Opus medium, ~150k token di contesto) | 0 minuti desktop | 0 token review
