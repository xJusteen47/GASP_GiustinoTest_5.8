# capdet-2: montaggio A/B, cache a hash, regressione pose approvate (punti 5-7 delle "7 aggiunte")
Branch claude/project-thread-nfhw4i (base claude/project-thread-uvohng 93f823b2, capdet-1). Macchina: cloud, nessun editor. Non aggiorna _ue59.
## Cosa c'e' (3 file nuovi in Tools/AnimPipeline/capdet/, 1 file di test, 2 modifiche piccole)
- **5. `montage.py <base_cap> <var_cap> <sequence> [--spec s.json]`**: prende le due cartelle di cattura che capdet.py ha gia' scritto,
  sceglie UNA lista di frame e la usa per tutti gli output, cosi' griglia e video non possono sfasarsi di un frame. `grid_<vista>.png`
  UNA GRIGLIA PER VISTA (righe = frame, colonne = base | variante, ogni cella etichettata con vista/frame/lato) + `sb_<vista>.mp4`
  side-by-side a 60 fps; senza ffmpeg (il portatile) diventa `sb_<vista>.webp` animata, stessi frame e stesso timing. I frame vengono
  da `"ab": {"hit": f, "faf": f, "around": 3, "video": [a, b]}` nello spec, o da `--frames/--hit/--faf`; senza hit/FAF usa gli estremi e
  il centro della finestra `expect`. Nessun modello e' coinvolto. Un frame che manca a una delle due catture e' FAIL, non una griglia
  spostata in silenzio. `montage.json` elenca gli output con la lista di frame usata.
- **6. `cache.py key|check|save|reuse|clear <spec.json> [--kind capture|build]`**: la chiave e' l'hash degli INPUT, non il mtime (un
  checkout riscrive i mtime e invaliderebbe tutto). Entra nella chiave: lo spec senza le chiavi che non cambiano un pixel (`monitor`,
  `max_minutes`, `name`), i file di `cache.inputs` (pose, sheet delle curve, dati) e il C++ di `cache.build_inputs` (default
  `Source/**`, `Plugins/*/Source/**`, l'.uproject: binari nuovi possono spostare una posa). Un input che non esiste e' registrato come
  `missing`, quindi una posa assente non puo' mai sembrare un hit. Store in `Saved/Capdet/_cache/<kind>/<key>/` (i PNG dei frame NON si
  copiano, solo report e video, salvo `--with-frames`). Agganciata a `capdet.py --stage all`: hit -> riusa gli output e **l'editor non
  parte** ("CACHE: HIT" + "CATTURA: SALTATA"); si salva solo una run che e' PASSATA. `--no-cache`, `--refresh` per forzare.
- **7. `regress.py [check] [--record <tag>] [--list] [--tol-cm 0.1]`**: per ogni riga di `docs/notes/anim-approvate.md` e ogni tag
  `approved/*`: (a) l'asset versionato si confronta col blob **allo stesso tag** (`git rev-parse <tag>:<path>`); per un file LFS il blob
  e' il pointer, che porta lo sha256 del contenuto, quindi vede l'animazione cambiata. Nessun lock file: il tag E' la baseline.
  (b) i file non in git (il `.casc` sotto `Saved/`) hanno lo sha256 in `docs/notes/anim-approvate-locks.json`, scritto una volta con
  `--record <tag>`. Una riga non verificabile e' FAIL e stampa il comando che la registra. (c) se esistono sia
  `docs/notes/approved-dumps/<tag-slug>.json` che il dump corrente, confronto giunto per giunto, frame per frame, tolleranza 0,1 cm.
  Un tag senza riga e una tabella vuota sono FAIL (non un pass per mancanza di dati).
  **Agganciata al build**: `Run-Timed.ps1 -Preset Build` lo esegue prima e **non compila** se FAIL (exit 4); `-SkipRegress` bypassa e
  l'output lo dichiara.
## Verifica (fatta qui, offline)
`python Tools/AnimPipeline/tests/test_capdet2.py` -> **42 test OK** su dati sintetici (liste di frame, frame mancante, fallback webp,
invalidazione cache, mtime che non invalida, `[]` != assente, repo git usa e getta con tag e file cambiati, tolleranza giunti).
`regress.py` girato sui dati VERI: 4 righe, 4 tag -> **FAIL 3/4**, e sono buchi reali, non bug dello script:
1. `AS_GSTN_RunStart_v4r5.uasset` **non esiste nell'albero del suo tag** `approved/runstart-v4r5`: la versione approvata non e' mai
   stata committata sotto quel tag. Decisione dell'owner: ri-taggare sul commit che contiene l'asset, o accettare il casc come baseline.
2. I `.casc` di v4r5 / w1 / w2 sono sotto `Saved/` (non in git) e non hanno lock: `regress.py --record <tag>` va lanciato **sul fisso**,
   dove quei file esistono, e `anim-approvate-locks.json` committato. Da qui non si puo' fare (i file non sono nel container).
   `approved/runstart-v4r7` passa gia' del tutto: il suo casc e' tracciato in git.
## NON verificato (serve il fisso)
1. `montage.py` su due catture reali: i frame sintetici provano la logica, non le viste `det` vere.
2. Il gate nel Build di `Run-Timed.ps1`: PowerShell non gira qui. Il blocco e' piccolo e isolato nel ramo `'Build'` dello switch.
3. `cache.py` su una cattura reale: il riuso e' testato su cartelle finte. Primo giro sul fisso con `--refresh` e poi senza.
4. `tests/test_capdet.py` (di capdet-1) non e' rotto ma e' LENTO qui: la classe `Post` ci mette 144 s (7 test, ffmpeg nel container).
   Passa, con e senza le mie modifiche. Chi lo lancia metta un timeout generoso o `-k` sulla classe che gli serve.
COSTO: 2.0 $ stimati | 0 min desktop | 0 token review
