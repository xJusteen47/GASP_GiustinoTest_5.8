# Handover anim pipeline: fase B, jab v12 e Ganondorf v3.1 sul rig (2026-11-02, branch claude/project-thread-s125cq)

Esito: **le due mosse sono bakate dal motore unico** (`python -m animpipe build cards/<mossa>.json`, ~10 s ciascuna):
`/Game/AnimPipeline/Rig/AS_GanonFTiltV31_Build` (11 pose, 10659 chiavi) e `AS_JabV12_Build` (9 pose, 8685 chiavi).
Checkpoint 2 è nella storia git di questo file. Non esistono file Step dell'owner per queste mosse: le pose sono i frame
chiave delle clip v3.1/v12 (Ganondorf: i 5 frame dell'owner a 0/7/10/27/37 più 6 intermedi).

- Checkpoint pose (una griglia per mossa, `Tools/AnimPipeline/sheets/`, non in git): `ganon_ftilt_v31_poses.png` (riga in
  alto = i 5 frame dell'owner), `jab_v12_poses.png`; build sopra sorgente: `ganon_ftilt_v31_poses_ab.png`.
- Ganondorf, difetti risolti con `edits` nella scheda (rotazioni FK risolte dallo Jacobiano del rig, `fk_solve.py`):
  gomito dietro fuori profilo da 31 a 13 frame (gomito ~20 cm dietro il petto invece di 33-35, chiavi 20-32);
  avambraccio sinistro nel fianco allo scatto: clearance al frame 10 da -4,0 a +4,1 cm (mano 28 cm di lato, era 13).
  Bloccanti 5/6 come la sorgente: legibility FAIL ereditato (controlli da pugno su un calcio), ma migliore della sorgente.
- Jab v12: bloccanti 6/6 PASS (come la sorgente). **Pugno dietro non risolto** dopo 2 tentativi mirati (togliere la
  torsione della mano 1-3: 70°/frame, peggio; mano + braccio: 36,5°, uguale). I 36° del frame 0->1 vengono dallo swing
  del braccio di guardia nel contatto di 1 frame, non dalla torsione: serve una decisione di posa (domanda 1).
- Motore: ultimo frame ora bakato giusto (la sezione tornava al frame 0 a t = durata; play rate 0,99999); `lock_lengths`
  (stretch spento sul bake, traslazioni locali fisse; tolto fino a 1,56 cm sul collo) -> bone_lengths PASS su entrambe;
  scheda: `edits`, `drop`, `spacing` per posa, `sheet_extra`; `check --src` (stessi controlli sulla sorgente); planted
  solo sui piedi della scheda; `probe <take>` (canali del rig); `poses` (griglia, `--over`, `--blind`).
- Da guardare: ginocchio dietro del jab 9 cm più avanti della sorgente in guardia (IK gamba del rig, frame 0 e 18-37);
  spaziatura Ganon 3-7 a 0,17 (sorgente 0,78); motion_check FAIL identico alla sorgente su entrambe.
- A/B cieco (Sonnet, A = sorgente, B = build): jab B di poco (pugno più pulito dal busto al contatto); Ganondorf B
  (recupero più basso e piantato, più vicino al riferimento), ma segnala al frame 10 vista frontale il braccio sinistro
  che si chiude a triangolo verso il busto: effetto dell'abduzione, da guardare in gioco.
- Non fatto: giudizio in gioco (montage + hitstop + camera), per budget. Editor chiuso col kill (asset salvati dal build).
  Scratch in `/Game/AnimPipeline/Rig`: `AS_/LS_Jac1_Build`, `Jac2_Build`, `GanonAxisProbe_Build` (cancellabili).

Domande per l'owner:
1. Jab: il braccio di guardia scatta indietro col busto nel frame di contatto e il pugno gira. Lo tengo fermo al mento
   (la guardia segue il petto, cambio di posa al frame 1) o accetti lo scatto stile Mario?
2. Ganondorf: braccio dietro ora vicino al corpo nel recupero e braccio sinistro aperto allo scatto (griglia `_ab`): ok?

Costo stimato: Opus ~85 chiamate, contesto finale ~130k, circa $9-11; subagente Sonnet (A/B cieco) ~75k token, < $0,5.

## Checkpoint 3 (2026-11-02): jab con guardia al mento e giudizio in gioco

Decisioni owner applicate: 60 fps, round-trip 1 cm, stretch OFF + ossa 0,1 cm; 4a pugno dietro fermo al mento; 4b braccia Ganon OK.
- **Jab rebuild** (`build cards/jab_v12.json`, 15 `edits` nella scheda, nessun frame a mano): chiavi 1/2/3/5/9 su
  `upperarm_r_fk_ctrl` (gomito al posto del frame 0 nel riferimento del petto), `lowerarm_r_fk_ctrl` (pugno sotto la testa)
  e `hand_r_fk_ctrl` (orientamento del frame 0 portato dal petto). Soluzioni dallo Jacobiano del rig (`fk_solve.py` per le
  posizioni, sonde +10° su X/Y/Z per la mano). Bloccanti **6/6 PASS** (clearance minima 0,27 cm al frame 6, stretta ma ok).
- Misure nel riferimento del petto (prima -> dopo): pugno dal mento 18-21 cm -> 0,6-3,6 cm; errore di orientamento ai frame
  1-3 54° -> 9-11°. Il salto 0->1 in spazio componente (50°) ora è solo il petto che gira di 52° al contatto: la guardia
  segue il busto, il pugno non gira più contro di esso. Griglia `sheets/jab_v12_poses.png` (build sopra sorgente).
- **Giudizio in gioco** (`ingame_judge.py <card> <hitstop>`, nuovo): crea `AM_<nome>` accanto alla build, cattura ogni frame
  dalla camera laterale Duel (el 12, 450 cm, FOV 55) e ripete il primo frame attivo per l'hitstop; mp4 60 fps + striscia in
  `sheets/ingame/`. Non è PIE con l'abilità vera: hitstop ipotizzati 4 (jab) e 8 (ftilt), da tarare in combat.
  - Jab: **verde**. Leggibile (braccio teso chiaro di lato, guardia chiusa visibile), forza dal busto + affondo + hitstop;
    smoothness ok sulla striscia, motion_check FAIL ereditato (stesso della sorgente, non bloccante).
  - Ganondorf ftilt: **verde con note**. Silhouette del calcio molto leggibile, peso sulla gamba d'appoggio, braccio sinistro
    aperto non si legge come "triangolo" a 450 cm. Nota: dal frame 16 al 24 la gamba scende in un blocco solo (spaziatura
    0,17 già segnalata) e il FAF a 37 chiude in una posa raccolta diversa dalla guardia del jab.
- Montage `AM_JabV12_Build`, `AM_GanonFTiltV31_Build` in `/Game/AnimPipeline/Rig` (Content non versionato).
- Editor: riaperto direttamente in `L_PilotCapture` (DefaultLevel risultava dirty all'avvio e non è stato toccato); l'indice
  di ripristino autosave è stato spostato in `Saved/Autosaves_restore_bak/` (nessun autosave cancellato).

Pushato su `feature/gas-npp-5.8` come fast-forward puro: `deeab05` (rebase pulito sopra `13cc565`).
Prossimo: (1) Ganondorf, gamba che scende in un blocco solo ai frame 16-24 (spaziatura), non corretta qui; (2) hitstop e
camera veri in PIE quando le abilità combat usano questi montage.
Costo stimato checkpoint 3: Opus ~95 chiamate, contesto ~110k, circa $8-10.

## Checkpoint 4 (2026-11-02): anteprime senza hitstop (whiff)
- `ingame_judge.py` ora produce sempre: `<mossa>_ingame.mp4` (hitstop), `_ingame_whiff.mp4`, `_ingame_compare.mp4` (hitstop a sinistra, whiff a destra); `--cached` rifà i video dai frame già catturati senza editor.
- Rigenerati jab v12 (hitstop 4) e Ganon ftilt v3.1 (hitstop 8) con l'editor aperto; compositi `<mossa>_hitstop_vs_whiff.png` in `sheets/ingame/` (non in git), copiati nei project files.
- Whiff: pose identiche all'hitstop (il congelamento ripete solo il frame attivo). Unica nota whiff: ftilt, recupero 19->25 cade in un blocco solo (già segnalato, spaziatura 0,17); nessun problema nuovo. Animazioni non toccate.
- Pushato su feature: `061bdbb`. Costo stimato: ~25 chiamate, contesto ~60k, circa $1,5-2 (Sonnet 5.5), niente subagenti.

## Checkpoint 5 (2026-11-02): Ganondorf ftilt, gamba che scende in un blocco (spaziatura recupero)
- Diagnosi (piede sinistro, cm/frame): il segmento 10-20 era `hold_recoil` con chiave piatta a 20 -> il piede si fermava
  a 20 (0,6) e poi cadeva 22 / 19 / 12,5 ai frame 21-23 (40 cm in 3 frame).
- Tentativo 1 (scheda: posa `active` 10 con `spacing: limb_led_recovery`, niente stop a 20): 5,1 -> 14,9 -> 23,3 -> 15,1. Picco ancora.
- Tentativo 2 (tenuto; scheda: `drop` a 20 su `foot_l_ik_ctrl` Location/Rotation e `leg_l_pv_ik_ctrl` Location):
  la gamba viaggia 10 -> 23 senza fermarsi: 7,9 -> 5,6 cm/frame quasi costante, poi 7,3 / 5,4 all'appoggio 24-25.
  Nessuna posa dell'owner cambiata (0/7/10/27/37), nessun frame a mano, il resto del corpo tiene la chiave a 20.
- Rovescio: niente più "blocco", ma la discesa è quasi lineare e il calcio teso non si tiene (a f14 piede 70 cm, prima 81).
  Bloccanti 5/6 come prima (legibility FAIL ereditato); motion_check uguale; planted 1,03 cm; clearance PASS.
- Prossimo passo proposto (motore, non scheda): oggi una chiave piatta il cui segmento seguente è `limb_led_recovery` ha
  peso di uscita 0 (`ue_rig.py`, out_w None) -> nessun ease-out da 10. Dare a quella chiave il peso di uscita del profilo
  del segmento entrante (1/3) darebbe: tenuta a 10-14, accelerazione, appoggio a 23-25. Serve l'ok dell'owner (cambia il
  motore per tutte le mosse). Alternativa solo-scheda: spostare la posa `hold_end` 20 -> 17 (stop più alto, caduta su 6 frame).
- Bug strumenti corretto: `legibility.py` riusa `sheets/raw/<take>_legbones.json` per percorso asset, quindi dopo un
  rebuild i check leggevano le ossa vecchie. `animpipe build` ora cancella quel file. (I check dei checkpoint precedenti
  dopo un rebuild potrebbero essere stati letti sul dump vecchio.)
- Anteprime in gioco (`ingame_judge.py`, hitstop 8 + whiff): `sheets/ingame/ganon_ftilt_v31_*.mp4` e composito
  `ganon_ftilt_v31_recovery_before_after.png` (prima whiff / dopo whiff / dopo hitstop, frame 10-27, vista laterale), nei project files.
- Costo stimato: Opus ~40 chiamate, contesto ~75k, circa $4-5; niente subagenti. Editor lasciato aperto su L_PilotCapture.

## Checkpoint 6 (2026-11-02): ease-out opt-in, Ganondorf tiene il calcio e poi cade
- Decisione coordinatore (regola 6): ease-out come opzione per posa nella scheda, `ease_out` (0-1), default spento.
  Motore (`ue_rig.py`): una chiave piatta seguita da un segmento senza pesi esce con peso `ease_out` x durata segmento.
- Jab v12 ricostruito col nuovo motore: ossa identiche al riferimento (differenza massima 0,0), check 6/6 PASS.
  Ricontrollo jab sulle ossa fresche (fix cache): 6/6 PASS, clearance minima 0,27 cm a f6, nessun risultato cambiato.
  `motion_curves` del jab fallisce nel worktree feature solo perché manca il ref Mario (non versionato): misura, non bloccante.
- Ganondorf: `ease_out: 0.333` sulla posa 10 (con `drop` a 20 e `limb_led_recovery` del checkpoint 5), 1 tentativo.
  Piede sinistro cm/frame: tenuta 10-14 (0,3 / 0,9 / 1,7 / 2,7), caduta che accelera 15-20 (picco 14,3, prima 22 dopo
  uno stop), rallenta all'appoggio 21-25 (14 / 10,8 / 7 / 7,3 / 5,4). Bloccanti 5/6 come prima (legibility ereditato;
  contatore "gomito dietro la schiena" 15 frame, prima 12-13), planted 1,03 cm, vs sorgente 33,28 cm come prima.
- Attenzione: il primo build del jab dopo i build Ganon è uscito rotto (mano a 103 cm); rifatto identico, era il bake e non
  il codice (riprodotto senza la modifica: ok). Se `vs source` supera ~40 cm, rifai il build una volta.
- Anteprime: `sheets/ingame/ganon_ftilt_v31_*.mp4` e composito `ganon_ftilt_v31_recovery_before_after.png`, nei project files.
- Costo stimato checkpoint 5+6: Opus ~65 chiamate, contesto ~95k, circa $6-8; niente subagenti. Editor lasciato aperto.

## Checkpoint 7 (2026-11-02): anticipazione Ganondorf più estrema (feedback owner)
- Owner: "gamba più verso il corpo nell'anticipazione, posa più estrema". Solo `edits` nella scheda, frame 7, 2 tentativi.
- Sonda: gli edit `Location` su `foot_l_ik_ctrl` / `body_ctrl` vanno 1:1 sugli assi componente (X sinistra, Y avanti, Z su).
- Tentativo 1 (piede [0,-15,+6], corpo [0,-3,-4]): il ginocchio finiva 14 cm dietro l'anca (IK senza guida). Scartato.
- Tentativo 2 (tenuto): piede [0,-19,-8], `leg_l_pv_ik_ctrl` [0,+40,+10] (ginocchio avanti e alto), corpo [0,-3,-4].
  Al frame 7: piede 19 cm più vicino all'anca (distanza anca-piede 20 cm, prima 43), ginocchio sopra l'anca, bacino 4 cm
  più basso; lo scatto 7 -> 9 ha più corsa. Bloccanti 5/6 come prima, clearance PASS, planted 1,02 cm.
- Non fatto: busto più inclinato indietro e braccia più caricate (servono sonde FK, `fk_solve.py`); da proporre se l'owner
  vuole ancora più estremo. Nota: il ginocchio si sposta di 8 cm verso il centro (x), da guardare in vista frontale.
- Anteprime: `sheets/ingame/ganon_ftilt_v31_*.mp4` e composito `ganon_ftilt_v31_anticipation_before_after.png`, nei project files.
- Costo stimato checkpoint 7: Opus ~20 chiamate, contesto ~110k (cumulato), circa $3-4.

## Checkpoint 8 (2026-11-02): niente scatto dopo un frame con hitbox (regola owner, tutte le mosse)
- Owner: durante l'hitstop il frame congelato è seguito da un salto grande delle braccia; lo scatto va NEL frame in cui
  esce la hitbox, non subito dopo. Regola generale, documentata nel README (regole owner + tabella check).
- Check `hitstop_snap` in `animpipe check`: **bloccante, default su ogni mossa** (scelta: non opt-in, come chiesto).
  Per ogni frame attivo f misura lo spostamento massimo per osso f -> f+1 (spazio componente, 65 ossa), limite 10 cm
  per frame anim (card `hitstop_max_cm` per cambiarlo, esenzione `hitstop_snap` possibile).
- Opzione scheda `hitstop_blend` [{frame, toward, amount, match}]: la chiave a `frame` va di `amount` verso quella a
  `toward` sui controlli indicati (vuoto = tutti), dopo gli `edits`; rotazioni per la via corta. Esplicita per mossa
  (scelta: il motore non la applica da solo, perché quale posa spostare è una decisione di posa).
- Ganondorf: 9 -> 10 da 63 cm (mani 49) a **9,8 cm**: posa 9 all'85% verso la 10, il salto ora è 8 -> 9 (entrata
  nella hitbox). Pose owner 0/7/10/27/37 intatte. Bloccanti 6/7: legibility FAIL ereditato come prima.
- Jab v12: 2 -> 3 da 29 cm a **7,4 cm** (1° tentativo 0,7 -> 13,7 cm, tenuto 0,85): il pugno resta teso al 3 e
  rientra 3-5 (3 -> 4 ora più veloce, fuori dalla finestra attiva). Bloccanti **7/7 PASS**.
- Anteprime: `ingame_judge.py ... --at=<frame attivo>` (nuovo: hitstop su un frame attivo qualsiasi); jab hitstop 4
  a f2, Ganon hitstop 8 a f9; mp4 hitstop/whiff/compare in `sheets/ingame/`. Compositi prima/dopo con sovrapposizione
  rosso/ciano `hitstop_sheet.py` -> `<mossa>_hitstop_snap.png` (prima jab: cattura del worktree aaqr80, ossa identiche).
- Da guardare: Ganon 9,8 cm è al limite (amount 0,9 darebbe margine); hitstop vero in PIE quando le abilità usano i montage.
- Costo stimato: Opus ~35 chiamate, contesto ~70k, circa $3-4; niente subagenti.

## Checkpoint 9 (2026-11-02): kit overlord e down air v1 (Ganondorf dair + stomp di M. Bison)
- Owner: mosse raggruppate per **kit** (proxy di un personaggio). Schede in `cards/<kit>/<kit>_<mossa>_vN.json` (`cards/README.md`).
  Kit **overlord** (nome provvisorio): stile "violenza elegante" alla M. Bison / Rugal (KOF): composto, potenza controllata,
  braccia conserte. Ganondorf ftilt spostato in `overlord/overlord_ftilt_v31.json` (`move` rinominato: i nuovi output avranno il
  prefisso overlord_; l'asset UE resta `AS_GanonFTiltV31_Build`). Jab nel kit `pilot`.
- Nuovo `python -m animpipe author <card>`: senza clip né pose Step, le pose chiave sono dati nella scheda (blocco `author`:
  bacino, colonna, testa, clavicole, mani/piedi IK con pole, in assi componente) -> ossa -> `ue_write_take.py` ->
  `AS_Pilot_OverlordDAir_V1` (sorgente del build). Pugni dal template Ganon V2.
- Dair v1 (`cards/overlord/overlord_dair_v1.json`): tempi UFD Ganondorf dair (startup 16, attivi 16-18, totale 44 -> anim
  attivi 14-16, FAF 42). Pose 0/6/12/14/16/26/34/42: raccolta in aria, braccia che salgono e si incrociano sopra la testa (12),
  al 14 gambe giù dritte e braccia conserte sul petto (lo scatto grande è SUL primo frame di hitbox), tenuta, ginocchia su al FAF.
  Build `AS_OverlordDAirV1_Build`; montage e video con hitstop 8 a f14 e whiff.
- Check: hitstop_snap PASS (0,6 cm/frame dopo gli attivi), ossa, contatto/FAF, pugni PASS; legibility FAIL (controlli da pugno
  su un calcio, come il ftilt). Clearance stampa PASS ma ha violazioni: avambracci incrociati che si toccano e avambraccio dx
  nella testa ai frame 9-13 (fino a -8 cm), braccio conserto nel petto a 34-36. 2 tentativi sulle pose, poi fermo (regola 5).
- A occhio (griglia laterale): fasi leggibili; le braccia conserte sporgono troppo in avanti (sembrano tese), lo stomp è
  pulito ma poco "pesante"; nessun giro elegante dopo l'hit (fase successiva, non fatta).
- Bug strumenti: `animpipe check` andava in errore senza piedi piantati (aereo): ora "n/a". L'editor va lanciato con
  `MSYS_NO_PATHCONV=1`, altrimenti Git Bash storpia `/Game/...` e si apre DefaultLevel (sporco). `ue_write_take` fallisce la
  rilettura subito dopo il salvataggio (dialog): il build campiona bene lo stesso.
- Output nei project files: `dair_v1_griglia_laterale.png`, `dair_v1_ingame_hitstop_vs_whiff.mp4`, `dair_v1_pose_lato_fronte.png`.
- v1b dopo le immagini dell'owner (Bison SF4 e SF2): braccia conserte strette e alte sul petto, mani sul braccio opposto, punte
  dei piedi tese allo stomp. Output `dair_v1b_*`. Fermato qui (owner darà pose Cascadeur): la compenetrazione 9-13 resta.
- Pose owner in ingresso: `.casc` in `Saved/AnimPipeline/overlord_dair_owner.casc` (mai sovrascritto), FBX a step accanto,
  import in `/Game/AnimPipeline/Pilot/` (`import_take.py`); nuova scheda `overlord_dair_v2.json` con `source_anim` = quella take,
  `source: {file, frame}` per posa e niente blocco `author`; poi `build`, `check`, `ingame_judge.py ... 8 --at=14`.
- Costo stimato: Opus ~75 chiamate, contesto ~110k, circa $7-9; niente subagenti (render e build erano 3 comandi).

## Checkpoint 10 (2026-11-02): down air v2 dalle pose Cascadeur dell'owner
- Pose owner lette dal file aperto (`autosave2-Down air.casc`, copia in `Saved/AnimPipeline/overlord_dair_owner.casc`; numeri in
  `Saved/AnimPipeline/dair_v2/owner_poses_cascadeur.json`). **15 = carica** (palla compatta, ginocchia al petto, avambracci
  incrociati davanti alla faccia), **23 = stomp** (corpo dritto verticale un po' indietro, gambe unite, braccia conserte alte).
  Nessun giro dopo l'hit nelle sue pose. Mani nel suo file = default: ignorate.
- Nuovo `casc_to_author.py <pose.json> <frame>...`: punti di controllo Cascadeur -> pose del blocco `author` (rotazioni bacino e
  busto da terne di riferimento, mani/piedi nei frame di author.py, poli = gomito/ginocchio spinti fuori). Riusabile per ogni
  posa dell'owner. `author.py`: chiave `fingers: [0..1,0,0]` = **mano villain** (aperta, dita dritte e unite), blend per frame.
  `checks.py`: `reach_axis` nella scheda (dair: `-z`), esenzione `fists` per frame.
- Scheda `cards/overlord/overlord_dair_v2.json`: tempi Ganondorf (owner conferma). Sua 15 -> anim 12 (+ tenuta 13 compressa,
  mano villain 8-13), sua 23 -> anim 14 (tutta l'estensione SUL primo frame di hitbox), punte dei piedi -45 ("troppo è giusto").
  Build `AS_OverlordDAirV2_Build`, sorgente `AS_Pilot_OverlordDAir_V2`.
- Check: 6/7 blocchi PASS (contact_faf, fists, hitstop_snap 3,3 cm, ossa, clearance stampa PASS); legibility FAIL ereditato
  (controlli da pugno su un calcio). Clearance dopo 2 tentativi: avambraccio dx nella testa -5 cm al 13; contatti delle braccia
  conserte (mano sul braccio opposto -4/-5, avambracci -5) restano. Per staccarli ho spinto le mani in avanti di 9-16 cm al 12:
  **nella vista laterale le braccia al 12 sembrano tese in avanti**, meno fedeli alla sua 15. Decisione chiesta all'owner.
- Output: `overlord_dair_v2_pose_owner_15_23.png`, `overlord_dair_v2_lato.png` (composito lato in gioco),
  `overlord_dair_v2_hitstop_vs_whiff.mp4`. Non fatto: compositi fronte/3-4 in gioco (budget); `animpipe poses` va in errore
  (ncols 0 con --take/--over), da sistemare.
- Lezioni: il server script Cascadeur va avviato dall'owner; la finestra minimizzata blocca i render (`ui.restore_window()`).
- Costo stimato: Opus ~75 chiamate, contesto ~100k, circa $7-9; niente subagenti (ogni build/render era un comando).
- **v3 (owner review di v2)**: v2 non aveva sinergia (braccia ferme sul busto mentre busto e gambe ruotavano) e la testa
  scattava dopo l'hit. Cause: braccia fisse nel frame del petto da 6 a 12; `part_delays.head = 1` (la testa arrivava alla
  posa dello stomp un frame dopo l'hit, quindi dopo l'hitstop). `hitstop_snap` ora conta la rotazione della testa (arco a
  20 cm): v2 = 27 cm al 14->15, v3 = 1,4 cm. Scheda `overlord_dair_v3.json`: braccia che viaggiano rispetto al busto in ogni
  tratto (basse 0, larghe e in salita 6, incrocio a petto 10, tua 15 al 12, spinta su al 13, giù sul petto al 14 con le
  gambe), busto che si chiude per gradi, mano villain che si apre da 4 a 12. Clearance: resta l'avambraccio dx nella testa
  -5,6 cm al 12-13 (posa dell'owner su un manichino con proporzioni diverse). Output `overlord_dair_v3_lato.png`,
  `overlord_dair_v3_hitstop_vs_whiff.mp4`. Costo totale del thread circa $9-11.

## Revisione pipeline (2026-11-02, dopo il feedback owner sul dair v2)
- Doc: `docs/notes/reviews/anim-pipeline-review-2026-11-02.md` (cosa è scappato, tagli di costo, piano A/B/C).
- **Bug**: il blocco `clearance` di `animpipe check` era sempre PASS (`clearance.py` non stampa verdetto né exit code).
  Ora il verdetto viene da `clearance.json`: FAIL se gap < 0, 0-2 cm solo riportato, esenzioni per coppia nella scheda.
- `hitstop_snap`: rotazione di tutte le ossa foglia (testa 20 cm, punte piedi 8, punte dita 2,5), non solo la testa.
- Nuovo blocco **`co_motion`** (default su ogni mossa): nello startup nessun arto fermo sul genitore (< 1 cm/frame per 3+
  passi) mentre il corpo si muove (> 3 cm/frame). Gambe piantate saltate; `co_motion_skip` nella scheda (jab: `arm_r`).
- Provati solo su dati sintetici (niente editor qui). **Soglie da tarare** (thread A): atteso dair v2 FAIL, v3 FAIL solo
  clearance finché non si esentano i contatti delle braccia conserte e si risolve l'avambraccio nella testa.
- Prossimo: thread A (Sonnet, taratura + `animpipe review` + fix `animpipe poses`), poi C (proporzioni), poi B (legibility).
- Costo stimato: Opus ~20 chiamate, contesto ~70k, circa $2-3; niente subagenti.

## Thread A (2026-11-02): taratura dei check nuovi + `animpipe review`
- Dati: solo cache `sheets/raw/*_legbones.json` e `clearance.json` già in `Saved/` (nessun editor, nessuna posa toccata).
- `hitstop_snap`: invariato, già corretto. jab v12 7,4 cm, ftilt v3.1 9,8 cm (margine sottile sotto i 10), dair v3 1,4, **dair v2 27,3 (testa) FAIL**.
- `co_motion`: la regola per passo non prendeva v2 (il corpo accelera piano: 0,5-3 cm/frame proprio dove le braccia sono ferme).
  Ora una corsa di 3+ passi con arto < 1 cm/frame FAIL se il resto del corpo percorre >= 4 cm sulla corsa (`CO_FROZEN 1.0`, `CO_BODY 4.0`,
  somma). Con 1,5 cm/frame l'ftilt dava falso positivo (braccio sx 0-3 mentre il piede sx fa il passo). Esito: v2 FAIL (arm_r 6-9),
  jab v12, ftilt v3.1, dair v3 PASS. Taratura su 4 mosse sole: soglie provvisorie, rivedere con la prossima mossa accettata.
- `clearance` (dati salvati): jab PASS; v2 FAIL (testa -5,1 + braccia conserte); ftilt aveva -4,4 cm avambraccio dx/busto in recupero
  (32-33, dopo l'hit): esentato nella scheda con motivo (accettata dall'owner); dair v3: esentati i contatti delle braccia conserte
  (frame 9-42, coppie elencate), restano FAIL solo avambraccio dx/mano sx/avambraccio sx **nella testa** (10-13, -5,6 cm al 13).
  Nota: i dati di clearance dell'ftilt sono sotto il vecchio nome `ganon_ftilt_v31_check`, vanno rigenerati con `animpipe check`.
- Nuovo `animpipe review <card> [--hitstop=8] [--at=] [--force]`: build + check + `ingame_judge` (hitstop, whiff, compare, strip);
  niente render se un blocco fallisce. **Non provato end to end** (serve l'editor, non aperto: l'owner potrebbe usarlo).
- Fix `animpipe poses` senza `--frames` e senza immagini: usa fino a 6 pose chiave della scheda (provato su dair v3 over v2).
- Resta: legibility sul dair (thread B, FAIL atteso), eseguire un `animpipe check` completo dall'editor per confermare (clearance
  e legibility vere), thread C (proporzioni). Costo: Sonnet ~30 chiamate, ~$1.5.
- **v4 (timing dell'owner)**: il Ganondorf originale resta il riferimento; sbagliati erano i tempi che v1-v3 ricavavano solo
  dai numeri UFD. Scheda `overlord_dair_v4.json`: 0 posa iniziale quasi in piedi, ginocchio sx su, braccia aperte, mani
  aperte -> carica estrema al 5 (sua 15) -> carica tenuta e viva fino all'11 (piedi +8 cm, mani +8 cm, busto -6°, insieme)
  -> rilascio che inizia al 13 -> stomp al 14 -> tenuta fino al 30 -> recupero al 42. Gate: hitstop_snap 1,9 cm, contact_faf
  PASS; FAIL legibility (ereditato) e clearance
  (avambraccio dx nella testa -5,7 cm nella carica, avambracci incrociati -5: aperto per l'owner). Rilascio distribuito
  11->14 come chiesto (15% al 12, 42% al 13, il 58% sul 14); co_motion ora PASS. Timing: d'ora in poi li dà l'owner con
  la reference (il modello legge male i tempi dalle GIF).
  Output `overlord_dair_v4_lato.png`, `overlord_dair_v4_hitstop_vs_whiff.mp4`. Costo totale del thread circa $12-14.
- **v4 ultimo giro (owner)**: hitbox dal frame 15 (0-based): `card.py` accetta `anim_active` nella scheda (tempi dati
  dall'owner, vincono sul CSV). Chiavi 0/3/5/12/13/14/15/17/31/37/42. Frame 0 distinto dall'air idle ma non già in carica:
  gambe raccolte a metà, busto chiuso al 30%, braccia spalancate dietro le spalle e mani aperte. Al 3 le braccia salgono
  sopra la testa e cominciano a incrociarsi, al 5 c'è la carica, più chiusa di prima. Gate come prima (clearance aperta).
  Giro successivo (owner): ginocchia davanti e raccolte al petto nella carica (polo del ginocchio in avanti, piedi sotto le
  anche, la tenuta avanza invece di salire: ginocchio da y -20 a +12 cm dal bacino); chiave al 2 con le due braccia che
  salgono larghe insieme (il braccio sx solitario in alto è sparito). Costo totale del thread circa $17-19.
- Ultimo giro (owner): piedi affiancati allo stomp, ginocchia avanti nel rilascio, braccia sopra la testa al 3-4, mani
  villain piegate 35° in fuori al polso (`author.py` `wrist_out`), avambracci più alti nella carica: testa da -6 a -3,9 cm
  (2 tentativi, resta da chiudere con gli avambracci incrociati -5,7). Regole dell'owner raccolte in `cards/README.md`.
  Aperti: clearance braccia; salto 4->5 delle braccia da sopra la testa a davanti (da guardare). Prossimi thread (owner):
  rifinitura della pipeline, agente Fable che suggerisce correzioni alla timeline. Thread chiuso per contesto.
- **Ripristino (owner, 2026-11-02 sera)**: l'ultimo giro aveva peggiorato le braccia e le ginocchia "a rana" al 14 andavano
  bene. Dalla scheda di 2c000a7 (v4c) tornano: braccia alle chiavi 3/5/12/13 (busto al 3 compreso, avambraccio dx della
  carica) e poli delle ginocchia al 13-14. Tenuti: piedi affiancati allo stomp (15-31) e `wrist_out` 35° (0-12). Timing
  invariato (hitbox al 15). Check: co_motion PASS, hitstop_snap PASS 1,9 cm, contact_faf PASS; clearance FAIL come in v4c
  (avambraccio dx nella testa -6,0 cm al 5, avambracci incrociati -5,2 all'8), legibility FAIL ereditato. La clearance delle
  braccia resta aperta: va risolta senza cambiare la lettura delle braccia di v4c. Costo: Opus ~15 chiamate, ~$1.
- **Livello "Manuale" (owner, stessa sera)**: ritocchi a mano in Unreal che sopravvivono ad `animpipe build`. La build
  crea sulla traccia del Control Rig di `LS_<name>` una sezione additiva su tutta la clip (section to key); prima di
  ricostruire ne legge le chiavi e le rimette (copia in `Saved/AnimPipeline/animpipe/<move>_manual_layer.json`, usata solo
  se la sequenza manca). Uso in `cards/README.md`. Prova sul dair: +20° `upperarm_r_fk_ctrl` Rot Z al 25 (0 al 20 e al 30)
  -> dopo il rebuild mano dx +10 cm al 25, clearance 73 -> 68 coppie sotto i 2 cm; tolto il ritocco torna identico. Livello
  vuoto = clip identica. Attenzione: braccia in FK su questo rig (i controlli IK della mano non fanno nulla). Il bridge a
  volte fallisce la prima chiamata dopo un salvataggio: si rilancia. Livello del dair lasciato vuoto. Costo extra ~$3.
- **Sequenza senza rig (stessa sera)**: una serie di Ctrl+Z dell'owner ha annullato "Esegui il baking nel Control Rig" della
  build (la cronologia degli annullamenti comprende le operazioni degli script); chiuso senza salvare, il file su disco era
  integro (traccia rig + Manuale). Avviso aggiunto in `cards/README.md`.
- **Avambraccio dx nella testa (-6,0 cm, 3-13)**: non si risolve muovendo solo la testa. Prova in UE: testa +15° indietro
  -> -5,8 (ritirata). Ricerca offline (Poser + forme di `clearance.py`, ~0,3-0,8 cm più pessimista di UE): testa che ruota
  via dal gomito dx entra nell'avambraccio sx (meglio: -3,1 cm); gomiti più larghi (polo) o mani più avanti non aiutano.
  La testa è chiusa tra i due gomiti: serve cambiare la posa delle braccia approvata (spalle/gomiti) oppure un'esenzione.
  Lasciata all'owner: scheda e clip come le pose approvate. Costo extra ~$3.
- **Pose dell'owner bloccate (02/11 22:43)**: carica (5, tenuta identica al 12) = suo Cascadeur f15, stomp (15, 17; 31 con
  l'assestamento di 2 cm) = suo f23. Lette da `cards/overlord/refs/overlord_dair_owner_poses.json` (copia in git del file
  letto alle 21:07 da `autosave2-Down air.casc`; script server di Cascadeur spento, non riletto dal vivo) con il
  `casc_to_author.py` della PR #24 (rescale; rapporti segmenti 1,0 su questi rig: la lettura non era sbagliata). Della
  pipeline restano: altezza del bacino, dita, `wrist_out`, piedi affiancati x ±9 allo stomp (regola owner; f23 ha le
  caviglie incrociate). Clavicole tolte a 5/12. `source` sulle pose 5/12/15/17/31, blocchi `approved` 5+12 e 15+17
  (formato PR #24, scritti a mano: PR #24 non mergiata, conflitti solo in README e handover) più quello di PR #24 (gambe 13-14).
- Perché era derivata: i giri successivi a v2 avevano alzato/avanzato le mani di 8 cm, aggiunto clavicole, cambiato busto
  e testa a mano. Diff per parte vs f15 al frame 5 prima -> dopo: gomito dx 17,2 -> 5,4 cm, mano dx 16,9 -> 5,4, mano sx
  13,3 -> 5,0, polpacci 25-36 -> 6, testa 3,8 -> 3,8 (posizioni dal bacino; il resto ~5 cm è la forma del busto UE).
  Stomp vs f23: mani 3,0 -> 3,7, gomiti 4,2-4,9 -> 3,6-3,9, testa 3,1; piedi 7-17 cm per i piedi affiancati (voluto).
- Check: co_motion, hitstop_snap (1,0 cm), contact_faf, bone_lengths, fists, clearance PASS; legibility FAIL (ereditato).
  Clearance con esenzioni accettate dall'owner: avambracci/mani contro la testa 3-14 (sua f15: -3,0 dx, -4,7 sx; 13 resta
  -5,0 nel rilascio fra le due pose), braccia conserte sul petto 15-34 (sua f23, fino a -3,7). Frame 3-4, 13-14, timing invariati.
- Aperti: carica ferma 5-12 (prima "viva"): se l'owner la vuole viva va sbloccata; `source_fidelity` (piano §10) non fatto.
  Il bridge fallisce a volte la prima chiamata: si rilancia. Costo del thread circa $12.
- **Carica viva (owner 22:56)**: solo il 5 è la sua f15 così com'è (bloccato, `approved` ora solo [5]); il 12 = f15 più le
  differenze di tenuta di v4c (busto -6°, testa +4°, mani +4/+10 avanti e +7 su) e piedi +6 avanti/+6 su (il primo
  tentativo con i soli +4 di v4c dava co_motion FAIL, gambe ferme 7-12). Check: tutto PASS tranne legibility (ereditato).

## Giro scheda ↔ Cascadeur a chiavi sparse (2026-11-03)
- Owner: non riesce a lavorare con il livello additivo Manuale; vuole modificare in Cascadeur con le chiavi sparse.
  Nuovo `animpipe/cascadeur.py` (`casc-export`, `casc-import`, `casc-revert`, `casc-diff`), `casc_ue.py` (export FBX via
  bridge), `casc_prepare.py` (lato Cascadeur). Uso in `cards/README.md`. 8 test offline in `tests/test_casc_roundtrip.py`.
- Import per parte (arm_x con clavicola, leg_x, trunk, head) confrontata con la rilettura di Cascadeur subito dopo
  l'export (`--ref`): la differenza nuova-rilettura si applica alle ossa della scheda e poi si ricavano i campi `author`.
  Così la distorsione propria del rig Cascadeur (rollio cosce/collo 5-11°) non entra nella scheda. Parti non toccate:
  identiche byte per byte. Lock `approved` rispettati, snapshot in `cards/<kit>/history/` a ogni import.
- Trappole Cascadeur (2026.2.3): `import_animation` di un FBX di Unreal dà una scena di 1 frame; serve
  `import_animation_to_selected_objects` con tutti i Joint selezionati, e `FbxSettings.up_axis = Z` (sennò il personaggio
  è sdraiato). `export_all_objects` esporta anche la mesh (UE crea una SkeletalMesh): usare `export_joints`. Le sezioni di
  1 frame restano STEP (nessun intermedio, innocuo). Rileggendo in UE la radice ha la conversione di asse: il campionamento
  la toglie. `MSYS_NO_PATHCONV=1` per i `--take=/Game/...` da Git Bash.
- Prova sulla dair v4 (scheda di d3c5efa, non modificata): take UE = scheda (0 parti cambiate); rilettura Cascadeur vs
  scheda max 0,84 cm (clavicola dx); giro senza modifiche: scheda identica, riesportazione dopo salvataggio = 0,00004 cm.
  Giro con modifica (copia della scheda nello scratch): mano dx +10 cm su al 31 -> solo arm_r@31 cambiato, mano +10,01 cm,
  residuo 0,06 cm, la clip cambia solo 18-36. Build in UE della scheda modificata non fatta (avrebbe sovrascritto la dair).
- Pronto per l'owner: `Saved/AnimPipeline/casc/overlord_dair_v4.casc` (aperto in Cascadeur, chiavi 0/2/3/5/12/13/14/15/
  17/31/37/42) e la rilettura `overlord_dair_v4_ref.json`. File di prova `*_test1/_edit1/_lying_test/_edittest/_base`: da buttare.
- Conflitti PR #24: tocca `__main__.py` (6 righe mie: comando `casc-*`), legge `approved` nello stesso formato di locks.py;
  quando #24 entra, `casc-diff` può lasciare il posto al suo `diff`. Nessun file in comune oltre `__main__.py`.
- Costo stimato: Opus ~110 chiamate, contesto ~150k, circa $10-12; niente subagenti.
- **Stomp = f23 intero (owner 23:16, "f23 andava bene")**: tolta l'eccezione dei piedi affiancati, gambe e caviglie
  incrociate come f23 a 15/17/31 (31 con l'assestamento di 2 cm), lock esteso a [15, 17, 31]. Check invariati: tutto PASS
  tranne legibility.

## Brief: interpolazione fatta da Claude (thread Opus sul desktop, DOPO il merge di PR #24)
Decisione dell'owner 02/11 23:28Z, proposta `docs/notes/reviews/proposta-interpolazione-claude-2026-11-02.md`.
Obiettivo: l'owner mette in Cascadeur solo le sue chiavi; Claude scrive le pose intermedie; le chiavi dell'owner non si
toccano mai. Prerequisito: PR #24 su feature (`locks.py`, `approve/diff`, `rules`; `source_fidelity` se fatto).
Codice esistente da estendere, non duplicare: `animpipe/cascadeur.py` (`casc-export/import/revert/diff`, `merge()`,
`locked()`, snapshot in `cards/<kit>/history/`), `casc_prepare.py` (lato Cascadeur), `author.py` (`frames()` = smoothstep
tra le pose). Quando entra #24, `casc-diff` può lasciare il posto al `diff` di #24; `locked()` deve usare `locks.py`.
Passi (Opus medium, subagenti Sonnet per i test):
1. `casc_prepare.py`: comando Scripts "GSTN → Segna chiavi mie": salva, `export_joints`, scrive
   `Saved/AnimPipeline/casc/<move>_owner_keys.json` {frames, timing opzionale, stamp}.
2. `casc-import --owner-keys`: per ogni frame dell'owner aggiunge/aggiorna un blocco `approved` (`by: owner`, tutte le
   parti, dita e `wrist_out` esclusi) nel formato di `locks.py`.
3. `animpipe interp <card>`: prepara su file il pacchetto per l'agente (pose dell'owner, tempi, `cards/README.md`,
   ultimi check) e valida la risposta. Pose nuove in `author` con `origin: "claude"` e `why`. Rifiuta tutta la
   scheda se una posa cade su un frame bloccato. Eventuali `spacing`/`part_delays` per segmento.
4. Gate: `build` + `check` (co_motion, hitstop_snap, clearance con esenzioni, bone_lengths, frame 0/`rules`). Massimo 2
   tentativi, poi si ferma e scrive quale check fallisce e su quali frame.
5. `casc-export`: scena con le chiavi dell'owner e quelle di Claude; nel file `_owner_keys.json` va segnato chi ha messo
   ogni chiave (l'API non colora le chiavi). Una chiave di Claude spostata dall'owner diventa sua e si blocca al
   prossimo import.
6. `casc-revert` e `diff` invariati. Test offline in `tests/` (niente chiavi dell'owner cambiate, rifiuto sul frame
   bloccato, giro senza modifiche identico). Prova vera sulla dair v4 su una copia della scheda.
Avvio a regime: l'owner scrive al coordinatore "interpola la <mossa>", e il coordinatore apre un thread desktop con
questo brief e la scheda. Costo atteso: $1-3 per giro. Fuori scopo per ora: pulsante che lancia `claude -p`, Sonnet.

## Spacing Cascadeur = build, interruttori fisica (2026-11-03, brief casc-spacing-brief.md)
- `animpipe casc-spacing <card> --fit|--apply|--score` (`animpipe/spacing.py`) + lato Cascadeur `casc_spacing.py`. Verità:
  ossa della build per frame (`sheets/raw/<take>_legbones.json`); metrica: scarto peggiore per intermedio con il divario
  delle pose chiave tolto (la build stessa sposta le ginocchia fino a 7 cm alle chiavi). Fit per segmento e per layer
  (Fingers esclusi): LINEAR o CLAMPED/BEZIER/LOW_AMPLITUDE con pesi 0.1/0.33/0.6, discesa a coordinate 2 passate su una
  copia `<move>_spacelab.casc`, obiettivo = somma per frame del peggiore (col max sulla finestra peggiorava i vicini).
- Solo API chiavi: nessuna chiave aggiunta o spostata. `--apply` rifiuta se una posa chiave si muove (dair: 0,0 cm).
- Dair v4 applicata a `casc/overlord_dair_v4.casc` (backup `_prespacing.casc/.fbx`, `_ref_prespacing.json`), --ref rifatta
  in UE: peggiore per segmento 0-2 5,4->4,7; 3-5 9,2->5,9; 5-12 37,6->8,8; 15-17 0,75; 17-31 2,5->1,5; 31-37 12,5->6,1;
  37-42 0,45->0,39 cm. Obiettivo < 1 cm NON raggiunto su 4 segmenti: resta forma del percorso (ginocchia, avambracci,
  bacino 3-4 cm), non timing. Su questo rig un intervallo bezier fa uscire il ginocchio fuori da entrambe le chiavi anche
  con pesi 0: per questo molti layer sono LINEAR. Nessuna API per la direzione delle tangenti.
- Garanzie: pose chiave nuova vs vecchia readback 0,0001 cm; import senza modifiche = 0 parti, scheda identica (hash);
  13 test verdi (`tests/test_casc_spacing.py` nuovi 5). revert/diff non toccati. part_delays != 0: rifiutato (servirebbero
  chiavi fuori dai frame della scheda), la dair ha tutti 0.
- Fisica: `casc_physics.py` (PHYSICS=piano). Interruttori del pannello (Physics corrector, Smooth trajectory/rotation,
  Compensation, Separation, Secondary) ora da script: click sintetico su Off/On della riga nel pannello Physics settings
  (verificato sulla copia: Secondary e Separation Off, Smooth trajectory On, letti con get_bool_value). Valori per punto
  (`points`, es. muscle_stiffness mani 5->15, verificato). Snap to AutoPhysics: solo su copia `_physics/_spacelab` con
  `allow_key_changes` (sposta le pose chiave = violerebbe i lock); non eseguito. Nessun valore cambiato nella scena
  dell'owner: tutti gli interruttori lì sono On. Dettagli in `cascadeur_api_verified.md` §6c.
- Scelte fatte senza owner: LINEAR ammesso nel fit; nessuna scelta di valori fisici (servono i suoi). Prossimo: se vuole
  < 1 cm, chiavi intermedie solo-tempo fuori dalla scheda (import le deve ignorare) oppure accettare la differenza di rig.
- Costo stimato: ~90 chiamate, contesto ~140k, circa $8-10; niente subagenti; editor in hold ~5 min.

## Pulsanti GSTN in Cascadeur (2026-11-03)
- Codice: `Tools/AnimPipeline/cascadeur_menu/` (`gstn_scripts/` = voci menu, `gstn_events/` = apertura barra all'avvio,
  `install.py`). Menu **Scripts > GSTN**: Esporta per Unreal, Segna chiave (frame corrente), Annulla ultimo import (chiedi a
  Claude), Stato, Mostra barra pulsanti. Gli stessi 4 comandi sono pulsanti in una barra GSTN che si apre da sola nella
  barra del titolo di Cascadeur (a destra, segue la finestra).
- Toolbar nativa: NON personalizzabile in 2026.2.3 (QML fisso, nessuna config). La barra GSTN è una finestra Qt a parte,
  stesso codice delle voci menu (`call_action("Scripts.GSTN.<nome>")`). Tasti: sì, Settings > Hotkeys Window > Scripts
  (le voci script sono azioni legabili, verificato con Quick export); nessuna scorciatoia preimpostata.
- I comandi non lanciano animpipe e non toccano la scheda. Solo scene `<CASC_DIR>/<mossa>.casc` con `<mossa>_export.json`
  (copie di prova rifiutate). Esporta = salva il tab + `export_joints` cotto in `<mossa>_casc.fbx` + richiesta. Segna
  chiave = alterna il frame in `<mossa>_owner_keys.json` {frames, marked{frame: stamp}} (avvisa se il frame non è una
  chiave della scena). Richieste: `casc/requests/<stamp>_<mossa>_<import|revert>.json` con `command` pronto
  (casc-import con --fbx/--keys, casc-revert) e `owner_keys`. Chi la esegue la sposta in `requests/done/`.
- Prova (copia `gstntest.casc`, poi cancellata): rifiutata la `_spacelab`; segna/togli 12, 20 con avviso; export 12 chiavi,
  fbx 1 MB; revert e stato ok; barra visibile nel titolo e clic → azione (via Quick export). Scheda e dair non toccate.
- **Da fare dall'owner, una volta** (a me negato: modifica impostazioni persistenti di Cascadeur, file sola lettura):
  `python -I Tools/AnimPipeline/cascadeur_menu/install.py` dal worktree `_feature` e riavviare Cascadeur. Backup
  `settings.json.bak_gstn`; `--uninstall` toglie tutto. Le voci GSTN in menu/hotkey compaiono solo dopo il riavvio.
- Il percorso in settings punta al worktree `GASP_Giustino2_feature`: se il worktree cambia, rilanciare install.py.
- Non fatto: richiesta "interpola" (il comando `interp` non esiste ancora), lettura automatica delle richieste lato
  coordinatore (oggi: leggere `requests/` a mano o su "importa").
- Costo stimato: ~75 chiamate, contesto ~110k, circa $7-9; niente subagenti.

## Rifinitura pipeline (2026-11-02 sera, PR #24, thread Fable + subagenti Sonnet/Opus, nessun editor)
- Piano approvato dall'owner: `docs/notes/reviews/anim-pipeline-refinement-plan-2026-11-02.md`. Fatti i punti senza editor;
  brief `handovers/anim-pipeline-refinement-brief.md`. Tutto provato SOLO con dati sintetici (`Tools/AnimPipeline/tests/`,
  21 test, `python3 tests/test_*.py`, niente pytest).
- **Lock pose approvate** (`animpipe/locks.py`): blocco `approved` nella scheda (frames x parti, ref, why); `load()` ferma
  build/check/review se le pose `author` bloccate differiscono dal ref/snapshot (0,5 cm / 1°); `--unlock part@frame`;
  `animpipe approve` (scrive `cards/<kit>/approved/<move>.json`); `animpipe diff` (cambiato / chiavi nuove-tolte / intatto
  (lock) / manuale, 5 righe, json completo). Dair v4: il lock gambe 13-14 della PR è stato lasciato fuori al merge (03/11): l'owner ha rifatto le chiavi in Cascadeur con un timing nuovo, la scheda è del thread di import. Guardia 2 (chiavi del rig dopo il
  build vs snapshot) NON fatta: serve l'editor.
- **Regole README come check** (`checks.py`, opt-in via `rules` nella scheda): frame_0, knees_front, feet_side_by_side,
  villain_hand. Soglie da tarare sul desktop (dair v4 e jab v12 devono passare). `animpipe card` stampa "timing: owner/CSV".
- **Legibility `strike`** (`legibility.py`): limb/hiders/pose_checks dal move sheet (`sheet_extra.strike`); bug: la sheet
  era cercata per nome take, ora anche per asset, quindi dair e ftilt erano giudicati come pugni. Da verificare sul desktop:
  dair v4 e ftilt v3.1 come calci (la regola B può segnalare le braccia conserte), jab v12 invariato; il take `_src` resta pugno.
- **Proporzioni** (`casc_to_author.py`): target mani/piedi riscalati sulle catene UE (`--no-rescale` = vecchio). Da provare
  sulla posa 15 dell'owner con clearance. **Pollice** (`author.py` chiave `thumb`: "along" default, "open"): owner deve scegliere.
- **Capture dello strato Manuale** (`animpipe capture`): copia `Saved/.../<move>_manual_layer.json` in
  `cards/<kit>/manual/<move>.json` + blocco `manual`; build fallback sequenza -> Saved -> copia nella scheda; diff avvisa se
  una posa cambia sotto un ritocco manuale. Da verificare: nomi reali dei canali (`hand_r_fk_ctrl.Rotation.X` assunto).
- Prossimo thread (desktop, Opus medium): `animpipe check` su dair v4 e jab v12 con `rules` e `strike`, taratura soglie,
  prova capture + build senza sequenza, guardia 2. Costo di questo thread: Fable ~25 chiamate (~$2) + subagenti ~360k token (~$15).

## Chiusura pipeline: merge PR #24, source_fidelity, revisione visiva (2026-11-03, thread cloud Opus, nessun editor)
- PR #24 unita in feature (5652ab8). Conflitti solo nei doc (README carte + questo file, tenuti entrambi). La scheda
  dair v4 è rimasta quella di feature (lock nello stesso formato di PR #24, nessuna conversione necessaria); il lock
  13-14 della PR e il suo snapshot sono stati lasciati fuori perché si riferivano al timing vecchio. `test_locks` ora usa
  una copia congelata della scheda di PR #24 (`tests/fixtures/`), così le modifiche alla scheda vera non lo rompono.
- `source_fidelity` (121bbee, `animpipe/fidelity.py`, `animpipe fidelity <scheda>`): le pose con `source` devono essere
  quelle dell'owner entro 0,5 cm / 1° (dita, `wrist_out`, `hand_rot` e altezza del bacino esclusi; altri scarti solo in
  `source.keep` con why; `derived` = non confrontata). Il file deve stare in `cards/<kit>/refs/`. Blocca `check`,
  `approve`, e `load()` per le schede con pose `origin: owner`. Senza `sheets/raw` (cloud) dice n/a.
- Revisione visiva (e2e4b6f): skill `.claude/skills/gstn-visual-review/` (SKILL.md, rubric.md, verdict.schema.json,
  calibrazione.md); `animpipe visual-gate` legge `Saved/AnimPipeline/visual/<move>/r<N>/verdict.json` (0 passa,
  1 bloccato, 2 nessun verdetto; max 2 giri); `approve` rifiuta se l'ultimo verdetto blocca. README: diagnosi di 3 righe
  prima di ogni build, niente render prima del gate numerico.
- Test: 9 file in `tests/` passano qui (aggiunti `test_fidelity.py`, `test_visual.py`); `test_casc_roundtrip.py` richiede
  `sheets/raw` (solo desktop), non eseguito. Pollice villain: resta "along" (default approvato).
- Non fatto (serve il desktop): `review --visual` (CP2), calibrazione della skill, `--owner-keys` + `interp` (CP4),
  `fidelity` con il rig vero sulla dair. Brief: `handovers/cp2-cp4-desktop-brief.md`.
- Costo stimato: ~45 chiamate, contesto ~85k, circa $4-6; niente subagenti.


## Dair v4: chiavi nuove dell'owner da Cascadeur (2026-11-03, thread desktop Opus)
- Scena dell'owner (salvata 02:48) esportata da me (nessuna richiesta Esporta). Chiavi trovate: 0/3/8/14/15/17/31/37/42
  (l'owner ha scritto "4 = carica": nella scena è 3, preso 3; domanda aperta). Copia intatta della sua scena:
  `casc/overlord_dair_v4_20261103_030200.casc` (+ `_owner_rekey.fbx`); backup prima di tutto `_preowner2_bak.casc`.
- Scheda: chiavi dell'owner prese intere e bloccate (un solo blocco `approved`, tutte le parti); tolte 2/5/12/13 e i vecchi
  lock; hitbox 15-17 invariata; pugni chiusi al frame 0; wrist_out a 8 = 8 (con 35 la mano dx entrava nella clavicola).
- casc-import più fedele: `spine_chain`/`neck_chain` per osso in author.py (piega del tronco/collo non uniforme), target
  con le lunghezze della scheda (stretch riportato a parte, `stretch_cm`), chiave nuova = ossa di Cascadeur intere (prima
  prendeva un delta dall'intermedio di Cascadeur: errore di 10 cm sulla mano dx al frame 8). Residuo fit 0,08-0,3 cm.
- Fedeltà Cascadeur owner vs scena nuova (rilettura, relativa al bacino): 0,14-0,53 cm su 0/14/15/17/31/37/42; 3 = 1,2 cm,
  8 = 2,3 cm (upperarm: Cascadeur allunga il tronco, la scheda ha lunghezze fisse); dita dei piedi (ball) non in scheda,
  13° al frame 0. File `casc/overlord_dair_v4_fidelity.json`.
- Intermedi miei (`origin: claude`): 6 (ginocchia +3 cm nella carica) e 11 (mani caricano contro il colpo, 15% di 14->15);
  tangenti automatiche su 3/6/8/11 (con flat o hold_recoil il braccio dx restava fermo 8-12). Check: clearance, co_motion,
  hitstop_snap, bone_lengths, fists, contact_faf PASS. Rosso: legibility (KeyError foot_l, blocco strike di PR #24 sui
  calci) e source_fidelity (vuole le pose dell'owner in `cards/overlord/refs/`: casc-import dovrebbe scriverle lì; non
  fatto, la ref pre-rekey è stata sovrascritta). La build sposta le ginocchia fino a 7,6 cm alle chiavi (rig, già noto).
- Scena per l'owner: `casc/overlord_dair_v4.casc` riesportata (11 chiavi) con spacing della build applicato (pose chiave
  ferme 0,0 cm); scarto intermedi vs build: 0-3 11,4, 31-37 5,8, 37-42 3,1 cm, resto < 1,6 (forma del percorso, noto).
- NON fatto: cattura in gioco (vista laterale, hitstop vs whiff): il livello DefaultLevel aperto nell'editor ha modifiche
  non salvate, capture_ue.setup rifiuta di cambiare livello. Fatte solo le traiettorie (`sheets/OverlordDAirV4_Build_trails.png`).
- ue_write_take: la rilettura finale (`read_anim_sequence`) torna vuota ed esce 1, ma il take è scritto (build lo legge).
- Test: 33/34; `test_capture` (PR #24) fallisce su Windows per il separatore di percorso, non toccato.
- Costo stimato: ~110 chiamate, contesto ~150k, circa $10-12; niente subagenti.
- **Regole owner per il prossimo passo desktop (03/11 03:01, dair in pausa finché il processo non è solido):**
  1. Mai spostare il frame di una chiave dell'owner: proporre e aspettare. Qui nessuna chiave è stata spostata: la scena
     salvata aveva la carica al 3 (lui ha scritto 4). Proposta aperta: verificare con lui se nella sua versione era al 4.
  2. `approve` solo dopo l'ok esplicito dell'owner sulla griglia composita (il blocco `approved` di questo giro è stato
     scritto senza: da confermare o togliere).
  3. Le dita delle chiavi dell'owner in Cascadeur hanno la precedenza sulla scheda; cambiarle solo su sua richiesta. Oggi
     la scheda impone ancora le mani del kit (fingers/wrist_out) su 3/8/14: da allineare a questa regola nel codice di import.
- Catture: serve un livello dedicato alle catture (non DefaultLevel), così modifiche non salvate altrove non le bloccano.
  Stato editor: nessun livello cambiato né salvato da questo thread; Cascadeur con la sola scena `overlord_dair_v4.casc`.

## Proposta: pipeline come skill pubblica + app (2026-11-03, thread cloud, solo design)
- Doc: `docs/notes/reviews/skill-pubblica-proposta-2026-11-03.md`. Nessun codice; aspetta il via dell'owner.
- Verbi rigidi `animpipe <verbo> <mossa>` (status, import, revert, interp, build, check, render, verdict, variant,
  snapshot/restore, export, owner-ok, approve), una riga JSON + exit code fissi, snapshot automatici, `animpipe.toml`.
- Skill unica `animpipe-move` = segue il `next` di `status`; assorbe `gstn-visual-review`; niente nomi di modelli.
- Cascadeur: solo Segna chiave + Invia + riga di stato. App PySide6 "GSTN Studio": griglia+verdetto, Ok/Approva,
  varianti, snapshot. Costo in più ~$15-20; ordine: punti 2-6 (Sonnet, senza editor) PRIMA di CP2/CP4.
- Costo di questo thread: ~12 chiamate, contesto ~60k, circa $1-1,5.

## Skill pubblica + GSTN Studio, punti 2-7 e 9-11 della proposta (2026-11-03, thread desktop Opus, nessun editor)
- Ramo `claude/project-thread-n3qk2z` (da `origin/feature/gas-npp-5.8` 2ee3ef3), nel worktree `_feature`; i file dair
  non salvati dell'owner (`move_sheets.json`, `overlord_dair_v*_bones/author`, `cards/overlord/history/`) non toccati.
- `animpipe.toml` + `animpipe/config.py`: niente più percorsi fissi (`__main__`, card, cascadeur, locks, `_bridge`).
  Su questo desktop `animpipe.local.toml` (non in git) punta `data_dir` a `GASP_Giustino2/Saved/AnimPipeline`.
- `animpipe/verbs.py`: `animpipe <verbo> <mossa>` (status, import, interp, build, check, render, verdict, owner-ok,
  approve, export, variant, snapshot, restore, revert); una riga JSON, exit 0/1/2/3/4, run in `<data>/runs/<mossa>/`.
  I vecchi comandi con percorso della scheda funzionano ancora (strato interno).
- Divieti dentro i comandi: `interp` rifiuta (exit 3) frame che sono chiavi owner (segnate, con `source`, o lock owner)
  o bloccati, e pose senza why; `import` rifiuta se una chiave owner manca dall'export e ripristina se la perde;
  `approve` rifiuta senza `owner_ok.json` sul giro corrente con la stessa scheda; `variant apply` vuole `--quote`.
- `animpipe/snapshots.py`: zip automatici prima di ogni verbo che scrive; `restore <mossa> <data>|last` fa prima uno
  snapshot dello stato attuale; la `.casc` dell'owner va solo in `casc/restored/`, mai sovrascritta.
- `status`: macchina a stati import → interp → build → check → render → verdict → owner-ok → approve → export → done;
  max 2 check falliti o 2 giri bloccati → `ask_owner` (exit 3). Varianti: `<mossa>_var<A>` accanto alla scheda
  (gitignored), clip `AS_..._Var<A>` separata, lock confrontati con l'originale.
- Skill `.claude/skills/animpipe-move/` (SKILL.md, review.md = ex gstn-visual-review, rubric, schema, calibrazione,
  rules.md, kits/overlord.md, kits/pilot.md): nessun nome di modello.
- Cascadeur: menu GSTN = Segna chiave + Invia (ex "Esporta per Unreal", l'id azione cambia: rifare l'eventuale hotkey) +
  riga di stato nella barra ogni 2 s; via Stato e Annulla import. **Non provato dentro Cascadeur** (server script spento).
- GSTN Studio `python -m animpipe.app`: **tkinter invece di PySide6** (PySide6 non installato; così niente da
  installare). Mosse + status, griglia del giro + verdetto, Ok (chiede le parole dell'owner), Approva, variante
  originale/variante con frame e why, Applica/Scarta, snapshot + Ripristina, Annulla ultimo import. Provata con dati finti.
- Test: `tests/test_verbs.py` 4 nuovi verdi; suite 44/45 (`test_capture` fallisce come prima, separatore Windows).
- Non fatto: `render` usa la griglia `poses` esistente (non ancora il composito CP2 con fantasma owner); dita owner
  nell'import (CP4); script storici in `legacy/` (punto 12). La richiesta di import dair in attesa non è stata toccata.
- Costo stimato: ~45 chiamate, contesto ~110k, circa $8-10; niente subagenti.

## Dair v4: check rossi tecnici, chiavi owner in refs/, FAF 37 (2026-11-03 mattina, thread desktop Opus, editor solo bridge)
- Ramo `claude/project-thread-66dv82` (da feature 60da6fa) nel worktree `_feature`; file dair non salvati dell'owner non toccati.
- Owner 10:03: carica resta al 3 com'è, FAF 37 (`faf_game` 39 nella scheda, finestre anim: active 15-17, faf 37).
  Tolto il blocco `approved` del giro 03:02 (scritto senza il suo ok sulla griglia: regola owner 2).
- Richiesta Invia delle 02:49 **archiviata, non consumata** (`casc/requests/archived/`): stesse chiavi già importate alle 03:02
  dalla copia intatta, e il suo .fbx era stato sovrascritto dalla mia riesportazione. L'owner non ha capito la domanda: scelta mia.
- `legibility` (KeyError foot_l): le viste ortho erano fisse a z 95 ± 100 cm, il piede della dair a z -3..-17 usciva dal
  quadro (maschera vuota). Ora `fit_center` sposta il centro solo se la take esce dal quadro (dair: z 66; jab ecc. invariati),
  e una parte senza maschera vale 0 % (non leggibile). Il check ora gira: verdetto FAIL di contenuto (piede non distinto al
  contatto in 6 viste, gamba coperta da camera di gioco 3 e PvP 6). Soglie della mano applicate al piede: da tarare.
- `casc-import --owner-keys[=0,3,..]` (anche dal verbo `import`, dai `owner_keys` della richiesta + chiavi owner già nella scheda):
  ossa dell'owner intere in `cards/overlord/refs/overlord_dair_v4_owner_bones.json` (in git), pose con `origin: owner` e
  `source {file, frame, tool: casc-import}`. `merge()` toglie un `source` di tipo punti quando riscrive quella posa.
  Eseguito sul vero `casc/overlord_dair_v4_owner_rekey.fbx` (snapshot `history/overlord_dair_v4_20261103_110709.json`).
- **`source_fidelity` sui dati veri: FAIL, e quindi `load()` ora ferma la scheda dair** (guard per `origin: owner`). Scarti
  (cm posizione / gradi): twist della coscia 8-12° (la scheda non ha il twist della coscia: swing solo 1-4°), ball_l 13° al
  frame 0 (punta del piede, non in scheda), gambe 2,1-2,4 cm a 15/17/31 e tronco/braccia 0,9-1,2 cm a 3/8 (Cascadeur allunga
  le ossa, la scheda ha lunghezze fisse), testa 3-5° (neck chain). L'import riporta residuo 0,1-0,2 cm perché misura solo
  le posizioni sul bersaglio rigido. Etichetta: il messaggio stampa "cm" anche quando il peggiore è in gradi.
- **Prossimo passo (scelta di design, non fatta per regola dei 2 tentativi):** o fidelity confronta col bersaglio rigido
  dell'import (rotazioni owner sulle lunghezze della scheda) e solo lo swing per le ossa di cui la scheda non ha il twist,
  o si aggiungono twist coscia / ball / neck al Poser. Poi `source.keep` con why solo per ciò che resta. Fino ad allora
  build/check della dair si fermano (per sbloccare: `animpipe restore overlord_dair_v4 last` o la snapshot sopra).
- Non fatti (budget): griglia composita in `render`, livello di cattura dedicato, dita owner nell'import, interp,
  owner-ok/approve, varianti, jab v12, GSTN Studio con dati veri, `test_capture` (Windows) ancora rosso. Nessuna build.
- Per l'owner al ritorno: rilanciare `install.py` di Cascadeur e avviare il server script per provare barra GSTN e riga di stato.
- Test: 44/45 (`test_part_delays_refused` adattato: copiava la scheda dair vera, ora bloccata dal guard).
- Costo stimato: ~55 chiamate, contesto ~100k, circa $8-9; niente subagenti, niente build, editor usato solo per import/sample.

## Dair: pose owner intere, fidelity senza lunghezze, griglia composita CP2 (2026-11-03, thread desktop Opus medium)
- Ramo `claude/project-thread-dgqsku` (da 66dv82 8a83d48) nel worktree `_feature`. File non salvati dell'owner e `move_sheets.json` non toccati.
- **Decisione del coordinatore applicata:** le pose owner entrano INTERE. Campo nuovo `bones` nella posa author
  (`author.py`: rotazioni locali di Cascadeur, pelvis in spazio componente; mani/dita/twist esclusi, restano della scheda);
  il Poser fa FK con quelle rotazioni sulle lunghezze fisse del rig (stretch Cascadeur 1-2 cm = limite noto, normalizzato).
  In-between vicino a una posa `bones`: fusione delle rotazioni (`_bone_pair`). `casc-import --owner-keys` scrive `bones`;
  `merge()` toglie le `bones` delle parti cambiate. `fidelity`: bersaglio = rotazioni owner sul rig con altezza bacino e mani
  della scheda; confronta rotazioni ≤1° e posizioni ≤0,5 cm dopo normalizzazione, mai la lunghezza; unità del peggiore corretta.
- Dair v4 migrata da `refs/` (snapshot `history/overlord_dair_v4_20261103_111440.json`); chiavi owner non spostate, carica 3, FAF 37.
  `fidelity` PASS 9/9, `load()` di nuovo ok. Diagnosi prima della build in `Saved/.../visual/overlord_dair_v4/diagnosi_b_rotations.md`.
- Build ok. Check: tutto PASS tranne **legibility** (il piede viene giudicato con le soglie della mano: va tarata, non correggibile
  sulle chiavi owner). Moto identico a prima (salto di 70 cm a 14→15 = stomp dell'owner).
- **CP2 fatto (subagente Sonnet):** `animpipe/composite.py` + `v_render`: una `grid.png` 1568 px (griglia laterale di tutti i
  frame per parte con fantasma grigio owner, pose chiave camera di gioco + da dietro, traiettorie mani/piedi/testa, passo per
  frame con finestra hitbox) e `manifest.json` (segmenti, active, faf, hitstop, risultati del check). Prima griglia vera:
  `visual/overlord_dair_v4/r2/grid.png` (render con `--force` per la legibility). Limiti: viste 2D dalla cache ossa (niente
  cattura UE, niente livello dedicato: non si cambia livello con il DefaultLevel dell'owner non salvato), "camera di gioco" =
  vista laterale, hitstop 8 di default (`--hitstop=N`), manca la striscia con/senza hitstop.
- Owner-ok gate su approve, snapshot/restore e varianti erano già in `verbs.py`: non toccati. Nessun approve (manca l'ok owner).
- **Jab v12:** build+check: FAIL `contact_faf` (picco di portata a f0, finestra 1-2), `fists` (apertura 0,95: mani aperte su un
  pugno?) e legibility. Nessun blocco `rules`/`strike` in jab v12 né in dair v4. Da indagare (take sorgente?) e poi tarare le regole.
- Prossimo: soglie legibility per il piede (dair) e `rules` su dair+jab; verdetto visivo della skill su r2 (cloud); jab v12.
- Owner al ritorno: barra GSTN in Cascadeur + `install.py`; ok sulla griglia r2 per l'approve.
- Test: tutti ok tranne `test_capture` (Windows, già rosso); nuovi `test_owner_bones_whole_and_stretch_ignored`, `test_composite`.
- Costo stimato: ~45 chiamate, contesto ~95k (~$8) + subagente Sonnet ~116k token (~$1,5).

## Dair: legibility del piede, regole owner su dair e jab v12, video multi-angolo pronto (2026-11-03, thread desktop Opus medium)
- Ramo `claude/project-thread-dgqsku` (worktree `_feature`), commit 2e0a519. Nessun approve, nessun merge su feature.
- **Legibility calci:** la dair spinge giù entrambi i piedi ma la scheda diceva `strike_limb leg_l`: l'altra gamba contava come
  "corpo che nasconde il piede" e giudicavano le soglie del braccio del jab. Ora `sheet_extra.strike = {limb: legs}`; in
  `legibility.configure` le parti delle due gambe di `legs` sono vicine fra loro (toccarsi non è nascondere); un calcio legge
  il blocco `kick` di `legibility_thresholds.json` (letture come il braccio, portata ≥40 cm, ginocchio fuori ≥10 cm), tarato
  su dair v4 e ganon ftilt v3.1 (passano con margine; valori nel `_kick_note`). Test `test_legs_are_one_limb_and_kick_thresholds`.
- **Jab v12:** i FAIL (`contact_faf` picco f0, `fists` 0,95, legibility) erano una build salvata rotta (quasi T-pose, mani
  ferme). Rifatta la build senza toccare la scheda: tutti i check PASS.
- **Regole owner:** `rules` + `_rules_note` su dair (frame_0 charge 3 max_frac 1,0 perché f0 è già la raccolta aerea
  dell'owner; knees_front 0-14; feet_side_by_side min 4 cm) e jab (frame_0 su braccio avanti/tronco/testa, max 0,8;
  piedi ≥15 cm; niente knees_front: la guardia sfalsata ha il ginocchio dietro). Entrambe: check PASS 12/12 e 11/11.
- Diagnosi scritte in `Saved/AnimPipeline/visual/overlord_dair_v4/r2/diagnosi_legibility.md` e `visual/jab_v12/diagnosi.md`.
- Ganon ftilt v3.1: `source_fidelity` FAIL (sorgenti in `refs/owner_ganon_ftilt/*.png`, non in `cards/<kit>/refs/`): non toccato.
- **Video owner (non fatto):** il salvataggio della DefaultLevel dall'editor è stato bloccato dal classificatore di sicurezza
  (anche con l'ok owner); backup già fatto in `Saved/Backups/DefaultLevel_20261103_131028`. L'owner salva di persona e scrive.
  Poi, nel thread nuovo, con l'editor su mappa non sporca: `cd Tools/AnimPipeline; python ingame_multi.py
  cards/overlord/overlord_dair_v4.json 8 <scratch>/overlord_dair_v5_hitstop_vs_whiff.mp4` (`setup` carica L_PilotCapture; 4
  camere lato, 3/4 fronte, dietro, alto; hitstop | whiff), caricarlo in project files, poi riaprire `/Game/Levels/DefaultLevel`.
- Già caricati: `overlord_dair_v4_r2_grid.png`, `overlord_dair_v5_capsule_preview.mp4` (anteprima a capsule senza editor,
  `preview_multi.py`, stesso layout).
- **In sospeso per l'owner:** ok sulla griglia/video per l'`approve` della dair; le altre modifiche di timing/pose che ha
  annunciato ("ci sono altri cambiamenti di timing proposti", 03/11) non sono ancora state scritte: le darà con il video.
  Decisione presa: la dair tiene i pugni chiusi, niente mano da cattivo (owner 03/11).
- Costo stimato: ~75 chiamate, contesto ~100k (~$9).

## Dair: video multi-angolo in-game reso, export verso Cascadeur fermo su .casc esistente (2026-11-03, thread desktop Sonnet)
- Ramo `claude/project-thread-dgqsku`. Nessun approve, nessun merge.
- **Video fatto:** `ingame_multi.py cards/overlord/overlord_dair_v4.json 8 ...` → `overlord_dair_v5_hitstop_vs_whiff.mp4`
  (43 frame, 4 camere lato/3-4 fronte/dietro/alto, hitstop 8 a f15 | whiff). Copia in `Saved/AnimPipeline/` del worktree
  `_feature` e caricata in project files. L'editor è stato rimesso su `/Game/Levels/DefaultLevel` (load_level, nessun save).
- **Cascadeur:** il verbo `import` va Cascadeur→scheda (serve una richiesta "Invia" in `casc/requests/`, ora non ce ne sono);
  per mandare la dair in Cascadeur il verbo è `export overlord_dair_v4` (il nome mossa è quello della scheda, non `dair`).
  Rifiutato: `Saved/AnimPipeline/casc/overlord_dair_v4.casc` esiste già (modificato oggi 16:16, probabili modifiche
  dell'owner). Non ho usato `--force`. Decisione all'owner: aprire quel .casc così com'è, oppure `export --force` (il vecchio
  resta da parte) per rifarlo dalla scheda attuale.
- Costo stimato: ~20 chiamate, contesto ~45k (~$1,5).

## Dair variante A: ritimata windup/release (2026-11-03, thread desktop Sonnet)
- Ramo `claude/project-thread-dgqsku`. Variante `overlord_dair_v4_varA` aperta (snapshot 20261103_182459), build+check PASS,
  grid r1 + video `overlord_dair_v6_varA_hitstop_vs_whiff.mp4`. NON applicata: l'owner decide `variant apply|discard`.
- Richiesta owner (frame = quelli del video, cioè della build): posa del f10 → f12, posa del f13 → f14, f0 più piegato in
  avanti, nulla da f15. Fatto: key 11 (claude) e 14 (owner, spostata su sua richiesta) tolte; nuove key 12 e 14 con le pose
  campionate dalla build ai f10/f13; f0 tronco+spina 60% verso f3 (`source.keep` trunk/head/arm_l/arm_r con why sul f0).
- Curve: restano smoothstep per segmento (già sinusoidali a tratti); le traiettorie dei control non sono state rifatte oltre
  al ritimo. Se vuole di più: key aggiuntive o spacing per posa, da concordare.
- .casc: il file delle 16:16 non è stato toccato né riesportato; Cascadeur script server spento.
- Costo stimato: ~35 chiamate, ~$2,5.

## Dair: variante A applicata, variante B aperta (2026-11-03, thread desktop Sonnet)
- Ramo `claude/project-thread-dgqsku`. A applicata su ok owner ("Ok applica la variante A"): `variant apply` rifiuta perché la
  A cambia le sue key 0 e 14 (chieste da lui); copiata a mano la scheda della variante sull'originale, snapshot 20261103_183835,
  build+check PASS.
- Variante B aperta (`overlord_dair_v4_varB`, build+check PASS, grid r1, video v7), NON applicata: secondary motion dai f19/23/27
  (bacino, testa, mani; in-between claude) e piegamento del busto FAF ridotto al 45% (pelvis+spina ai key 37/42 verso f31,
  `source.keep` con why). Da guardare nel video: se il FAF è ancora troppo piegato/veloce, abbassare il fattore.
- Costo stimato: ~25 chiamate, ~$2.

## Dair: B scartata, variante C aperta (2026-11-03, thread desktop Sonnet)
- B scartata su feedback owner. Variante C (`overlord_dair_v4_varC`, dalla scheda con A applicata): secondary motion ×~2,5 ai
  f19/23/27 (bacino -5/+3,5/-1,5 cm, testa +10/-6/+2,5°, spina, mani) e busto FAF solo "un pelo" (15% da f31 ai key 37/42).
  Build+check PASS, grid r1, video a CAPSULE (`preview_multi.py`): il render in-game è fallito 2 volte, DefaultLevel sporco
  nell'editor dell'owner (setup rifiuta di cambiare mappa). Se vuole il lit, salvi la mappa e rilancio `ingame_multi.py`.
- Non applicata. Costo: ~12 chiamate, ~$1.

## Dair: varianti D/F bloccate da gate numerici (2026-11-03, thread desktop Sonnet)
- **Causa di B e C senza secondary motion:** `build` legge il take scritto da `python -m animpipe author <card.json>`; va lanciato
  prima di ogni `build` quando cambiano le pose. B e C non lo avevano fatto (nessun movimento nel clip). Da ora: author→build→check.
- **Scoperta sulla A applicata:** rigenerato il take (author+build+check sul card principale): `co_motion` FAIL (braccia incollate
  f7-11 / f7-10, effetto del ritimo 10→12, 13→14 della A). Prima risultava PASS perché il check girava su un take vecchio.
  Il clip principale `AS_OverlordDAirV4_Build` ora è ricostruito dalla A.
- **Busto (lean avanti in gradi, `Tools/AnimPipeline/lean.py`):** f0 12,4→26,3 (A); hold -22,3; f37 13,7→-4,3; f42 27,4→8,5 (D/F,
  fattori 0,5/0,62). Il 15% di C dava quasi -17 (indietro): era l'errore.
- **Follow-through (D/F):** bacino scende (mezzo coseno smorzato, periodo 8f) e ritardi spina+1, testa+2, mani+3, ginocchia+2 con
  keys author per frame 18-30 (`dair_followthrough_draft.py`). Picchi sfalsati visibili, ampiezza ±2 cm (poca). Gate FAIL: contact_faf
  (picco reach f18: le key senza `bones` risolvono i piedi diversamente da f17), clearance (mano-spina 0,27 cm), co_motion (eredità A).
- **Proposta:** il follow-through va fatto con `edits` (delta sui control, non nuove key author) e il co_motion della A va risolto
  prima (arm_l/arm_r f7-11 si muovono con il corpo). Variante F aperta, non mostrata all'owner; req in
  `cards/overlord/dair/review/overlord_dair_v4_varF_req.json`. Nessuna immagine nuova: il render è bloccato dai gate.
- Costo stimato: ~35 chiamate, ~$3.
